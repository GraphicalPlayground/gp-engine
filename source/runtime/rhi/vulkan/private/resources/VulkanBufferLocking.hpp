// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Maps a range of `buffer` from the RHI timeline.
/// @note Backs `DynamicRHI::lockBufferBottomOfPipe()`. `buffer` must be a `VulkanBuffer`.
/// @param[in,out] cmdList Executing command list.
/// @param[in,out] buffer Buffer to map.
/// @param[in] offset Byte offset of the range.
/// @param[in] numBytes Byte count to map.
/// @param[in] lockMode Access requested.
/// @return Pointer to the first byte of the range.
[[nodiscard]] GP_RHI_VULKAN_API void* lockVulkanBufferBottomOfPipe(
    command::CommandListBase& cmdList,
    resource::Buffer* buffer,
    gp::UInt32 offset,
    gp::UInt32 numBytes,
    resource::ResourceLockMode lockMode
) noexcept;

/// @brief Ends a lock started by `lockVulkanBufferBottomOfPipe()`.
/// @note Backs `DynamicRHI::unlockBufferBottomOfPipe()`.
/// @param[in,out] cmdList Executing command list.
/// @param[in,out] buffer Locked buffer.
GP_RHI_VULKAN_API void
    unlockVulkanBufferBottomOfPipe(command::CommandListBase& cmdList, resource::Buffer* buffer) noexcept;

/// @brief Maps a range of `buffer` from the recording thread.
/// @details The Vulkan backend defers every native command itself, so this is the same operation as the bottom of
///          pipe lock and needs no command list flush.
/// @note Backs `DynamicRHI::lockBuffer()`.
/// @param[in,out] cmdList Recording command list.
/// @param[in,out] buffer Buffer to map.
/// @param[in] offset Byte offset of the range.
/// @param[in] numBytes Byte count to map.
/// @param[in] lockMode Access requested.
/// @return Pointer to the first byte of the range.
[[nodiscard]] GP_RHI_VULKAN_API void* lockVulkanBuffer(
    command::CommandListBase& cmdList,
    resource::Buffer* buffer,
    gp::UInt32 offset,
    gp::UInt32 numBytes,
    resource::ResourceLockMode lockMode
) noexcept;

/// @brief Ends a lock started by `lockVulkanBuffer()`.
/// @note Backs `DynamicRHI::unlockBuffer()`.
/// @param[in,out] cmdList Recording command list.
/// @param[in,out] buffer Locked buffer.
GP_RHI_VULKAN_API void unlockVulkanBuffer(command::CommandListBase& cmdList, resource::Buffer* buffer) noexcept;

}   // namespace gp::rhi::vulkan

