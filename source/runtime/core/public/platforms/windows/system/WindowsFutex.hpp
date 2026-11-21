// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/GenericFutex.hpp"

namespace gp::platform::windows
{

/// @brief Win32 address-based kernel parking implementation backed by `WaitOnAddress`, `WakeByAddressSingle`, and
///        `WakeByAddressAll` ([ADR-0058]).
/// @details Uses the Windows kernel keyed-event address hash table (introduced in Windows 8 / Server 2012 via
///          `api-ms-win-core-synch-l1-2-0.dll` / `Synchronization.lib`) to park threads on a 4-byte `Atomic<UInt32>`
///          with zero kernel handle allocations.
class GP_CORE_API Futex : public gp::platform::generic::Futex
{
public:
    Futex() = delete;

    /// @brief Blocks the calling OS thread in the Windows kernel while `address == expectedValue`.
    /// @param[in] address 4-byte-aligned atomic state word to monitor.
    /// @param[in] expectedValue Value that `address` must equal for the thread to sleep.
    /// @param[in] timeoutNanoseconds Maximum wait time in nanoseconds, or `kInfiniteFutexTimeoutNs`.
    /// @return `FutexResult` indicating wakeup, value mismatch, or timeout.
    [[nodiscard]] static FutexResult waitOnAddress(
        const gp::thread::Atomic<gp::UInt32>& address,
        gp::UInt32 expectedValue,
        gp::UInt64 timeoutNanoseconds = kInfiniteFutexTimeoutNs
    ) noexcept;

    /// @brief Wakes one thread waiting on `address` via `WakeByAddressSingle`.
    /// @param[in] address 4-byte-aligned atomic state word.
    static void wakeOne(const gp::thread::Atomic<gp::UInt32>& address) noexcept;

    /// @brief Wakes all threads waiting on `address` via `WakeByAddressAll`.
    /// @param[in] address 4-byte-aligned atomic state word.
    static void wakeAll(const gp::thread::Atomic<gp::UInt32>& address) noexcept;
};

}   // namespace gp::platform::windows
