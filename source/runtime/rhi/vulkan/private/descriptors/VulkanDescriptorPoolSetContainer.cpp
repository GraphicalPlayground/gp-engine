// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorPoolSetContainer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

VulkanDescriptorPoolSetContainer::VulkanDescriptorPoolSetContainer(
    VulkanDevice& device, const gp::UInt64 currentFrame
) noexcept
    : m_device(device)
    , m_lastFrameUsed(currentFrame)
{}

gp::UInt32 VulkanDescriptorPoolSetContainer::computePoolKey(const VulkanDescriptorSetsLayout& layout) noexcept
{
#if GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID
    return layout.getTypesUsageID();
#else
    return layout.getHash();
#endif
}

VulkanTypedDescriptorPoolSet&
    VulkanDescriptorPoolSetContainer::acquireTypedPoolSet(const VulkanDescriptorSetsLayout& layout) noexcept
{
    const gp::UInt32 key = computePoolKey(layout);

    gp::USize first = 0u;
    gp::USize count = static_cast<gp::USize>(m_typedPoolSets.size());
    while (count > 0u)
    {
        const gp::USize half = count / 2u;
        const gp::USize middle = first + half;
        if (m_typedPoolSets[middle].key < key)
        {
            first = middle + 1u;
            count -= half + 1u;
        }
        else
        {
            count = half;
        }
    }
    if ((first < static_cast<gp::USize>(m_typedPoolSets.size())) && (m_typedPoolSets[first].key == key))
    {
        return *m_typedPoolSets[first].poolSet;
    }

    m_typedPoolSets.insertAt(first, Entry{ key, gp::makeUnique<VulkanTypedDescriptorPoolSet>(m_device, layout) });
    return *m_typedPoolSets[first].poolSet;
}

void VulkanDescriptorPoolSetContainer::reset() noexcept
{
    for (Entry& entry: m_typedPoolSets)
    {
        entry.poolSet->reset();
    }
}

void VulkanDescriptorPoolSetContainer::setUsed(const bool isUsed, const gp::UInt64 currentFrame) noexcept
{
    m_isUsed = isUsed;
    if (isUsed)
    {
        m_lastFrameUsed = currentFrame;
    }
}

}   // namespace gp::rhi::vulkan
