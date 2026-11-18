// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixFutex.hpp"

namespace gp::platform::apple
{

/// @brief Apple Darwin kernel address-wait implementation backed by `__ulock_wait` and `__ulock_wake` ([ADR-0058]).
class GP_CORE_API Futex : public gp::platform::unix::Futex
{
public:
    Futex() = delete;

    /// @brief Blocks the calling thread in the XNU kernel via `__ulock_wait(UL_COMPARE_AND_WAIT)` while
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

    /// @brief Wakes a single thread waiting on `address` via `__ulock_wake(UL_COMPARE_AND_WAIT)`.
    /// @param[in] address 4-byte-aligned atomic state word.
    static void wakeOne(const gp::thread::Atomic<gp::UInt32>& address) noexcept;

    /// @brief Wakes all threads waiting on `address` via `__ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL)`.
    /// @param[in] address 4-byte-aligned atomic state word.
    static void wakeAll(const gp::thread::Atomic<gp::UInt32>& address) noexcept;
};

}   // namespace gp::platform::apple
