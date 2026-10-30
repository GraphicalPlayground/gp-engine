// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::memory
{

/// @brief Configuration bitflags controlling memory allocator behavior and instrumentation.
enum class AllocatorFlags : gp::UInt8
{
    /// @brief Standard allocation behavior with no extra overhead.
    None = 0x00,

    /// @brief Automatically zero-initializes all newly allocated memory blocks.
    ZeroMemory = 0x01,

    /// @brief Indicates that the allocator synchronizes access internally for multi-threaded callers.
    ThreadSafe = 0x02,

    /// @brief Permits the allocator to spill over into a secondary fallback allocator when capacity is exhausted.
    AllowFallback = 0x04,

    /// @brief Enables named memory pool telemetry and high-water mark tracking in the profiler.
    TrackTelemetry = 0x08
};

/// @brief Growth strategy applied by containers when expanding capacity.
enum class AllocatorGrowthPolicy : gp::UInt8
{
    /// @brief Amortized geometric slack growth (1.375x + constant slack, quantized to allocator bin size).
    Geometric = 0,

    /// @brief Exact allocation matching the requested element count with zero slack.
    Exact = 1,

    /// @brief Fixed capacity; growing beyond the initial capacity is disallowed.
    Fixed = 2
};

}   // namespace gp::memory

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::memory::AllocatorFlags);
