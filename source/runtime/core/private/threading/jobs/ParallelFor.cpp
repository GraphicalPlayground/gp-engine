// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/jobs/ParallelFor.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/jobs/JobSystem.hpp"
#include "threading/queues/JobQueueDesc.hpp"
#include "threading/sync/JobCounter.hpp"

namespace gp::thread
{

namespace detail
{

/// @brief Stack-allocated shared coordination block for a single `ParallelFor::dispatchWithDesc` invocation.
/// @details Lives on the calling fiber or thread stack while `waitForCounter` suspends the fiber (preserving its stack
///          frame) or work-helps until all `batchCount - 1` spawned batch jobs have completed.
struct alignas(gp::memory::HardwareConstants::cacheLineSize) ParallelForSharedState final
{
    /// @brief Cache-line-isolated completion counter tracking the `batchCount - 1` spawned worker jobs.
    CacheAlignedJobCounter completionCounter{};

    /// @brief Atomic index counter incremented via `fetchAdd(1, Relaxed)` by each worker claiming a batch.
    Atomic<gp::UInt32> nextBatchIndex{ 0u };

    /// @brief Stateless user batch callback receiving `(startIndex, count, userData, const JobContext&)`.
    ParallelForFunction callback{ nullptr };

    /// @brief Opaque caller-supplied payload pointer forwarded to `callback`.
    void* userData{ nullptr };

    /// @brief Owning `JobSystem` executing the parallel range.
    JobSystem* jobSystem{ nullptr };

    /// @brief Total number of elements in `[0, elementCount)`.
    gp::UInt32 elementCount{ 0u };

    /// @brief Total number of batches partitioned across `[0, elementCount)`.
    gp::UInt32 batchCount{ 0u };

    /// @brief Minimum batch grain size (`>= 1`).
    gp::UInt32 minBatchSize{ 1u };

    /// @brief Partitioning strategy used by `ParallelFor::computeBatchRange`.
    JobSplitStrategy strategy{ JobSplitStrategy::Adaptive };

    /// @brief Priority band assigned to the batch jobs.
    JobPriority priority{ JobPriority::Normal };

    /// @brief Fiber stack tier assigned to the batch jobs.
    FiberStackTier stackTier{ FiberStackTier::Standard64KiB };

    /// @brief Atomically claims the next unclaimed batch index from `nextBatchIndex` and invokes `callback`.
    void executeOneClaimedBatch() noexcept
    {
        const gp::UInt32 batchIdx = nextBatchIndex.fetchAdd(1u, gp::MemoryOrder::Relaxed);
        if (batchIdx >= batchCount) [[unlikely]]
        {
            return;
        }

        gp::UInt32 startIndex = 0u;
        gp::UInt32 count = 0u;
        ParallelFor::computeBatchRange(elementCount, batchCount, minBatchSize, strategy, batchIdx, startIndex, count);
        if (count == 0u) [[unlikely]]
        {
            return;
        }

        JobContext ctx = JobSystem::getCurrentJobContext();
        if (ctx.jobSystem == nullptr)
        {
            ctx.jobSystem = jobSystem;
            ctx.priority = priority;
            ctx.stackTier = stackTier;
        }

        callback(startIndex, count, userData, ctx);
    }

    /// @brief Static `JobEntryFunction` trampoline passed to `JobSystem::dispatchRepeated` for worker batch jobs.
    /// @param[in] rawState Pointer to the stack-resident `ParallelForSharedState`.
    static void batchJobEntry(void* const rawState) noexcept
    {
        ParallelForSharedState* const state = static_cast<ParallelForSharedState*>(rawState);
        if (state != nullptr) [[likely]]
        {
            state->executeOneClaimedBatch();
        }
    }
};

}   // namespace detail

JobSystemResult ParallelFor::dispatch(
    JobSystem& jobSystem,
    const gp::UInt32 elementCount,
    const gp::UInt32 minBatchSize,
    const ParallelForFunction callback,
    void* const userData,
    const JobPriority priority
) noexcept
{
    ParallelForDesc desc{};
    desc.elementCount = elementCount;
    desc.minBatchSize = (minBatchSize == 0u) ? 1u : minBatchSize;
    desc.maxBatchCount = kMaxParallelForBatches;
    desc.strategy = JobSplitStrategy::Adaptive;
    desc.priority = priority;
    desc.stackTier = FiberStackTier::Standard64KiB;
    desc.flags = JobFlags::PreferLocalQueue;
    return dispatchWithDesc(jobSystem, desc, callback, userData);
}

JobSystemResult ParallelFor::dispatchWithDesc(
    JobSystem& jobSystem, const ParallelForDesc& desc, const ParallelForFunction callback, void* const userData
) noexcept
{
    if (!jobSystem.isInitialized()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (desc.elementCount == 0u)
    {
        return JobSystemResult::Success;
    }

    if (callback == nullptr) [[unlikely]]
    {
        return JobSystemResult::InvalidArguments;
    }

    const gp::UInt32 batchCount = computeBatchCount(
        desc.elementCount, desc.minBatchSize, desc.maxBatchCount, jobSystem.getTotalWorkerSlotCount(), desc.strategy
    );

    if (batchCount <= 1u)
    {
        JobContext ctx = JobSystem::getCurrentJobContext();
        if (ctx.jobSystem == nullptr)
        {
            ctx.jobSystem = &jobSystem;
            ctx.priority = desc.priority;
            ctx.stackTier = desc.stackTier;
        }

        callback(0u, desc.elementCount, userData, ctx);
        return JobSystemResult::Success;
    }

    const gp::UInt32 spawnedJobCount = batchCount - 1u;

    detail::ParallelForSharedState sharedState{};
    (void)sharedState.completionCounter.reset(spawnedJobCount);
    sharedState.nextBatchIndex.store(0u, gp::MemoryOrder::Relaxed);
    sharedState.callback = callback;
    sharedState.userData = userData;
    sharedState.jobSystem = &jobSystem;
    sharedState.elementCount = desc.elementCount;
    sharedState.batchCount = batchCount;
    sharedState.minBatchSize = (desc.minBatchSize == 0u) ? 1u : desc.minBatchSize;
    sharedState.strategy = desc.strategy;
    sharedState.priority = desc.priority;
    sharedState.stackTier = desc.stackTier;

    const JobDecl templateDecl = JobDecl::make(
        &detail::ParallelForSharedState::batchJobEntry,
        &sharedState,
        &sharedState.completionCounter.counter,
        desc.priority,
        desc.stackTier,
        static_cast<gp::UInt16>(desc.flags)
    );

    const JobSystemResult dispatchRes = jobSystem.dispatchRepeated(templateDecl, spawnedJobCount, false);
    if (dispatchRes != JobSystemResult::Success) [[unlikely]]
    {
        return dispatchRes;
    }

    // Claim and run one batch on the calling thread or fiber before waiting on the remaining batches.
    sharedState.executeOneClaimedBatch();

    return jobSystem.waitForCounter(sharedState.completionCounter, 0u);
}

}   // namespace gp::thread
