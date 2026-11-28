// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Returns the pipeline stage timestamps are written at (`r.Vulkan.TimestampQueryStage`).
/// @return `VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT` (default) or `VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT`.
[[nodiscard]] GP_RHI_VULKAN_API VkPipelineStageFlagBits getVulkanTimestampQueryStage() noexcept;

/// @brief Returns the number of queries in each reusable timestamp pool (`r.Vulkan.TimingQueryPoolSize`).
/// @return Query count, at least 1.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 getVulkanTimingQueryPoolSize() noexcept;

/// @brief Returns how many frames an unused pool waits before it is destroyed (`r.Vulkan.QueryPoolDeletionDelay`).
/// @return Frame count.
[[nodiscard]] GP_RHI_VULKAN_API gp::Int32 getVulkanQueryPoolDeletionDelay() noexcept;

/// @brief Returns the control flags occlusion queries begin with (`r.Vulkan.QueryUseControlPrecisionBit`).
/// @return `VK_QUERY_CONTROL_PRECISE_BIT` when enabled, otherwise 0.
[[nodiscard]] GP_RHI_VULKAN_API VkQueryControlFlags getVulkanOcclusionQueryControlFlags() noexcept;

/// @brief Converts a raw GPU timestamp to microseconds.
/// @details The tick frequency is truncated to an integer number of ticks per second first, so results match the
///          frequency reported to profilers.
/// @param[in] ticks Raw timestamp in GPU ticks.
/// @param[in] timestampPeriod Nanoseconds per tick (`VkPhysicalDeviceLimits::timestampPeriod`).
/// @return Timestamp in microseconds, or 0 when `timestampPeriod` is not positive.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64
    convertVulkanTimestampToMicroseconds(gp::UInt64 ticks, gp::Float32 timestampPeriod) noexcept;

}   // namespace gp::rhi::vulkan
