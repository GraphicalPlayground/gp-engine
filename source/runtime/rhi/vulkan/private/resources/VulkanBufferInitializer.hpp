// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/ResourceForward.hpp"              // IWYU pragma: keep
#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "resources/BufferInitializer.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Creates a `VulkanBuffer` and returns the initializer that fills it.
/// @details Handles every `BufferInitAction`:
///          - `Default` and null buffers: no data; the initializer only hands the buffer back.
///          - `Zeroed`: clears host-visible memory right away, otherwise records a `vkCmdFillBuffer()` on the upload
///            context when the initializer is finalized.
///          - `ResourceArray`: copies the initial data now, through the host mapping or a write lock.
///          - `Initializer`: exposes the host mapping or a write lock for the caller to fill; the lock is released
///            when the initializer is finalized.
///
///          Every path registers the buffer as the owner of its pool range so defragmentation can move it.
/// @note Top of pipe. Backs `DynamicRHI::createBufferInitializer()`.
/// @param[in] services Device, memory and context services.
/// @param[in,out] cmdList Command list the deferred work is recorded on.
/// @param[in] createDesc Creation description.
/// @return Initializer owning a reference to the new buffer.
[[nodiscard]] GP_RHI_VULKAN_API resource::BufferInitializer createVulkanBufferInitializer(
    const VulkanResourceServices& services,
    command::CommandListBase& cmdList,
    const resource::BufferCreateDesc& createDesc
) noexcept;

}   // namespace gp::rhi::vulkan

