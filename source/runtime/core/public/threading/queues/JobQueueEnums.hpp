// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

/// @brief Status codes returned by lock-free job queue operations ([ADR-0051]).
enum class JobQueueResult : gp::UInt8
{
    /// @brief The queue initialization, push, pop, or steal operation completed successfully.
    Success = 0,

    /// @brief The queue or deque currently contains no available items (`top >= bottom` or no committed MPMC slot).
    Empty = 1,

    /// @brief The bounded ring buffer has reached its maximum element capacity.
    Full = 2,

    /// @brief A non-empty element slot was observed during `tryStealTop()` but lost to a concurrent CAS race.
    /// @details Distinguishes an empty victim deque (`Empty`) from an active victim deque under contention
    ///          (`Contended`) so thief loops can decide whether to retry or move to another victim.
    Contended = 3,

    /// @brief Invalid capacity (`0`), null external buffer, non-power-of-two external capacity, or misaligned memory.
    InvalidArguments = 4,

    /// @brief Backing memory allocation via `gp::memory::Malloc` failed.
    OutOfMemory = 5,

    /// @brief Operation attempted on an uninitialized queue (`isValid() == false`).
    NotInitialized = 6
};

/// @brief Scheduling priority bands for job execution ([ADR-0051]).
enum class JobPriority : gp::UInt8
{
    /// @brief Highest priority for frame-critical tasks, main-thread unblocking, and immediate RHI/Audio handoffs.
    High = 0,

    /// @brief Standard priority for gameplay simulation, animation, culling, and physics tasks.
    Normal = 1,

    /// @brief Background priority for multi-frame streaming, decompression, and precomputation tasks.
    Low = 2,

    /// @brief Total number of priority bands.
    Count = 3
};

/// @brief Discriminator identifying the payload stored inside a 32-byte `gp::thread::JobQueueItem` ([ADR-0051]).
enum class JobQueueItemKind : gp::UInt8
{
    /// @brief Item represents a fresh stateless `JobDecl` requiring binding to an idle pooled `gp::thread::Fiber`.
    JobEntry = 0,

    /// @brief Item represents a suspended `gp::thread::Fiber*` unblocked by a `JobCounter` or `JobFence` and ready
    ///        for immediate context-switch resumption.
    ResumedFiber = 1
};

}   // namespace gp::thread
