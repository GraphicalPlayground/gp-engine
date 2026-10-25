// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixTime.hpp"

namespace gp::platform::apple
{

/// @brief Shared Apple (Darwin / Mach) hardware timekeeping implementation for macOS, iOS, tvOS, and visionOS.
/// @details Inherits POSIX UTC/calendar queries and CPU cycle counters from `gp::platform::unix::Time`,
///          while overriding monotonic counter and sleep methods with Darwin commpage-backed
///          `clock_gettime_nsec_np(CLOCK_UPTIME_RAW)`, `mach_absolute_time()`, and `mach_wait_until()`.
class Time : public gp::platform::unix::Time
{
public:
    Time() = delete;

    /// @brief Queries Darwin's commpage-accelerated `CLOCK_UPTIME_RAW` in nanoseconds.
    /// @return Monotonic hardware timestamp in nanoseconds.
    [[nodiscard]] static gp::UInt64 nowNanoseconds() noexcept;

    /// @brief Queries raw Mach absolute time ticks via `mach_absolute_time()`.
    /// @return Raw Mach hardware counter ticks.
    [[nodiscard]] static gp::UInt64 rawPerformanceCounter() noexcept;

    /// @brief Queries the Mach timebase hardware frequency in ticks per second (e.g., 24 MHz on Apple Silicon).
    /// @return Hardware counter frequency in Hz.
    [[nodiscard]] static gp::UInt64 performanceFrequency() noexcept;

    /// @brief Suspends the calling thread until an exact Mach absolute deadline via `mach_wait_until()`.
    /// @param[in] nanoseconds Duration to sleep in nanoseconds.
    static void sleepNanoseconds(gp::UInt64 nanoseconds) noexcept;
};

}   // namespace gp::platform::apple
