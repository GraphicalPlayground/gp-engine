// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                          // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Snapshot of the device and platform capabilities that shape render passes and framebuffers.
/// @details Render target layouts, render pass builders and dynamic rendering descriptions only read these bits.
///          Gathering them once, instead of querying the device and the RHI globals everywhere, keeps those types
///          pure functions of their inputs: they can be built and inspected without a GPU, and a layout built twice
///          from the same inputs always hashes the same.
struct GP_RHI_VULKAN_API VulkanRenderTargetCaps final
{
    /// @brief `VK_KHR_depth_stencil_resolve` is enabled: multisampled depth can resolve inside the pass.
    bool supportsDepthStencilResolve{ false };

    /// @brief Attachment-based variable rate shading is exposed to the renderer.
    bool supportsAttachmentVariableRateShading{ false };

    /// @brief `VK_KHR_fragment_shading_rate` attachments are usable (feature `attachmentFragmentShadingRate`).
    bool supportsFragmentShadingRateAttachment{ false };

    /// @brief `VK_KHR_create_renderpass2` is enabled.
    bool hasKHRRenderPass2{ false };

    /// @brief `VK_KHR_multiview` is enabled.
    bool hasKHRMultiview{ false };

    /// @brief `VK_EXT_fragment_density_map` is enabled.
    bool hasEXTFragmentDensityMap{ false };

    /// @brief `VK_EXT_load_store_op_none` is enabled.
    bool hasEXTLoadStoreOpNone{ false };

    /// @brief `VK_KHR_dynamic_rendering` is enabled.
    bool hasKHRDynamicRendering{ false };

    /// @brief Separate depth and stencil layouts are tracked (`synchronization2` path).
    bool supportsParallelRendering{ false };

    /// @brief Platform quirk: a depth-stencil attachment that stores one aspect must store both.
    bool requiresDepthStencilFullWrite{ false };

    /// @brief Platform quirk: separate stencil layouts must be folded into the depth layout.
    bool requiresMergedDepthStencilLayout{ false };

    /// @brief How the shading rate image encodes rates.
    pipeline::VRSImageDataType vrsImageDataType{ pipeline::VRSImageDataType::NotSupported };

    /// @brief Engine format of the shading rate image.
    PixelFormat vrsImageFormat{ PixelFormat::Unknown };

    /// @brief Width in pixels covered by one shading rate texel.
    gp::UInt32 vrsTileWidth{ 0u };

    /// @brief Height in pixels covered by one shading rate texel.
    gp::UInt32 vrsTileHeight{ 0u };

    /// @brief Reports whether the shading rate image has a known encoding.
    /// @return True for palette or fractional encodings.
    [[nodiscard]] bool hasValidShadingRateDataType() const noexcept;

    /// @brief Returns the layout the shading rate attachment is used in.
    /// @return `FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL` for palettes, `FRAGMENT_DENSITY_MAP_OPTIMAL` for fractional
    ///         maps, `UNDEFINED` when unsupported.
    [[nodiscard]] VkImageLayout getShadingRateImageLayout() const noexcept;

    /// @brief Reports whether render pass descriptions keep separate depth and stencil layouts.
    /// @return True when parallel rendering is on and the platform does not ask for merged layouts.
    [[nodiscard]] bool usesSeparateDepthStencilLayouts() const noexcept;
};

/// @brief Builds the capability snapshot of a device.
/// @details Reads the enabled optional extensions and their features, the RHI variable rate shading globals and the
///          platform quirks.
/// @param[in] device Logical device.
/// @return Capability snapshot.
[[nodiscard]] GP_RHI_VULKAN_API VulkanRenderTargetCaps makeVulkanRenderTargetCaps(const VulkanDevice& device) noexcept;

}   // namespace gp::rhi::vulkan
