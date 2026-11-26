// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTextureSize.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureStats.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "RHIPixelFormat.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief One cached platform size query.
struct TextureSizeCacheEntry final
{
    gp::UInt64 hash{ 0u };
    resource::TextureDesc desc{};
    resource::CalcTextureSizeResult result{};
};

/// @brief Process-wide cache of platform size queries.
struct TextureSizeCache final
{
    gp::thread::Mutex mutex{};
    gp::Vector<TextureSizeCacheEntry> entries{};
};

/// @brief Returns the process-wide size cache.
/// @return Cache shared by every query.
[[nodiscard]] TextureSizeCache& getTextureSizeCache() noexcept
{
    static TextureSizeCache s_cache{};
    return s_cache;
}

/// @brief Builds the descriptor of the mip chain resident from `firstMipIndex`.
/// @param[in] desc Full descriptor.
/// @param[in] firstMipIndex First resident mip (below `desc.numMips`).
/// @return Descriptor starting at that mip.
[[nodiscard]] resource::TextureDesc
    makeResidentDesc(const resource::TextureDesc& desc, const gp::UInt32 firstMipIndex) noexcept
{
    resource::TextureDesc residentDesc = desc;
    if (firstMipIndex > 0u)
    {
        residentDesc.extent.x = gp::math::max<gp::Int32>(1, desc.extent.x >> firstMipIndex);
        residentDesc.extent.y = gp::math::max<gp::Int32>(1, desc.extent.y >> firstMipIndex);
        if (desc.isTexture3D())
        {
            residentDesc.depth = static_cast<gp::UInt16>(gp::math::max<gp::UInt32>(1u, desc.depth >> firstMipIndex));
        }
        residentDesc.numMips = static_cast<gp::UInt8>(desc.numMips - firstMipIndex);
    }
    return residentDesc;
}

/// @brief Queries the memory requirements of an image the device would create from `desc`.
/// @param[in] services Texture services.
/// @param[in] desc Resident descriptor.
/// @param[out] outRequirements Receives the requirements.
/// @return False when the descriptor does not fit the device or the query failed.
[[nodiscard]] bool queryImageMemoryRequirements(
    const VulkanResourceServices& services, const resource::TextureDesc& desc, VkMemoryRequirements& outRequirements
) noexcept
{
    const VulkanDevice& device = *services.device;
    const bool supportsMemoryless =
        services.memoryManager->getDeviceMemoryManager().supportsMemoryType(VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);

    VulkanImageCreateInfo createInfo{};
    VkFormat viewFormat = VK_FORMAT_UNDEFINED;
    VkFormat storageFormat = VK_FORMAT_UNDEFINED;
    if (!generateVulkanImageCreateInfo(device, desc, false, supportsMemoryless, createInfo, viewFormat, storageFormat))
    {
        return false;
    }

    const VkDevice deviceHandle = device.getHandle();
    if (device.getOptionalExtensions().hasKHRMaintenance4 != 0u &&
        dynamic_api::vkGetDeviceImageMemoryRequirementsKHR != nullptr)
    {
        // No image needed: the driver answers from the create info alone.
        VkDeviceImageMemoryRequirements requirementsInfo =
            zeroVulkanStruct<VkDeviceImageMemoryRequirements>(VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS);
        requirementsInfo.pCreateInfo = &createInfo.imageCreateInfo;
        VkMemoryRequirements2 requirements =
            zeroVulkanStruct<VkMemoryRequirements2>(VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2);
        dynamic_api::vkGetDeviceImageMemoryRequirementsKHR(deviceHandle, &requirementsInfo, &requirements);
        outRequirements = requirements.memoryRequirements;
        return true;
    }

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(deviceHandle, &createInfo.imageCreateInfo, GP_VULKAN_CPU_ALLOCATOR, &image) != VK_SUCCESS)
    {
        return false;
    }
    vkGetImageMemoryRequirements(deviceHandle, image, &outRequirements);
    vkDestroyImage(deviceHandle, image, GP_VULKAN_CPU_ALLOCATOR);
    return true;
}

}   // namespace

resource::CalcTextureSizeResult calcVulkanTexturePlatformSize(
    const VulkanResourceServices& services, const resource::TextureDesc& desc, const gp::UInt32 firstMipIndex
) noexcept
{
    if (!services.isValid() || !isValidPixelFormat(desc.format) || desc.extent.x <= 0 || desc.extent.y <= 0 ||
        desc.numMips == 0u || firstMipIndex >= static_cast<gp::UInt32>(desc.numMips))
    {
        return resource::CalcTextureSizeResult{};
    }

    const resource::TextureDesc residentDesc = makeResidentDesc(desc, firstMipIndex);
    const gp::UInt64 hash = residentDesc.getHash();

    TextureSizeCache& cache = getTextureSizeCache();
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
        for (const TextureSizeCacheEntry& entry: cache.entries)
        {
            if (entry.hash == hash && entry.desc == residentDesc)
            {
                return entry.result;
            }
        }
    }

    VkMemoryRequirements requirements{};
    if (!queryImageMemoryRequirements(services, residentDesc, requirements))
    {
        return resource::CalcTextureSizeResult{};
    }

    const resource::CalcTextureSizeResult result{ static_cast<gp::UInt64>(requirements.size),
                                                  static_cast<gp::UInt32>(requirements.alignment) };

    // Two threads may race to insert the same descriptor; the duplicate is harmless and the lookup returns the first.
    gp::thread::UniqueLock<gp::thread::Mutex> lock(cache.mutex);
    cache.entries.emplaceBack(TextureSizeCacheEntry{ hash, residentDesc, result });
    return result;
}

gp::UInt64 computeVulkanTextureMemorySize(resource::Texture* const texture) noexcept
{
    if (texture == nullptr)
    {
        return 0u;
    }
    return static_cast<gp::UInt64>(static_cast<VulkanTexture*>(texture)->getMemoryRequirements().size);
}

void fillVulkanTextureMemoryStats(
    const VulkanResourceServices& services, resource::TextureMemoryStats& outStats
) noexcept
{
    outStats = resource::TextureMemoryStats{};
    if (services.memoryManager == nullptr)
    {
        return;
    }

    VulkanDeviceMemoryManager& deviceMemory = services.memoryManager->getDeviceMemoryManager();
    const gp::Int64 deviceLocalBytes = static_cast<gp::Int64>(deviceMemory.getTotalMemory(true));
    const gp::Int64 hostBytes = static_cast<gp::Int64>(deviceMemory.getTotalMemory(false));

    outStats.dedicatedVideoMemory = deviceLocalBytes;
    outStats.dedicatedSystemMemory = deviceMemory.hasUnifiedMemory() ? 0 : hostBytes;
    outStats.sharedSystemMemory = -1;
    outStats.totalGraphicsMemory = deviceLocalBytes;
    outStats.nonStreamingMemorySize = getVulkanTextureMemoryStats().totalTextureBytes;
    outStats.streamingMemorySize = 0;
    outStats.texturePoolSize = 0;
}

}   // namespace gp::rhi::vulkan
