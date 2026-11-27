// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanGenericDescriptorPool.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

VulkanGenericDescriptorPool::VulkanGenericDescriptorPool(
    VulkanDevice& device, const gp::UInt32 maxDescriptorSets, const VulkanDescriptorPoolSizeRatios& poolSizeRatios
) noexcept
    : m_device(device)
    , m_maxDescriptorSets(maxDescriptorSets)
{
    gp::Array<VkDescriptorPoolSize, kVulkanNumCoreDescriptorTypes> poolSizes{};
    computePoolSizes(maxDescriptorSets, poolSizeRatios, poolSizes);
    for (gp::USize index = 0u; index < kVulkanNumCoreDescriptorTypes; ++index)
    {
        m_poolSizes[index] = poolSizes[index].descriptorCount;
    }

    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreateDescriptorPool == nullptr) ||
        (m_maxDescriptorSets == 0u))
    {
        return;
    }

    VkDescriptorPoolCreateInfo createInfo;
    zeroVulkanStruct(createInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    createInfo.maxSets = m_maxDescriptorSets;
    createInfo.poolSizeCount = static_cast<gp::UInt32>(kVulkanNumCoreDescriptorTypes);
    createInfo.pPoolSizes = poolSizes.data();

    const VkResult result =
        vkCreateDescriptorPool(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_descriptorPool);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateDescriptorPool failed with {}.", vulkanResultToString(result));
        m_descriptorPool = VK_NULL_HANDLE;
    }
}

VulkanGenericDescriptorPool::~VulkanGenericDescriptorPool() noexcept
{
    if ((m_descriptorPool != VK_NULL_HANDLE) && (dynamic_api::vkDestroyDescriptorPool != nullptr))
    {
        vkDestroyDescriptorPool(m_device.getHandle(), m_descriptorPool, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_descriptorPool = VK_NULL_HANDLE;
}

void VulkanGenericDescriptorPool::computePoolSizes(
    const gp::UInt32 maxDescriptorSets,
    const VulkanDescriptorPoolSizeRatios& poolSizeRatios,
    gp::Array<VkDescriptorPoolSize, kVulkanNumCoreDescriptorTypes>& outPoolSizes
) noexcept
{
    const gp::Float32 numSets = static_cast<gp::Float32>(maxDescriptorSets);
    for (gp::USize index = 0u; index < kVulkanNumCoreDescriptorTypes; ++index)
    {
        // The default ratios are a floor: a new pool sized from measured usage never drops a type to zero, so a
        // shader that starts using a rare type does not fail allocation straight away.
        const gp::Float32 defaultSize = kVulkanDefaultDescriptorPoolSizeRatios[index] * numSets;
        const gp::Float32 minSize = (defaultSize > kVulkanMinGenericPoolDescriptorsPerType)
                                        ? defaultSize
                                        : kVulkanMinGenericPoolDescriptorsPerType;
        const gp::Float32 requestedSize = poolSizeRatios[index] * numSets;
        const gp::Float32 size = (requestedSize > minSize) ? requestedSize : minSize;

        outPoolSizes[index].type = getVulkanDescriptorTypeForSlot(index);
        outPoolSizes[index].descriptorCount = static_cast<gp::UInt32>(size);
    }
}

gp::UInt32 VulkanGenericDescriptorPool::getPoolSize(const VkDescriptorType type) const noexcept
{
    const gp::USize slot = getVulkanDescriptorTypeSlot(type);
    return (slot < kVulkanNumCoreDescriptorTypes) ? m_poolSizes[slot] : 0u;
}

void VulkanGenericDescriptorPool::reset() noexcept
{
    if ((m_descriptorPool == VK_NULL_HANDLE) || (dynamic_api::vkResetDescriptorPool == nullptr))
    {
        return;
    }
    const VkResult result = vkResetDescriptorPool(m_device.getHandle(), m_descriptorPool, 0u);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkResetDescriptorPool failed with {}.", vulkanResultToString(result));
    }
}

bool VulkanGenericDescriptorPool::allocateDescriptorSet(
    const VkDescriptorSetLayout layout, VkDescriptorSet& outSet
) noexcept
{
    if ((m_descriptorPool == VK_NULL_HANDLE) || (dynamic_api::vkAllocateDescriptorSets == nullptr))
    {
        return false;
    }

    VkDescriptorSetAllocateInfo allocateInfo;
    zeroVulkanStruct(allocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    allocateInfo.descriptorPool = m_descriptorPool;
    allocateInfo.descriptorSetCount = 1u;
    allocateInfo.pSetLayouts = &layout;

    // An out-of-pool result is expected: the cache then grows by one pool.
    return vkAllocateDescriptorSets(m_device.getHandle(), &allocateInfo, &outSet) == VK_SUCCESS;
}

}   // namespace gp::rhi::vulkan
