// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/TextureInitializer.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Maps one subresource of a texture for CPU access.
/// @details
///          - `CPUReadback` textures return their persistently mapped readback buffer at the mip offset.
///          - `ReadOnly` copies the subresource into a host-cached staging buffer, submits and blocks until the GPU
///            is idle, then returns the staging pointer.
///          - Writes return a staging buffer; the copy into the image is recorded by `unlockVulkanTexture()`.
///
///          The returned data is tightly packed: `stride` is one row of blocks.
/// @note Top of pipe. Backs `DynamicRHI::lockTexture()`.
/// @param[in,out] cmdList Immediate command list the deferred work is recorded on.
/// @param[in] args Texture (a `VulkanTexture`), subresource and access.
/// @return Mapped data, byte count and row stride.
[[nodiscard]] GP_RHI_VULKAN_API resource::TextureLockResult
    lockVulkanTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept;

/// @brief Ends a lock started by `lockVulkanTexture()`.
/// @details Reads release the staging buffer. Writes record `tracked layout -> TRANSFER_DST`, the copy and the
///          transition back on the command list context, then release the staging buffer once that work completes.
/// @note Top of pipe. Backs `DynamicRHI::unlockTexture()`.
/// @param[in,out] cmdList Immediate command list the deferred work is recorded on.
/// @param[in] args Same arguments as the matching lock.
GP_RHI_VULKAN_API void
    unlockVulkanTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept;

/// @brief Writes a rectangle of one mip of a 2D texture.
/// @details The rows are packed into a staging buffer right away, so `sourceData` can be released on return. The copy
///          is recorded on the command list context.
/// @note Top of pipe. Backs `DynamicRHI::updateTexture2D()`.
/// @param[in,out] cmdList Command list the copy is recorded on.
/// @param[in] texture Destination texture (a `VulkanTexture`).
/// @param[in] mipIndex Destination mip.
/// @param[in] updateRegion Destination rectangle and source offset, in texels.
/// @param[in] sourcePitch Byte pitch of one source row of blocks.
/// @param[in] sourceData First byte of the source image.
GP_RHI_VULKAN_API void updateVulkanTexture2D(
    command::CommandListBase& cmdList,
    resource::Texture* texture,
    gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion2D& updateRegion,
    gp::UInt32 sourcePitch,
    const gp::UInt8* sourceData
) noexcept;

/// @brief Writes a box of one mip of a 3D texture.
/// @details Same as `updateVulkanTexture2D()` with one more dimension: every slice is packed into the staging buffer.
/// @note Top of pipe. Backs `DynamicRHI::updateTexture3D()`.
/// @param[in,out] cmdList Command list the copy is recorded on.
/// @param[in] texture Destination texture (a `VulkanTexture`).
/// @param[in] mipIndex Destination mip.
/// @param[in] updateRegion Destination box and source offset, in texels.
/// @param[in] sourceRowPitch Byte pitch of one source row of blocks.
/// @param[in] sourceDepthPitch Byte pitch of one source slice.
/// @param[in] sourceData First byte of the source volume.
GP_RHI_VULKAN_API void updateVulkanTexture3D(
    command::CommandListBase& cmdList,
    resource::Texture* texture,
    gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion3D& updateRegion,
    gp::UInt32 sourceRowPitch,
    gp::UInt32 sourceDepthPitch,
    const gp::UInt8* sourceData
) noexcept;

}   // namespace gp::rhi::vulkan
