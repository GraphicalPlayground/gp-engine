// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"              // IWYU pragma: keep
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                          // IWYU pragma: keep
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native description of a pass recorded with `VK_KHR_dynamic_rendering`.
/// @details The render pass and framebuffer objects disappear: each attachment carries its own view, layout, load and
///          store ops, clear value and resolve target, and `renderingInfo` points into this struct. One instance per
///          recording context is reused pass after pass, so beginning a pass allocates nothing.
/// @note Not copyable: `renderingInfo` points into the object itself.
struct GP_RHI_VULKAN_API VulkanDynamicRenderingInfo final
{
    /// @brief Color attachments.
    gp::Array<VkRenderingAttachmentInfo, kMaxSimultaneousRenderTargets> colorAttachments{};

    /// @brief Depth attachment, referenced only when depth is used.
    VkRenderingAttachmentInfo depthAttachment{};

    /// @brief Stencil attachment, referenced only when stencil is used.
    VkRenderingAttachmentInfo stencilAttachment{};

    /// @brief Shading rate image of palette encodings.
    VkRenderingFragmentShadingRateAttachmentInfoKHR shadingRateAttachment{};

    /// @brief Fragment density map of fractional encodings.
    VkRenderingFragmentDensityMapAttachmentInfoEXT fragmentDensityAttachment{};

    /// @brief Rendering info pointing at the attachments.
    VkRenderingInfo renderingInfo{};

    /// @brief Color views, kept to bind color targets as input attachments.
    gp::Array<VkImageView, kMaxSimultaneousRenderTargets> colorViews{};

    /// @brief Depth-only view of the depth-stencil target, kept to read depth as an input attachment.
    VkImageView partialDepthView{ VK_NULL_HANDLE };

    /// @brief Creates an empty description.
    VulkanDynamicRenderingInfo() noexcept = default;

    VulkanDynamicRenderingInfo(const VulkanDynamicRenderingInfo&) = delete;
    VulkanDynamicRenderingInfo& operator=(const VulkanDynamicRenderingInfo&) = delete;
    VulkanDynamicRenderingInfo(VulkanDynamicRenderingInfo&&) = delete;
    VulkanDynamicRenderingInfo& operator=(VulkanDynamicRenderingInfo&&) = delete;
};

/// @brief Returns the layouts the depth and stencil aspects of a pass are used in.
/// @details Written aspects use `ATTACHMENT_OPTIMAL`, read aspects `READ_ONLY_OPTIMAL`. Unused aspects stay
///          `UNDEFINED`, unless the device merges depth and stencil layouts (no parallel rendering): the untouched
///          aspect then keeps the tracked layout of the texture, since both aspects transition together.
/// @param[in] caps Device capabilities.
/// @param[in] rpInfo Pass description.
/// @param[out] outDepthLayout Depth layout, `UNDEFINED` without depth.
/// @param[out] outStencilLayout Stencil layout, `UNDEFINED` without stencil.
GP_RHI_VULKAN_API void extractVulkanDepthStencilLayouts(
    const VulkanRenderTargetCaps& caps,
    const command::RenderPassInfo& rpInfo,
    VkImageLayout& outDepthLayout,
    VkImageLayout& outStencilLayout
) noexcept;

/// @brief Fills a dynamic rendering description from a pass description.
/// @details Mirrors the render target layout built from the same `rpInfo`: same ops, same layouts, same full-write
///          and unused-aspect rules, so both paths render identically. Attachment views come from the textures'
///          internal view caches.
/// @param[in] caps Device capabilities.
/// @param[in] rpInfo Pass description; textures must be `VulkanTexture`s.
/// @param[out] outInfo Description; `outInfo.renderingInfo` is ready for `vkCmdBeginRendering`.
GP_RHI_VULKAN_API void fillVulkanDynamicRenderingInfo(
    const VulkanRenderTargetCaps& caps, const command::RenderPassInfo& rpInfo, VulkanDynamicRenderingInfo& outInfo
) noexcept;

}   // namespace gp::rhi::vulkan
