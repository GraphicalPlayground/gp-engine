// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorPool.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

VulkanDescriptorPool::VulkanDescriptorPool(
    VulkanDevice& device, const VulkanDescriptorSetsLayout& layout, const gp::UInt32 maxSetsAllocations
) noexcept
    : VulkanDescriptorPool(
          device, layout.getLayoutTypes(), static_cast<gp::UInt32>(layout.getLayouts().size()), maxSetsAllocations
      )
{}

VulkanDescriptorPool::VulkanDescriptorPool(
    VulkanDevice& device,
    const VulkanDescriptorTypeCounts& typeCounts,
    const gp::UInt32 numSetLayouts,
    const gp::UInt32 maxSetsAllocations
) noexcept
    : m_device(device)
    , m_typeCounts(typeCounts)
{
#if GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID
    static_cast<void>(numSetLayouts);
    m_maxDescriptorSets = maxSetsAllocations;
#else
    m_maxDescriptorSets = maxSetsAllocations * numSetLayouts;
#endif

    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreateDescriptorPool == nullptr) ||
        (m_maxDescriptorSets == 0u))
    {
        return;
    }

    gp::Array<VkDescriptorPoolSize, kVulkanNumDescriptorTypeSlots> poolSizes{};
    const gp::UInt32 numPoolSizes = computePoolSizes(m_typeCounts, maxSetsAllocations, poolSizes);

    VkDescriptorPoolCreateInfo createInfo;
    zeroVulkanStruct(createInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    // No FREE_DESCRIPTOR_SET_BIT: the pool is reset as a whole, which keeps it a bump allocator.
    createInfo.maxSets = m_maxDescriptorSets;
    createInfo.poolSizeCount = numPoolSizes;
    createInfo.pPoolSizes = (numPoolSizes != 0u) ? poolSizes.data() : nullptr;

    const VkResult result =
        vkCreateDescriptorPool(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_descriptorPool);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateDescriptorPool failed with {}.", vulkanResultToString(result));
        m_descriptorPool = VK_NULL_HANDLE;
    }
}

VulkanDescriptorPool::~VulkanDescriptorPool() noexcept
{
    if ((m_descriptorPool != VK_NULL_HANDLE) && (dynamic_api::vkDestroyDescriptorPool != nullptr))
    {
        vkDestroyDescriptorPool(m_device.getHandle(), m_descriptorPool, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_descriptorPool = VK_NULL_HANDLE;
}

gp::UInt32 VulkanDescriptorPool::computePoolSizes(
    const VulkanDescriptorTypeCounts& typeCounts,
    const gp::UInt32 maxSetsAllocations,
    gp::Array<VkDescriptorPoolSize, kVulkanNumDescriptorTypeSlots>& outPoolSizes
) noexcept
{
    gp::UInt32 numPoolSizes = 0u;
    for (gp::USize slot = 0u; slot < kVulkanNumDescriptorTypeSlots; ++slot)
    {
        if (typeCounts[slot] == 0u)
        {
            continue;
        }
        VkDescriptorPoolSize& poolSize = outPoolSizes[numPoolSizes];
        poolSize.type = getVulkanDescriptorTypeForSlot(slot);
        poolSize.descriptorCount = typeCounts[slot] * maxSetsAllocations;
        ++numPoolSizes;
    }
    return numPoolSizes;
}

bool VulkanDescriptorPool::canAllocate(const VulkanDescriptorSetsLayout& layout) const noexcept
{
    return (m_numAllocatedDescriptorSets + static_cast<gp::UInt32>(layout.getLayouts().size())) <= m_maxDescriptorSets;
}

bool VulkanDescriptorPool::trackAddUsage(const VulkanDescriptorSetsLayout& layout) noexcept
{
    if (!hasSameShape(layout))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Descriptor pool tracked a layout with a different shape.");
        return false;
    }
    m_numAllocatedDescriptorSets += static_cast<gp::UInt32>(layout.getLayouts().size());
    if (m_numAllocatedDescriptorSets > m_peakAllocatedDescriptorSets)
    {
        m_peakAllocatedDescriptorSets = m_numAllocatedDescriptorSets;
    }
    return true;
}

bool VulkanDescriptorPool::trackRemoveUsage(const VulkanDescriptorSetsLayout& layout) noexcept
{
    const gp::UInt32 numSets = static_cast<gp::UInt32>(layout.getLayouts().size());
    if (!hasSameShape(layout) || (numSets > m_numAllocatedDescriptorSets))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Descriptor pool released a layout it did not track.");
        return false;
    }
    m_numAllocatedDescriptorSets -= numSets;
    return true;
}

void VulkanDescriptorPool::reset() noexcept
{
    if ((m_descriptorPool != VK_NULL_HANDLE) && (dynamic_api::vkResetDescriptorPool != nullptr))
    {
        const VkResult result = vkResetDescriptorPool(m_device.getHandle(), m_descriptorPool, 0u);
        if (result != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkResetDescriptorPool failed with {}.", vulkanResultToString(result));
        }
    }
    m_numAllocatedDescriptorSets = 0u;
}

bool VulkanDescriptorPool::allocateDescriptorSets(
    const VkDescriptorSetAllocateInfo& allocateInfo, VkDescriptorSet* const outSets
) noexcept
{
    if ((m_descriptorPool == VK_NULL_HANDLE) || (dynamic_api::vkAllocateDescriptorSets == nullptr))
    {
        return false;
    }

    VkDescriptorSetAllocateInfo poolAllocateInfo = allocateInfo;
    poolAllocateInfo.descriptorPool = m_descriptorPool;
    // Out-of-pool results are expected here: the caller moves on to the next pool.
    return vkAllocateDescriptorSets(m_device.getHandle(), &poolAllocateInfo, outSets) == VK_SUCCESS;
}

bool VulkanDescriptorPool::hasSameShape(const VulkanDescriptorSetsLayout& layout) const noexcept
{
    const VulkanDescriptorTypeCounts& layoutTypes = layout.getLayoutTypes();
    for (gp::USize slot = 0u; slot < kVulkanNumDescriptorTypeSlots; ++slot)
    {
        if (layoutTypes[slot] != m_typeCounts[slot])
        {
            return false;
        }
    }
    return true;
}

}   // namespace gp::rhi::vulkan
