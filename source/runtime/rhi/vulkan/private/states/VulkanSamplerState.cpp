// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanSamplerState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "device/VulkanPhysicalDeviceFeatures.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "hashing/Crc32.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/SamplerStateDesc.hpp"
#include "states/VulkanStateSettings.hpp"
#include "threading/atomics/Atomic.hpp"
#include "translation/VulkanSamplerEnums.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Source of process-unique sampler identifiers.
gp::thread::Atomic<gp::UInt32> g_samplerIdCounter{ 0u };

/// @brief Reports whether `filter` requests anisotropic filtering.
/// @param[in] filter RHI sampler filter.
/// @return True for both anisotropic filters.
[[nodiscard]] constexpr bool isAnisotropicFilter(const pipeline::SamplerFilter filter) noexcept
{
    return (filter == pipeline::SamplerFilter::AnisotropicLinear) ||
           (filter == pipeline::SamplerFilter::AnisotropicPoint);
}

}   // namespace

VulkanSamplerState::VulkanSamplerState(
    VulkanDevice& device,
    const VkSamplerCreateInfo& createInfo,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const bool isImmutable
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
    , m_samplerId(g_samplerIdCounter.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed) + 1u)
    , m_isImmutable(isImmutable)
{
    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreateSampler == nullptr))
    {
        return;
    }

    const VkResult result = vkCreateSampler(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_sampler);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateSampler failed with {}.", vulkanResultToString(result));
        m_sampler = VK_NULL_HANDLE;
    }
}

VulkanSamplerState::~VulkanSamplerState() noexcept
{
    if (m_sampler == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_deferredDeletionQueue != nullptr)
    {
        m_deferredDeletionQueue->enqueueHandle(VulkanDeferredDeletionQueue::EntryType::Sampler, m_sampler);
    }
    else if (dynamic_api::vkDestroySampler != nullptr)
    {
        vkDestroySampler(m_device.getHandle(), m_sampler, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_sampler = VK_NULL_HANDLE;
}

void VulkanSamplerState::setupSamplerCreateInfo(
    const pipeline::SamplerStateDesc& desc, const VulkanDevice& device, VkSamplerCreateInfo& outCreateInfo
) noexcept
{
    // Zeroed in place so the padding after `sType` is deterministic: the whole struct is hashed for deduplication.
    zeroVulkanStruct(outCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

    outCreateInfo.magFilter = samplerFilterToVkFilter(desc.filter);
    outCreateInfo.minFilter = samplerFilterToVkFilter(desc.filter);
    outCreateInfo.mipmapMode = samplerFilterToVkMipmapMode(desc.filter);
    outCreateInfo.addressModeU = samplerAddressModeToVkAddressMode(desc.addressU);
    outCreateInfo.addressModeV = samplerAddressModeToVkAddressMode(desc.addressV);
    outCreateInfo.addressModeW = samplerAddressModeToVkAddressMode(desc.addressW);
    outCreateInfo.mipLodBias = desc.mipBias;

    outCreateInfo.maxAnisotropy = 1.0f;
    const bool hasAnisotropyFeature = device.getFeatures().getCore10().samplerAnisotropy == VK_TRUE;
    if (isAnisotropicFilter(desc.filter) && hasAnisotropyFeature)
    {
        const gp::Float32 deviceLimit = gp::math::max(1.0f, device.getLimits().maxSamplerAnisotropy);
        const gp::Float32 requested = static_cast<gp::Float32>(computeVulkanSamplerAnisotropy(desc.maxAnisotropy));
        outCreateInfo.maxAnisotropy = gp::math::clamp(requested, 1.0f, deviceLimit);
    }
    outCreateInfo.anisotropyEnable = (outCreateInfo.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;

    outCreateInfo.compareEnable =
        (desc.samplerComparisonFunction != pipeline::SamplerCompareFunction::Never) ? VK_TRUE : VK_FALSE;
    outCreateInfo.compareOp = samplerCompareFunctionToVkCompareOp(desc.samplerComparisonFunction);
    outCreateInfo.minLod = desc.minMipLevel;
    outCreateInfo.maxLod = desc.maxMipLevel;
    outCreateInfo.borderColor =
        (desc.borderColor == 0u) ? VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK : VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
}

gp::UInt32 VulkanSamplerState::computeCreateInfoHash(const VkSamplerCreateInfo& createInfo) noexcept
{
    // Chained structures would make the hash depend on pointer values rather than on their contents.
    GP_CHECK(createInfo.pNext == nullptr);

    gp::hashing::Crc32 hasher;
    hasher.update(static_cast<const void*>(&createInfo), sizeof(createInfo));
    return hasher.finalize();
}

bool VulkanSamplerState::isImmutable() const noexcept
{
    return m_isImmutable;
}

view::DescriptorHandle VulkanSamplerState::getBindlessHandle() const noexcept
{
    return m_bindlessHandle;
}

}   // namespace gp::rhi::vulkan
