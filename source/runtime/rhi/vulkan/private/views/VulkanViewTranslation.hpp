// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"
#include "views/VulkanTextureViewDesc.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Returns the bindless descriptor type a shader resource view is written as.
/// @details Vulkan has no read-only storage buffer descriptor, so structured and raw SRVs are written as storage
///          buffers (`BufferUAV`). `R64_UInt` textures cannot be sampled and are bound as storage images
///          (`TextureUAV`); the shader compiler patches the matching declarations.
/// @param[in] viewDesc SRV description.
/// @return Descriptor type, or `DescriptorType::Invalid` for an unknown buffer view type.
[[nodiscard]] GP_RHI_VULKAN_API view::DescriptorType
    getVulkanSRVDescriptorType(const view::ViewDesc& viewDesc) noexcept;

/// @brief Returns the bindless descriptor type an unordered access view is written as.
/// @param[in] viewDesc UAV description.
/// @return Descriptor type, or `DescriptorType::Invalid` for an unknown buffer view type.
[[nodiscard]] GP_RHI_VULKAN_API view::DescriptorType
    getVulkanUAVDescriptorType(const view::ViewDesc& viewDesc) noexcept;

/// @brief Translates the dimension of a texture SRV into a native view type.
/// @param[in] dimension View dimension; `Unknown` falls back to `textureViewType`.
/// @param[in] textureViewType View type matching the dimension of the texture itself.
/// @return Native view type, or `VK_IMAGE_VIEW_TYPE_MAX_ENUM` for an invalid dimension.
[[nodiscard]] GP_RHI_VULKAN_API VkImageViewType
    getVulkanSRVImageViewType(view::TextureViewDimension dimension, VkImageViewType textureViewType) noexcept;

/// @brief Translates the dimension of a texture UAV into a native view type.
/// @details Storage images cannot be cube views, so cube and cube-array UAVs become 2D array views over the faces.
/// @param[in] dimension View dimension; `Unknown` falls back to `textureViewType`.
/// @param[in] textureViewType View type matching the dimension of the texture itself.
/// @return Native view type, or `VK_IMAGE_VIEW_TYPE_MAX_ENUM` for an invalid dimension.
[[nodiscard]] GP_RHI_VULKAN_API VkImageViewType
    getVulkanUAVImageViewType(view::TextureViewDimension dimension, VkImageViewType textureViewType) noexcept;

/// @brief Converts an RHI array range into a native layer range.
/// @details RHI cube views count whole cubes, Vulkan counts faces: the range is scaled by six for cube dimensions.
/// @param[in] dimension View dimension.
/// @param[in] arrayRange Resolved RHI array range.
/// @param[in] textureLayerCount Layer count of the image (faces included); used to validate the result.
/// @param[out] outFirstLayer Receives the first native layer.
/// @param[out] outNumLayers Receives the native layer count.
GP_RHI_VULKAN_API void getVulkanCubeAdjustedArrayRange(
    view::TextureViewDimension dimension,
    const resource::Range16& arrayRange,
    gp::UInt32 textureLayerCount,
    gp::UInt32& outFirstLayer,
    gp::UInt32& outNumLayers
) noexcept;

/// @brief Builds the backend description of a texture SRV.
/// @details The view keeps the partial aspect (depth only for depth-stencil images), applies the format swizzle and
///          drops the storage usage, which sRGB and many sampled formats do not support.
/// @param[in] texture Target texture.
/// @param[in] viewInfo Resolved SRV parameters.
/// @return Backend view description.
[[nodiscard]] GP_RHI_VULKAN_API VulkanTextureViewDesc translateVulkanTextureViewDesc(
    const VulkanTexture& texture, const view::ViewDesc::TextureSRV::ViewInfo& viewInfo
) noexcept;

/// @brief Builds the backend description of a texture UAV.
/// @details The view covers one mip, never applies sRGB and always uses the identity swizzle (storage images
///          require it).
/// @param[in] texture Target texture.
/// @param[in] viewInfo Resolved UAV parameters.
/// @return Backend view description.
[[nodiscard]] GP_RHI_VULKAN_API VulkanTextureViewDesc translateVulkanTextureViewDesc(
    const VulkanTexture& texture, const view::ViewDesc::TextureUAV::ViewInfo& viewInfo
) noexcept;

/// @brief Builds the create info of a texel buffer view.
/// @details The range is clamped to `maxTexelBufferElements` texels. The buffer must have a texel buffer usage and
///          the offset must honor `minTexelBufferOffsetAlignment`.
/// @param[in] device Device providing the limits.
/// @param[in] buffer Target buffer.
/// @param[in] format Texel format; must not be `Unknown`.
/// @param[in] offset Byte offset inside the buffer, not including its allocation offset.
/// @param[in] size Byte size of the view.
/// @return Create info ready for `vkCreateBufferView()`.
[[nodiscard]] GP_RHI_VULKAN_API VkBufferViewCreateInfo translateVulkanTypedBufferViewDesc(
    const VulkanDevice& device, const VulkanBuffer& buffer, PixelFormat format, gp::UInt32 offset, gp::UInt32 size
) noexcept;

/// @brief Returns the byte size a structured buffer view really covers.
/// @details Volatile buffers are backed by per-lock allocations that may be smaller than the creation size, so the
///          view is clamped to the current allocation.
/// @param[in] buffer Target buffer.
/// @param[in] size Requested byte size.
/// @return Byte size of the view.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32
    getVulkanStructuredBufferViewSize(const VulkanBuffer& buffer, gp::UInt32 size) noexcept;

}   // namespace gp::rhi::vulkan
