// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/GenericTime.hpp"

namespace gp::platform::windows
{

/// @brief Win32 implementation of hardware timekeeping and high-resolution waitable timer sleep.
/// @details Uses `QueryPerformanceCounter` with a 1-cycle fast path for 10 MHz invariant TSC clocks,
///          `__rdtsc()` for cycle-accurate profiling, and `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` for
///          sub-millisecond thread pacing without polluting global OS timer resolution (ADR-0031).
class Time : public gp::platform::generic::Time
{
public:
    Time() = delete;

    /// @brief Queries the Win32 monotonic performance counter converted to nanoseconds without 64-bit overflow.
    /// @return Monotonic hardware timestamp in nanoseconds.
    [[nodiscard]] static gp::UInt64 nowNanoseconds() noexcept;

    /// @brief Queries the raw `QueryPerformanceCounter` value.
    /// @return Raw QPC ticks.
    [[nodiscard]] static gp::UInt64 rawPerformanceCounter() noexcept;

    /// @brief Queries the cached `QueryPerformanceFrequency` value in ticks per second.
    /// @return QPC frequency in Hz.
    [[nodiscard]] static gp::UInt64 performanceFrequency() noexcept;

    /// @brief Reads the hardware x86/x64 `__rdtsc()` or ARM64 virtual timer register.
    /// @return 64-bit processor cycle counter.
    [[nodiscard]] static gp::UInt64 cpuCycles() noexcept;

    /// @brief Queries the current UTC Unix epoch time in microseconds via `GetSystemTimePreciseAsFileTime`.
    /// @return UTC microseconds since January 1, 1970.
    [[nodiscard]] static gp::UInt64 utcTimestampMicroseconds() noexcept;

    /// @brief Retrieves the current local calendar date and time via `GetLocalTime`.
    /// @return Populated local `DateTime`.
    [[nodiscard]] static DateTime localDateTime() noexcept;

    /// @brief Retrieves the current UTC calendar date and time via `GetSystemTime`.
    /// @return Populated UTC `DateTime`.
    [[nodiscard]] static DateTime utcDateTime() noexcept;

    /// @brief Suspends the calling thread using a thread-local `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` handle.
    /// @param[in] nanoseconds Duration to sleep in nanoseconds (rounded to 100ns Win32 kernel intervals).
    static void sleepNanoseconds(gp::UInt64 nanoseconds) noexcept;
};

}   // namespace gp::platform::windows
