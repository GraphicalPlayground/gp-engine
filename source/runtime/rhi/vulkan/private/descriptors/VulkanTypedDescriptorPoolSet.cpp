// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanTypedDescriptorPoolSet.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

VulkanTypedDescriptorPoolSet::VulkanTypedDescriptorPoolSet(
    VulkanDevice& device, const VulkanDescriptorSetsLayout& layout
) noexcept
    : m_device(device)
    , m_typeCounts(layout.getLayoutTypes())
    , m_numSetLayouts(static_cast<gp::UInt32>(layout.getLayouts().size()))
{
    static_cast<void>(pushNewPool());
}

bool VulkanTypedDescriptorPoolSet::allocateDescriptorSets(
    const VulkanDescriptorSetsLayout& layout, VkDescriptorSet* const outSets
) noexcept
{
    if (layout.getHandles().isEmpty())
    {
        return true;
    }

    for (;;)
    {
        VulkanDescriptorPool& pool = *m_pools[m_currentPool];
        // `maxSets` is a hard limit: some drivers allocate past it instead of failing, which validation rejects.
        if (pool.canAllocate(layout) && pool.allocateDescriptorSets(layout.getAllocateInfo(), outSets))
        {
            static_cast<void>(pool.trackAddUsage(layout));
            return true;
        }

        // A pool that holds nothing and still fails will not be helped by another pool: give up.
        if (pool.isEmpty())
        {
            GP_LOG_ERROR(LogVulkanRHI, "Descriptor set allocation failed on an empty pool.");
            return false;
        }

        if ((m_currentPool + 1u) < static_cast<gp::USize>(m_pools.size()))
        {
            ++m_currentPool;
        }
        else
        {
            static_cast<void>(pushNewPool());
        }
    }
}

void VulkanTypedDescriptorPoolSet::reset() noexcept
{
    for (gp::UniquePtr<VulkanDescriptorPool>& pool: m_pools)
    {
        pool->reset();
    }
    m_currentPool = 0u;
}

VulkanDescriptorPool& VulkanTypedDescriptorPoolSet::pushNewPool() noexcept
{
    const gp::UInt32 maxSetsAllocations = computeMaxSetsAllocations(static_cast<gp::USize>(m_pools.size()));
    m_pools.pushBack(gp::makeUnique<VulkanDescriptorPool>(m_device, m_typeCounts, m_numSetLayouts, maxSetsAllocations));
    m_currentPool = static_cast<gp::USize>(m_pools.size()) - 1u;
    return *m_pools[m_currentPool];
}

}   // namespace gp::rhi::vulkan
