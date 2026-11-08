// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "threading/fibers/FiberForward.hpp"
#include "threading/sync/JobSyncEnums.hpp"

namespace gp::thread
{

/// @brief 32-bit unsigned reference count value tracked by `gp::thread::JobCounter`.
using JobCounterValue = gp::UInt32;

/// @brief Stateless C-style callback invoked when a child `gp::thread::Fiber` parks on a `JobCounter` or `JobFence`
///        and must yield execution back to the worker thread's scheduler loop ([ADR-0022], [ADR-0050]).
/// @param[in,out] currentFiber Reference to the currently executing child fiber that has just been linked into a wait
///                             list.
/// @param[in] userData Opaque scheduler context pointer registered via `JobFiberParker`.
/// @return `JobSyncResult::Success` when the fiber is later resumed, or `JobSyncResult::YieldFailed`.
using JobYieldFunction = JobSyncResult (*)(Fiber& currentFiber, void* userData) noexcept;

/// @brief Stateless C-style callback invoked by the thread that decrements a `JobCounter` or signals a `JobFence` for
///        each unblocked fiber waiter ([ADR-0022], [ADR-0050]).
/// @param[in] waitingFiber Pointer to the suspended child `Fiber` whose wait condition is now satisfied.
/// @param[in] userData Opaque scheduler context pointer captured in the `JobWaitNode`.
using JobWakeFunction = void (*)(Fiber* waitingFiber, void* userData) noexcept;

/// @brief Maximum 32-bit job count representable in the lower half of `JobCounter`'s 64-bit state word.
inline constexpr JobCounterValue kMaxJobCounterValue = 0xFFFFFFFFu;

/// @brief Bitmask isolating the 32-bit `count` field (`bits [0..31]`) of `JobCounter`'s 64-bit state word.
inline constexpr gp::UInt64 kJobCounterValueMask = 0x00000000FFFFFFFFULL;

/// @brief Bit shift to the 30-bit `waiterCount` field (`bits [32..61]`) of `JobCounter`'s 64-bit state word.
inline constexpr gp::UInt32 kJobCounterWaiterShift = 32u;

/// @brief Increment quantum representing one parked waiter (`1ULL << 32`) in `JobCounter`'s 64-bit state word.
inline constexpr gp::UInt64 kJobCounterOneWaiter = (1ULL << kJobCounterWaiterShift);

/// @brief Bitmask isolating the 30-bit `waiterCount` field (`bits [32..61]`) of `JobCounter`'s 64-bit state word.
inline constexpr gp::UInt64 kJobCounterWaiterMask = 0x3FFFFFFF00000000ULL;

/// @brief Maximum number of concurrent waiters representable in `JobCounter`'s 30-bit `waiterCount` field.
inline constexpr gp::UInt32 kMaxJobCounterWaiters = 0x3FFFFFFFu;

/// @brief 1-bit flag (`bit [62]`) indicating at least one parked waiter in `JobCounter` has `targetCount > 0`.
/// @details Allows intermediate decrements (`newCount > 0`) to bypass `notifyWaiters()` completely when all parked
///          fibers are waiting for the counter to reach `0` (`targetCount == 0`).
inline constexpr gp::UInt64 kJobCounterHasThresholdWaiterBit = (1ULL << 62u);

/// @brief 1-bit Test-and-Test-and-Set (TTAS) spin-latch bit (`bit [63]`) serializing wait-list mutations in
///        `JobCounter`.
inline constexpr gp::UInt64 kJobCounterLockBit = (1ULL << 63u);

/// @brief Additive 64-bit modular delta (`0x8000000100000000ULL`) that simultaneously increments `waiterCount` by `1`
///        and wraps `kJobCounterLockBit` (`bit 63`) from `1` to `0` without modifying `count` (`bits [0..31]`).
inline constexpr gp::UInt64 kJobCounterAddWaiterAndUnlockDelta = (kJobCounterLockBit + kJobCounterOneWaiter);

/// @brief Low-bit flag (`bit [0]`) packed into `JobFence`'s 8-byte-aligned head pointer indicating the fence is
///        signaled.
inline constexpr gp::UIntPtr kJobFenceSignaledBit = 0x1ULL;

/// @brief Low-bit TTAS spin-latch (`bit [1]`) packed into `JobFence`'s 8-byte-aligned head pointer serializing
///        wait-list linkage.
inline constexpr gp::UIntPtr kJobFenceLockBit = 0x2ULL;

/// @brief Combined mask (`0x3`) isolating the flag bits packed into `JobFence`'s atomic state word.
inline constexpr gp::UIntPtr kJobFenceFlagsMask = (kJobFenceSignaledBit | kJobFenceLockBit);

/// @brief Pointer mask (`~0x3`) isolating the 8-byte-aligned `JobWaitNode*` head pointer in `JobFence`.
inline constexpr gp::UIntPtr kJobFencePointerMask = ~kJobFenceFlagsMask;

}   // namespace gp::thread
