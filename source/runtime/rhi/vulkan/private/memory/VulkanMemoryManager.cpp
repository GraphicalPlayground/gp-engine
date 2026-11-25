// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanMemoryManager.hpp"
#include "allocators/PoolAllocatorEnums.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleManager.hpp"        // IWYU pragma: keep
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/RHIGlobals.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "memory/VulkanPoolAllocator.hpp"
#include "threading/sync/SharedLock.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <algorithm>
#include <utility>                // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarUsePoolAllocator(
    "r.Vulkan.UsePoolAllocator",
    1,
    "Serve buffer and image allocations from VulkanPoolAllocator pools rather than direct allocations.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarPreCreateManualPools(
    "r.Vulkan.PoolAllocator.PreCreateManualPools",
    1,
    "Pre-create manual sub-allocation pools for uniform buffers and common device-local buffers during init.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarPoolAllocatorPoolSizeKB(
    "r.Vulkan.PoolAllocator.PoolSizeKB",
    0,
    "Overrides pool size in KB for testing multi-pool allocation and release (0 disables override).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarSingleAllocationPerResource(
    "r.Vulkan.SingleAllocationPerResource",
    GP_VULKAN_SINGLE_ALLOCATION_PER_RESOURCE,
    "When non-zero, forces a separate VkDeviceMemory allocation per resource.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarMemoryFallbackToHost(
    "r.Vulkan.MemoryFallbackToHost",
    1,
    "Allows falling back to host memory when device-local memory allocation fails.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Float32> g_cvarEvictionLimitPercentage(
    "r.Vulkan.EvictionLimitPercentage",
    70.0f,
    "Primary heap usage percentage above which evictable resources spill to host memory.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Float32> g_cvarEvictionReenablePercentage(
    "r.Vulkan.EvictionLimitPercentageReenableLimit",
    60.0f,
    "Hysteresis percentage relative to EvictionLimitPercentage before re-enabling local allocations.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

/// @brief Aligns `value` downward to the nearest multiple of `alignment`.
/// @param[in] value Input 64-bit byte size.
/// @param[in] alignment Positive alignment granularity.
/// @return Rounded-down byte size.
[[nodiscard]] constexpr gp::UInt64 alignDownU64(const gp::UInt64 value, const gp::UInt64 alignment) noexcept
{
    if (alignment <= 1u)
    {
        return value;
    }
    return (value / alignment) * alignment;
}

/// @brief Probes representative optimal-tiling images to determine a safe shared image pool alignment.
/// @param[in] device Active `VulkanDevice`.
/// @return Maximum required alignment in bytes across probed image types.
[[nodiscard]] gp::UInt32 probeImagePoolAlignment(const VulkanDevice& device) noexcept
{
    gp::UInt32 alignment = 256u;
    if (!device.isValid() || dynamic_api::vkCreateImage == nullptr ||
        dynamic_api::vkGetImageMemoryRequirements == nullptr || dynamic_api::vkDestroyImage == nullptr)
    {
        return alignment;
    }

    const auto probeImage = [&device, &alignment](const VkFormat format, const VkImageUsageFlags usage) noexcept -> void
    {
        VkImageCreateInfo imageInfo = zeroVulkanStruct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = VkExtent3D{ 4u, 4u, 1u };
        imageInfo.mipLevels = 1u;
        imageInfo.arrayLayers = 1u;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage image = VK_NULL_HANDLE;
        if (vkCreateImage(device.getHandle(), &imageInfo, GP_VULKAN_CPU_ALLOCATOR, &image) != VK_SUCCESS)
        {
            return;
        }

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device.getHandle(), image, &memReqs);
        alignment = gp::math::max<gp::UInt32>(alignment, static_cast<gp::UInt32>(memReqs.alignment));

        vkDestroyImage(device.getHandle(), image, GP_VULKAN_CPU_ALLOCATOR);
    };

    probeImage(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    probeImage(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    probeImage(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    return alignment;
}

/// @brief Computes a conservative buffer pool alignment covering UBO, SSBO, texel buffer, and non-coherent limits.
/// @param[in] device Active `VulkanDevice`.
/// @return Alignment in bytes (`>= 256`).
[[nodiscard]] gp::UInt32 computeBufferPoolAlignment(const VulkanDevice& device) noexcept
{
    const VkPhysicalDeviceLimits& limits = device.getLimits();
    VkDeviceSize alignment = gp::math::max<VkDeviceSize>(
        limits.minUniformBufferOffsetAlignment,
        gp::math::max<VkDeviceSize>(limits.minStorageBufferOffsetAlignment, limits.minTexelBufferOffsetAlignment)
    );
    alignment = gp::math::max<VkDeviceSize>(alignment, limits.nonCoherentAtomSize);
    return gp::math::max<gp::UInt32>(static_cast<gp::UInt32>(alignment), 256u);
}

/// @brief Applies debug pool-size overrides and aligns `poolSize` and `maxAllocationSize` down to `poolAlignment`.
/// @param[in,out] config Pool configuration to finalize.
void finalizePoolConfig(VulkanPoolConfig& config) noexcept
{
    const gp::Int32 overrideKB = g_cvarPoolAllocatorPoolSizeKB.getValueOnAnyThread(true);
    if (overrideKB > 0)
    {
        config.poolSize = static_cast<gp::UInt64>(overrideKB) * 1024u;
        config.maxAllocationSize =
            static_cast<gp::UInt32>(gp::math::min<gp::UInt64>(config.maxAllocationSize, config.poolSize / 2u));
    }

    const gp::UInt64 align64 = gp::math::max<gp::UInt64>(config.poolAlignment, 1u);
    config.poolSize = gp::math::max<gp::UInt64>(alignDownU64(config.poolSize, align64), align64);
    config.maxAllocationSize = static_cast<gp::UInt32>(
        gp::math::min<gp::UInt64>(alignDownU64(config.maxAllocationSize, align64), config.poolSize)
    );
    if (config.maxAllocationSize == 0u)
    {
        config.maxAllocationSize = static_cast<gp::UInt32>(config.poolSize);
    }
}

}   // namespace

VkBuffer createVulkanBuffer(
    const VulkanDevice& device, const VkDeviceSize size, const VkBufferUsageFlags usageFlags
) noexcept
{
    if (!device.isValid() || size == 0u || dynamic_api::vkCreateBuffer == nullptr)
    {
        return VK_NULL_HANDLE;
    }

    VkBufferCreateInfo createInfo = zeroVulkanStruct<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    createInfo.size = size;
    createInfo.usage = usageFlags;

    const gp::Vector<gp::UInt32>& activeFamilies = device.getQueueFamilyIndices().activeQueueFamilies;
    if (activeFamilies.size() > 1u)
    {
        createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<gp::UInt32>(activeFamilies.size());
        createInfo.pQueueFamilyIndices = activeFamilies.data();
    }
    else
    {
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkBuffer buffer = VK_NULL_HANDLE;
    const VkResult result = vkCreateBuffer(device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &buffer);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateBuffer failed ({} bytes, result={}).", size, vulkanResultToString(result));
        return VK_NULL_HANDLE;
    }

    return buffer;
}

VulkanMemoryManager::VulkanMemoryManager(VulkanDevice& device, VulkanDeviceMemoryManager& deviceMemoryManager) noexcept
    : m_device(device)
    , m_deviceMemoryManager(deviceMemoryManager)
{}

VulkanMemoryManager::~VulkanMemoryManager() noexcept
{
    deinit();
}

void VulkanMemoryManager::init() noexcept
{
    if (m_isInitialized || !m_device.isValid() || m_deviceMemoryManager.getNumMemoryTypes() == 0u)
    {
        return;
    }

    m_imagePoolAlignment = probeImagePoolAlignment(m_device);
    m_bufferPoolAlignment = computeBufferPoolAlignment(m_device);
    initPoolAllocators();
    m_isInitialized = true;
}

void VulkanMemoryManager::deinit() noexcept
{
    if (!m_isInitialized)
    {
        return;
    }

    releaseEmptyPools(true, 0u);

    for (gp::USize resourceIdx = 0u; resourceIdx < kVulkanResourceTypeCount; ++resourceIdx)
    {
        for (gp::USize memTypeIdx = 0u; memTypeIdx < VK_MAX_MEMORY_TYPES; ++memTypeIdx)
        {
            m_placedPoolAllocators[resourceIdx][memTypeIdx].reset();
        }
    }

    m_dedicatedManualPoolAllocators.clear();

    {
        gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_manualPoolsMutex);
        m_manualPoolAllocators.clear();
    }

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_directAllocationsMutex);
        for (gp::USize index = 0u; index < m_directAllocations.size(); ++index)
        {
            DirectAllocationSlot& slot = m_directAllocations[index];
            if (slot.ownedBuffer != VK_NULL_HANDLE && m_device.isValid() && dynamic_api::vkDestroyBuffer != nullptr)
            {
                vkDestroyBuffer(m_device.getHandle(), slot.ownedBuffer, GP_VULKAN_CPU_ALLOCATOR);
                slot.ownedBuffer = VK_NULL_HANDLE;
            }
            if (slot.memoryAllocation != nullptr)
            {
                m_deviceMemoryManager.free(slot.memoryAllocation);
            }
        }
        m_directAllocations.clear();
        m_freeDirectSlotIndices.clear();
    }

    m_isInitialized = false;
}

gp::UInt32 VulkanMemoryManager::calculateBufferAlignment(
    const VulkanDevice& device, const VkBufferUsageFlags bufferUsageFlags
) noexcept
{
    const VkPhysicalDeviceLimits& limits = device.getLimits();
    gp::UInt32 alignment = 16u;

    const bool isTexel = (bufferUsageFlags &
                          (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)) != 0u;
    const bool isStorage = (bufferUsageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0u;
    const bool isUniform = (bufferUsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0u;
    const bool isAccelStruct = (bufferUsageFlags & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR) != 0u;

    if (isTexel || isStorage)
    {
        alignment = gp::math::max<gp::UInt32>(alignment, static_cast<gp::UInt32>(limits.minTexelBufferOffsetAlignment));
        alignment =
            gp::math::max<gp::UInt32>(alignment, static_cast<gp::UInt32>(limits.minStorageBufferOffsetAlignment));
    }
    if (isUniform)
    {
        alignment =
            gp::math::max<gp::UInt32>(alignment, static_cast<gp::UInt32>(limits.minUniformBufferOffsetAlignment));
    }
    if (isAccelStruct)
    {
        alignment =
            gp::math::max<gp::UInt32>(alignment, gp::rhi::getRHIGlobals().rayTracing.accelerationStructureAlignment);
    }

    return alignment;
}

gp::Float32 VulkanMemoryManager::calculateBufferPriority(const VkBufferUsageFlags bufferUsageFlags) noexcept
{
    if ((bufferUsageFlags & (VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)) != 0u)
    {
        return kVulkanMemoryHighestPriority;
    }
    if ((bufferUsageFlags & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) != 0u)
    {
        return kVulkanMemoryMediumPriority;
    }
    if ((bufferUsageFlags &
         (VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR)) != 0u)
    {
        return kVulkanMemoryMediumPriority;
    }
    if ((bufferUsageFlags & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) != 0u)
    {
        return kVulkanMemoryLowPriority;
    }
    if ((bufferUsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0u)
    {
        return kVulkanMemoryHigherPriority;
    }
    return kVulkanMemoryMediumPriority;
}

gp::UInt64 VulkanMemoryManager::getPoolSizeForMemoryType(const gp::UInt32 memoryTypeIndex) const noexcept
{
    const VkPhysicalDeviceMemoryProperties& memProps = m_deviceMemoryManager.getMemoryProperties();
    if (memoryTypeIndex >= memProps.memoryTypeCount)
    {
        return kDefaultBufferPoolSize;
    }

    const VkMemoryPropertyFlags propFlags = memProps.memoryTypes[memoryTypeIndex].propertyFlags;
    if ((propFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u &&
        (propFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0u)
    {
        return kStagingPoolPageSize;
    }

    const gp::UInt32 heapIndex = memProps.memoryTypes[memoryTypeIndex].heapIndex;
    const gp::UInt64 heapSize = memProps.memoryHeaps[heapIndex].size;
    return gp::math::max<gp::UInt64>(
        gp::math::min<gp::UInt64>(heapSize / 8u, kGpuOnlyPoolPageSize), kDefaultBufferPoolSize
    );
}

VulkanPoolConfig VulkanMemoryManager::makePlacedPoolConfig(
    const VulkanResourceType resourceType, const gp::UInt32 memoryTypeIndex
) const noexcept
{
    const VkPhysicalDeviceMemoryProperties& memProps = m_deviceMemoryManager.getMemoryProperties();

    VulkanPoolConfig config{};
    config.resourceTypes = (resourceType == VulkanResourceType::Buffer)
                               ? gp::rhi::allocator::PoolResourceTypes::Buffers
                               : (gp::rhi::allocator::PoolResourceTypes::RTDSTextures |
                                  gp::rhi::allocator::PoolResourceTypes::NonRTDSTextures);
    config.strategy = gp::rhi::allocator::ResourceAllocationStrategy::PlacedResource;
    config.memoryTypeIndex = memoryTypeIndex;
    config.poolAlignment = (resourceType == VulkanResourceType::Buffer) ? m_bufferPoolAlignment : m_imagePoolAlignment;
    config.isDeviceLocal =
        (memoryTypeIndex < memProps.memoryTypeCount) &&
        ((memProps.memoryTypes[memoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u);
    config.poolSize = getPoolSizeForMemoryType(memoryTypeIndex);
    config.maxAllocationSize =
        static_cast<gp::UInt32>(gp::math::min<gp::UInt64>(kMaxSubAllocationSize, config.poolSize));

    finalizePoolConfig(config);
    return config;
}

VulkanPoolConfig VulkanMemoryManager::makeManualPoolConfig(
    const VkBufferUsageFlags bufferUsageFlags,
    const VkMemoryPropertyFlags memoryPropertyFlags,
    const gp::UInt64 poolSize
) const noexcept
{
    VulkanPoolConfig config{};
    config.resourceTypes = gp::rhi::allocator::PoolResourceTypes::Buffers;
    config.strategy = gp::rhi::allocator::ResourceAllocationStrategy::ManualSubAllocation;
    config.bufferUsageFlags = bufferUsageFlags;
    config.memoryPropertyFlags = memoryPropertyFlags;
    config.poolAlignment = m_bufferPoolAlignment;
    config.isDeviceLocal = (memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u;
    config.poolSize = poolSize;
    config.maxAllocationSize = static_cast<gp::UInt32>(gp::math::max<gp::UInt64>(poolSize / 4u, m_bufferPoolAlignment));

    finalizePoolConfig(config);
    return config;
}

void VulkanMemoryManager::initPoolAllocators() noexcept
{
    if (g_cvarUsePoolAllocator.getValueOnAnyThread(true) == 0)
    {
        return;
    }

    const gp::UInt32 numMemTypes = m_deviceMemoryManager.getNumMemoryTypes();
    for (gp::UInt32 resourceTypeIdx = 0u; resourceTypeIdx < static_cast<gp::UInt32>(kVulkanResourceTypeCount);
         ++resourceTypeIdx)
    {
        for (gp::UInt32 memTypeIdx = 0u; memTypeIdx < numMemTypes && memTypeIdx < VK_MAX_MEMORY_TYPES; ++memTypeIdx)
        {
            const gp::UInt32 allocatorIdx = (resourceTypeIdx * numMemTypes) + memTypeIdx;
            const VulkanPoolConfig config =
                makePlacedPoolConfig(static_cast<VulkanResourceType>(resourceTypeIdx), memTypeIdx);
            m_placedPoolAllocators[resourceTypeIdx][memTypeIdx] =
                gp::makeUnique<VulkanPoolAllocator>(m_device, m_deviceMemoryManager, config, allocatorIdx);
        }
    }

    if (g_cvarPreCreateManualPools.getValueOnAnyThread(true) != 0)
    {
        // Uniform buffer manual sub-allocator.
        {
            const VkBufferUsageFlags uboUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            const VkMemoryPropertyFlags uboMemFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            const VulkanPoolConfig config = makeManualPoolConfig(uboUsage, uboMemFlags, kUniformBufferPoolSize);
            const gp::UInt32 allocatorIdx =
                getPlacedPoolCount() + static_cast<gp::UInt32>(m_dedicatedManualPoolAllocators.size());
            m_dedicatedManualPoolAllocators.pushBack(
                gp::makeUnique<VulkanPoolAllocator>(m_device, m_deviceMemoryManager, config, allocatorIdx)
            );
        }

        // Common pooled device-local buffer manual sub-allocator.
        {
            VkBufferUsageFlags commonUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                             VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                             VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            if (m_device.getOptionalExtensions().hasBufferDeviceAddress != 0u)
            {
                commonUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            }
            if (m_device.getOptionalExtensions().hasAccelerationStructure != 0u)
            {
                commonUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            }

            const VkMemoryPropertyFlags memFlags =
                m_deviceMemoryManager.hasUnifiedMemory()
                    ? (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                    : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            gp::UInt32 memTypeIndex = 0u;
            const gp::UInt32 allTypeBits = (numMemTypes >= 32u) ? 0xFFFFFFFFu : ((1u << numMemTypes) - 1u);
            if (m_deviceMemoryManager.getMemoryTypeFromProperties(allTypeBits, memFlags, &memTypeIndex) == VK_SUCCESS)
            {
                const VulkanPoolConfig config =
                    makeManualPoolConfig(commonUsage, memFlags, getPoolSizeForMemoryType(memTypeIndex));
                const gp::UInt32 allocatorIdx =
                    getPlacedPoolCount() + static_cast<gp::UInt32>(m_dedicatedManualPoolAllocators.size());
                m_dedicatedManualPoolAllocators.pushBack(
                    gp::makeUnique<VulkanPoolAllocator>(m_device, m_deviceMemoryManager, config, allocatorIdx)
                );
            }
        }
    }
}

VulkanPoolAllocator* VulkanMemoryManager::findOrCreatePoolAllocator(const VulkanPoolConfig& config) noexcept
{
    if (config.strategy == gp::rhi::allocator::ResourceAllocationStrategy::ManualSubAllocation)
    {
        for (gp::USize index = 0u; index < m_dedicatedManualPoolAllocators.size(); ++index)
        {
            if (m_dedicatedManualPoolAllocators[index] &&
                m_dedicatedManualPoolAllocators[index]->supportsAllocation(config))
            {
                return m_dedicatedManualPoolAllocators[index].get();
            }
        }

        {
            gp::thread::SharedLock<gp::thread::SharedMutex> readLock(m_manualPoolsMutex);
            for (gp::USize index = 0u; index < m_manualPoolAllocators.size(); ++index)
            {
                if (m_manualPoolAllocators[index] && m_manualPoolAllocators[index]->supportsAllocation(config))
                {
                    return m_manualPoolAllocators[index].get();
                }
            }
        }

        gp::thread::UniqueLock<gp::thread::SharedMutex> writeLock(m_manualPoolsMutex);
        for (gp::USize index = 0u; index < m_manualPoolAllocators.size(); ++index)
        {
            if (m_manualPoolAllocators[index] && m_manualPoolAllocators[index]->supportsAllocation(config))
            {
                return m_manualPoolAllocators[index].get();
            }
        }

        const gp::UInt32 newAllocatorIdx = getPlacedPoolCount() +
                                           static_cast<gp::UInt32>(m_dedicatedManualPoolAllocators.size()) +
                                           static_cast<gp::UInt32>(m_manualPoolAllocators.size());
        if (newAllocatorIdx >= 0xFFFFu)
        {
            return nullptr;
        }

        m_manualPoolAllocators.pushBack(
            gp::makeUnique<VulkanPoolAllocator>(m_device, m_deviceMemoryManager, config, newAllocatorIdx)
        );
        return m_manualPoolAllocators.back().get();
    }

    const gp::USize resourceIdx = static_cast<gp::USize>(config.getResourceType());
    if (resourceIdx < kVulkanResourceTypeCount && config.memoryTypeIndex < VK_MAX_MEMORY_TYPES)
    {
        return m_placedPoolAllocators[resourceIdx][config.memoryTypeIndex].get();
    }
    return nullptr;
}

bool VulkanMemoryManager::tryAllocateFromPool(
    VulkanAllocation& outAllocation,
    const VulkanPoolConfig& config,
    const gp::UInt32 size,
    const gp::UInt32 alignment,
    const VulkanAllocationType allocType,
    const VulkanAllocationMetaType metaType
) noexcept
{
    if (g_cvarUsePoolAllocator.getValueOnAnyThread(true) == 0)
    {
        return false;
    }

    VulkanPoolAllocator* const allocator = findOrCreatePoolAllocator(config);
    return (allocator != nullptr) && allocator->tryAllocate(outAllocation, size, alignment, allocType, metaType);
}

bool VulkanMemoryManager::allocateDirect(
    VulkanAllocation& outAllocation,
    const VkDeviceSize size,
    const gp::UInt32 memoryTypeIndex,
    const VulkanAllocationType allocType,
    const VulkanAllocationMetaType metaType,
    const bool mapAllocation,
    void* const dedicatedAllocateInfo,
    const gp::Float32 priority,
    const bool isExternal,
    const VkBuffer ownedBuffer
) noexcept
{
    if (size == 0u || size > 0xFFFFFFFFULL)
    {
        return false;
    }

    VulkanDeviceMemoryAllocation* const deviceAlloc =
        m_deviceMemoryManager.alloc(true, size, memoryTypeIndex, dedicatedAllocateInfo, priority, isExternal);
    if (deviceAlloc == nullptr)
    {
        return false;
    }

    if (mapAllocation && deviceAlloc->canBeMapped())
    {
        [[maybe_unused]] void* const mapped = deviceAlloc->map(size, 0u);
    }

    gp::UInt32 slotIndex = 0u;
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_directAllocationsMutex);
        if (!m_freeDirectSlotIndices.isEmpty())
        {
            slotIndex = m_freeDirectSlotIndices.back();
            m_freeDirectSlotIndices.popBack();
            m_directAllocations[slotIndex] = DirectAllocationSlot{ deviceAlloc, ownedBuffer, allocType, metaType };
        }
        else
        {
            slotIndex = static_cast<gp::UInt32>(m_directAllocations.size());
            m_directAllocations.pushBack(DirectAllocationSlot{ deviceAlloc, ownedBuffer, allocType, metaType });
        }
    }

    outAllocation.init(
        allocType,
        metaType,
        reinterpret_cast<gp::UInt64>(ownedBuffer),
        static_cast<gp::UInt32>(size),
        0u,
        0u,
        slotIndex,
        0u
    );
    outAllocation.isPoolAllocated = 0u;
    return true;
}

void VulkanMemoryManager::freeDirectAllocation(VulkanAllocation& allocation) noexcept
{
    VulkanDeviceMemoryAllocation* deviceAlloc = nullptr;
    VkBuffer ownedBuffer = VK_NULL_HANDLE;

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_directAllocationsMutex);
        if (allocation.allocationIndex < m_directAllocations.size())
        {
            DirectAllocationSlot& slot = m_directAllocations[allocation.allocationIndex];
            deviceAlloc = slot.memoryAllocation;
            ownedBuffer = slot.ownedBuffer;
            slot = DirectAllocationSlot{};
            m_freeDirectSlotIndices.pushBack(allocation.allocationIndex);
        }
    }

    if (ownedBuffer != VK_NULL_HANDLE && m_device.isValid() && dynamic_api::vkDestroyBuffer != nullptr)
    {
        vkDestroyBuffer(m_device.getHandle(), ownedBuffer, GP_VULKAN_CPU_ALLOCATOR);
    }
    if (deviceAlloc != nullptr)
    {
        m_deviceMemoryManager.free(deviceAlloc);
    }

    gp::memory::zeroMemory(&allocation, sizeof(allocation));
    allocation.setType(VulkanAllocationType::Empty);
}

bool VulkanMemoryManager::allocateBufferPooled(
    VulkanAllocation& outAllocation,
    const gp::UInt32 size,
    const gp::UInt32 minAlignment,
    const VkBufferUsageFlags bufferUsageFlags,
    const VkMemoryPropertyFlags memoryPropertyFlags,
    const VulkanAllocationMetaType metaType
) noexcept
{
    if (!m_isInitialized || size == 0u)
    {
        return false;
    }

    const gp::UInt32 alignment = gp::math::max(minAlignment, calculateBufferAlignment(m_device, bufferUsageFlags));

    if (g_cvarUsePoolAllocator.getValueOnAnyThread(true) != 0 &&
        g_cvarSingleAllocationPerResource.getValueOnAnyThread(true) == 0)
    {
        const VulkanPoolConfig config = makeManualPoolConfig(bufferUsageFlags, memoryPropertyFlags);
        if (tryAllocateFromPool(outAllocation, config, size, alignment, VulkanAllocationType::PooledBuffer, metaType))
        {
            return true;
        }
    }

    // Fallback: create a standalone VkBuffer + direct VkDeviceMemory.
    VkBuffer buffer = createVulkanBuffer(m_device, size, bufferUsageFlags);
    if (buffer == VK_NULL_HANDLE || dynamic_api::vkGetBufferMemoryRequirements == nullptr ||
        dynamic_api::vkBindBufferMemory == nullptr)
    {
        if (buffer != VK_NULL_HANDLE && dynamic_api::vkDestroyBuffer != nullptr)
        {
            vkDestroyBuffer(m_device.getHandle(), buffer, GP_VULKAN_CPU_ALLOCATOR);
        }
        return false;
    }

    VkMemoryRequirements memReqs{};
    vkGetBufferMemoryRequirements(m_device.getHandle(), buffer, &memReqs);

    gp::UInt32 memTypeIndex = 0u;
    if (m_deviceMemoryManager.getMemoryTypeFromProperties(memReqs.memoryTypeBits, memoryPropertyFlags, &memTypeIndex) !=
        VK_SUCCESS)
    {
        if (dynamic_api::vkDestroyBuffer != nullptr)
        {
            vkDestroyBuffer(m_device.getHandle(), buffer, GP_VULKAN_CPU_ALLOCATOR);
        }
        return false;
    }

    const bool mapAllocation = (memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
    const gp::Float32 priority = calculateBufferPriority(bufferUsageFlags);
    bool allocated = allocateDirect(
        outAllocation,
        memReqs.size,
        memTypeIndex,
        VulkanAllocationType::PooledBuffer,
        metaType,
        mapAllocation,
        nullptr,
        priority,
        false,
        buffer
    );

    if (!allocated && g_cvarMemoryFallbackToHost.getValueOnAnyThread(true) != 0)
    {
        const VkMemoryPropertyFlags fallbackFlags =
            memoryPropertyFlags & ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        gp::UInt32 fallbackTypeIndex = 0u;
        if (m_deviceMemoryManager.getMemoryTypeFromPropertiesExcluding(
                memReqs.memoryTypeBits, fallbackFlags, memTypeIndex, &fallbackTypeIndex
            ) == VK_SUCCESS)
        {
            const bool fallbackMap = (fallbackFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
            allocated = allocateDirect(
                outAllocation,
                memReqs.size,
                fallbackTypeIndex,
                VulkanAllocationType::PooledBuffer,
                metaType,
                fallbackMap,
                nullptr,
                priority,
                false,
                buffer
            );
        }
    }

    if (!allocated)
    {
        if (dynamic_api::vkDestroyBuffer != nullptr)
        {
            vkDestroyBuffer(m_device.getHandle(), buffer, GP_VULKAN_CPU_ALLOCATOR);
        }
        return false;
    }

    const VkResult bindResult = outAllocation.bindBuffer(*this, buffer);
    if (bindResult != VK_SUCCESS)
    {
        freeDirectAllocation(outAllocation);
        return false;
    }

    outAllocation.size = size;
    return true;
}

bool VulkanMemoryManager::allocateBufferMemory(
    VulkanAllocation& outAllocation,
    const VkBuffer buffer,
    VulkanAllocationFlags allocFlags,
    const char* const debugName,
    const gp::UInt32 forceMinAlignment
) noexcept
{
    if (!m_isInitialized || !m_device.isValid() || buffer == VK_NULL_HANDLE)
    {
        return false;
    }

    VkMemoryRequirements memReqs{};
    bool prefersDedicated = false;

    if (dynamic_api::vkGetBufferMemoryRequirements2 != nullptr)
    {
        VkBufferMemoryRequirementsInfo2 reqInfo =
            zeroVulkanStruct<VkBufferMemoryRequirementsInfo2>(VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2);
        reqInfo.buffer = buffer;

        VkMemoryDedicatedRequirements dedicatedReqs =
            zeroVulkanStruct<VkMemoryDedicatedRequirements>(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS);
        VkMemoryRequirements2 reqs2 = zeroVulkanStruct<VkMemoryRequirements2>(VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2);
        reqs2.pNext = &dedicatedReqs;

        vkGetBufferMemoryRequirements2(m_device.getHandle(), &reqInfo, &reqs2);
        memReqs = reqs2.memoryRequirements;
        prefersDedicated = (dedicatedReqs.requiresDedicatedAllocation != VK_FALSE) ||
                           (dedicatedReqs.prefersDedicatedAllocation != VK_FALSE);
    }
    else if (dynamic_api::vkGetBufferMemoryRequirements != nullptr)
    {
        vkGetBufferMemoryRequirements(m_device.getHandle(), buffer, &memReqs);
    }
    else
    {
        return false;
    }

    memReqs.alignment = gp::math::max<VkDeviceSize>(memReqs.alignment, static_cast<VkDeviceSize>(forceMinAlignment));
    if (prefersDedicated)
    {
        allocFlags |= VulkanAllocationFlags::Dedicated;
    }

    VkMemoryPropertyFlags memPropFlags = getMemoryPropertyFlags(allocFlags, m_deviceMemoryManager.hasUnifiedMemory());
    const bool preferBAR = hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::PreferBAR);
    const bool isExternal = hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::External);
    const bool forceSeparate = hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::Dedicated) || isExternal ||
                               (g_cvarSingleAllocationPerResource.getValueOnAnyThread(true) != 0);

    gp::UInt32 memTypeIndex = 0u;
    VkResult findResult =
        m_deviceMemoryManager.getMemoryTypeFromProperties(memReqs.memoryTypeBits, memPropFlags, &memTypeIndex);
    if (findResult != VK_SUCCESS)
    {
        if ((memPropFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0u)
        {
            memPropFlags &= ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        }
        if ((memPropFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0u)
        {
            memPropFlags &= ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
        }
        if (preferBAR)
        {
            memPropFlags &= ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }

        findResult =
            m_deviceMemoryManager.getMemoryTypeFromProperties(memReqs.memoryTypeBits, memPropFlags, &memTypeIndex);
        if (findResult != VK_SUCCESS)
        {
            return false;
        }
    }

    bool allocated = false;
    if (!forceSeparate && memReqs.size <= 0xFFFFFFFFULL)
    {
        const VulkanPoolConfig config = makePlacedPoolConfig(VulkanResourceType::Buffer, memTypeIndex);
        allocated = tryAllocateFromPool(
            outAllocation,
            config,
            static_cast<gp::UInt32>(memReqs.size),
            static_cast<gp::UInt32>(memReqs.alignment),
            VulkanAllocationType::Buffer,
            VulkanAllocationMetaType::BufferOther
        );
    }

    if (!allocated)
    {
        const bool mapAllocation = (memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
        VkMemoryDedicatedAllocateInfo dedicatedInfo =
            zeroVulkanStruct<VkMemoryDedicatedAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
        dedicatedInfo.buffer = buffer;
        void* const dedicatedPtr = hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::Dedicated)
                                       ? static_cast<void*>(&dedicatedInfo)
                                       : nullptr;

        allocated = allocateDirect(
            outAllocation,
            memReqs.size,
            memTypeIndex,
            VulkanAllocationType::Buffer,
            VulkanAllocationMetaType::BufferOther,
            mapAllocation,
            dedicatedPtr,
            kVulkanMemoryMediumPriority,
            isExternal
        );

        if (!allocated && (preferBAR || g_cvarMemoryFallbackToHost.getValueOnAnyThread(true) != 0))
        {
            const VkMemoryPropertyFlags fallbackFlags =
                memPropFlags & ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            gp::UInt32 fallbackTypeIndex = 0u;
            if (m_deviceMemoryManager.getMemoryTypeFromPropertiesExcluding(
                    memReqs.memoryTypeBits, fallbackFlags, memTypeIndex, &fallbackTypeIndex
                ) == VK_SUCCESS)
            {
                const bool fallbackMap = (fallbackFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
                allocated = allocateDirect(
                    outAllocation,
                    memReqs.size,
                    fallbackTypeIndex,
                    VulkanAllocationType::Buffer,
                    VulkanAllocationMetaType::BufferOther,
                    fallbackMap,
                    dedicatedPtr,
                    kVulkanMemoryMediumPriority,
                    isExternal
                );
            }
        }
    }

    if (!allocated)
    {
        return false;
    }

    if (hasAnyAllocationFlags(allocFlags, VulkanAllocationFlags::AutoBind))
    {
        const VkResult bindResult = outAllocation.bindBuffer(*this, buffer);
        if (bindResult != VK_SUCCESS)
        {
            freeVulkanAllocation(outAllocation, VulkanFreeFlags::DontDefer);
            return false;
        }
    }

    if (debugName != nullptr)
    {
        m_device.setObjectName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<gp::UInt64>(buffer), debugName);
    }

    return true;
}

bool VulkanMemoryManager::allocateImageMemory(
    VulkanAllocation& outAllocation,
    const VkMemoryRequirements& memoryReqs,
    VkMemoryPropertyFlags memoryPropertyFlags,
    const VulkanAllocationMetaType metaType,
    const bool isExternal
) noexcept
{
    if (!m_isInitialized || memoryReqs.size == 0u)
    {
        return false;
    }

    if (!m_deviceMemoryManager.hasUnifiedMemory() && metaType == VulkanAllocationMetaType::ImageOther &&
        memoryPropertyFlags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && updateEvictThreshold())
    {
        memoryPropertyFlags = m_deviceMemoryManager.getEvictedMemoryProperties();
    }

    gp::UInt32 memTypeIndex = 0u;
    if (m_deviceMemoryManager.getMemoryTypeFromProperties(
            memoryReqs.memoryTypeBits, memoryPropertyFlags, &memTypeIndex
        ) != VK_SUCCESS)
    {
        if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0u)
        {
            memoryPropertyFlags &= ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
            if (m_deviceMemoryManager.getMemoryTypeFromProperties(
                    memoryReqs.memoryTypeBits, memoryPropertyFlags, &memTypeIndex
                ) != VK_SUCCESS)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    const bool forceSeparate = isExternal || ((memoryPropertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0u) ||
                               (g_cvarSingleAllocationPerResource.getValueOnAnyThread(true) != 0);

    if (!forceSeparate && memoryReqs.size <= 0xFFFFFFFFULL)
    {
        const VulkanPoolConfig config = makePlacedPoolConfig(VulkanResourceType::Image, memTypeIndex);
        if (tryAllocateFromPool(
                outAllocation,
                config,
                static_cast<gp::UInt32>(memoryReqs.size),
                static_cast<gp::UInt32>(memoryReqs.alignment),
                VulkanAllocationType::Image,
                metaType
            ))
        {
            return true;
        }
    }

    const bool mapAllocation = (memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
    if (allocateDirect(
            outAllocation,
            memoryReqs.size,
            memTypeIndex,
            VulkanAllocationType::Image,
            metaType,
            mapAllocation,
            nullptr,
            kVulkanMemoryHighestPriority,
            isExternal
        ))
    {
        return true;
    }

    if (g_cvarMemoryFallbackToHost.getValueOnAnyThread(true) != 0)
    {
        const VkMemoryPropertyFlags fallbackFlags =
            memoryPropertyFlags & ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        gp::UInt32 fallbackTypeIndex = 0u;
        if (m_deviceMemoryManager.getMemoryTypeFromPropertiesExcluding(
                memoryReqs.memoryTypeBits, fallbackFlags, memTypeIndex, &fallbackTypeIndex
            ) == VK_SUCCESS)
        {
            const bool fallbackMap = (fallbackFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
            return allocateDirect(
                outAllocation,
                memoryReqs.size,
                fallbackTypeIndex,
                VulkanAllocationType::Image,
                metaType,
                fallbackMap,
                nullptr,
                kVulkanMemoryMediumPriority,
                isExternal
            );
        }
    }

    return false;
}

bool VulkanMemoryManager::allocateDedicatedImageMemory(
    VulkanAllocation& outAllocation,
    const VkImage image,
    VkMemoryPropertyFlags memoryPropertyFlags,
    const VulkanAllocationMetaType metaType,
    const bool isExternal,
    const bool forceDedicatedAllocation
) noexcept
{
    if (!m_isInitialized || !m_device.isValid() || image == VK_NULL_HANDLE)
    {
        return false;
    }

    VkMemoryRequirements memReqs{};
    bool prefersDedicated = false;

    if (dynamic_api::vkGetImageMemoryRequirements2 != nullptr)
    {
        VkImageMemoryRequirementsInfo2 reqInfo =
            zeroVulkanStruct<VkImageMemoryRequirementsInfo2>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2);
        reqInfo.image = image;

        VkMemoryDedicatedRequirements dedicatedReqs =
            zeroVulkanStruct<VkMemoryDedicatedRequirements>(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS);
        VkMemoryRequirements2 reqs2 = zeroVulkanStruct<VkMemoryRequirements2>(VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2);
        reqs2.pNext = &dedicatedReqs;

        vkGetImageMemoryRequirements2(m_device.getHandle(), &reqInfo, &reqs2);
        memReqs = reqs2.memoryRequirements;
        prefersDedicated = (dedicatedReqs.requiresDedicatedAllocation != VK_FALSE) ||
                           (dedicatedReqs.prefersDedicatedAllocation != VK_FALSE);
    }
    else if (dynamic_api::vkGetImageMemoryRequirements != nullptr)
    {
        vkGetImageMemoryRequirements(m_device.getHandle(), image, &memReqs);
    }
    else
    {
        return false;
    }

    if (forceDedicatedAllocation || prefersDedicated)
    {
        gp::UInt32 memTypeIndex = 0u;
        if (m_deviceMemoryManager.getMemoryTypeFromProperties(
                memReqs.memoryTypeBits, memoryPropertyFlags, &memTypeIndex
            ) != VK_SUCCESS)
        {
            return false;
        }

        VkMemoryDedicatedAllocateInfo dedicatedInfo =
            zeroVulkanStruct<VkMemoryDedicatedAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
        dedicatedInfo.image = image;

        if (allocateDirect(
                outAllocation,
                memReqs.size,
                memTypeIndex,
                VulkanAllocationType::ImageDedicated,
                metaType,
                false,
                &dedicatedInfo,
                kVulkanMemoryHighestPriority,
                isExternal
            ))
        {
            return true;
        }

        if (g_cvarMemoryFallbackToHost.getValueOnAnyThread(true) != 0)
        {
            const VkMemoryPropertyFlags fallbackFlags =
                memoryPropertyFlags & ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            gp::UInt32 fallbackTypeIndex = 0u;
            if (m_deviceMemoryManager.getMemoryTypeFromPropertiesExcluding(
                    memReqs.memoryTypeBits, fallbackFlags, memTypeIndex, &fallbackTypeIndex
                ) == VK_SUCCESS)
            {
                return allocateDirect(
                    outAllocation,
                    memReqs.size,
                    fallbackTypeIndex,
                    VulkanAllocationType::ImageDedicated,
                    metaType,
                    false,
                    &dedicatedInfo,
                    kVulkanMemoryMediumPriority,
                    isExternal
                );
            }
        }
        return false;
    }

    return allocateImageMemory(outAllocation, memReqs, memoryPropertyFlags, metaType, isExternal);
}

void VulkanMemoryManager::freeVulkanAllocation(VulkanAllocation& allocation, const VulkanFreeFlags freeFlags) noexcept
{
    if (!allocation.hasAllocation())
    {
        return;
    }

    const bool dontDefer =
        (static_cast<gp::UInt8>(freeFlags) & static_cast<gp::UInt8>(VulkanFreeFlags::DontDefer)) != 0u;
    if (!dontDefer && m_deferredDeletionQueue != nullptr)
    {
        m_deferredDeletionQueue->enqueueResourceAllocation(allocation);
        gp::memory::zeroMemory(&allocation, sizeof(allocation));
        allocation.setType(VulkanAllocationType::Empty);
        return;
    }

    if (allocation.isPoolAllocated != 0u)
    {
        freePoolAllocations(gp::VectorView<VulkanAllocation>(&allocation, 1u));
        return;
    }

    freeDirectAllocation(allocation);
}

void VulkanMemoryManager::freePoolAllocations(gp::VectorView<VulkanAllocation> allocations) noexcept
{
    if (allocations.isEmpty())
    {
        return;
    }

    for (gp::USize index = 0u; index < allocations.size(); ++index)
    {
        allocations[index].disown();
    }

    std::sort(
        allocations.begin(),
        allocations.end(),
        [](const VulkanAllocation& lhs, const VulkanAllocation& rhs) noexcept -> bool
    {
        return lhs.allocatorIndex < rhs.allocatorIndex;
    }
    );

    gp::USize runStart = 0u;
    while (runStart < allocations.size())
    {
        const gp::UInt16 runAllocatorIndex = allocations[runStart].allocatorIndex;
        gp::USize runEnd = runStart + 1u;
        while (runEnd < allocations.size() && allocations[runEnd].allocatorIndex == runAllocatorIndex)
        {
            ++runEnd;
        }

        gp::VectorView<VulkanAllocation> run(allocations.data() + runStart, runEnd - runStart);
        VulkanPoolAllocator* const poolAllocator = getPoolAllocator(runAllocatorIndex);
        if (poolAllocator != nullptr)
        {
            poolAllocator->freeBatch(run);
        }

        for (gp::USize idx = 0u; idx < run.size(); ++idx)
        {
            gp::memory::zeroMemory(&run[idx], sizeof(VulkanAllocation));
            run[idx].setType(VulkanAllocationType::Empty);
        }

        runStart = runEnd;
    }
}

void VulkanMemoryManager::getPoolAllocatorsSnapshot(gp::Vector<VulkanPoolAllocator*>& outAllocators) noexcept
{
    outAllocators.clear();
    const gp::UInt32 numMemTypes = m_deviceMemoryManager.getNumMemoryTypes();

    for (gp::USize resourceIdx = 0u; resourceIdx < kVulkanResourceTypeCount; ++resourceIdx)
    {
        for (gp::UInt32 memTypeIdx = 0u; memTypeIdx < numMemTypes && memTypeIdx < VK_MAX_MEMORY_TYPES; ++memTypeIdx)
        {
            if (m_placedPoolAllocators[resourceIdx][memTypeIdx])
            {
                outAllocators.pushBack(m_placedPoolAllocators[resourceIdx][memTypeIdx].get());
            }
        }
    }

    for (gp::USize index = 0u; index < m_dedicatedManualPoolAllocators.size(); ++index)
    {
        if (m_dedicatedManualPoolAllocators[index])
        {
            outAllocators.pushBack(m_dedicatedManualPoolAllocators[index].get());
        }
    }

    gp::thread::SharedLock<gp::thread::SharedMutex> readLock(m_manualPoolsMutex);
    for (gp::USize index = 0u; index < m_manualPoolAllocators.size(); ++index)
    {
        if (m_manualPoolAllocators[index])
        {
            outAllocators.pushBack(m_manualPoolAllocators[index].get());
        }
    }
}

void VulkanMemoryManager::releaseEmptyPools(const bool forceFree, const gp::UInt32 currentFrame) noexcept
{
    gp::UInt32 frameLag = 0u;
    if (gp::console::IConsoleVariable* const cvar =
            gp::console::ConsoleManager::get().findConsoleVariable("r.Vulkan.NumFramesToWaitForResourceDelete");
        cvar != nullptr)
    {
        frameLag = static_cast<gp::UInt32>(gp::math::max<gp::Int32>(0, cvar->getInt()));
    }

    gp::Vector<VulkanPoolAllocator*> snapshot{};
    getPoolAllocatorsSnapshot(snapshot);

    for (gp::USize index = 0u; index < snapshot.size(); ++index)
    {
        snapshot[index]->releaseEmptyPools(frameLag, forceFree, currentFrame);
    }
}

void VulkanMemoryManager::releaseFreedPages(const gp::UInt32 currentFrame) noexcept
{
    releaseEmptyPools(false, currentFrame);
    m_deviceMemoryManager.trimMemory(false, currentFrame);
}

bool VulkanMemoryManager::updateEvictThreshold() noexcept
{
    gp::UInt64 primaryAllocated = 0u;
    gp::UInt64 primaryLimit = 1u;
    m_deviceMemoryManager.getPrimaryHeapStatus(primaryAllocated, primaryLimit);

    const gp::Float64 allocatedPct = 100.0 * static_cast<gp::Float64>(primaryAllocated) /
                                     static_cast<gp::Float64>(gp::math::max<gp::UInt64>(primaryLimit, 1u));
    gp::Float64 evictionLimit = static_cast<gp::Float64>(g_cvarEvictionLimitPercentage.getValueOnAnyThread(true));
    if (m_isEvicting)
    {
        evictionLimit *= static_cast<gp::Float64>(g_cvarEvictionReenablePercentage.getValueOnAnyThread(true)) / 100.0;
    }

    m_isEvicting = (allocatedPct > evictionLimit);
    return m_isEvicting;
}

VulkanPoolAllocator* VulkanMemoryManager::getPoolAllocator(const gp::UInt32 allocatorIndex) noexcept
{
    const gp::UInt32 placedCount = getPlacedPoolCount();
    const gp::UInt32 numMemTypes = m_deviceMemoryManager.getNumMemoryTypes();

    if (allocatorIndex < placedCount && numMemTypes > 0u)
    {
        const gp::UInt32 resourceIdx = allocatorIndex / numMemTypes;
        const gp::UInt32 memTypeIdx = allocatorIndex % numMemTypes;
        if (resourceIdx < kVulkanResourceTypeCount && memTypeIdx < VK_MAX_MEMORY_TYPES)
        {
            return m_placedPoolAllocators[resourceIdx][memTypeIdx].get();
        }
        return nullptr;
    }

    const gp::UInt32 dedicatedEnd = placedCount + static_cast<gp::UInt32>(m_dedicatedManualPoolAllocators.size());
    if (allocatorIndex < dedicatedEnd)
    {
        return m_dedicatedManualPoolAllocators[allocatorIndex - placedCount].get();
    }

    gp::thread::SharedLock<gp::thread::SharedMutex> readLock(m_manualPoolsMutex);
    const gp::UInt32 manualIdx = allocatorIndex - dedicatedEnd;
    return (manualIdx < m_manualPoolAllocators.size()) ? m_manualPoolAllocators[manualIdx].get() : nullptr;
}

VulkanDeviceMemoryAllocation*
    VulkanMemoryManager::getDeviceMemoryAllocation(const VulkanAllocation& allocation) noexcept
{
    if (!allocation.isValid())
    {
        return nullptr;
    }

    if (allocation.isPoolAllocated != 0u)
    {
        VulkanPoolAllocator* const poolAllocator = getPoolAllocator(allocation.allocatorIndex);
        if (poolAllocator == nullptr)
        {
            return nullptr;
        }
        VulkanMemoryPool* const pool = poolAllocator->getPool(allocation);
        return (pool != nullptr) ? pool->getMemoryAllocation() : nullptr;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_directAllocationsMutex);
    return (allocation.allocationIndex < m_directAllocations.size())
               ? m_directAllocations[allocation.allocationIndex].memoryAllocation
               : nullptr;
}

void VulkanMemoryManager::getMemoryDump(VulkanResourceHeapStats& outStats) noexcept
{
    outStats = VulkanResourceHeapStats{};

    gp::Vector<VulkanPoolAllocator*> snapshot{};
    getPoolAllocatorsSnapshot(snapshot);
    for (gp::USize index = 0u; index < snapshot.size(); ++index)
    {
        snapshot[index]->getMemoryDump(outStats);
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_directAllocationsMutex);
    for (gp::USize index = 0u; index < m_directAllocations.size(); ++index)
    {
        const DirectAllocationSlot& slot = m_directAllocations[index];
        if (slot.memoryAllocation == nullptr)
        {
            continue;
        }

        const gp::UInt64 bytes = slot.memoryAllocation->getSize();
        outStats.totalMemory += bytes;
        outStats.pages += 1u;
        if (slot.type == VulkanAllocationType::Image || slot.type == VulkanAllocationType::ImageDedicated)
        {
            outStats.imageAllocations += 1u;
            outStats.usedImageMemory += bytes;
            outStats.imagePages += 1u;
        }
        else
        {
            outStats.bufferAllocations += 1u;
            outStats.usedBufferMemory += bytes;
            outStats.bufferPages += 1u;
        }
    }
}

}   // namespace gp::rhi::vulkan
