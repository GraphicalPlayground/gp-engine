// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryForward.hpp"
#include "platforms/generic/system/GenericThread.hpp"
#include "threading/fibers/FiberEnums.hpp"
#include "threading/jobs/JobSystemEnums.hpp"
#include "threading/jobs/JobSystemForward.hpp"
#include "threading/jobs/JobSystemTypes.hpp"
#include "threading/queues/JobQueueEnums.hpp"

namespace gp::thread
{

/// @brief Plain-old-data (POD) configuration descriptor for initializing `gp::thread::JobSystem` ([ADR-0052]).
struct JobSystemDesc
{
    /// @brief Number of dedicated background OS worker threads to spawn (`0u` selects
    ///        `clamp(Thread::getProcessConcurrency() - 1, 1, kMaxJobSystemWorkers)`).
    gp::UInt32 workerThreadCount{ 0u };

    /// @brief Number of pooled `Standard64KiB` (`64 KiB` stack) cooperative fibers to pre-allocate (`>= 1`).
    gp::UInt16 standardFiberCount{ kDefaultStandardFiberCount };

    /// @brief Number of pooled `Large512KiB` (`512 KiB` stack) cooperative fibers to pre-allocate (`>= 1`).
    gp::UInt16 largeFiberCount{ kDefaultLargeFiberCount };

    /// @brief Capacity of each worker slot's bounded `WorkStealingDeque<JobQueueItem>` (rounded up to a power of two).
    gp::USize workerDequeCapacity{ kDefaultWorkerDequeCapacity };

    /// @brief Capacity of each global priority band's `MPMCRingQueue<JobQueueItem>` (rounded up to a power of two).
    gp::USize globalQueueCapacity{ kDefaultGlobalQueueCapacity };

    /// @brief Page size in bytes for the shared `FiberScratchArena` `PagePool` when `initializeFiberScratchPool` is
    ///        true (`64 KiB` default).
    gp::USize scratchPageSizeInBytes{ 64ULL * 1024ULL };

    /// @brief Maximum number of pages in the shared `FiberScratchArena` `PagePool` when `initializeFiberScratchPool`
    ///        is true.
    gp::UInt32 maxScratchPages{ 256u };

    /// @brief Number of `Thread::spinWait()` iterations an idle worker executes before yielding its OS time slice.
    gp::UInt32 spinIterationsBeforeYield{ kDefaultSpinIterations };

    /// @brief Number of `Thread::yieldExecution()` iterations an idle worker executes before entering sleep backoff.
    gp::UInt32 yieldIterationsBeforeSleep{ kDefaultYieldIterations };

    /// @brief Sub-millisecond sleep quantum in nanoseconds for parked idle workers (`100 us` default).
    gp::UInt64 idleSleepNanoseconds{ kDefaultIdleSleepNanoseconds };

    /// @brief Hardware scheduling priority applied to spawned background worker threads.
    gp::platform::ThreadPriority workerThreadPriority{ gp::platform::ThreadPriority::Normal };

    /// @brief Optional custom `gp::memory::Malloc` backend (`nullptr` defaults to `gp::memory::getGlobalMalloc()`).
    gp::memory::Malloc* backingMalloc{ nullptr };

    /// @brief If true, pins each background worker thread `i` (`1 .. N`) to logical CPU core `i % hardwareCores`.
    bool enableCoreAffinity{ false };

    /// @brief If true and `FiberScratchArena::tryGetSharedPagePool()` is null, initializes and manages the shared
    ///        `FiberScratchArena` page pool.
    bool initializeFiberScratchPool{ true };
};

/// @brief Plain-old-data (POD) configuration descriptor for data-parallel `parallelFor` execution ([ADR-0052]).
struct ParallelForDesc
{
    /// @brief Total number of contiguous elements in the index range `[0, elementCount)`.
    gp::UInt32 elementCount{ 0u };

    /// @brief Minimum number of elements assigned to a single batch (`>= 1u`). In `FixedBatchSize` mode, specifies the
    ///        batch size.
    gp::UInt32 minBatchSize{ 1u };

    /// @brief Maximum number of batches to generate (`1 .. kMaxParallelForBatches`).
    gp::UInt32 maxBatchCount{ kMaxParallelForBatches };

    /// @brief Range partitioning strategy (`Adaptive`, `FixedBatchSize`, or `SingleBatchPerWorker`).
    JobSplitStrategy strategy{ JobSplitStrategy::Adaptive };

    /// @brief Scheduling priority band for spawned batch jobs.
    JobPriority priority{ JobPriority::Normal };

    /// @brief Required fiber stack tier for batch execution (`Standard64KiB` by default).
    FiberStackTier stackTier{ FiberStackTier::Standard64KiB };

    /// @brief Queue routing flags (`PreferLocalQueue` by default).
    JobFlags flags{ JobFlags::PreferLocalQueue };
};

/// @brief Snapshot of `gp::thread::JobSystem` runtime telemetry and pool availability ([ADR-0052]).
struct JobSystemStats
{
    /// @brief Cumulative number of `JobDecl` tasks submitted via `dispatch()`, `dispatchBatch()`, and
    ///        `dispatchRepeated()`.
    gp::UInt64 totalJobsDispatched{ 0ULL };

    /// @brief Cumulative number of jobs executed across all worker threads and work-helping calls.
    gp::UInt64 totalJobsExecuted{ 0ULL };

    /// @brief Cumulative number of jobs stolen from peer worker `WorkStealingDeque` instances.
    gp::UInt64 totalJobsStolen{ 0ULL };

    /// @brief Cumulative number of cooperative fiber context switches executed by the scheduler.
    gp::UInt64 totalFiberSwitches{ 0ULL };

    /// @brief Number of dedicated background OS worker threads active in the pool.
    gp::UInt32 workerThreadCount{ 0u };

    /// @brief Total number of worker context slots (`workerThreadCount + 1`, including `kMainThreadWorkerIndex`).
    gp::UInt32 totalWorkerSlotCount{ 0u };

    /// @brief Current approximate number of available idle fibers in the `Standard64KiB` pool.
    gp::UInt32 availableStandardFibers{ 0u };

    /// @brief Current approximate number of available idle fibers in the `Large512KiB` pool.
    gp::UInt32 availableLargeFibers{ 0u };

    /// @brief Current number of background worker threads parked in the idle sleep bitmask.
    gp::UInt32 sleepingWorkerCount{ 0u };
};

}   // namespace gp::thread
