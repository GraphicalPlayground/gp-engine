// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Reports whether `flags` contains any bit of `mask`.
/// @param[in] flags Texture creation flags.
/// @param[in] mask Bits to test.
/// @return True when at least one bit of `mask` is set.
[[nodiscard]] inline constexpr bool
    hasAnyTextureFlags(const resource::TextureCreateFlags flags, const resource::TextureCreateFlags mask) noexcept
{
    return (flags & mask) != resource::TextureCreateFlags::None;
}

/// @brief Translates texture creation flags into the `VkImageUsageFlags` the image is created with.
/// @details Every image can be a transfer source and destination and can be sampled; targetable, UAV and presentable
///          flags add the matching attachment and storage bits. A memoryless render target on a device that supports
///          lazily allocated memory becomes a transient attachment and loses the transfer and sampled bits, because
///          tile memory is never written back.
/// @param[in] flags Texture creation flags.
/// @param[in] supportsMemoryless True when the device exposes `LAZILY_ALLOCATED` memory.
/// @return Usage flags before the per-format capability filter of `filterVulkanImageUsageByFormatFeatures()`.
[[nodiscard]] GP_RHI_VULKAN_API VkImageUsageFlags
    getVulkanImageUsageFlags(resource::TextureCreateFlags flags, bool supportsMemoryless) noexcept;

/// @brief Removes the usage bits a format cannot honor with the chosen tiling.
/// @details Storage usage is kept on a format without storage support when the image declares a separate UAV format
///          (`hasUAVFormat`); the image is then created with `VK_IMAGE_CREATE_EXTENDED_USAGE_BIT` instead.
/// @param[in] usage Requested usage.
/// @param[in] formatFeatures Linear or optimal tiling features of the image format.
/// @param[in] hasUAVFormat True when a separate UAV view format is declared.
/// @param[out] outNeedsExtendedUsage Receives true when `VK_IMAGE_CREATE_EXTENDED_USAGE_BIT` is required.
/// @return Usage restricted to what the format supports.
[[nodiscard]] GP_RHI_VULKAN_API VkImageUsageFlags filterVulkanImageUsageByFormatFeatures(
    VkImageUsageFlags usage, VkFormatFeatureFlags formatFeatures, bool hasUAVFormat, bool& outNeedsExtendedUsage
) noexcept;

/// @brief Returns the layout a freshly created image must be transitioned to so it matches its initial RHI access.
/// @param[in] access Initial access state of the texture.
/// @param[in] isDepthStencilTarget True for depth/stencil targetable textures.
/// @param[in] supportsSampling True when the image has `SAMPLED` usage.
/// @param[in] supportsRenderingLocalRead True when render targets use `VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR`.
/// @return Target layout, or `VK_IMAGE_LAYOUT_UNDEFINED` when no transition is needed.
[[nodiscard]] GP_RHI_VULKAN_API VkImageLayout getVulkanInitialImageLayout(
    sync::ResourceAccess access, bool isDepthStencilTarget, bool supportsSampling, bool supportsRenderingLocalRead
) noexcept;

/// @brief Returns the number of Vulkan array layers of a texture (cube faces count as layers).
/// @param[in] desc Texture description.
/// @return `arraySize * 6` for cubes, `arraySize` for arrays, `1` for 2D and 3D textures.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 getVulkanTextureLayerCount(const resource::TextureDesc& desc) noexcept;

/// @brief Reports whether views of the image may reinterpret its format (sRGB pairs, UAV or aliasable formats).
/// @param[in] desc Texture description.
/// @return True when `VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT` is needed.
[[nodiscard]] GP_RHI_VULKAN_API bool isVulkanTextureFormatMutable(const resource::TextureDesc& desc) noexcept;

/// @brief Resolves the `VkFormat` the device picked for `format`.
/// @details Uses the device format table (which accounts for fallbacks) and the static table when the device did not
///          map the format.
/// @param[in] device Logical device.
/// @param[in] format Engine pixel format.
/// @param[in] isSRGB True for the sRGB variant.
/// @return Native format, or `VK_FORMAT_UNDEFINED`.
[[nodiscard]] GP_RHI_VULKAN_API VkFormat
    resolveVulkanTextureFormat(const VulkanDevice& device, PixelFormat format, bool isSRGB) noexcept;

/// @brief Fills `outInfo` with the image creation parameters of `desc`.
/// @details Validates the extent against the device limits, decides tiling, sample count and flags, and chains the
///          view format list when `VK_KHR_image_format_list` is available and views reinterpret the format.
/// @param[in] device Logical device.
/// @param[in] desc Texture description.
/// @param[in] forceLinearTexture True to create a linearly tiled image (CPU readback).
/// @param[in] supportsMemoryless True when the device exposes `LAZILY_ALLOCATED` memory.
/// @param[out] outInfo Receives the create info; must stay where it is (it holds internal pointers).
/// @param[out] outViewFormat Receives the format of shader resource views.
/// @param[out] outStorageFormat Receives the format of unordered access views.
/// @return False when the description exceeds a device limit.
[[nodiscard]] GP_RHI_VULKAN_API bool generateVulkanImageCreateInfo(
    const VulkanDevice& device,
    const resource::TextureDesc& desc,
    bool forceLinearTexture,
    bool supportsMemoryless,
    VulkanImageCreateInfo& outInfo,
    VkFormat& outViewFormat,
    VkFormat& outStorageFormat
) noexcept;

/// @brief Computes where a subresource lives in a tightly packed, mip-major upload buffer.
/// @details Vulkan copies one region per mip covering every layer, so all layers of mip 0 come first, then all layers
///          of mip 1, and so on. Inside a mip, layer `L` starts at `L * mipSize`.
/// @param[in] desc Texture description.
/// @param[in] mipIndex Mip level.
/// @param[in] layerIndex Combined array layer.
/// @param[out] outRowStride Receives the byte pitch of one row of blocks.
/// @param[out] outSubresourceSize Receives the byte size of the subresource.
/// @return Byte offset of the subresource, or `0` with zero outputs for out-of-range indices.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64 calculateVulkanSubresourceOffset(
    const resource::TextureDesc& desc,
    gp::UInt32 mipIndex,
    gp::UInt32 layerIndex,
    gp::UInt64& outRowStride,
    gp::UInt64& outSubresourceSize
) noexcept;

}   // namespace gp::rhi::vulkan
