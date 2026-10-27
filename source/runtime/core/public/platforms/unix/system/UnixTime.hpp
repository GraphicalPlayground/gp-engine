// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/GenericTime.hpp"

namespace gp::platform::unix
{

/// @brief Shared POSIX hardware timekeeping implementation for Unix-derived operating systems.
/// @details Uses `clock_gettime(CLOCK_MONOTONIC)` for invariant nanosecond timekeeping,
///          reentrant `localtime_r`/`gmtime_r` for thread-safe calendar queries, and an `EINTR`-safe
///          `nanosleep` loop for sub-millisecond thread suspension.
class GP_CORE_API Time : public gp::platform::generic::Time
{
public:
    Time() = delete;

    /// @brief Queries `clock_gettime(CLOCK_MONOTONIC)` converted to nanoseconds.
    /// @return Monotonic hardware timestamp in nanoseconds.
    [[nodiscard]] static gp::UInt64 nowNanoseconds() noexcept;

    /// @brief Queries the raw POSIX monotonic clock in nanoseconds (1 tick = 1ns on POSIX).
    /// @return Raw monotonic counter value.
    [[nodiscard]] static gp::UInt64 rawPerformanceCounter() noexcept;

    /// @brief Returns the POSIX monotonic clock frequency (1 GHz / 1,000,000,000 Hz).
    /// @return Always returns `1'000'000'000ULL`.
    [[nodiscard]] static constexpr gp::UInt64 performanceFrequency() noexcept
    {
        return 1'000'000'000ULL;
    }

    /// @brief Reads the hardware cycle counter (`rdtsc` on x86_64, `cntvct_el0` on ARM64).
    /// @return 64-bit processor cycle counter.
    [[nodiscard]] static gp::UInt64 cpuCycles() noexcept;

    /// @brief Queries `clock_gettime(CLOCK_REALTIME)` converted to UTC microseconds since Jan 1, 1970.
    /// @return UTC Unix epoch timestamp in microseconds.
    [[nodiscard]] static gp::UInt64 utcTimestampMicroseconds() noexcept;

    /// @brief Queries the current local calendar date and time using thread-safe `localtime_r`.
    /// @return Populated local `DateTime`.
    [[nodiscard]] static DateTime localDateTime() noexcept;

    /// @brief Queries the current UTC calendar date and time using thread-safe `gmtime_r`.
    /// @return Populated UTC `DateTime`.
    [[nodiscard]] static DateTime utcDateTime() noexcept;

    /// @brief Suspends the calling thread using POSIX `nanosleep`, automatically resuming if interrupted by `EINTR`.
    /// @param[in] nanoseconds Duration to sleep in nanoseconds.
    static void sleepNanoseconds(gp::UInt64 nanoseconds) noexcept;
};

}   // namespace gp::platform::unix
