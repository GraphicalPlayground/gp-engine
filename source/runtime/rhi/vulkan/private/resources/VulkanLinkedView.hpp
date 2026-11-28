// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/PoolMoveRequest.hpp"
#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief View (SRV or UAV) that caches native state derived from its resource's current allocation.
/// @details Views are kept in an intrusive singly-anchored list on the resource. When the resource changes its backing
///          memory (lock rename, `replaceResources()`, defragmentation), it walks the list and calls `updateView()` so
///          each view rebuilds its descriptor. The list costs two pointers per view and no allocation.
/// @note Linking and unlinking happen on the RHI timeline only, so the list is not guarded.
class GP_RHI_VULKAN_API VulkanLinkedView
{
public:
    VulkanLinkedView(const VulkanLinkedView&) = delete;
    VulkanLinkedView& operator=(const VulkanLinkedView&) = delete;
    VulkanLinkedView(VulkanLinkedView&&) = delete;
    VulkanLinkedView& operator=(VulkanLinkedView&&) = delete;

    /// @brief Rebuilds the view against its resource's current allocation.
    /// @param[in] contexts Pipeline contexts active when the resource was renamed.
    virtual void updateView(const allocator::ContextArray& contexts) noexcept = 0;

    /// @brief Inserts this view at the front of the list anchored at `head`.
    /// @note Asserts that the view is not linked yet.
    /// @param[in,out] head List anchor owned by the resource.
    void linkHead(VulkanLinkedView*& head) noexcept;

    /// @brief Removes this view from its list; does nothing when not linked.
    void unlink() noexcept;

    /// @brief Reports whether this view is currently in a list.
    /// @return True when linked.
    [[nodiscard]] bool isLinked() const noexcept
    {
        return m_prevLink != nullptr;
    }

    /// @brief Returns the next view in the list.
    /// @return Next view, or `nullptr` at the tail.
    [[nodiscard]] VulkanLinkedView* getNext() const noexcept
    {
        return m_next;
    }

protected:
    /// @brief Constructs an unlinked view.
    VulkanLinkedView() noexcept = default;

    /// @brief Unlinks the view so the resource never sees a dangling node.
    virtual ~VulkanLinkedView() noexcept;

private:
    VulkanLinkedView* m_next{ nullptr };
    VulkanLinkedView** m_prevLink{ nullptr };
};

}   // namespace gp::rhi::vulkan

