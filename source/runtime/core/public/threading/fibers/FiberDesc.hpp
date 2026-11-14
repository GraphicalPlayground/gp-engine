// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryForward.hpp"
#include "threading/fibers/FiberEnums.hpp"
#include "threading/fibers/FiberForward.hpp"
#include "threading/fibers/FiberTypes.hpp"

namespace gp::thread
{

/// @brief Plain-old-data (POD) configuration descriptor for creating an engine `gp::thread::Fiber`.
struct FiberDesc
{
    /// @brief UTF-8 debug label for profiler (Tracy) and crash dump identification.
    const char* name{ "gp::Fiber" };

    /// @brief Logical pool index or diagnostic identifier for this fiber.
    FiberId id{ 0u };

    /// @brief Optional initial task entry point (`nullptr` creates the fiber in `FiberState::Idle` for later
    /// `bindTask`).
    FiberEntryFunction entry{ nullptr };

    /// @brief Opaque user context pointer passed to `entry(userData)`.
    void* userData{ nullptr };

    /// @brief Fiber to automatically switch back to when `entry(userData)` returns (typically the worker thread anchor
    ///        or scheduler fiber; may be updated later via `bindTask` or `setReturnFiber`).
    Fiber* returnFiber{ nullptr };

    /// @brief Stack provisioning tier (`Standard64KiB`, `Large512KiB`, or `Custom`).
    FiberStackTier stackTier{ FiberStackTier::Standard64KiB };

    /// @brief Custom stack size in bytes (used only when `stackTier == FiberStackTier::Custom`).
    gp::USize customStackSizeBytes{ kStandardFiberStackBytes };

    /// @brief Optional explicit `PagePool` backing this fiber's `PagedLinearAllocator` (`nullptr` uses the shared
    ///        `FiberScratchArena` pool lazily on first scratch allocation).
    gp::memory::PagePool* scratchPagePool{ nullptr };

    /// @brief If true (default), automatically resets the fiber's `PagedLinearAllocator` (returning all leased pages to
    ///        `PagePool`) whenever a bound task function returns.
    bool resetScratchOnTaskCompletion{ true };

    /// @brief If true (default), places an OS hardware guard page at the bottom of the stack to trap overflows.
    bool enableGuardPage{ true };
};

}   // namespace gp::thread
