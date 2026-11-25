// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/PoolMoveRequest.hpp"
#include "containers/arrays/Vector.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Mixin for resources whose views and listeners must follow their backing allocation.
/// @details Buffers and textures inherit from this next to their `resource::` base. Whenever the allocation changes,
///          the resource calls `updateLinkedViews()` once, which refreshes every linked view and then notifies every
///          rename listener.
/// @note Not thread-safe: linking, registration and updates all run on the RHI timeline.
class GP_RHI_VULKAN_API VulkanViewableResource
{
public:
    /// @brief Constructs a resource with no views or listeners.
    VulkanViewableResource() noexcept = default;

    /// @brief Destructor; asserts that every view and listener was removed first.
    virtual ~VulkanViewableResource() noexcept;

    VulkanViewableResource(const VulkanViewableResource&) = delete;
    VulkanViewableResource& operator=(const VulkanViewableResource&) = delete;
    VulkanViewableResource(VulkanViewableResource&&) = delete;
    VulkanViewableResource& operator=(VulkanViewableResource&&) = delete;

    /// @brief Reports whether any view is linked to this resource.
    /// @return True when at least one view is linked.
    [[nodiscard]] bool hasLinkedViews() const noexcept
    {
        return m_linkedViews != nullptr;
    }

    /// @brief Links `view` so it is refreshed whenever this resource is renamed.
    /// @param[in,out] view View created on this resource.
    void linkView(VulkanLinkedView& view) noexcept;

    /// @brief Registers `listener` for rename notifications.
    /// @param[in] listener Non-null listener; must be removed before either side is destroyed.
    void addRenameListener(IVulkanResourceRenameListener* listener) noexcept;

    /// @brief Unregisters `listener`; asserts that it was registered.
    /// @param[in] listener Listener previously passed to `addRenameListener()`.
    void removeRenameListener(IVulkanResourceRenameListener* listener) noexcept;

    /// @brief Returns the number of registered rename listeners.
    /// @return Listener count.
    [[nodiscard]] gp::USize getNumRenameListeners() const noexcept
    {
        return m_renameListeners.size();
    }

    /// @brief Refreshes every linked view, then notifies every rename listener.
    /// @param[in] contexts Pipeline contexts active during the rename.
    virtual void updateLinkedViews(const allocator::ContextArray& contexts) noexcept;

private:
    VulkanLinkedView* m_linkedViews{ nullptr };
    gp::InlineVector<IVulkanResourceRenameListener*, 1u> m_renameListeners{};
};

}   // namespace gp::rhi::vulkan

