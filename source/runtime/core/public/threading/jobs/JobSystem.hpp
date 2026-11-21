// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/PagedLinearAllocator.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryForward.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/Fiber.hpp"
#include "threading/jobs/JobSystemDesc.hpp"
#include "threading/jobs/JobSystemEnums.hpp"
#include "threading/jobs/JobSystemForward.hpp"
#include "threading/jobs/JobSystemTypes.hpp"
#include "threading/queues/JobQueueDesc.hpp"
#include "threading/queues/JobQueueEnums.hpp"
#include "threading/queues/MPMCRingQueue.hpp"
#include "threading/queues/WorkStealingDeque.hpp"
#include "threading/sync/JobCounter.hpp"
#include "threading/sync/JobFence.hpp"

namespace gp::thread
{

/// @brief Layer 3 $M:N$ cooperative fiber work-stealing job scheduler ([ADR-0052]).
/// @details Runs $N$ dedicated OS worker threads (`gp::platform::Thread`) and a main-thread helping slot (`workerIndex
///          == 0`) over a two-tier pool of reusable cooperative fibers (`Standard64KiB` and `Large512KiB`). Queued
///          `JobDecl` items occupy 32 bytes in lock-free queues and only lease a `gp::thread::Fiber` when a worker
///          begins executing them. Workers drain unblocked `ResumedFiber` items before starting fresh `JobEntry` tasks
///          to bound active fiber stack and `FiberScratchArena` page usage. Every task and `parallelFor` batch can
///          query a deterministic `workerIndex` in `[0, getTotalWorkerSlotCount() - 1]` via `getCurrentJobContext()`
///          for lock-free RDG `IRHICommandList` recording.
class GP_CORE_API JobSystem final
{
private:
    /// @brief Cache-line-isolated pooled fiber entry pairing a `Fiber` with its currently bound `JobDecl`.
    struct alignas(gp::memory::HardwareConstants::cacheLineSize) FiberPoolSlot final
    {
        /// @brief Pre-allocated cooperative fiber execution context.
        Fiber fiber{};

        /// @brief Task declaration currently bound to this fiber while leased.
        JobDecl activeDecl{};

        /// @brief Owning `JobSystem` instance that allocated this pool slot.
        JobSystem* owner{ nullptr };

        /// @brief Zero-based index of this slot inside `m_standardFibers` or `m_largeFibers`.
        gp::UInt16 poolIndex{ kInvalidFiberPoolIndex };

        /// @brief Stack capacity tier (`Standard64KiB` or `Large512KiB`) of `fiber`.
        FiberStackTier tier{ FiberStackTier::Standard64KiB };

        /// @brief Explicit zero padding for alignment.
        gp::UInt8 reserved{ 0u };
    };

    /// @brief Cache-line-isolated per-worker execution state owning a local Chase-Lev `WorkStealingDeque`.
    struct alignas(gp::memory::HardwareConstants::cacheLineSize) WorkerSlot final
    {
        /// @brief Bounded per-worker Chase-Lev work-stealing deque (`pushBottom`/`popBottom` by owner, `stealTop` by
        ///        peers).
        WorkStealingDeque<JobQueueItem> localDeque{};

        /// @brief Dedicated background OS worker thread handle (unused on slot `0`).
        gp::platform::Thread thread{};

        /// @brief Native OS thread anchor fiber bound to this worker thread during its lifetime.
        Fiber anchorFiber{};

        /// @brief Dedicated per-worker emergency scratch arena used when inline overflow or fiber pool exhaustion
        ///        forces a task to run directly on the worker's `anchorFiber` stack.
        gp::memory::PagedLinearAllocator emergencyScratchArena{};

        /// @brief Owning `JobSystem` instance.
        JobSystem* owner{ nullptr };

        /// @brief Deterministic worker slot index (`0` for the main-thread slot, `1 .. N` for background workers).
        gp::UInt32 workerIndex{ kMainThreadWorkerIndex };

        /// @brief Per-worker PRNG state used to randomize victim selection during work-stealing scans.
        gp::UInt32 rngState{ 0x9E3779B9u };

        /// @brief Re-entrancy depth counter tracking nested inline/emergency task execution on this worker slot.
        gp::UInt32 emergencyExecutionDepth{ 0u };

        /// @brief Non-zero once `thread.start()` has succeeded for this background worker slot.
        Atomic<gp::UInt8> threadStarted{ 0u };

        /// @brief Null-terminated debug name assigned to the background OS worker thread.
        char debugName[32]{};
    };

private:
    MPMCRingQueue<JobQueueItem> m_resumedFiberQueue{};
    MPMCRingQueue<JobQueueItem> m_globalQueues[static_cast<gp::USize>(JobPriority::Count)]{};
    MPMCRingQueue<gp::UInt16> m_freeStandardFibers{};
    MPMCRingQueue<gp::UInt16> m_freeLargeFibers{};

    WorkerSlot* m_workerSlots{ nullptr };
    FiberPoolSlot* m_standardFibers{ nullptr };
    FiberPoolSlot* m_largeFibers{ nullptr };
    gp::memory::Malloc* m_allocator{ nullptr };

    gp::UInt32 m_workerThreadCount{ 0u };
    gp::UInt32 m_totalWorkerSlotCount{ 0u };
    gp::UInt16 m_standardFiberCount{ 0u };
    gp::UInt16 m_largeFiberCount{ 0u };
    gp::UInt32 m_spinIterationsBeforeYield{ kDefaultSpinIterations };
    gp::UInt32 m_yieldIterationsBeforeSleep{ kDefaultYieldIterations };
    gp::UInt64 m_idleSleepNanoseconds{ kDefaultIdleSleepNanoseconds };
    bool m_ownsScratchPool{ false };

    alignas(gp::memory::HardwareConstants::cacheLineSize) Atomic<gp::UInt64> m_sleepingWorkerMask{ 0ULL };
    Atomic<gp::UInt64> m_workAvailableEpoch{ 0ULL };
    Atomic<gp::UInt8> m_shouldShutdown{ 0u };
    Atomic<gp::UInt8> m_isInitialized{ 0u };

    alignas(gp::memory::HardwareConstants::cacheLineSize) Atomic<gp::UInt64> m_totalJobsDispatched{ 0ULL };
    Atomic<gp::UInt64> m_totalJobsExecuted{ 0ULL };
    Atomic<gp::UInt64> m_totalJobsStolen{ 0ULL };
    Atomic<gp::UInt64> m_totalFiberSwitches{ 0ULL };

public:
    /// @brief Constructs an uninitialized `JobSystem` instance (`isInitialized() == false`).
    [[nodiscard]] constexpr JobSystem() noexcept = default;

    /// @brief Destroys the `JobSystem`, calling `shutdown()` if still initialized.
    ~JobSystem() noexcept;

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    JobSystem(JobSystem&&) = delete;
    JobSystem& operator=(JobSystem&&) = delete;

public:
    /// @section Lifecycle Management

    /// @brief Provisions the two-tier fiber pool, lock-free queues, and background OS worker threads.
    /// @param[in] desc Configuration parameters (`workerThreadCount`, fiber pool counts, queue capacities, etc.).
    /// @return `JobSystemResult::Success` on initialization, or an error code.
    [[nodiscard]] JobSystemResult initialize(const JobSystemDesc& desc = JobSystemDesc{}) noexcept;

    /// @brief Signals all background worker threads to exit, joins them, and releases all queues and pooled fibers.
    /// @note Safe to call multiple times; no-op if already shut down.
    void shutdown() noexcept;

    /// @brief Checks whether this `JobSystem` is initialized and ready to accept jobs.
    /// @return True if initialized, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isInitialized() const noexcept
    {
        return m_isInitialized.load(gp::MemoryOrder::Acquire) != 0u;
    }

public:
    /// @section Job Submission (`dispatch`, `dispatchBatch`, & `dispatchRepeated`)

    /// @brief Submits a 32-byte `JobDecl` for asynchronous execution.
    /// @param[in] decl Populated 32-byte job declaration (`decl.isValid()` must be true).
    /// @param[in] autoIncrementCounter If true and `decl.counter != nullptr`, atomically increments `decl.counter` by
    ///                                 `1` before pushing the job into the queue.
    /// @return `JobSystemResult::Success` on submission, or an error code.
    [[nodiscard]] JobSystemResult dispatch(const JobDecl& decl, bool autoIncrementCounter = true) noexcept;

    /// @brief Constructs and submits a stateless `JobEntryFunction` (`void (*)(void*)`) task.
    /// @note If `counter != nullptr`, atomically increments `counter` by `1` before queuing the job, and decrements
    ///       `counter` by `1` when the job completes.
    /// @param[in] entry Stateless callback `void (*)(void* userData)`.
    /// @param[in] userData Opaque caller-managed task payload pointer.
    /// @param[in,out] counter Optional completion counter incremented before enqueue and decremented on completion.
    /// @param[in] priority Scheduling priority band (`JobPriority::Normal` by default).
    /// @param[in] stackTier Required fiber stack tier (`FiberStackTier::Standard64KiB` by default).
    /// @param[in] flags Queue routing flags (`JobFlags::PreferLocalQueue` by default).
    /// @return `JobSystemResult::Success` on submission, or an error code.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSystemResult dispatch(
        const JobEntryFunction entry,
        void* const userData = nullptr,
        JobCounter* const counter = nullptr,
        const JobPriority priority = JobPriority::Normal,
        const FiberStackTier stackTier = FiberStackTier::Standard64KiB,
        const JobFlags flags = JobFlags::PreferLocalQueue
    ) noexcept
    {
        const JobDecl decl =
            JobDecl::make(entry, userData, counter, priority, stackTier, static_cast<gp::UInt16>(flags));
        return dispatch(decl, true);
    }

    /// @brief Submits a batch of `count` `JobDecl` descriptors with amortized queue commit and worker wakeups.
    /// @param[in] decls Contiguous array of `count` valid `JobDecl` items.
    /// @param[in] count Number of jobs in `decls` (`0` returns `JobSystemResult::Success` as a no-op).
    /// @param[in] autoIncrementCounters If true, increments each job's non-null `JobDecl::counter` before submission.
    /// @return `JobSystemResult::Success`, or an error code.
    [[nodiscard]] JobSystemResult
        dispatchBatch(const JobDecl* decls, gp::UInt32 count, bool autoIncrementCounters = true) noexcept;

    /// @brief Enqueues `repeatCount` copies of a single `JobDecl` without requiring a caller-allocated `JobDecl` array.
    /// @param[in] decl Template job declaration (`decl.isValid()` must be true).
    /// @param[in] repeatCount Number of copies to enqueue (`0` returns `JobSystemResult::Success` as a no-op).
    /// @param[in] autoIncrementCounter If true and `decl.counter != nullptr`, adds `repeatCount` to `decl.counter`
    ///                                 prior to enqueuing.
    /// @return `JobSystemResult::Success`, or an error code.
    [[nodiscard]] JobSystemResult
        dispatchRepeated(const JobDecl& decl, gp::UInt32 repeatCount, bool autoIncrementCounter = true) noexcept;

public:
    /// @section Data-Parallel Execution (`parallelFor`)

    /// @brief Splits `[0, elementCount)` into batches, dispatches them across worker slots, and waits for completion.
    /// @param[in] elementCount Total number of elements (`0` returns `JobSystemResult::Success` immediately).
    /// @param[in] minBatchSize Minimum number of elements per batch (`>= 1`).
    /// @param[in] callback Stateless batch callback receiving `(startIndex, count, userData, const JobContext&)`.
    /// @param[in] userData Opaque caller-managed context pointer.
    /// @param[in] priority Scheduling priority band (`JobPriority::Normal` by default).
    /// @return `JobSystemResult::Success` once all elements have been processed, or an error code.
    [[nodiscard]] JobSystemResult parallelFor(
        gp::UInt32 elementCount,
        gp::UInt32 minBatchSize,
        ParallelForFunction callback,
        void* userData = nullptr,
        JobPriority priority = JobPriority::Normal
    ) noexcept;

    /// @brief Splits `[0, desc.elementCount)` using an explicit `ParallelForDesc` and waits for completion.
    /// @param[in] desc Range partitioning and priority descriptor.
    /// @param[in] callback Stateless batch callback receiving `(startIndex, count, userData, const JobContext&)`.
    /// @param[in] userData Opaque caller-managed context pointer.
    /// @return `JobSystemResult::Success` once all batches finish, or an error code.
    [[nodiscard]] JobSystemResult
        parallelFor(const ParallelForDesc& desc, ParallelForFunction callback, void* userData = nullptr) noexcept;

public:
    /// @section Synchronization & Cooperative Work-Helping

    /// @brief Waits until `counter.getCount() <= targetCount`.
    /// @details When called from a pooled child `gp::thread::Fiber`, yields the fiber back to the worker thread's
    ///          scheduler loop without growing the call stack. When called from an external OS thread (such as the
    ///          Main or Render Thread) or a `ThreadAnchor`, helps execute ready jobs from the queues until
    ///          `counter.getCount() <= targetCount`.
    /// @param[in,out] counter Target `JobCounter` to wait on.
    /// @param[in] targetCount Threshold value at or below which the wait is satisfied (`0u` by default).
    /// @return `JobSystemResult::Success` once satisfied, or an error code.
    [[nodiscard]] JobSystemResult waitForCounter(JobCounter& counter, JobCounterValue targetCount = 0u) noexcept;

    /// @brief Overload waiting on a 64-byte cache-line-isolated `CacheAlignedJobCounter`.
    /// @param[in,out] counter Target `CacheAlignedJobCounter` to wait on.
    /// @param[in] targetCount Threshold value at or below which the wait is satisfied (`0u` by default).
    /// @return `JobSystemResult::Success` once satisfied, or an error code.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSystemResult
        waitForCounter(CacheAlignedJobCounter& counter, const JobCounterValue targetCount = 0u) noexcept
    {
        return waitForCounter(counter.counter, targetCount);
    }

    /// @brief Waits until `fence.isSignaled()` is true, yielding if on a child fiber or work-helping if on an OS
    ///        thread.
    /// @param[in,out] fence Target `JobFence` completion barrier.
    /// @return `JobSystemResult::Success` once signaled, or an error code.
    [[nodiscard]] JobSystemResult waitForFence(JobFence& fence) noexcept;

    /// @brief Attempts to pop or steal and execute a single ready work item on the calling thread.
    /// @return True if a work item was found and executed, false if all queues were empty when inspected.
    bool helpOneWorkItem() noexcept;

public:
    /// @section Worker Topology, Context & Telemetry Queries

    /// @brief Queries the number of dedicated background OS worker threads managed by this `JobSystem`.
    /// @return Dedicated background worker count (`1 .. kMaxJobSystemWorkers`).
    [[nodiscard]] constexpr gp::UInt32 getWorkerThreadCount() const noexcept
    {
        return m_workerThreadCount;
    }

    /// @brief Queries the total number of worker context slots (`m_workerThreadCount + 1`, where slot `0` is reserved
    ///        for the external/main thread).
    /// @details Use `getTotalWorkerSlotCount()` when sizing per-worker RDG `IRHICommandList` arrays indexed by
    ///          `JobContext::workerIndex`.
    /// @return Total worker slot count (`2 .. kMaxJobSystemWorkerSlots`).
    [[nodiscard]] constexpr gp::UInt32 getTotalWorkerSlotCount() const noexcept
    {
        return m_totalWorkerSlotCount;
    }

    /// @brief Queries the total number of pre-allocated `Standard64KiB` fibers in the pool.
    /// @return Standard fiber pool capacity.
    [[nodiscard]] constexpr gp::UInt16 getStandardFiberCapacity() const noexcept
    {
        return m_standardFiberCount;
    }

    /// @brief Queries the total number of pre-allocated `Large512KiB` fibers in the pool.
    /// @return Large fiber pool capacity.
    [[nodiscard]] constexpr gp::UInt16 getLargeFiberCapacity() const noexcept
    {
        return m_largeFiberCount;
    }

    /// @brief Captures a snapshot of runtime scheduler telemetry and free fiber pool counts.
    /// @return Populated `JobSystemStats` structure.
    [[nodiscard]] JobSystemStats getStats() const noexcept;

    /// @brief Queries the worker slot index (`0 .. getTotalWorkerSlotCount() - 1`) of the calling OS thread.
    /// @note Marked `GP_FORCENOINLINE` in the implementation so compilers never cache the TLS value across a
    ///       cooperative fiber switch and cross-core migration ([ADR-0049], [ADR-0052]).
    /// @return Active worker slot index (`0` on the Main Thread or unbound threads, `1 .. N` on dedicated workers).
    [[nodiscard]] static gp::UInt32 getCurrentWorkerIndex() noexcept;

    /// @brief Queries the active `JobSystem*` bound to the calling OS thread, or `nullptr` if none is bound.
    /// @return Pointer to the active `JobSystem`, or `nullptr`.
    [[nodiscard]] static JobSystem* getCurrentJobSystem() noexcept;

    /// @brief Queries the 32-byte `JobContext` (`jobSystem`, `workerIndex`, `fiberId`, `priority`, `stackTier`) for
    ///        the currently executing fiber or worker slot.
    /// @return Active `JobContext` on the calling OS thread.
    [[nodiscard]] static JobContext getCurrentJobContext() noexcept;

    /// @brief Checks whether the calling OS thread is a dedicated background worker thread (`workerIndex >= 1`).
    /// @return True if executing on a background worker thread spawned by `JobSystem`.
    [[nodiscard]] static bool isWorkerThread() noexcept;

private:
    friend class JobGraph;

    /// @brief Routes a `JobQueueItem` into the caller's local `WorkStealingDeque`, the global priority `MPMCRingQueue`,
    ///        or inline execution on overflow.
    /// @param[in] item 32-byte queue item (`JobEntry` or `ResumedFiber`) to enqueue.
    /// @param[in] callerSlotIndex Active worker slot index of the submitting thread, or `kInvalidWorkerIndex` if
    ///                            external.
    /// @param[in] flags Queue routing and overflow bitflags (`PreferLocalQueue`, `ForceGlobalQueue`,
    ///                  `AllowInlineOnOverflow`).
    /// @return `JobSystemResult::Success` on enqueue or inline execution, or `JobSystemResult::QueueFull`.
    [[nodiscard]] JobSystemResult
        enqueueItem(const JobQueueItem& item, gp::UInt32 callerSlotIndex, JobFlags flags) noexcept;

    /// @brief Executes a `JobDecl` synchronously on the calling thread or fiber when all queues or fiber pools are
    ///        saturated.
    /// @details Binds `emergencyScratchArena` if the calling thread has no active `FiberScratchArena` or if a nested
    ///          emergency call occurs on the `ThreadAnchor`, preserving any suspended fiber's scratch allocations.
    /// @param[in] decl Task declaration to execute inline.
    void executeInlineJobItem(const JobDecl& decl) noexcept;

    /// @brief Wakes up to `maxWorkersToWake` parked background worker threads by clearing bits in
    ///        `m_sleepingWorkerMask` and advancing `m_workAvailableEpoch`.
    /// @param[in] maxWorkersToWake Maximum number of sleeping workers to wake (`0u` is a no-op).
    void wakeSleepingWorkers(gp::UInt32 maxWorkersToWake) noexcept;

    /// @brief Clears `m_sleepingWorkerMask` and advances `m_workAvailableEpoch` to wake all parked workers.
    void wakeAllWorkers() noexcept;

    /// @brief Performs a relaxed scan across all resumed, global priority, and per-worker queues to check if any work
    ///        is waiting.
    /// @return True if at least one queue or deque appears non-empty.
    [[nodiscard]] bool hasAnyPendingWorkApprox() const noexcept;

    /// @brief Attempts to pop or steal the highest-priority ready `JobQueueItem` for `slot`.
    /// @details Drains `m_resumedFiberQueue` first, then checks `High` global, `slot.localDeque` (`popBottom`),
    ///          `Normal` global, peer worker deques (`stealTop`), and finally `Low` global.
    /// @param[in,out] slot Worker slot attempting to acquire work.
    /// @param[out] outItem Receives the popped or stolen `JobQueueItem` on success.
    /// @return True if a work item was obtained, false if all queues were empty.
    [[nodiscard]] bool tryPopNextWorkItem(WorkerSlot& slot, JobQueueItem& outItem) noexcept;

    /// @brief Executes a popped `JobQueueItem` (`ResumedFiber` or fresh `JobEntry`) from `slot`'s `anchorFiber`.
    /// @param[in,out] slot Active worker slot executing the item.
    /// @param[in,out] anchorFiber The worker thread's native OS `ThreadAnchor` fiber.
    /// @param[in] item Popped `JobQueueItem` to resume or launch on a pooled fiber.
    void executeWorkItem(WorkerSlot& slot, Fiber& anchorFiber, const JobQueueItem& item) noexcept;

    /// @brief Leases an idle `FiberPoolSlot` from `m_freeStandardFibers` or `m_freeLargeFibers`.
    /// @note Falls back to `m_freeLargeFibers` if `requestedTier == FiberStackTier::Standard64KiB` and the standard
    ///       pool is temporarily empty.
    /// @param[in] requestedTier Stack tier requested by the `JobDecl`.
    /// @return Pointer to the leased `FiberPoolSlot`, or `nullptr` if no compatible fiber is available.
    [[nodiscard]] FiberPoolSlot* acquireFiberSlot(FiberStackTier requestedTier) noexcept;

    /// @brief Resets `fiberSlot.fiber`'s scratch arena and pushes its index back to the appropriate lock-free free
    ///        queue.
    /// @param[in,out] fiberSlot Completed pooled fiber slot to return to the pool.
    void releaseFiberSlot(FiberPoolSlot& fiberSlot) noexcept;

    /// @brief Maps a `gp::thread::Fiber*` back to its owning `FiberPoolSlot` using the bit-prefix encoded in
    ///        `fiber->getId()`.
    /// @param[in] fiber Pointer to a pooled fiber.
    /// @return Pointer to the owning `FiberPoolSlot`, or `nullptr` if `fiber` is null or a `ThreadAnchor`.
    [[nodiscard]] FiberPoolSlot* resolveFiberPoolSlot(const Fiber* fiber) const noexcept;

    /// @brief Transitions `completedSlot.fiber` from `Executing` to `Completed` and returns it to the free pool once
    ///        execution is back on the worker's `ThreadAnchor`.
    /// @param[in,out] completedSlot Pooled fiber slot that finished its `activeDecl`.
    void retireCompletedFiberOnAnchor(FiberPoolSlot& completedSlot) noexcept;

    /// @brief Main scheduling and 3-phase backoff loop executed by each dedicated background worker thread.
    /// @param[in,out] slot Per-worker state for the calling background thread.
    void workerThreadLoop(WorkerSlot& slot) noexcept;

    /// @brief Static `gp::platform::ThreadEntryFunction` entry point for background worker threads.
    /// @param[in] rawWorkerSlot Pointer to the thread's `WorkerSlot`.
    /// @return Thread exit code (`0u`).
    static gp::UInt32 workerThreadEntryPoint(void* rawWorkerSlot) noexcept;

    /// @brief Static `FiberEntryFunction` entry point invoked when a pooled fiber starts executing its bound task.
    /// @param[in] rawFiberPoolSlot Pointer to the active `FiberPoolSlot`.
    static void fiberTaskEntryPoint(void* rawFiberPoolSlot) noexcept;

    /// @brief Static `FiberYieldCallback` passed to `JobCounter::waitWithFiberYield` and `JobFence::waitWithFiberYield`
    ///        to switch `currentFiber` back to the current worker thread's `ThreadAnchor`.
    /// @param[in,out] currentFiber The pooled child fiber that was just registered on a wait list.
    /// @param[in] rawJobSystem Pointer to the active `JobSystem`.
    /// @return `JobSyncResult::Success` once the fiber is later resumed and switched back onto a worker thread.
    static JobSyncResult onFiberYield(Fiber& currentFiber, void* rawJobSystem) noexcept;

    /// @brief Static `FiberWakeCallback` registered on every `JobWaitNode` to re-enqueue an unblocked `Fiber*` into
    ///        `m_resumedFiberQueue`.
    /// @param[in] waitingFiber Pointer to the unblocked pooled `Fiber` ready to resume.
    /// @param[in] rawJobSystem Pointer to the owning `JobSystem`.
    static void onFiberWake(Fiber* waitingFiber, void* rawJobSystem) noexcept;
};

}   // namespace gp::thread
