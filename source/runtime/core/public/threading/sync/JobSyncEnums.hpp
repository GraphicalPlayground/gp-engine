// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

/// @brief Explicit status codes for `gp::thread::JobCounter` and `gp::thread::JobFence` synchronization operations.
enum class [[nodiscard]] JobSyncResult : gp::UInt8
{
    /// @brief Operation completed successfully.
    Success = 0,

    /// @brief One or more arguments (e.g., zero delta on `add()` or `decrement()`) are invalid.
    InvalidArguments,

    /// @brief Attempted to decrement a `JobCounter` by a delta greater than its current active count.
    CounterUnderflow,

    /// @brief Attempted to increment a `JobCounter` beyond `kMaxJobCounterValue` (`0xFFFFFFFFu`).
    CounterOverflow,

    /// @brief Cooperative fiber yield failed while parking on a wait list.
    YieldFailed,

    /// @brief Attempted to reset a synchronization primitive while waiters are actively parked on its wait list.
    BusyWaiting
};

/// @brief Execution wait policy when blocking on a `JobCounter` or `JobFence`.
enum class JobWaitMode : gp::UInt8
{
    /// @brief Automatically yields the active child `gp::thread::Fiber` (via the thread-local `JobFiberParker` or
    ///        `Fiber::yieldToReturnFiber()`), or spin-waits via `gp::platform::Thread::spinWait()` when invoked from
    ///        a raw OS thread or `ThreadAnchor` fiber.
    Auto = 0,

    /// @brief Forces a non-yielding hardware spin-wait (`gp::platform::Thread::spinWait()`) even when invoked from
    ///        inside a child `gp::thread::Fiber`.
    SpinOnly = 1
};

/// @brief Lifecycle state of a stack-allocated intrusive `JobWaitNode`.
enum class JobWaitNodeState : gp::UInt8
{
    /// @brief Node is initialized on the caller's stack and not yet linked into a wait list.
    Idle = 0,

    /// @brief Node is linked inside a `JobCounter` or `JobFence` wait list awaiting completion.
    Waiting = 1,

    /// @brief The target threshold or fence condition has been met and the waiter has been released.
    Signaled = 2
};

}   // namespace gp::thread
