// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "resources/BufferEnums.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Reports whether `usage` contains any bit of `flags`.
/// @param[in] usage Buffer usage to test.
/// @param[in] flags Bits to look for.
/// @return True when at least one bit of `flags` is set in `usage`.
[[nodiscard]] constexpr bool
    hasAnyBufferUsage(const resource::BufferUsageFlags usage, const resource::BufferUsageFlags flags) noexcept
{
    return (usage & flags) != resource::BufferUsageFlags::None;
}

/// @brief Reports whether a buffer with `usage` lives in host-visible memory that is rewritten often.
/// @param[in] usage Buffer usage.
/// @return True for `Dynamic` or `Volatile` buffers.
[[nodiscard]] constexpr bool isDynamicBufferUsage(const resource::BufferUsageFlags usage) noexcept
{
    return hasAnyBufferUsage(usage, resource::BufferUsageFlags::Dynamic | resource::BufferUsageFlags::Volatile);
}

/// @brief Translates engine buffer usage into the `VkBufferUsageFlags` a `VkBuffer` must be created with.
/// @details `TRANSFER_SRC` is always set: it costs nothing on current hardware and debug tooling relies on it.
///          Zero-size buffers only get the bits that select a descriptor type, since no memory is ever bound.
///          Ray tracing support adds device-address and build-input bits, and vertex or index buffers also get
///          `STORAGE_BUFFER` so hit groups can fetch geometry from them.
/// @param[in] extensions Optional device extensions enabled on the device.
/// @param[in] usage Engine buffer usage.
/// @param[in] isZeroSize True when the buffer is created with a size of zero.
/// @return Native buffer usage flags.
[[nodiscard]] GP_RHI_VULKAN_API VkBufferUsageFlags translateBufferUsageFlags(
    const OptionalVulkanDeviceExtensions& extensions, resource::BufferUsageFlags usage, bool isZeroSize
) noexcept;

/// @brief Queries the GPU virtual address of `buffer` when buffer device address is enabled.
/// @param[in] device Active `VulkanDevice`.
/// @param[in] buffer Native buffer handle.
/// @return Device address, or `0` when the extension is unavailable or `buffer` is null.
[[nodiscard]] GP_RHI_VULKAN_API VkDeviceAddress
    getVulkanBufferDeviceAddress(const VulkanDevice& device, VkBuffer buffer) noexcept;

}   // namespace gp::rhi::vulkan

