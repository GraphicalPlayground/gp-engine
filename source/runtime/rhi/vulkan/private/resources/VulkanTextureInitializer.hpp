// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/ResourceForward.hpp"              // IWYU pragma: keep
#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/TextureInitializer.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"

namespace gp::rhi::vulkan
{

/// @brief Records the copy of a tightly packed, mip-major staging buffer into every subresource of `texture`.
/// @details Recorded on the upload context: `UNDEFINED -> TRANSFER_DST`, one `vkCmdCopyBufferToImage()` region per
///          mip covering every layer, then `TRANSFER_DST -> default layout`. The staging buffer is released against
///          the completion sync point of the upload context.
/// @note Bottom of pipe when called from a deferred lambda; `texture` must be image backed.
/// @param[in,out] cmdList Command list whose upload context records the copy.
/// @param[in,out] texture Destination texture.
/// @param[in,out] stagingBuffer Staging buffer holding the data; reset to `nullptr` on return.
GP_RHI_VULKAN_API void recordVulkanTextureUpload(
    command::CommandListBase& cmdList, VulkanTexture& texture, VulkanStagingBuffer*& stagingBuffer
) noexcept;

/// @brief Creates a `VulkanTexture` and returns the initializer that fills it.
/// @details Handles every `TextureInitAction`:
///          - `Default`: no data; the constructor already recorded the initial layout (and clear).
///          - `BulkData`: copies the bulk data into a staging buffer and records the upload right away.
///          - `Initializer`: exposes a mip-major staging buffer the caller fills; uploaded on finalize.
///          - `DataProvider`: pulls every subresource from the provider into the staging buffer, then behaves like
///            `Initializer`.
///
///          Every path registers the texture as the owner of its pool range so defragmentation can move it. CPU
///          readback textures cannot receive initial data; it is discarded.
/// @note Top of pipe. Backs `DynamicRHI::createTextureInitializer()`.
/// @param[in] services Device, memory and context services.
/// @param[in,out] cmdList Command list the deferred work is recorded on.
/// @param[in] createDesc Creation description.
/// @return Initializer owning a reference to the new texture.
[[nodiscard]] GP_RHI_VULKAN_API resource::TextureInitializer createVulkanTextureInitializer(
    const VulkanResourceServices& services,
    command::CommandListBase& cmdList,
    const resource::TextureCreateDesc& createDesc
) noexcept;

}   // namespace gp::rhi::vulkan
