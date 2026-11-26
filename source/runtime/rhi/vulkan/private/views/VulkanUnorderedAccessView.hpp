// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/PoolMoveRequest.hpp"
#include "commands/CommandForward.hpp"
#include "core/ResourceForward.hpp"              // IWYU pragma: keep
#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/VulkanLinkedView.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "resources/VulkanResourceServices.hpp"
#include "RHIVulkan.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewForward.hpp"   // IWYU pragma: keep
#include "views/VulkanView.hpp"

namespace gp::rhi::vulkan
{

/// @brief Vulkan implementation of an unordered access view.
/// @details Combines three roles:
///          - `view::UnorderedAccessView` keeps the RHI description and a strong reference to the resource;
///          - `VulkanView` holds the native view (storage image view, storage texel buffer view or storage buffer
///            range);
///          - `VulkanLinkedView` puts the view in its resource's list, so the native view is rebuilt whenever the
///            resource is renamed (buffer lock rename, `replaceResources()`, defragmentation).
///
///          The native view is built on the RHI timeline by `enqueueLinkAndUpdate()`, after the resource is
///          guaranteed to have its own native object.
///          UAV clears are recorded by the command context and are not part of the view.
/// @note Base classes are destroyed in reverse order: the view unlinks, then releases its native object, then drops
///       the resource reference. The resource therefore outlives its link.
class GP_RHI_VULKAN_API VulkanUnorderedAccessView final
    : public view::UnorderedAccessView
    , public VulkanView
    , public VulkanLinkedView
{
public:
    /// @brief Constructs a UAV of `targetResource` without building its native view yet.
    /// @param[in] services Device and deletion services of the resource.
    /// @param[in] targetResource Target `VulkanBuffer` or `VulkanTexture`.
    /// @param[in] viewDesc UAV description.
    VulkanUnorderedAccessView(
        const VulkanResourceServices& services,
        resource::ViewableResource* targetResource,
        const view::ViewDesc& viewDesc
    ) noexcept;

    /// @brief Returns the target resource as a Vulkan viewable resource.
    /// @return Target buffer or texture.
    [[nodiscard]] VulkanViewableResource* getBaseResource() const noexcept;

    /// @copydoc VulkanLinkedView::updateView()
    /// @note Invalidates the native view and rebuilds it from the resource's current allocation. Null buffers keep
    ///       no native view; atomic and append counters are not supported.
    virtual void updateView(const allocator::ContextArray& contexts) noexcept override;

    /// @copydoc view::View::getBindlessHandle()
    [[nodiscard]] virtual view::DescriptorHandle getBindlessHandle() const noexcept override;

    /// @brief Links the view to its resource and builds the native view, on the RHI timeline.
    /// @details Runs inline on a bypassed or bottom-of-pipe command list; otherwise the lambda holds a reference
    ///          to the view until it executes.
    /// @note The view must already be held by a reference: the deferred lambda takes another one.
    /// @param[in,out] cmdList Command list ordering the work after the resource creation.
    void enqueueLinkAndUpdate(command::CommandListBase& cmdList) noexcept;

protected:
    /// @copydoc view::UnorderedAccessView::~UnorderedAccessView()
    virtual ~VulkanUnorderedAccessView() noexcept override;
};

/// @brief Creates an unordered access view of a buffer.
/// @note Backs `DynamicRHI::createUnorderedAccessView()` for buffers.
/// @param[in,out] cmdList Command list the native view creation is ordered on.
/// @param[in] buffer Target buffer (a `VulkanBuffer`).
/// @param[in] desc Buffer UAV description.
/// @return Owning reference.
[[nodiscard]] GP_RHI_VULKAN_API view::UnorderedAccessViewRef createVulkanUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Buffer* buffer, const view::ViewDesc& desc
) noexcept;

/// @brief Creates an unordered access view of a texture.
/// @note Backs `DynamicRHI::createUnorderedAccessView()` for textures.
/// @param[in,out] cmdList Command list the native view creation is ordered on.
/// @param[in] texture Target texture (a `VulkanTexture`).
/// @param[in] desc Texture UAV description.
/// @return Owning reference.
[[nodiscard]] GP_RHI_VULKAN_API view::UnorderedAccessViewRef createVulkanUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Texture* texture, const view::ViewDesc& desc
) noexcept;

}   // namespace gp::rhi::vulkan
