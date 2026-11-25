// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanViewableResource.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "resources/IVulkanResourceRenameListener.hpp"
#include "resources/VulkanLinkedView.hpp"

namespace gp::rhi::vulkan
{

VulkanViewableResource::~VulkanViewableResource() noexcept
{
    GP_CHECKF(
        !hasLinkedViews(), "All linked views must have been removed before the underlying resource can be deleted."
    );
    GP_CHECKF(
        m_renameListeners.isEmpty(),
        "All rename listeners must have been removed before the underlying resource can be deleted."
    );
}

void VulkanViewableResource::linkView(VulkanLinkedView& view) noexcept
{
    view.linkHead(m_linkedViews);
}

void VulkanViewableResource::addRenameListener(IVulkanResourceRenameListener* const listener) noexcept
{
    GP_CHECK(listener != nullptr);
    for (const IVulkanResourceRenameListener* const existing: m_renameListeners)
    {
        GP_CHECKF(existing != listener, "Rename listener registered twice.");
    }
    m_renameListeners.pushBack(listener);
}

void VulkanViewableResource::removeRenameListener(IVulkanResourceRenameListener* const listener) noexcept
{
    for (gp::USize index = 0u; index < m_renameListeners.size(); ++index)
    {
        if (m_renameListeners[index] == listener)
        {
            m_renameListeners.removeAtSwap(index);
            return;
        }
    }
    GP_CHECKF(false, "Rename listener was not registered.");
}

void VulkanViewableResource::updateLinkedViews(const allocator::ContextArray& contexts) noexcept
{
    for (VulkanLinkedView* view = m_linkedViews; view != nullptr; view = view->getNext())
    {
        view->updateView(contexts);
    }

    for (IVulkanResourceRenameListener* const listener: m_renameListeners)
    {
        listener->resourceRenamed(contexts, this);
    }
}

}   // namespace gp::rhi::vulkan

