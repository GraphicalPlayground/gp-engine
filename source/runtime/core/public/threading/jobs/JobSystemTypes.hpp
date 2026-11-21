// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Memory.hpp"
#include "CoreMinimal.hpp"
#include "threading/fibers/FiberEnums.hpp"
#include "threading/fibers/FiberTypes.hpp"
#include "threading/jobs/JobSystemForward.hpp"
#include "threading/queues/JobQueueEnums.hpp"

namespace gp::thread
{

/// @brief Worker slot index reserved for the external or main thread when participating via `waitForCounter` or
///        `waitForFence` ([ADR-0052]).
inline constexpr gp::UInt32 kMainThreadWorkerIndex = 0u;

/// @brief Sentinel constant indicating the calling OS thread is not bound to any `JobSystem` worker slot.
inline constexpr gp::UInt32 kInvalidWorkerIndex = ~0u;

/// @brief Maximum number of dedicated background OS worker threads supported by `JobSystem` (`63` background workers
///        plus `1` main-thread slot = `64` total slots, fitting inside a 64-bit atomic sleep bitmask).
inline constexpr gp::UInt32 kMaxJobSystemWorkers = 63u;

/// @brief Maximum total worker slots (`kMainThreadWorkerIndex` plus up to `kMaxJobSystemWorkers` background workers).
inline constexpr gp::UInt32 kMaxJobSystemWorkerSlots = 64u;

/// @brief Default number of `Standard64KiB` (`64 KiB`) cooperative fibers pre-allocated in the `JobSystem` pool.
inline constexpr gp::UInt16 kDefaultStandardFiberCount = 128u;

/// @brief Default number of `Large512KiB` (`512 KiB`) cooperative fibers pre-allocated in the `JobSystem` pool.
inline constexpr gp::UInt16 kDefaultLargeFiberCount = 32u;

/// @brief Default power-of-two capacity for each worker thread's `WorkStealingDeque<JobQueueItem>`.
inline constexpr gp::USize kDefaultWorkerDequeCapacity = 1024u;

/// @brief Default power-of-two capacity for each global priority `MPMCRingQueue<JobQueueItem>`.
inline constexpr gp::USize kDefaultGlobalQueueCapacity = 4096u;

/// @brief Default number of `Thread::spinWait()` iterations executed by an idle worker before yielding its time slice.
inline constexpr gp::UInt32 kDefaultSpinIterations = 256u;

/// @brief Default number of `Thread::yieldExecution()` iterations executed by an idle worker before parking.
inline constexpr gp::UInt32 kDefaultYieldIterations = 32u;

/// @brief Default sub-millisecond sleep quantum (`100 us`) for parked idle workers ([ADR-0031], [ADR-0052]).
inline constexpr gp::UInt64 kDefaultIdleSleepNanoseconds = 100'000ULL;

/// @brief Maximum number of `parallelFor` batches dispatched in a single stack-resident wave.
inline constexpr gp::UInt32 kMaxParallelForBatches = 256u;

/// @brief Sentinel pool index indicating an unacquired or inline fiber slot.
inline constexpr gp::UInt16 kInvalidFiberPoolIndex = 0xFFFFu;

/// @brief Identifier for a node inside a `gp::thread::JobGraph` ([ADR-0052]).
using JobGraphNodeId = gp::UInt32;

/// @brief Sentinel identifier representing an invalid `JobGraphNodeId`.
inline constexpr JobGraphNodeId kInvalidJobGraphNodeId = ~0u;

/// @brief 32-byte POD execution context passed to context-aware tasks and `parallelFor` batches ([ADR-0052]).
/// @details Exposes a deterministic `workerIndex` in `[0, totalWorkerSlotCount - 1]` so Render Dependency Graph (RDG)
///          passes can index directly into per-worker `IRHICommandList` and command allocator arrays without locks or
///          cache-line contention.
struct alignas(16) JobContext
{
    /// @brief Pointer to the owning `JobSystem` running this execution slot (`nullptr` if unbound).
    JobSystem* jobSystem{ nullptr };

    /// @brief Worker slot index (`0` for the external/main thread slot, `1 .. N` for dedicated background workers).
    gp::UInt32 workerIndex{ kMainThreadWorkerIndex };

    /// @brief Identifier of the currently executing cooperative `gp::thread::Fiber` (`kInvalidFiberId` if inline).
    FiberId fiberId{ kInvalidFiberId };

    /// @brief Priority band of the currently executing job.
    JobPriority priority{ JobPriority::Normal };

    /// @brief Stack tier of the currently executing fiber.
    FiberStackTier stackTier{ FiberStackTier::Standard64KiB };

    /// @brief Explicit 16-bit zero padding to reach a 24-byte boundary before `reserved64`.
    gp::UInt16 reserved{ 0u };

    /// @brief Explicit 64-bit zero padding to reach a 32-byte aligned footprint.
    gp::UInt64 reserved64{ 0ULL };

    /// @brief Checks whether this context is bound to a valid `JobSystem` instance and worker slot.
    /// @return True if `jobSystem != nullptr` and `workerIndex < kMaxJobSystemWorkerSlots`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return jobSystem != nullptr && workerIndex < kMaxJobSystemWorkerSlots;
    }

    /// @brief Checks whether this execution is occurring on the external/main thread worker slot (`workerIndex == 0`).
    /// @return True if `workerIndex == kMainThreadWorkerIndex`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isMainThreadSlot() const noexcept
    {
        return workerIndex == kMainThreadWorkerIndex;
    }

    /// @brief Checks whether this execution is occurring on a dedicated background worker thread (`workerIndex >= 1`).
    /// @return True if `workerIndex > kMainThreadWorkerIndex && workerIndex < kMaxJobSystemWorkerSlots`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isWorkerThreadSlot() const noexcept
    {
        return workerIndex > kMainThreadWorkerIndex && workerIndex < kMaxJobSystemWorkerSlots;
    }
};

static_assert(sizeof(JobContext) == 32u, "JobContext must occupy 32 bytes.");
static_assert(alignof(JobContext) == 16u, "JobContext must be 16-byte aligned.");
static_assert(gp::concepts::IsTriviallyCopyable<JobContext>, "JobContext must be trivially copyable.");
static_assert(gp::concepts::IsStandardLayout<JobContext>, "JobContext must have standard layout.");

/// @brief Stateless C-style function pointer signature for context-aware tasks ([ADR-0022], [ADR-0052]).
/// @param[in] userData Opaque caller-managed task payload pointer.
/// @param[in] context Active worker and fiber execution context (`workerIndex`, `fiberId`, `jobSystem`).
using JobTaskFunction = void (*)(void* userData, const JobContext& context);

/// @brief Stateless C-style function pointer signature for data-parallel `parallelFor` range batches ([ADR-0022],
///        [ADR-0052]).
/// @param[in] startIndex Inclusive starting index of this batch within `[0, elementCount)`.
/// @param[in] count Number of contiguous elements in this batch (`count >= 1`).
/// @param[in] userData Opaque caller-managed context pointer.
/// @param[in] context Active worker and fiber execution context (`context.workerIndex` indexes per-worker RHI state).
using ParallelForFunction =
    void (*)(gp::UInt32 startIndex, gp::UInt32 count, void* userData, const JobContext& context);

}   // namespace gp::thread
