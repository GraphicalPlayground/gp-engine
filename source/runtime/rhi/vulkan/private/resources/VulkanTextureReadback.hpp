// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"       // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/ColorForward.hpp"   // IWYU pragma: keep
#include "resources/TextureEnums.hpp"
#include "resources/TextureInitializer.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Reads a rectangle of one subresource into 8-bit colors.
/// @details Copies the rectangle into a host-cached staging buffer (`tracked layout -> TRANSFER_SRC`, copy, back),
///          submits and blocks until the GPU is idle, then converts the texels on the CPU. `CPUReadback` textures are
///          read in place. With `flags.outputStencil` on a depth/stencil format the stencil aspect is read as gray.
///          The rectangle is clamped to the mip; `IntRect::max` is exclusive.
/// @note Blocks the calling thread on the GPU. Multisampled and block-compressed textures are refused.
/// @param[in,out] cmdList Immediate command list the copy is recorded and submitted on.
/// @param[in] texture Source texture (a `VulkanTexture`).
/// @param[in] rect Texel rectangle in the mip.
/// @param[out] outData Receives `width * height` colors, row-major; emptied on failure.
/// @param[in] flags Subresource (mip, face, array slice) and conversion options.
/// @return True when `outData` was filled.
GP_RHI_VULKAN_API bool readVulkanSurfaceData(
    command::CommandListImmediate& cmdList,
    resource::Texture* texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::Color>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept;

/// @brief Reads a rectangle of one subresource into linear colors.
/// @details Same copy as the `Color` overload. 8-bit texels are sRGB decoded when the texture was created with
///          `TextureCreateFlags::SRGB`.
/// @note Blocks the calling thread on the GPU.
/// @param[in,out] cmdList Immediate command list the copy is recorded and submitted on.
/// @param[in] texture Source texture (a `VulkanTexture`).
/// @param[in] rect Texel rectangle in the mip.
/// @param[out] outData Receives `width * height` colors, row-major; emptied on failure.
/// @param[in] flags Subresource (mip, face, array slice) and conversion options.
/// @return True when `outData` was filled.
GP_RHI_VULKAN_API bool readVulkanSurfaceData(
    command::CommandListImmediate& cmdList,
    resource::Texture* texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::LinearColor>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept;

/// @brief Reads a rectangle of one subresource of a float texture into half-precision colors.
/// @note Blocks the calling thread on the GPU. Only float formats are accepted.
/// @param[in,out] cmdList Immediate command list the copy is recorded and submitted on.
/// @param[in] texture Source texture (a `VulkanTexture`).
/// @param[in] rect Texel rectangle in the mip.
/// @param[out] outData Receives `width * height` colors, row-major; emptied on failure.
/// @param[in] cubeFace Cube face (ignored by non-cube textures).
/// @param[in] arrayIndex Array slice.
/// @param[in] mipIndex Mip level.
/// @return True when `outData` was filled.
GP_RHI_VULKAN_API bool readVulkanSurfaceFloatData(
    command::CommandListImmediate& cmdList,
    resource::Texture* texture,
    const resource::IntRect& rect,
    gp::Vector<resource::Float16Color>& outData,
    resource::CubeFace cubeFace,
    gp::Int32 arrayIndex,
    gp::Int32 mipIndex
) noexcept;

/// @brief Reads a box of mip 0 of a 3D float texture into half-precision colors.
/// @note Blocks the calling thread on the GPU. Only float formats are accepted.
/// @param[in,out] cmdList Immediate command list the copy is recorded and submitted on.
/// @param[in] texture Source 3D texture (a `VulkanTexture`).
/// @param[in] rect Texel rectangle of every slice.
/// @param[in] zMinMax Slice range `[x, y)`.
/// @param[out] outData Receives `width * height * depth` colors, slice-major then row-major; emptied on failure.
/// @return True when `outData` was filled.
GP_RHI_VULKAN_API bool read3DVulkanSurfaceFloatData(
    command::CommandListImmediate& cmdList,
    resource::Texture* texture,
    const resource::IntRect& rect,
    resource::IntPoint zMinMax,
    gp::Vector<resource::Float16Color>& outData
) noexcept;

/// @brief Maps mip 0 of a `CPUReadback` texture.
/// @details The readback buffer is persistently mapped, so this only invalidates the host cache and returns the
///          pointer. The caller must have waited for the copy that filled the texture (its GPU fence).
/// @note Backs `DynamicRHI::mapStagingSurface()`.
/// @param[in] texture Source texture (a `CPUReadback` `VulkanTexture`).
/// @param[out] outData Receives the first texel, or `nullptr` when the texture is not a readback texture.
/// @param[out] outWidth Receives the row pitch in texels.
/// @param[out] outHeight Receives the row count.
GP_RHI_VULKAN_API void mapVulkanStagingSurface(
    resource::Texture* texture, void*& outData, gp::Int32& outWidth, gp::Int32& outHeight
) noexcept;

/// @brief Ends a `mapVulkanStagingSurface()` mapping.
/// @details The memory stays mapped for the texture's lifetime, so there is nothing to release.
/// @note Backs `DynamicRHI::unmapStagingSurface()`.
/// @param[in] texture Mapped texture.
GP_RHI_VULKAN_API void unmapVulkanStagingSurface(resource::Texture* texture) noexcept;

/// @brief Makes `destTexture` share the image and memory of `srcTexture`.
/// @note Backs `DynamicRHI::aliasTextureResources()`. See `VulkanTexture::aliasTextureResources()`.
/// @param[in,out] destTexture Texture whose storage is replaced.
/// @param[in,out] srcTexture Texture providing the storage.
GP_RHI_VULKAN_API void
    aliasVulkanTextureResources(resource::TextureRef& destTexture, resource::TextureRef& srcTexture) noexcept;

}   // namespace gp::rhi::vulkan
