// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/jobs/JobSystem.hpp"
#include "memory/allocators/FiberScratchAllocator.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/Fiber.hpp"
#include "threading/jobs/JobSystemDesc.hpp"
#include "threading/jobs/JobSystemEnums.hpp"
#include "threading/jobs/JobSystemTypes.hpp"
#include "threading/queues/JobQueueDesc.hpp"
#include "threading/queues/JobQueueEnums.hpp"
#include "threading/sync/JobCounter.hpp"
#include "threading/sync/JobFence.hpp"
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace detail
{

/// @brief Compile-time verification helper for `gp::thread::JobContext` state queries.
/// @return True if all `constexpr` `JobContext` invariants hold.
[[nodiscard]] constexpr bool testConstexprJobContext() noexcept
{
    JobContext defaultCtx{};
    if (defaultCtx.isValid() || !defaultCtx.isMainThreadSlot() || defaultCtx.isWorkerThreadSlot())
    {
        return false;
    }

    JobContext validWorkerCtx{};
    validWorkerCtx.workerIndex = 3u;
    return !validWorkerCtx.isMainThreadSlot() && validWorkerCtx.isWorkerThreadSlot();
}

static_assert(testConstexprJobContext(), "Constexpr JobContext evaluation failed.");

/// @brief Task payload used to verify atomic accumulation and worker context validity across dispatched jobs.
struct SimpleAddPayload
{
    Atomic<gp::UInt64>* accumulator{ nullptr };
    gp::UInt64 value{ 0ULL };
    Atomic<gp::UInt32>* validContextCount{ nullptr };
    gp::UInt32 maxAllowedSlot{ 0u };
};

/// @brief Stateless `JobEntryFunction` that validates `JobSystem::getCurrentJobContext()` and adds `value` to
///        `accumulator`.
/// @param[in] rawData Pointer to `SimpleAddPayload`.
static void simpleAddJobEntry(void* const rawData) noexcept
{
    const SimpleAddPayload* const payload = static_cast<const SimpleAddPayload*>(rawData);
    if (payload == nullptr)
    {
        return;
    }

    const JobContext ctx = JobSystem::getCurrentJobContext();
    if (ctx.isValid() && ctx.workerIndex < payload->maxAllowedSlot && payload->validContextCount != nullptr)
    {
        payload->validContextCount->fetchAdd(1u, gp::MemoryOrder::Relaxed);
    }

    if (payload->accumulator != nullptr)
    {
        payload->accumulator->fetchAdd(payload->value, gp::MemoryOrder::AcqRel);
    }
}

/// @brief Shared context for 3-level nested fork-join stress testing under deliberate fiber pool exhaustion.
struct ThreeLevelForkJoinContext
{
    JobSystem* jobSystem{ nullptr };
    Atomic<gp::UInt32> grandchildExecutions{ 0u };
    Atomic<gp::UInt32> intactChildCanaries{ 0u };
    Atomic<gp::UInt32> intactParentCanaries{ 0u };
    gp::UInt32 childrenPerParent{ 4u };
    gp::UInt32 grandchildrenPerChild{ 4u };
};

/// @brief Leaf task in the 3-level fork-join tree that allocates from `FiberScratchAllocator` and increments
///        `grandchildExecutions`.
/// @param[in] rawData Pointer to `ThreeLevelForkJoinContext`.
static void grandchildLeafJobEntry(void* const rawData) noexcept
{
    ThreeLevelForkJoinContext* const ctx = static_cast<ThreeLevelForkJoinContext*>(rawData);
    gp::memory::ScopedFiberScratchMark leafMark;
    gp::memory::FiberScratchAllocator alloc;
    gp::UInt32* const scratchVal = static_cast<gp::UInt32*>(alloc.allocate(sizeof(gp::UInt32), alignof(gp::UInt32)));
    if (scratchVal != nullptr)
    {
        *scratchVal = 1u;
        ctx->grandchildExecutions.fetchAdd(*scratchVal, gp::MemoryOrder::AcqRel);
    }
}

/// @brief Middle-tier task in the 3-level fork-join tree that writes a scratch canary, spawns `grandchildrenPerChild`
///        leaf jobs, waits for them, and verifies its canary remained intact across fiber suspension or emergency
///        inline fallback.
/// @param[in] rawData Pointer to `ThreeLevelForkJoinContext`.
static void middleChildForkJoinJobEntry(void* const rawData) noexcept
{
    ThreeLevelForkJoinContext* const ctx = static_cast<ThreeLevelForkJoinContext*>(rawData);
    gp::memory::ScopedFiberScratchMark childMark;
    gp::memory::FiberScratchAllocator alloc;

    gp::UInt64* const childCanary = static_cast<gp::UInt64*>(alloc.allocate(sizeof(gp::UInt64), alignof(gp::UInt64)));
    ASSERT_NE(childCanary, nullptr);
    const gp::UInt64 expectedCanary = 0xA5A55A5AF00DFACEULL ^ reinterpret_cast<gp::UIntPtr>(childCanary);
    *childCanary = expectedCanary;

    JobCounter grandchildCounter(0u);
    const JobDecl grandchildDecl = JobDecl::make(
        &grandchildLeafJobEntry,
        ctx,
        &grandchildCounter,
        JobPriority::Normal,
        FiberStackTier::Standard64KiB,
        static_cast<gp::UInt16>(JobFlags::PreferLocalQueue | JobFlags::AllowInlineOnOverflow)
    );
    EXPECT_EQ(
        ctx->jobSystem->dispatchRepeated(grandchildDecl, ctx->grandchildrenPerChild, true), JobSystemResult::Success
    );

    // Help one local item directly before waiting on the remaining grandchildren.
    (void)ctx->jobSystem->helpOneWorkItem();
    EXPECT_EQ(ctx->jobSystem->waitForCounter(grandchildCounter, 0u), JobSystemResult::Success);

    if (*childCanary == expectedCanary)
    {
        ctx->intactChildCanaries.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }
}

/// @brief Root parent task in the 3-level fork-join tree that writes a scratch canary, dispatches `childrenPerParent`
///        middle-tier tasks, waits on `childCounter`, and verifies its scratch canary after resumption.
/// @param[in] rawData Pointer to `ThreeLevelForkJoinContext`.
static void parentForkJoinJobEntry(void* const rawData) noexcept
{
    ThreeLevelForkJoinContext* const ctx = static_cast<ThreeLevelForkJoinContext*>(rawData);
    gp::memory::ScopedFiberScratchMark parentMark;
    gp::memory::FiberScratchAllocator alloc;

    gp::UInt64* const canary = static_cast<gp::UInt64*>(alloc.allocate(sizeof(gp::UInt64), alignof(gp::UInt64)));
    ASSERT_NE(canary, nullptr);
    const gp::UInt64 expectedCanary = 0xCAFEBABE12345678ULL ^ reinterpret_cast<gp::UIntPtr>(canary);
    *canary = expectedCanary;

    JobCounter childCounter(0u);
    for (gp::UInt32 i = 0u; i < ctx->childrenPerParent; ++i)
    {
        EXPECT_EQ(
            ctx->jobSystem->dispatch(&middleChildForkJoinJobEntry, ctx, &childCounter, JobPriority::Normal),
            JobSystemResult::Success
        );
    }

    EXPECT_EQ(ctx->jobSystem->waitForCounter(childCounter, 0u), JobSystemResult::Success);

    if (*canary == expectedCanary)
    {
        ctx->intactParentCanaries.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }
}

/// @brief Coordination payload for testing cooperative `JobFence` waiting and signaling across fibers.
struct FenceCoordinationContext
{
    JobSystem* jobSystem{ nullptr };
    JobFence gateFence{ false };
    Atomic<gp::UInt32> stageReached{ 0u };
};

/// @brief Fiber job that records stage `1`, suspends on `gateFence`, and records stage `2` after waking.
/// @param[in] rawData Pointer to `FenceCoordinationContext`.
static void fenceWaiterFiberJob(void* const rawData) noexcept
{
    FenceCoordinationContext* const ctx = static_cast<FenceCoordinationContext*>(rawData);
    ctx->stageReached.store(1u, gp::MemoryOrder::Release);
    EXPECT_EQ(ctx->jobSystem->waitForFence(ctx->gateFence), JobSystemResult::Success);
    ctx->stageReached.store(2u, gp::MemoryOrder::Release);
}

/// @brief Fiber job that signals `gateFence` to wake any suspended waiter fibers.
/// @param[in] rawData Pointer to `FenceCoordinationContext`.
static void fenceSignalerJob(void* const rawData) noexcept
{
    FenceCoordinationContext* const ctx = static_cast<FenceCoordinationContext*>(rawData);
    ctx->gateFence.signal();
}

}   // namespace detail

TEST(JobSystemLifecycleTest, DefaultStateAndInvalidArguments)
{
    JobSystem jobSystem{};
    EXPECT_FALSE(jobSystem.isInitialized());
    EXPECT_EQ(jobSystem.getWorkerThreadCount(), 0u);
    EXPECT_EQ(jobSystem.getTotalWorkerSlotCount(), 0u);
    EXPECT_FALSE(jobSystem.helpOneWorkItem());

    JobDecl dummyDecl = JobDecl::make(&detail::simpleAddJobEntry, nullptr);
    EXPECT_EQ(jobSystem.dispatch(dummyDecl), JobSystemResult::NotInitialized);
    EXPECT_EQ(jobSystem.dispatchBatch(&dummyDecl, 1u), JobSystemResult::NotInitialized);
    EXPECT_EQ(jobSystem.dispatchRepeated(dummyDecl, 2u), JobSystemResult::NotInitialized);

    JobSystemDesc badDesc{};
    badDesc.standardFiberCount = 0u;
    EXPECT_EQ(jobSystem.initialize(badDesc), JobSystemResult::InvalidArguments);

    badDesc = JobSystemDesc{};
    badDesc.largeFiberCount = 0u;
    EXPECT_EQ(jobSystem.initialize(badDesc), JobSystemResult::InvalidArguments);

    badDesc = JobSystemDesc{};
    badDesc.workerThreadCount = kMaxJobSystemWorkers + 1u;
    EXPECT_EQ(jobSystem.initialize(badDesc), JobSystemResult::InvalidArguments);
}

TEST(JobSystemLifecycleTest, InitializeAndShutdownIdempotency)
{
    JobSystem jobSystem{};
    JobSystemDesc desc{};
    desc.workerThreadCount = 2u;
    desc.standardFiberCount = 16u;
    desc.largeFiberCount = 4u;
    desc.workerDequeCapacity = 64u;
    desc.globalQueueCapacity = 128u;
    desc.enableCoreAffinity = true;

    ASSERT_EQ(jobSystem.initialize(desc), JobSystemResult::Success);
    EXPECT_TRUE(jobSystem.isInitialized());
    EXPECT_EQ(jobSystem.getWorkerThreadCount(), 2u);
    EXPECT_EQ(jobSystem.getTotalWorkerSlotCount(), 3u);
    EXPECT_EQ(jobSystem.getStandardFiberCapacity(), 16u);
    EXPECT_EQ(jobSystem.getLargeFiberCapacity(), 4u);
    EXPECT_EQ(JobSystem::getCurrentJobSystem(), &jobSystem);
    EXPECT_EQ(JobSystem::getCurrentWorkerIndex(), kMainThreadWorkerIndex);
    EXPECT_FALSE(JobSystem::isWorkerThread());

    EXPECT_EQ(jobSystem.initialize(desc), JobSystemResult::AlreadyInitialized);
    EXPECT_EQ(jobSystem.dispatchBatch(nullptr, 0u), JobSystemResult::Success);
    EXPECT_EQ(jobSystem.dispatchBatch(nullptr, 2u), JobSystemResult::InvalidArguments);

    JobDecl invalidDecl{};
    EXPECT_EQ(jobSystem.dispatch(invalidDecl), JobSystemResult::InvalidArguments);
    EXPECT_EQ(jobSystem.dispatchRepeated(invalidDecl, 0u), JobSystemResult::Success);
    EXPECT_EQ(jobSystem.dispatchRepeated(invalidDecl, 2u), JobSystemResult::InvalidArguments);

    const JobSystemStats stats = jobSystem.getStats();
    EXPECT_EQ(stats.workerThreadCount, 2u);
    EXPECT_EQ(stats.totalWorkerSlotCount, 3u);
    EXPECT_EQ(stats.availableStandardFibers, 16u);
    EXPECT_EQ(stats.availableLargeFibers, 4u);

    jobSystem.shutdown();
    EXPECT_FALSE(jobSystem.isInitialized());
    jobSystem.shutdown();
}

TEST(JobSystemDispatchTest, SingleAndBatchDispatchAcrossPrioritiesAndStackTiers)
{
    JobSystem jobSystem{};
    JobSystemDesc desc{};
    desc.workerThreadCount = 3u;
    desc.standardFiberCount = 32u;
    desc.largeFiberCount = 8u;
    ASSERT_EQ(jobSystem.initialize(desc), JobSystemResult::Success);

    constexpr gp::UInt32 kJobCount = 128u;
    Atomic<gp::UInt64> accumulator{ 0ULL };
    Atomic<gp::UInt32> validContextCount{ 0u };
    detail::SimpleAddPayload payloads[kJobCount]{};
    JobDecl batchDecls[kJobCount]{};
    CacheAlignedJobCounter counter(0u);

    for (gp::UInt32 i = 0u; i < kJobCount; ++i)
    {
        payloads[i].accumulator = &accumulator;
        payloads[i].value = static_cast<gp::UInt64>(i + 1u);
        payloads[i].validContextCount = &validContextCount;
        payloads[i].maxAllowedSlot = jobSystem.getTotalWorkerSlotCount();

        const JobPriority prio =
            (i % 3u == 0u) ? JobPriority::High : ((i % 3u == 1u) ? JobPriority::Normal : JobPriority::Low);
        const FiberStackTier tier = (i % 8u == 0u) ? FiberStackTier::Large512KiB : FiberStackTier::Standard64KiB;
        const JobFlags flags = (i % 2u == 0u) ? JobFlags::PreferLocalQueue : JobFlags::ForceGlobalQueue;

        batchDecls[i] = JobDecl::make(
            &detail::simpleAddJobEntry, &payloads[i], &counter.counter, prio, tier, static_cast<gp::UInt16>(flags)
        );
    }

    ASSERT_EQ(jobSystem.dispatchBatch(batchDecls, kJobCount, true), JobSystemResult::Success);
    (void)jobSystem.helpOneWorkItem();
    ASSERT_EQ(jobSystem.waitForCounter(counter, 0u), JobSystemResult::Success);

    constexpr gp::UInt64 kExpectedSum = (static_cast<gp::UInt64>(kJobCount) * (kJobCount + 1ULL)) / 2ULL;
    EXPECT_EQ(accumulator.load(gp::MemoryOrder::Acquire), kExpectedSum);
    EXPECT_EQ(validContextCount.load(gp::MemoryOrder::Acquire), kJobCount);

    const JobSystemStats stats = jobSystem.getStats();
    EXPECT_EQ(stats.totalJobsDispatched, kJobCount);
    EXPECT_EQ(stats.totalJobsExecuted, kJobCount);
    EXPECT_EQ(stats.availableStandardFibers, 32u);
    EXPECT_EQ(stats.availableLargeFibers, 8u);

    jobSystem.shutdown();
}

TEST(JobSystemFiberYieldTest, NestedForkJoinWithFiberScratchAndPoolExhaustionResilience)
{
    // Provision a tiny fiber pool (4 Standard + 1 Large = 5 total fibers) and run a 3-level nested fork-join tree
    // (12 parents * 4 children * 4 grandchildren = 252 total jobs). When all 5 pooled fibers suspend on child
    // counters, workers fall back to recursive emergency execution on their ThreadAnchor without corrupting parent or
    // child scratch allocations.
    JobSystem jobSystem{};
    JobSystemDesc desc{};
    desc.workerThreadCount = 4u;
    desc.standardFiberCount = 4u;
    desc.largeFiberCount = 1u;
    ASSERT_EQ(jobSystem.initialize(desc), JobSystemResult::Success);

    constexpr gp::UInt32 kParentCount = 12u;
    constexpr gp::UInt32 kChildrenPerParent = 4u;
    constexpr gp::UInt32 kGrandchildrenPerChild = 4u;

    detail::ThreeLevelForkJoinContext forkCtx{};
    forkCtx.jobSystem = &jobSystem;
    forkCtx.childrenPerParent = kChildrenPerParent;
    forkCtx.grandchildrenPerChild = kGrandchildrenPerChild;

    CacheAlignedJobCounter parentCounter(0u);
    for (gp::UInt32 p = 0u; p < kParentCount; ++p)
    {
        ASSERT_EQ(
            jobSystem.dispatch(&detail::parentForkJoinJobEntry, &forkCtx, &parentCounter.counter, JobPriority::Normal),
            JobSystemResult::Success
        );
    }

    ASSERT_EQ(jobSystem.waitForCounter(parentCounter, 0u), JobSystemResult::Success);

    EXPECT_EQ(
        forkCtx.grandchildExecutions.load(gp::MemoryOrder::Acquire),
        kParentCount * kChildrenPerParent * kGrandchildrenPerChild
    );
    EXPECT_EQ(forkCtx.intactChildCanaries.load(gp::MemoryOrder::Acquire), kParentCount * kChildrenPerParent);
    EXPECT_EQ(forkCtx.intactParentCanaries.load(gp::MemoryOrder::Acquire), kParentCount);
    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);

    jobSystem.shutdown();
}

TEST(JobSystemSyncTest, CooperativeFenceWaitAndSignal)
{
    JobSystem jobSystem{};
    JobSystemDesc desc{};
    desc.workerThreadCount = 2u;
    desc.standardFiberCount = 8u;
    desc.largeFiberCount = 2u;
    ASSERT_EQ(jobSystem.initialize(desc), JobSystemResult::Success);

    detail::FenceCoordinationContext fenceCtx{};
    fenceCtx.jobSystem = &jobSystem;

    JobCounter completionCounter(0u);
    ASSERT_EQ(
        jobSystem.dispatch(&detail::fenceWaiterFiberJob, &fenceCtx, &completionCounter, JobPriority::High),
        JobSystemResult::Success
    );

    while (fenceCtx.stageReached.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    ASSERT_EQ(
        jobSystem.dispatch(&detail::fenceSignalerJob, &fenceCtx, nullptr, JobPriority::High), JobSystemResult::Success
    );

    EXPECT_EQ(jobSystem.waitForFence(fenceCtx.gateFence), JobSystemResult::Success);
    EXPECT_EQ(jobSystem.waitForCounter(completionCounter, 0u), JobSystemResult::Success);
    EXPECT_EQ(fenceCtx.stageReached.load(gp::MemoryOrder::Acquire), 2u);

    jobSystem.shutdown();
}

}   // namespace gp::thread::tests
