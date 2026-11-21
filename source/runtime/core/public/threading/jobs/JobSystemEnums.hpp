// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::thread
{

/// @brief Status codes returned by `gp::thread::JobSystem`, `gp::thread::JobGraph`, and `gp::thread::ParallelFor`
///        operations ([ADR-0052]).
enum class JobSystemResult : gp::UInt8
{
    /// @brief The job system, graph, or parallel dispatch operation completed.
    Success = 0,

    /// @brief Null callback, zero capacity, invalid node identifier, or self-referential dependency edge.
    InvalidArguments = 1,

    /// @brief `JobSystem::initialize()` was called on an already initialized scheduler instance.
    AlreadyInitialized = 2,

    /// @brief Operation attempted on an uninitialized `JobSystem` or `JobGraph` instance.
    NotInitialized = 3,

    /// @brief Backing memory or `FiberScratchArena` page pool allocation failed.
    OutOfMemory = 4,

    /// @brief Both the local `WorkStealingDeque` and global `MPMCRingQueue` were full and inline overflow was disabled.
    QueueFull = 5,

    /// @brief Fiber pool creation failed or all fiber slots are unavailable.
    FiberPoolExhausted = 6,

    /// @brief Spawning an OS worker thread via `gp::platform::Thread::start()` failed.
    ThreadStartFailed = 7,

    /// @brief `JobGraph::validateTopology()` found a directed cycle in the task dependency graph.
    GraphCycleDetected = 8,

    /// @brief Adding a node or edge exceeded the pre-allocated capacity of a `JobGraph`.
    CapacityExceeded = 9,

    /// @brief Underlying `JobCounter` or `JobFence` synchronization operation failed.
    SyncError = 10
};

/// @brief Partitioning strategy used by `gp::thread::ParallelFor` to split `[0, elementCount)` across workers
///        ([ADR-0052]).
enum class JobSplitStrategy : gp::UInt8
{
    /// @brief Divides the range into up to `4 * totalWorkerSlotCount` balanced batches (clamped by `minBatchSize` and
    ///        `maxBatchCount`) so work-stealing balances variable-cost iterations across cores.
    Adaptive = 0,

    /// @brief Splits the range into fixed-size chunks of `minBatchSize` elements (`ceil(N / minBatchSize)`).
    FixedBatchSize = 1,

    /// @brief Splits the range into at most `totalWorkerSlotCount` contiguous chunks (clamped by `minBatchSize`),
    ///        reducing command-list merges during RDG parallel draw recording.
    SingleBatchPerWorker = 2
};

/// @brief Bitflags controlling queue routing, overflow behavior, and RDG pass tagging in `JobDecl::flags` ([ADR-0052]).
enum class JobFlags : gp::UInt16
{
    /// @brief Default scheduling behavior (`PreferLocalQueue` when dispatched from a worker slot).
    None = 0x0000u,

    /// @brief Push to the calling worker thread's `WorkStealingDeque` for LIFO L1/L2 cache locality when possible.
    PreferLocalQueue = 0x0001u,

    /// @brief Bypass the calling worker's `WorkStealingDeque` and push directly to the global priority `MPMCRingQueue`.
    ForceGlobalQueue = 0x0002u,

    /// @brief Allow immediate inline execution on the calling thread or fiber if all queues are full.
    AllowInlineOnOverflow = 0x0004u,

    /// @brief Marks the job as a Render Dependency Graph (RDG) pass or barrier task for telemetry and diagnostics.
    RDGPass = 0x0008u
};

}   // namespace gp::thread

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::thread::JobFlags);
