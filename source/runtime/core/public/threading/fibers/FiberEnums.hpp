// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

/// @brief Explicit status codes for engine-level cooperative `gp::thread::Fiber` operations.
enum class [[nodiscard]] FiberResult : gp::UInt8
{
    /// @brief Operation completed successfully.
    Success = 0,

    /// @brief One or more arguments (e.g., null entry function or invalid return fiber) are invalid.
    InvalidArguments,

    /// @brief The Fiber instance is already initialized.
    AlreadyInitialized,

    /// @brief The Fiber instance (or target fiber) has not been initialized.
    NotInitialized,

    /// @brief The calling OS hardware thread has already been converted into a thread-anchor fiber.
    AlreadyThreadFiber,

    /// @brief The Fiber instance is not a converted thread-anchor fiber.
    NotAThreadFiber,

    /// @brief Attempted to re-bind a task or destroy a fiber while it is actively `Running` or `Suspended`.
    BusyRunningOrSuspended,

    /// @brief Attempted to switch into a child fiber that has no bound task (`FiberState::Idle` or `Completed`).
    NoTaskBound,

    /// @brief Attempted to switch a fiber directly to itself.
    SelfSwitchError,

    /// @brief Attempted to switch away from a fiber that is not the currently executing fiber on the calling thread.
    CallerNotInFiber,

    /// @brief Attempted to destroy a fiber while currently executing on its stack.
    CannotDestroyActiveFiber,

    /// @brief Underlying `gp::platform::FiberContext` creation or switch operation failed.
    ContextError,

    /// @brief Cooperative fibers are not supported on the current target platform.
    UnsupportedPlatform
};

/// @brief Lifecycle execution state of a `gp::thread::Fiber` instance.
enum class FiberState : gp::UInt8
{
    /// @brief Default unallocated state; owns no stack or platform context.
    Uninitialized = 0,

    /// @brief Represents a converted OS hardware thread root anchor (`convertCurrentThread`).
    ThreadAnchor,

    /// @brief Stack and context are allocated, but no task entry function is currently bound.
    Idle,

    /// @brief A task entry function and payload are bound and ready to begin execution.
    Ready,

    /// @brief Currently executing on a hardware worker thread.
    Running,

    /// @brief Cooperatively yielded mid-task (`switchTo`), awaiting resumption on any worker thread.
    Suspended,

    /// @brief Bound task finished execution and returned to `returnFiber`; ready to be re-armed via `bindTask()`.
    Completed
};

/// @brief Stack size provisioning tiers modeled after Naughty Dog's two-tier fiber pool.
enum class FiberStackTier : gp::UInt8
{
    /// @brief Standard 64 KiB fiber stack suitable for >80% of gameplay, animation, and culling jobs.
    Standard64KiB = 0,

    /// @brief Large 512 KiB fiber stack provisioned for deep recursive call trees, physics, or OS/driver calls.
    Large512KiB = 1,

    /// @brief Custom stack size specified via `FiberDesc::customStackSizeBytes`.
    Custom = 2
};

}   // namespace gp::thread
