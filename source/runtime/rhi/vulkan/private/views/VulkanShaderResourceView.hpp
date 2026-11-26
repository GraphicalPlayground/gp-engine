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
#include "views/ShaderResourceView.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewForward.hpp"   // IWYU pragma: keep
#include "views/VulkanView.hpp"

namespace gp::rhi::vulkan
{

/// @brief Vulkan implementation of a shader resource view.
/// @details Combines three roles:
///          - `view::ShaderResourceView` keeps the RHI description and a strong reference to the resource;
///          - `VulkanView` holds the native view (image view, texel buffer view, storage buffer range or
///            acceleration structure);
///          - `VulkanLinkedView` puts the view in its resource's list, so the native view is rebuilt whenever the
///            resource is renamed (buffer lock rename, `replaceResources()`, defragmentation).
///
///          The native view is built on the RHI timeline by `enqueueLinkAndUpdate()`, after the resource is
///          guaranteed to have its own native object.
/// @note Base classes are destroyed in reverse order: the view unlinks, then releases its native object, then drops
///       the resource reference. The resource therefore outlives its link.
class GP_RHI_VULKAN_API VulkanShaderResourceView final
    : public view::ShaderResourceView
    , public VulkanView
    , public VulkanLinkedView
{
public:
    /// @brief Constructs an SRV of `targetResource` without building its native view yet.
    /// @param[in] services Device and deletion services of the resource.
    /// @param[in] targetResource Target `VulkanBuffer` or `VulkanTexture`.
    /// @param[in] viewDesc SRV description.
    VulkanShaderResourceView(
        const VulkanResourceServices& services,
        resource::ViewableResource* targetResource,
        const view::ViewDesc& viewDesc
    ) noexcept;

    /// @brief Returns the target resource as a Vulkan viewable resource.
    /// @return Target buffer or texture.
    [[nodiscard]] VulkanViewableResource* getBaseResource() const noexcept;

    /// @copydoc VulkanLinkedView::updateView()
    /// @note Invalidates the native view and rebuilds it from the resource's current allocation. Null buffers keep
    ///       no native view.
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
    /// @copydoc view::ShaderResourceView::~ShaderResourceView()
    virtual ~VulkanShaderResourceView() noexcept override;
};

/// @brief Creates a shader resource view of a buffer.
/// @note Backs `DynamicRHI::createShaderResourceView()` for buffers.
/// @param[in,out] cmdList Command list the native view creation is ordered on.
/// @param[in] buffer Target buffer (a `VulkanBuffer`).
/// @param[in] desc Buffer SRV description.
/// @return Owning reference.
[[nodiscard]] GP_RHI_VULKAN_API view::ShaderResourceViewRef createVulkanShaderResourceView(
    command::CommandListBase& cmdList, resource::Buffer* buffer, const view::ViewDesc& desc
) noexcept;

/// @brief Creates a shader resource view of a texture.
/// @note Backs `DynamicRHI::createShaderResourceView()` for textures.
/// @param[in,out] cmdList Command list the native view creation is ordered on.
/// @param[in] texture Target texture (a `VulkanTexture`).
/// @param[in] desc Texture SRV description.
/// @return Owning reference.
[[nodiscard]] GP_RHI_VULKAN_API view::ShaderResourceViewRef createVulkanShaderResourceView(
    command::CommandListBase& cmdList, resource::Texture* texture, const view::ViewDesc& desc
) noexcept;

}   // namespace gp::rhi::vulkan
