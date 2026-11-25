// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::vulkan
{

/// @brief Behavioral and ownership flags controlling `VulkanSemaphore` creation, submission, and teardown.
enum class VulkanSemaphoreFlags : gp::UInt8
{
    /// @brief Standard device-owned binary semaphore.
    None = 0u,

    /// @brief Handle is owned externally and will not be destroyed when `VulkanSemaphore` destructs.
    ExternallyOwned = 1u << 1u,

    /// @brief Semaphore is signaled outside a normal submission payload (for example, `vkAcquireNextImageKHR`).
    ExternallySignaled = 1u << 2u,

    /// @brief Creates a `VK_SEMAPHORE_TYPE_TIMELINE` semaphore (requires `VK_KHR_timeline_semaphore` / Vulkan 1.2).
    Timeline = 1u << 3u,

    /// @brief Destroys the handle immediately on destruction rather than deferring to a deletion queue.
    ImmediateDeletion = 1u << 4u,

    /// @brief Semaphore is awaited outside a normal submission payload (for example, `vkQueuePresentKHR`).
    ExternallyWaited = 1u << 5u
};

/// @brief Scheduling priority category for a `VulkanSyncPoint`.
enum class VulkanSyncPointType : gp::UInt8
{
    /// @brief Time-critical GPU-to-CPU sync point signaled as soon as the queue completes its payload.
    GPUAndCPU = 0u,

    /// @brief Deferred completion sync point that may be signaled at frame boundaries or after command completion.
    Context
};

}   // namespace gp::rhi::vulkan

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::vulkan::VulkanSemaphoreFlags);

namespace gp::rhi::vulkan
{

/// @brief Reports whether any bit in `flag` is set inside `value`.
/// @param[in] value Combined flags.
/// @param[in] flag Bit(s) to test.
/// @return True when `(value & flag) != VulkanSemaphoreFlags::None`.
[[nodiscard]] inline constexpr bool
    hasAnySemaphoreFlags(const VulkanSemaphoreFlags value, const VulkanSemaphoreFlags flag) noexcept
{
    return (value & flag) != VulkanSemaphoreFlags::None;
}

}   // namespace gp::rhi::vulkan
