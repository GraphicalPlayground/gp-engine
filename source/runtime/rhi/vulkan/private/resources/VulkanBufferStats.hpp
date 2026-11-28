// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/VulkanBufferEnums.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Snapshot of the Vulkan buffer memory counters.
struct VulkanBufferMemoryStats final
{
    /// @brief Bytes per accounting bucket, indexed by `VulkanBufferStatCategory`.
    gp::Int64 categoryBytes[kVulkanBufferStatCategoryCount]{};

    /// @brief Total bytes of every live buffer allocation.
    gp::Int64 totalBufferBytes{ 0 };

    /// @brief Virtual size of every live reserved (sparse) buffer.
    gp::Int64 reservedVirtualBytes{ 0 };

    /// @brief Bytes of reserved buffers currently bound to memory.
    gp::Int64 reservedCommittedBytes{ 0 };
};

/// @brief Resolves the accounting bucket a buffer with `usage` is charged to.
/// @param[in] usage Buffer usage.
/// @return First matching bucket, `Misc` when none applies.
[[nodiscard]] GP_RHI_VULKAN_API VulkanBufferStatCategory
    getVulkanBufferStatCategory(resource::BufferUsageFlags usage) noexcept;

/// @brief Charges or refunds `bufferSize` bytes for a buffer with `usage`.
/// @note Thread-safe; counters are updated with relaxed atomics.
/// @param[in] usage Buffer usage selecting the bucket.
/// @param[in] bufferSize Allocation size in bytes.
/// @param[in] isAllocating True to charge, false to refund.
GP_RHI_VULKAN_API void
    updateVulkanBufferStats(resource::BufferUsageFlags usage, gp::UInt64 bufferSize, bool isAllocating) noexcept;

/// @brief Moves `commitDelta` bytes of a reserved buffer between the uncommitted and committed counters.
/// @note Thread-safe.
/// @param[in] commitDelta Bytes bound or unbound.
/// @param[in] isCommitting True when tiles are being bound, false when they are being released.
GP_RHI_VULKAN_API void updateVulkanReservedBufferStatsOnCommit(gp::UInt64 commitDelta, bool isCommitting) noexcept;

/// @brief Reads every buffer memory counter.
/// @return Snapshot of the counters.
[[nodiscard]] GP_RHI_VULKAN_API VulkanBufferMemoryStats getVulkanBufferMemoryStats() noexcept;

}   // namespace gp::rhi::vulkan

