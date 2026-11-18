// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "platforms/base/system/Time.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::platform
{

/// @brief Explicit status codes for address-based kernel wait (`Futex::waitOnAddress`) operations ([ADR-0058]).
enum class [[nodiscard]] FutexResult : gp::UInt8
{
    /// @brief The wait returned because the monitored value changed or a waker signaled the address.
    Success = 0,

    /// @brief The 32-bit value at the address already differed from `expectedValue` upon entry.
    ValueMismatch,

    /// @brief The wait duration elapsed before the address was woken.
    TimedOut,

    /// @brief The address pointer was `nullptr` or misaligned.
    InvalidArguments,

    /// @brief Underlying OS kernel address-wait call failed.
    WaitFailed
};

/// @brief Sentinel constant indicating an unbounded (infinite) address wait duration.
inline constexpr gp::UInt64 kInfiniteFutexTimeoutNs = ~0ULL;

}   // namespace gp::platform

namespace gp::platform::generic
{

/// @brief Stateless base contract and user-space backoff fallback for 32-bit address-based thread parking ([ADR-0058]).
/// @details Provides a portable `WaitOnAddress` / `futex` / `__ulock_wait` interface across operating systems.
///          On targets without a dedicated kernel address-wait primitive, `waitOnAddress()` falls back to a bounded
///          user-space spin-and-yield loop inspecting the `Atomic<UInt32>` word.
class Futex
{
public:
    Futex() = delete;

    /// @brief Blocks the calling thread while `address.load(Acquire) == expectedValue` or until `timeoutNanoseconds`
    ///        elapses.
    /// @note Spurious wakeups are permitted by all OS kernel futex implementations; callers must always re-verify their
    ///       synchronization predicate in a loop.
    /// @param[in] address 4-byte-aligned atomic state word to monitor.
    /// @param[in] expectedValue Value that `address` must equal for the thread to block.
    /// @param[in] timeoutNanoseconds Maximum wait duration in nanoseconds, or `kInfiniteFutexTimeoutNs`.
    /// @return `FutexResult::Success` on wakeup or value change, `FutexResult::ValueMismatch` if `address` already
    ///         differed on entry, or `FutexResult::TimedOut` if the timeout expired.
    [[nodiscard]] static FutexResult waitOnAddress(
        const gp::thread::Atomic<gp::UInt32>& address,
        const gp::UInt32 expectedValue,
        const gp::UInt64 timeoutNanoseconds = kInfiniteFutexTimeoutNs
    ) noexcept
    {
        if (address.load(gp::MemoryOrder::Acquire) != expectedValue)
        {
            return FutexResult::ValueMismatch;
        }

        if (timeoutNanoseconds == 0ULL)
        {
            return FutexResult::TimedOut;
        }

        const bool hasDeadline = (timeoutNanoseconds != kInfiniteFutexTimeoutNs);
        const gp::UInt64 startNs = hasDeadline ? gp::platform::Time::nowNanoseconds() : 0ULL;
        gp::UInt32 spinIteration = 0u;

        while (address.load(gp::MemoryOrder::Acquire) == expectedValue)
        {
            if (hasDeadline)
            {
                const gp::UInt64 elapsedNs = gp::platform::Time::nowNanoseconds() - startNs;
                if (elapsedNs >= timeoutNanoseconds)
                {
                    return FutexResult::TimedOut;
                }
            }

            if (spinIteration < 32u)
            {
                gp::platform::Thread::spinWait();
                ++spinIteration;
            }
            else
            {
                gp::platform::Thread::yieldExecution();
            }
        }

        return FutexResult::Success;
    }

    /// @brief Wakes a single thread currently parked inside `waitOnAddress()` on `address`.
    /// @param[in] address 4-byte-aligned atomic state word on which waiters are parked.
    static constexpr void wakeOne([[maybe_unused]] const gp::thread::Atomic<gp::UInt32>& address) noexcept
    {}

    /// @brief Wakes all threads currently parked inside `waitOnAddress()` on `address`.
    /// @param[in] address 4-byte-aligned atomic state word on which waiters are parked.
    static constexpr void wakeAll([[maybe_unused]] const gp::thread::Atomic<gp::UInt32>& address) noexcept
    {}
};

}   // namespace gp::platform::generic
