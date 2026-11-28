// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"              // IWYU pragma: keep
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                          // IWYU pragma: keep
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native framebuffer binding a set of image views to a compatible render pass.
/// @details Attachment order matches the layout's attachment descriptions: each color followed by its resolve target,
///          then depth-stencil, its resolve target and the shading rate image. The images behind the views are kept
///          so the cache can match a new set of render targets, or drop every framebuffer using an image about to be
///          destroyed, without dereferencing textures.
/// @note Views are owned by their textures (internal view cache), not by the framebuffer.
class GP_RHI_VULKAN_API VulkanFramebuffer final
{
public:
    /// @brief Creates the native framebuffer.
    /// @param[in] device Logical device; must outlive the framebuffer.
    /// @param[in] deferredDeletionQueue Queue retiring the handle, or `nullptr` to destroy immediately.
    /// @param[in] rtInfo Render targets; textures must be `VulkanTexture`s.
    /// @param[in] layout Layout built from `rtInfo`.
    /// @param[in] renderPass Render pass compatible with `layout`.
    VulkanFramebuffer(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* deferredDeletionQueue,
        const command::SetRenderTargetsInfo& rtInfo,
        const VulkanRenderTargetLayout& layout,
        const VulkanRenderPass& renderPass
    ) noexcept;

    /// @brief Retires the native framebuffer if `destroy()` was not called.
    ~VulkanFramebuffer() noexcept;

    VulkanFramebuffer(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer(VulkanFramebuffer&&) = delete;
    VulkanFramebuffer& operator=(VulkanFramebuffer&&) = delete;

    /// @brief Retires the native framebuffer.
    /// @param[in] immediate True to destroy now (no GPU work references it), false to go through the deletion queue.
    void destroy(bool immediate) noexcept;

    /// @brief Reports whether the framebuffer was built over the same images as `rtInfo`.
    /// @param[in] rtInfo Render targets.
    /// @return True when every color, resolve, depth-stencil and shading rate image matches.
    [[nodiscard]] bool matches(const command::SetRenderTargetsInfo& rtInfo) const noexcept;

    /// @brief Reports whether `image` is one of the attachments.
    /// @param[in] image Native image.
    /// @return True when any attachment views `image`.
    [[nodiscard]] bool containsImage(VkImage image) const noexcept;

    /// @brief Returns the native handle.
    /// @return Framebuffer, or `VK_NULL_HANDLE` after `destroy()` or on failure.
    [[nodiscard]] VkFramebuffer getHandle() const noexcept
    {
        return m_framebuffer;
    }

    /// @brief Returns the area render passes over this framebuffer cover.
    /// @return Layout offset and extent.
    [[nodiscard]] const VkRect2D& getRenderArea() const noexcept
    {
        return m_renderArea;
    }

    /// @brief Returns the number of layers.
    /// @return Framebuffer layer count; 1 for multiview.
    [[nodiscard]] gp::UInt32 getNumLayers() const noexcept
    {
        return m_numLayers;
    }

    /// @brief Returns the number of color render targets of the source description.
    /// @return Color render target count.
    [[nodiscard]] gp::UInt32 getNumColorRenderTargets() const noexcept
    {
        return m_numColorRenderTargets;
    }

    /// @brief Returns the number of color attachments with an image.
    /// @return Color attachment count.
    [[nodiscard]] gp::UInt32 getNumColorAttachments() const noexcept
    {
        return m_numColorAttachments;
    }

    /// @brief Returns the attachment views.
    /// @return `getNumAttachmentViews()` views, in attachment order.
    [[nodiscard]] const VkImageView* getAttachmentViews() const noexcept
    {
        return m_attachmentViews.data();
    }

    /// @brief Returns the number of attachment views.
    /// @return View count.
    [[nodiscard]] gp::UInt32 getNumAttachmentViews() const noexcept
    {
        return m_numAttachmentViews;
    }

    /// @brief Returns the depth-only view of the depth-stencil target, used to read depth as an input attachment.
    /// @return Partial view, or `VK_NULL_HANDLE` without depth.
    [[nodiscard]] VkImageView getPartialDepthView() const noexcept
    {
        return m_partialDepthView;
    }

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Queue retiring the handle, or `nullptr`.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Native handle.
    VkFramebuffer m_framebuffer{ VK_NULL_HANDLE };

    /// @brief Render area.
    VkRect2D m_renderArea{};

    /// @brief Images of the color targets, by color index.
    gp::Array<VkImage, kMaxSimultaneousRenderTargets> m_colorImages{};

    /// @brief Images of the color resolve targets, by color index.
    gp::Array<VkImage, kMaxSimultaneousRenderTargets> m_colorResolveImages{};

    /// @brief Attachment views, in attachment order.
    gp::Array<VkImageView, VulkanRenderTargetLayout::kMaxAttachmentDescriptions> m_attachmentViews{};

    /// @brief Image of the depth-stencil target.
    VkImage m_depthStencilImage{ VK_NULL_HANDLE };

    /// @brief Image of the depth-stencil resolve target.
    VkImage m_depthStencilResolveImage{ VK_NULL_HANDLE };

    /// @brief Image of the shading rate attachment.
    VkImage m_fragmentDensityImage{ VK_NULL_HANDLE };

    /// @brief Depth-only view of the depth-stencil target.
    VkImageView m_partialDepthView{ VK_NULL_HANDLE };

    /// @brief Number of attachment views.
    gp::UInt32 m_numAttachmentViews{ 0u };

    /// @brief Number of layers.
    gp::UInt32 m_numLayers{ 1u };

    /// @brief Number of color render targets of the source description.
    gp::UInt32 m_numColorRenderTargets{ 0u };

    /// @brief Number of color attachments with an image.
    gp::UInt32 m_numColorAttachments{ 0u };
};

}   // namespace gp::rhi::vulkan
