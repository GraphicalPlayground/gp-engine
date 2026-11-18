// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixFutex.hpp"

namespace gp::platform::linux
{

/// @brief Linux kernel `futex(2)` implementation (`FUTEX_WAIT_PRIVATE` / `FUTEX_WAKE_PRIVATE`) ([ADR-0058]).
class GP_CORE_API Futex : public gp::platform::unix::Futex
{
public:
    Futex() = delete;

    /// @brief Blocks the calling thread in the Linux kernel via `SYS_futex` (`FUTEX_WAIT_PRIVATE`) while
    ///        `address == expectedValue`.
    /// @param[in] address 4-byte-aligned atomic state word to monitor.
    /// @param[in] expectedValue Value that `address` must equal for the thread to sleep.
    /// @param[in] timeoutNanoseconds Maximum wait time in nanoseconds, or `kInfiniteFutexTimeoutNs`.
    /// @return `FutexResult` indicating wakeup, value mismatch, or timeout.
    [[nodiscard]] static FutexResult waitOnAddress(
        const gp::thread::Atomic<gp::UInt32>& address,
        gp::UInt32 expectedValue,
        gp::UInt64 timeoutNanoseconds = kInfiniteFutexTimeoutNs
    ) noexcept;

    /// @brief Wakes a single thread waiting on `address` via `FUTEX_WAKE_PRIVATE` (`1`).
    /// @param[in] address 4-byte-aligned atomic state word.
    static void wakeOne(const gp::thread::Atomic<gp::UInt32>& address) noexcept;

    /// @brief Wakes all threads waiting on `address` via `FUTEX_WAKE_PRIVATE` (`INT32_MAX`).
    /// @param[in] address 4-byte-aligned atomic state word.
    static void wakeAll(const gp::thread::Atomic<gp::UInt32>& address) noexcept;
};

}   // namespace gp::platform::linux
