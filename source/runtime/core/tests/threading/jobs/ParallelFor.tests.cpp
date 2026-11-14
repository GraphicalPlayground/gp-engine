// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/jobs/ParallelFor.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/jobs/JobSystem.hpp"
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace detail
{

/// @brief Compile-time verification helper for `ParallelFor::computeBatchCount` and `ParallelFor::computeBatchRange`.
/// @return True if all `constexpr` range partitioning invariants hold.
[[nodiscard]] constexpr bool testConstexprParallelForPartitioning() noexcept
{
    if (ParallelFor::computeBatchCount(0u, 1u, 64u, 4u, JobSplitStrategy::Adaptive) != 0u)
    {
        return false;
    }

    if (ParallelFor::computeBatchCount(10u, 16u, 64u, 4u, JobSplitStrategy::Adaptive) != 1u)
    {
        return false;
    }

    // FixedBatchSize: 100 elements with batch size 16 yields ceil(100 / 16) == 7 batches.
    const gp::UInt32 fixedBatches =
        ParallelFor::computeBatchCount(100u, 16u, 64u, 4u, JobSplitStrategy::FixedBatchSize);
    if (fixedBatches != 7u)
    {
        return false;
    }

    gp::UInt32 totalCovered = 0u;
    gp::UInt32 expectedStart = 0u;
    for (gp::UInt32 b = 0u; b < fixedBatches; ++b)
    {
        gp::UInt32 start = 0u;
        gp::UInt32 count = 0u;
        ParallelFor::computeBatchRange(100u, fixedBatches, 16u, JobSplitStrategy::FixedBatchSize, b, start, count);
        if (start != expectedStart || count == 0u)
        {
            return false;
        }
        expectedStart += count;
        totalCovered += count;
    }

    if (totalCovered != 100u)
    {
        return false;
    }

    // Prime element count (1009) across Adaptive strategy with 5 worker slots (20 batches).
    const gp::UInt32 adaptiveBatches = ParallelFor::computeBatchCount(1009u, 8u, 64u, 5u, JobSplitStrategy::Adaptive);
    if (adaptiveBatches != 20u)
    {
        return false;
    }

    gp::UInt32 adaptiveCovered = 0u;
    gp::UInt32 adaptiveNextStart = 0u;
    for (gp::UInt32 b = 0u; b < adaptiveBatches; ++b)
    {
        gp::UInt32 start = 0u;
        gp::UInt32 count = 0u;
        ParallelFor::computeBatchRange(1009u, adaptiveBatches, 8u, JobSplitStrategy::Adaptive, b, start, count);
        if (start != adaptiveNextStart || (count != 50u && count != 51u))
        {
            return false;
        }
        adaptiveNextStart += count;
        adaptiveCovered += count;
    }

    return adaptiveCovered == 1009u;
}

static_assert(testConstexprParallelForPartitioning(), "Constexpr ParallelFor range partitioning failed.");

/// @brief Tracks per-index visit counts and batch invocation counts during `parallelFor` tests.
struct IndexVisitContext
{
    static constexpr gp::UInt32 kMaxElements = 2048u;
    Atomic<gp::UInt32> visitCounts[kMaxElements]{};
    Atomic<gp::UInt32> batchInvocations{ 0u };
};

/// @brief Batch callback that increments `visitCounts[idx]` for each element in `[startIndex, startIndex + count)`.
/// @param[in] startIndex Batch starting index.
/// @param[in] count Number of elements in the batch.
/// @param[in] rawData Pointer to `IndexVisitContext`.
/// @param[in] context Active worker and fiber context.
static void recordIndexVisitsBatch(
    const gp::UInt32 startIndex, const gp::UInt32 count, void* const rawData, [[maybe_unused]] const JobContext& context
) noexcept
{
    IndexVisitContext* const ctx = static_cast<IndexVisitContext*>(rawData);
    ctx->batchInvocations.fetchAdd(1u, gp::MemoryOrder::Relaxed);

    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        const gp::UInt32 idx = startIndex + i;
        if (idx < IndexVisitContext::kMaxElements)
        {
            ctx->visitCounts[idx].fetchAdd(1u, gp::MemoryOrder::AcqRel);
        }
    }
}

/// @brief Cache-line-isolated mock per-worker RHI command list used to verify lock-free RDG command recording.
struct alignas(gp::memory::HardwareConstants::cacheLineSize) MockWorkerRHICommandList
{
    Atomic<gp::UInt32> activeRecorderCount{ 0u };
    Atomic<gp::UInt32> concurrentCollisionDetected{ 0u };
    gp::UInt32 recordedDrawCount{ 0u };
    gp::UInt64 recordedVertexHashSum{ 0ULL };
};

/// @brief Context holding one `MockWorkerRHICommandList` per `JobSystem` worker slot (`0 .. totalWorkerSlots - 1`).
struct RDGParallelRecordingContext
{
    MockWorkerRHICommandList workerCommandLists[kMaxJobSystemWorkerSlots]{};
    gp::UInt32 totalWorkerSlots{ 0u };
    Atomic<gp::UInt32> outOfBoundsSlotErrors{ 0u };
};

/// @brief Simulates RDG parallel draw-call recording into `workerCommandLists[context.workerIndex]` without locks.
/// @param[in] startIndex Inclusive starting draw index for this batch.
/// @param[in] count Number of draw calls in this batch.
/// @param[in] rawData Pointer to `RDGParallelRecordingContext`.
/// @param[in] context Active worker and fiber execution context.
static void rdgRecordDrawsParallelForCallback(
    const gp::UInt32 startIndex, const gp::UInt32 count, void* const rawData, const JobContext& context
) noexcept
{
    RDGParallelRecordingContext* const rdgCtx = static_cast<RDGParallelRecordingContext*>(rawData);
    if (context.workerIndex >= rdgCtx->totalWorkerSlots) [[unlikely]]
    {
        rdgCtx->outOfBoundsSlotErrors.fetchAdd(1u, gp::MemoryOrder::Relaxed);
        return;
    }

    // Index into this worker thread's private RHI command list without taking a lock ([ADR-0052]).
    MockWorkerRHICommandList& cmdList = rdgCtx->workerCommandLists[context.workerIndex];

    // Check exclusive per-worker slot occupancy during non-yielding command recording.
    const gp::UInt32 prevActive = cmdList.activeRecorderCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    if (prevActive != 0u)
    {
        cmdList.concurrentCollisionDetected.fetchAdd(1u, gp::MemoryOrder::Relaxed);
    }

    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        const gp::UInt32 drawIdx = startIndex + i;
        ++cmdList.recordedDrawCount;
        cmdList.recordedVertexHashSum += static_cast<gp::UInt64>(drawIdx + 1u);
    }

    (void)cmdList.activeRecorderCount.fetchSub(1u, gp::MemoryOrder::Release);
}

/// @brief Shared context for nested `parallelFor` inside `parallelFor` execution tests.
struct NestedParallelForContext
{
    JobSystem* jobSystem{ nullptr };
    gp::UInt32 innerCount{ 32u };
    Atomic<gp::UInt64> totalAccumulated{ 0ULL };
};

/// @brief Inner `parallelFor` batch callback that atomically adds `count` to `totalAccumulated`.
/// @param[in] startIndex Starting element index (unused).
/// @param[in] count Number of elements in the inner batch.
/// @param[in] rawData Pointer to `NestedParallelForContext`.
/// @param[in] context Active worker and fiber context.
static void innerParallelForCallback(
    const gp::UInt32 startIndex, const gp::UInt32 count, void* const rawData, [[maybe_unused]] const JobContext& context
) noexcept
{
    NestedParallelForContext* const ctx = static_cast<NestedParallelForContext*>(rawData);
    (void)startIndex;
    ctx->totalAccumulated.fetchAdd(static_cast<gp::UInt64>(count), gp::MemoryOrder::AcqRel);
}

/// @brief Outer `parallelFor` batch callback that dispatches a nested `parallelFor` for each outer element.
/// @param[in] startIndex Starting element index (unused).
/// @param[in] count Number of outer elements in this batch.
/// @param[in] rawData Pointer to `NestedParallelForContext`.
/// @param[in] context Active worker and fiber context.
static void outerParallelForCallback(
    const gp::UInt32 startIndex, const gp::UInt32 count, void* const rawData, [[maybe_unused]] const JobContext& context
) noexcept
{
    NestedParallelForContext* const ctx = static_cast<NestedParallelForContext*>(rawData);
    (void)startIndex;
    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        EXPECT_EQ(
            ctx->jobSystem->parallelFor(ctx->innerCount, 8u, &innerParallelForCallback, ctx), JobSystemResult::Success
        );
    }
}

}   // namespace detail

TEST(ParallelForExecutionTest, StrategiesVisitEveryElementExactlyOnce)
{
    JobSystem uninitSystem{};
    EXPECT_EQ(
        uninitSystem.parallelFor(8u, 1u, &detail::recordIndexVisitsBatch, nullptr), JobSystemResult::NotInitialized
    );

    JobSystem jobSystem{};
    JobSystemDesc sysDesc{};
    sysDesc.workerThreadCount = 4u;
    sysDesc.standardFiberCount = 32u;
    sysDesc.largeFiberCount = 8u;
    ASSERT_EQ(jobSystem.initialize(sysDesc), JobSystemResult::Success);

    // Zero elements is a no-op, and a null callback with non-zero elements returns InvalidArguments.
    EXPECT_EQ(jobSystem.parallelFor(0u, 1u, &detail::recordIndexVisitsBatch, nullptr), JobSystemResult::Success);
    EXPECT_EQ(jobSystem.parallelFor(16u, 1u, nullptr, nullptr), JobSystemResult::InvalidArguments);

    // Single-batch fast path (elementCount <= minBatchSize) runs directly on the calling thread without queuing.
    {
        detail::IndexVisitContext singleBatchCtx{};
        ASSERT_EQ(
            jobSystem.parallelFor(12u, 32u, &detail::recordIndexVisitsBatch, &singleBatchCtx), JobSystemResult::Success
        );
        EXPECT_EQ(singleBatchCtx.batchInvocations.load(gp::MemoryOrder::Acquire), 1u);
        for (gp::UInt32 i = 0u; i < 12u; ++i)
        {
            EXPECT_EQ(singleBatchCtx.visitCounts[i].load(gp::MemoryOrder::Acquire), 1u);
        }
    }

    const JobSplitStrategy strategies[3] = { JobSplitStrategy::Adaptive,
                                             JobSplitStrategy::FixedBatchSize,
                                             JobSplitStrategy::SingleBatchPerWorker };

    constexpr gp::UInt32 kElementCount = 1537u;
    for (gp::USize s = 0u; s < 3u; ++s)
    {
        detail::IndexVisitContext visitCtx{};
        ParallelForDesc pfDesc{};
        pfDesc.elementCount = kElementCount;
        pfDesc.minBatchSize = (strategies[s] == JobSplitStrategy::FixedBatchSize) ? 4u : 24u;
        pfDesc.strategy = strategies[s];
        pfDesc.priority = JobPriority::Normal;

        ASSERT_EQ(jobSystem.parallelFor(pfDesc, &detail::recordIndexVisitsBatch, &visitCtx), JobSystemResult::Success);
        EXPECT_GT(visitCtx.batchInvocations.load(gp::MemoryOrder::Acquire), 1u);

        if (strategies[s] == JobSplitStrategy::SingleBatchPerWorker)
        {
            EXPECT_EQ(visitCtx.batchInvocations.load(gp::MemoryOrder::Acquire), jobSystem.getTotalWorkerSlotCount());
        }
        else if (strategies[s] == JobSplitStrategy::FixedBatchSize)
        {
            // 1537 / 4 == 385 batches, which clamps to kMaxParallelForBatches (256) and still visits every element.
            EXPECT_EQ(visitCtx.batchInvocations.load(gp::MemoryOrder::Acquire), kMaxParallelForBatches);
        }

        for (gp::UInt32 i = 0u; i < kElementCount; ++i)
        {
            EXPECT_EQ(visitCtx.visitCounts[i].load(gp::MemoryOrder::Acquire), 1u);
        }
    }

    jobSystem.shutdown();
}

TEST(ParallelForRDGTest, LockFreePerWorkerRHICommandListRecording)
{
    JobSystem jobSystem{};
    JobSystemDesc sysDesc{};
    sysDesc.workerThreadCount = 4u;
    sysDesc.standardFiberCount = 32u;
    sysDesc.largeFiberCount = 8u;
    ASSERT_EQ(jobSystem.initialize(sysDesc), JobSystemResult::Success);

    constexpr gp::UInt32 kDrawCallCount = 4096u;
    detail::RDGParallelRecordingContext rdgCtx{};
    rdgCtx.totalWorkerSlots = jobSystem.getTotalWorkerSlotCount();

    ParallelForDesc pfDesc{};
    pfDesc.elementCount = kDrawCallCount;
    pfDesc.minBatchSize = 64u;
    pfDesc.strategy = JobSplitStrategy::Adaptive;
    pfDesc.priority = JobPriority::High;
    pfDesc.flags = JobFlags::PreferLocalQueue | JobFlags::RDGPass;

    ASSERT_EQ(
        jobSystem.parallelFor(pfDesc, &detail::rdgRecordDrawsParallelForCallback, &rdgCtx), JobSystemResult::Success
    );

    EXPECT_EQ(rdgCtx.outOfBoundsSlotErrors.load(gp::MemoryOrder::Acquire), 0u);

    gp::UInt32 totalDrawsRecorded = 0u;
    gp::UInt64 totalVertexHashSum = 0ULL;
    for (gp::UInt32 slot = 0u; slot < rdgCtx.totalWorkerSlots; ++slot)
    {
        EXPECT_EQ(rdgCtx.workerCommandLists[slot].concurrentCollisionDetected.load(gp::MemoryOrder::Acquire), 0u);
        totalDrawsRecorded += rdgCtx.workerCommandLists[slot].recordedDrawCount;
        totalVertexHashSum += rdgCtx.workerCommandLists[slot].recordedVertexHashSum;
    }

    constexpr gp::UInt64 kExpectedHashSum = (static_cast<gp::UInt64>(kDrawCallCount) * (kDrawCallCount + 1ULL)) / 2ULL;
    EXPECT_EQ(totalDrawsRecorded, kDrawCallCount);
    EXPECT_EQ(totalVertexHashSum, kExpectedHashSum);

    jobSystem.shutdown();
}

TEST(ParallelForNestingTest, NestedParallelForInsideParallelForYieldsCleanly)
{
    JobSystem jobSystem{};
    JobSystemDesc sysDesc{};
    sysDesc.workerThreadCount = 3u;
    sysDesc.standardFiberCount = 32u;
    sysDesc.largeFiberCount = 8u;
    ASSERT_EQ(jobSystem.initialize(sysDesc), JobSystemResult::Success);

    constexpr gp::UInt32 kOuterCount = 12u;
    constexpr gp::UInt32 kInnerCount = 32u;

    detail::NestedParallelForContext nestedCtx{};
    nestedCtx.jobSystem = &jobSystem;
    nestedCtx.innerCount = kInnerCount;

    ASSERT_EQ(
        jobSystem.parallelFor(kOuterCount, 2u, &detail::outerParallelForCallback, &nestedCtx), JobSystemResult::Success
    );
    EXPECT_EQ(
        nestedCtx.totalAccumulated.load(gp::MemoryOrder::Acquire),
        static_cast<gp::UInt64>(kOuterCount) * static_cast<gp::UInt64>(kInnerCount)
    );

    jobSystem.shutdown();
}

}   // namespace gp::thread::tests
