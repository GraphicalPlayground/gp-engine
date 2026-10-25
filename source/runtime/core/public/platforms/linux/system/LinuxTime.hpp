// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixTime.hpp"

namespace gp::platform::linux
{

/// @brief Linux-specific hardware timekeeping implementation (child of `gp::platform::unix::Time`).
/// @details Inherits vDSO-accelerated `CLOCK_MONOTONIC` and calendar queries from `unix::Time` and upgrades
///          `sleepNanoseconds()` to use `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` backed by
///          Linux kernel high-resolution timers (`hrtimers`) for zero-drift wakeup under signal interrupts.
class Time : public gp::platform::unix::Time
{
public:
    Time() = delete;

    /// @brief Suspends the calling thread until an absolute `CLOCK_MONOTONIC` deadline using `clock_nanosleep`.
    /// @param[in] nanoseconds Duration to sleep in nanoseconds.
    static void sleepNanoseconds(gp::UInt64 nanoseconds) noexcept;
};

}   // namespace gp::platform::linux
