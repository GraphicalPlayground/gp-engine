// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanDescriptorPoolSetAcquirer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPoolSetContainer.hpp"
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanTypedDescriptorPoolSet.hpp"
#include "logging/LogMacros.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

VulkanDescriptorPoolSetAcquirer::VulkanDescriptorPoolSetAcquirer(VulkanDescriptorPoolsManager& manager) noexcept
    : m_manager(manager)
{}

VulkanDescriptorPoolSetAcquirer::~VulkanDescriptorPoolSetAcquirer() noexcept
{
    release();
}

bool VulkanDescriptorPoolSetAcquirer::acquirePoolSetAndDescriptorsIfNeeded(
    const VulkanDescriptorSetsLayout& layout,
    const bool needDescriptors,
    const gp::UInt64 currentFrame,
    VkDescriptorSet* const outSets
) noexcept
{
    bool mustAllocate = needDescriptors;
    if (m_container == nullptr)
    {
        // Sets written during an earlier frame belong to a container that may already be resetting.
        m_container = &m_manager.acquirePoolSetContainer(currentFrame);
        mustAllocate = true;
    }

    const gp::USize numTypedPoolSetsBefore = m_container->getNumTypedPoolSets();
    VulkanTypedDescriptorPoolSet& typedPoolSet = m_container->acquireTypedPoolSet(layout);
    if (m_container->getNumTypedPoolSets() != numTypedPoolSetsBefore)
    {
        mustAllocate = true;
    }

    if (!mustAllocate)
    {
        return false;
    }

    if (!typedPoolSet.allocateDescriptorSets(layout, outSets))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to allocate {} descriptor sets.", layout.getHandles().size());
        return false;
    }
    return true;
}

void VulkanDescriptorPoolSetAcquirer::release() noexcept
{
    if (m_container != nullptr)
    {
        m_manager.releasePoolSet(*m_container);
        m_container = nullptr;
    }
}

}   // namespace gp::rhi::vulkan
