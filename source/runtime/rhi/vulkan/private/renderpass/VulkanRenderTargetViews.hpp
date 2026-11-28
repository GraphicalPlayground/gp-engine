// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "views/VulkanTextureViewDesc.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Array slice value meaning "every slice" in render pass descriptions.
inline constexpr gp::Int32 kAllRenderTargetSlices = -1;

/// @brief Converts a `RenderTargetView::arraySliceIndex` (all ones for every slice) to a signed slice index.
/// @param[in] arraySliceIndex Unsigned slice index.
/// @return Slice index, or `kAllRenderTargetSlices`.
[[nodiscard]] constexpr gp::Int32 toRenderTargetSliceIndex(const gp::UInt32 arraySliceIndex) noexcept
{
    return (arraySliceIndex == 0xFFFFFFFFu) ? kAllRenderTargetSlices : static_cast<gp::Int32>(arraySliceIndex);
}

/// @brief Builds the view description every attachment view starts from.
/// @details Full aspect mask, the texture's view format, one mip, every array level, identity swizzle (attachments
///          must not swizzle).
/// @param[in] texture Render target.
/// @param[in] mipIndex Mip rendered to.
/// @param[in] sliceIndex First array slice; negative values map to 0.
/// @return View description.
[[nodiscard]] GP_RHI_VULKAN_API VulkanTextureViewDesc
    makeVulkanRenderTargetViewDesc(const VulkanTexture& texture, gp::UInt32 mipIndex, gp::Int32 sliceIndex) noexcept;

/// @brief Finds or creates the view of a color attachment.
/// @details 2D and 2D array targets render every slice (layered rendering) or one slice; cube faces render one at a
///          time through a 2D view; volume textures render every depth slice through a 2D array view.
/// @param[in,out] texture Render target; owns the view.
/// @param[in] mipIndex Mip rendered to.
/// @param[in] sliceIndex Slice rendered to, or `kAllRenderTargetSlices`.
/// @param[in] multiViewCount Multiview count of the pass; multiview framebuffers must have one layer.
/// @param[in,out] inOutNumLayers Framebuffer layer count; updated for 2D and 2D array targets.
/// @return Image view, or `VK_NULL_HANDLE` for imageless textures or unsupported view types.
[[nodiscard]] GP_RHI_VULKAN_API VkImageView getVulkanColorRenderTargetView(
    VulkanTexture& texture,
    gp::UInt32 mipIndex,
    gp::Int32 sliceIndex,
    gp::UInt32 multiViewCount,
    gp::UInt32& inOutNumLayers
) noexcept;

/// @brief Finds or creates the view of a color resolve attachment.
/// @param[in,out] texture Resolve target; must be 2D or 2D array.
/// @param[in] mipIndex Mip resolved to.
/// @param[in] sliceIndex Slice resolved to, or `kAllRenderTargetSlices`.
/// @return Image view, or `VK_NULL_HANDLE` for other view types.
[[nodiscard]] GP_RHI_VULKAN_API VkImageView
    getVulkanColorResolveTargetView(VulkanTexture& texture, gp::UInt32 mipIndex, gp::Int32 sliceIndex) noexcept;

/// @brief Finds or creates the view of a depth-stencil attachment.
/// @details Depth-only passes over a cube render all six faces; 2D and 2D array targets get a dedicated
///          identity-swizzled view (validation rejects swizzled depth attachments).
/// @param[in,out] texture Depth-stencil target; must be 2D, 2D array or cube.
/// @param[in] numColorAttachments Number of color attachments of the pass.
/// @param[in] mipIndex Mip rendered to.
/// @param[in] multiViewCount Multiview count of the pass.
/// @param[in,out] inOutNumLayers Framebuffer layer count.
/// @return Image view, or `VK_NULL_HANDLE` for other view types.
[[nodiscard]] GP_RHI_VULKAN_API VkImageView getVulkanDepthStencilTargetView(
    VulkanTexture& texture,
    gp::UInt32 numColorAttachments,
    gp::UInt32 mipIndex,
    gp::UInt32 multiViewCount,
    gp::UInt32& inOutNumLayers
) noexcept;

/// @brief Finds or creates the view of a depth-stencil resolve attachment.
/// @param[in,out] texture Resolve target; must be 2D or 2D array.
/// @param[in] mipIndex Mip resolved to.
/// @return Image view, or `VK_NULL_HANDLE` for other view types.
[[nodiscard]] GP_RHI_VULKAN_API VkImageView
    getVulkanDepthStencilResolveTargetView(VulkanTexture& texture, gp::UInt32 mipIndex) noexcept;

/// @brief Finds or creates the view of a shading rate (or fragment density) attachment.
/// @param[in,out] texture Shading rate image; must be 2D or 2D array.
/// @param[in] mipIndex Mip read.
/// @return Image view, or `VK_NULL_HANDLE` for other view types.
[[nodiscard]] GP_RHI_VULKAN_API VkImageView
    getVulkanFragmentDensityAttachmentView(VulkanTexture& texture, gp::UInt32 mipIndex) noexcept;

}   // namespace gp::rhi::vulkan
