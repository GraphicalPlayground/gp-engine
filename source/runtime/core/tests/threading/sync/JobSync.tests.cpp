// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/FiberScratchAllocator.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/Fiber.hpp"
#include "threading/sync/JobCounter.hpp"
#include "threading/sync/JobFence.hpp"
#include "threading/sync/JobWaitNode.hpp"
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace
{

static_assert(sizeof(JobCounter) == 16u, "JobCounter must be 16 bytes.");
static_assert(alignof(JobCounter) == 8u, "JobCounter must be 8-byte aligned.");
static_assert(
    sizeof(CacheAlignedJobCounter) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedJobCounter must match CPU cache line size (64 bytes)."
);
static_assert(
    alignof(CacheAlignedJobCounter) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedJobCounter must be cache-line aligned (64 bytes)."
);
static_assert(sizeof(JobFence) == sizeof(gp::UIntPtr), "JobFence must be pointer-sized.");
static_assert(alignof(JobWaitNode) >= 8u, "JobWaitNode must be at least 8-byte aligned.");

constexpr bool testConstexprCounterBitPacking() noexcept
{
    const gp::UInt64 initialLocked = kJobCounterLockBit | (2ULL << kJobCounterWaiterShift) | 5ULL;
    const gp::UInt64 afterParkUnlock =
        initialLocked + (kJobCounterAddWaiterAndUnlockDelta | kJobCounterHasThresholdWaiterBit);

    const bool lockCleared = (afterParkUnlock & kJobCounterLockBit) == 0ULL;
    const bool thresholdSet = (afterParkUnlock & kJobCounterHasThresholdWaiterBit) != 0ULL;
    const bool waiterIncremented = JobCounter::unpackWaiterCount(afterParkUnlock) == 3u;
    const bool countPreserved = JobCounter::unpackCount(afterParkUnlock) == 5u;

    const gp::UInt64 relocked = afterParkUnlock | kJobCounterLockBit;
    const gp::UInt64 unlockAndWakeTwo =
        (2ULL << kJobCounterWaiterShift) | kJobCounterHasThresholdWaiterBit | kJobCounterLockBit;
    const gp::UInt64 afterWake = relocked - unlockAndWakeTwo;

    const bool wakeLockCleared = (afterWake & kJobCounterLockBit) == 0ULL;
    const bool wakeThresholdCleared = (afterWake & kJobCounterHasThresholdWaiterBit) == 0ULL;
    const bool wakeWaiterRemaining = JobCounter::unpackWaiterCount(afterWake) == 1u;
    const bool wakeCountPreserved = JobCounter::unpackCount(afterWake) == 5u;

    return lockCleared && thresholdSet && waiterIncremented && countPreserved && wakeLockCleared &&
           wakeThresholdCleared && wakeWaiterRemaining && wakeCountPreserved;
}

static_assert(testConstexprCounterBitPacking(), "64-bit JobCounter modular lock/waiter bit math failed.");

struct LifecycleTracker
{
    static inline Atomic<gp::Int32> s_constructCount{ 0 };
    static inline Atomic<gp::Int32> s_destructCount{ 0 };

    static void reset() noexcept
    {
        s_constructCount.store(0, gp::MemoryOrder::Release);
        s_destructCount.store(0, gp::MemoryOrder::Release);
    }

    gp::UInt32 token{ 0u };

    explicit LifecycleTracker(const gp::UInt32 inToken) noexcept
        : token(inToken)
    {
        s_constructCount.fetchAdd(1, gp::MemoryOrder::Relaxed);
    }

    ~LifecycleTracker() noexcept
    {
        s_destructCount.fetchAdd(1, gp::MemoryOrder::Relaxed);
    }
};

struct StandaloneCounterFiberState
{
    JobCounter* counter{ nullptr };
    JobFence* fence{ nullptr };
    gp::UInt32 stage{ 0u };
    gp::UInt32 resumeCount{ 0u };
};

static void standaloneCounterAndFenceFiberEntry(void* const rawData) noexcept
{
    StandaloneCounterFiberState* const state = static_cast<StandaloneCounterFiberState*>(rawData);
    LifecycleTracker tracker(0xCAFEBABEu);
    (void)tracker;

    state->stage = 1u;
    EXPECT_EQ(state->counter->wait(0u, JobWaitMode::Auto), JobSyncResult::Success);

    state->stage = 2u;
    EXPECT_EQ(state->fence->wait(JobWaitMode::Auto), JobSyncResult::Success);

    state->stage = 3u;
}

struct MiniSchedulerContext
{
    static constexpr gp::UInt32 kMaxReadyFibers = 16u;
    Fiber* anchorFiber{ nullptr };
    Fiber* readyQueue[kMaxReadyFibers]{};
    gp::UInt32 readyCount{ 0u };
    gp::UInt32 wakeOrderIds[kMaxReadyFibers]{};
    gp::UInt32 wakeEventCount{ 0u };
    gp::UInt32 yieldCallCount{ 0u };
    bool forceYieldFailure{ false };
};

static JobSyncResult miniSchedulerYieldCallback(Fiber& currentFiber, void* const rawData) noexcept
{
    MiniSchedulerContext* const sched = static_cast<MiniSchedulerContext*>(rawData);
    ++sched->yieldCallCount;
    if (sched->forceYieldFailure)
    {
        return JobSyncResult::YieldFailed;
    }

    if (sched->anchorFiber == nullptr)
    {
        return JobSyncResult::YieldFailed;
    }

    const FiberResult switchRes = currentFiber.switchTo(*sched->anchorFiber);
    return (switchRes == FiberResult::Success) ? JobSyncResult::Success : JobSyncResult::YieldFailed;
}

static void miniSchedulerWakeCallback(Fiber* const waitingFiber, void* const rawData) noexcept
{
    MiniSchedulerContext* const sched = static_cast<MiniSchedulerContext*>(rawData);
    if (waitingFiber != nullptr && sched->readyCount < MiniSchedulerContext::kMaxReadyFibers)
    {
        sched->readyQueue[sched->readyCount++] = waitingFiber;
        if (sched->wakeEventCount < MiniSchedulerContext::kMaxReadyFibers)
        {
            sched->wakeOrderIds[sched->wakeEventCount++] = waitingFiber->getId();
        }
    }
}

struct ThresholdWaiterTaskArg
{
    JobCounter* counter{ nullptr };
    JobCounterValue targetCount{ 0u };
    gp::UInt32 fiberSlot{ 0u };
    gp::UInt32 observedCountUponWake{ 999u };
    bool scratchVerified{ false };
    bool completed{ false };
};

static void thresholdWaiterFiberEntry(void* const rawData) noexcept
{
    ThresholdWaiterTaskArg* const arg = static_cast<ThresholdWaiterTaskArg*>(rawData);
    gp::memory::FiberScratchAllocator scratch{};
    gp::memory::ScopedFiberScratchMark mark{};

    gp::UInt32* const scratchData = static_cast<gp::UInt32*>(scratch.allocate(32u * sizeof(gp::UInt32), 16u));
    ASSERT_NE(scratchData, nullptr);

    for (gp::UInt32 i = 0u; i < 32u; ++i)
    {
        scratchData[i] = (arg->fiberSlot + 1u) * 1000u + i;
    }

    ASSERT_EQ(arg->counter->wait(arg->targetCount, JobWaitMode::Auto), JobSyncResult::Success);
    arg->observedCountUponWake = arg->counter->getCount(gp::MemoryOrder::Acquire);

    bool matches = true;
    for (gp::UInt32 i = 0u; i < 32u; ++i)
    {
        if (scratchData[i] != (arg->fiberSlot + 1u) * 1000u + i)
        {
            matches = false;
        }
    }

    arg->scratchVerified = matches;
    arg->completed = true;
}

struct YieldFailFiberArg
{
    JobCounter* counter{ nullptr };
    JobFence* fence{ nullptr };
    JobSyncResult counterWaitResult{ JobSyncResult::Success };
    JobSyncResult fenceWaitResult{ JobSyncResult::Success };
    JobSyncResult spinCounterWaitResult{ JobSyncResult::InvalidArguments };
    JobSyncResult spinFenceWaitResult{ JobSyncResult::InvalidArguments };
};

static void yieldFailureFiberEntry(void* const rawData) noexcept
{
    YieldFailFiberArg* const arg = static_cast<YieldFailFiberArg*>(rawData);

    // Both waits must fail cleanly via YieldFailed and unlink their stack-resident JobWaitNode from the wait lists.
    arg->counterWaitResult = arg->counter->wait(0u, JobWaitMode::Auto);
    arg->fenceWaitResult = arg->fence->wait(JobWaitMode::Auto);

    // Decrement counter and signal fence, then verify SpinOnly succeeds inside the child fiber without yielding.
    EXPECT_EQ(arg->counter->decrement(1u), JobSyncResult::Success);
    arg->fence->signal();

    arg->spinCounterWaitResult = arg->counter->wait(0u, JobWaitMode::SpinOnly);
    arg->spinFenceWaitResult = arg->fence->wait(JobWaitMode::SpinOnly);
}

struct SmpMigrationStressState
{
    static constexpr gp::UInt32 kTotalIterations = 256u;

    CacheAlignedJobCounter iterCounter{ 0u };
    Fiber migratingFiber{};

    Atomic<Fiber*> readyFiberSlot{ nullptr };
    Atomic<gp::UInt32> armedIteration{ 0u };
    Atomic<gp::UInt32> workerBReady{ 0u };
    Atomic<bool> allIterationsDone{ false };

    gp::UInt32 completedIterations{ 0u };
    bool allCanariesValid{ true };
};

static JobSyncResult smpYieldCallback(Fiber& currentFiber, void* const rawData) noexcept
{
    SmpMigrationStressState* const state = static_cast<SmpMigrationStressState*>(rawData);
    (void)state;
    Fiber* const returnAnchor = currentFiber.getReturnFiber();
    if (returnAnchor == nullptr)
    {
        return JobSyncResult::YieldFailed;
    }

    const FiberResult res = currentFiber.switchTo(*returnAnchor);
    return (res == FiberResult::Success) ? JobSyncResult::Success : JobSyncResult::YieldFailed;
}

static void smpWakeCallback(Fiber* const waitingFiber, void* const rawData) noexcept
{
    SmpMigrationStressState* const state = static_cast<SmpMigrationStressState*>(rawData);
    state->readyFiberSlot.store(waitingFiber, gp::MemoryOrder::Release);
}

static void smpMigratingFiberEntry(void* const rawData) noexcept
{
    SmpMigrationStressState* const state = static_cast<SmpMigrationStressState*>(rawData);
    gp::memory::FiberScratchAllocator scratch{};
    gp::memory::ScopedFiberScratchMark mark{};

    gp::UInt64* const scratchCanary =
        static_cast<gp::UInt64*>(scratch.allocate(SmpMigrationStressState::kTotalIterations * sizeof(gp::UInt64), 16u));
    ASSERT_NE(scratchCanary, nullptr);

    for (gp::UInt32 iter = 0u; iter < SmpMigrationStressState::kTotalIterations; ++iter)
    {
        const gp::UInt64 stackCanary = 0xA5A55A5A00000000ULL | static_cast<gp::UInt64>(iter);
        scratchCanary[iter] = stackCanary;

        // Arm iterCounter to 1, publish the iteration number to Worker B, and immediately park on iterCounter.
        // Worker B will race to decrement iterCounter (1 -> 0) and resume migratingFiber across OS threads!
        ASSERT_EQ(state->iterCounter.increment(), JobSyncResult::Success);
        state->armedIteration.store(iter + 1u, gp::MemoryOrder::Release);
        ASSERT_EQ(state->iterCounter.wait(0u, JobWaitMode::Auto), JobSyncResult::Success);

        if (scratchCanary[iter] != stackCanary)
        {
            state->allCanariesValid = false;
        }

        ++state->completedIterations;
    }

    state->allIterationsDone.store(true, gp::MemoryOrder::Release);
}

static gp::UInt32 smpWorkerBEntry(void* const rawData) noexcept
{
    SmpMigrationStressState* const state = static_cast<SmpMigrationStressState*>(rawData);
    ScopedThreadFiber anchorB(3002u, "SmpWorkerBAnchor");
    if (!anchorB.isValid())
    {
        return 1u;
    }

    JobFiberParker parker{};
    parker.yieldCallback = &smpYieldCallback;
    parker.wakeCallback = &smpWakeCallback;
    parker.userData = state;
    ScopedJobFiberParker parkerGuard(parker);

    state->workerBReady.store(1u, gp::MemoryOrder::Release);

    gp::UInt32 lastDecrementedIter = 0u;
    while (!state->allIterationsDone.load(gp::MemoryOrder::Acquire))
    {
        const gp::UInt32 armed = state->armedIteration.load(gp::MemoryOrder::Acquire);
        if (armed > lastDecrementedIter)
        {
            lastDecrementedIter = armed;
            EXPECT_EQ(state->iterCounter.decrement(1u), JobSyncResult::Success);
        }

        Fiber* const wokenFiber = state->readyFiberSlot.exchange(nullptr, gp::MemoryOrder::AcqRel);
        if (wokenFiber != nullptr)
        {
            // Migrate wokenFiber onto Worker B! Fiber::switchTo's m_isStackActive latch guarantees we wait the
            // ~10-20 ns for the yielding worker to finish vacating wokenFiber's stack before binding m_returnFiber
            // and entering wokenFiber on Worker B.
            EXPECT_EQ(anchorB.getFiber().switchTo(*wokenFiber), FiberResult::Success);
        }
        else
        {
            gp::platform::Thread::spinWait();
        }
    }

    return 0u;
}

struct MultiThreadCounterStressContext
{
    static constexpr gp::UInt32 kWorkerCount = 4u;
    static constexpr gp::UInt32 kIncrementsPerWorker = 2000u;

    CacheAlignedJobCounter sharedCounter{ kWorkerCount * kIncrementsPerWorker };
    JobFence goFence{};
    Atomic<gp::UInt32> readyThreads{ 0u };
};

static gp::UInt32 multiThreadDecrementWorkerEntry(void* const rawData) noexcept
{
    MultiThreadCounterStressContext* const ctx = static_cast<MultiThreadCounterStressContext*>(rawData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);

    EXPECT_EQ(ctx->goFence.wait(JobWaitMode::Auto), JobSyncResult::Success);

    for (gp::UInt32 i = 0u; i < MultiThreadCounterStressContext::kIncrementsPerWorker; ++i)
    {
        EXPECT_EQ(ctx->sharedCounter.decrement(1u), JobSyncResult::Success);
    }

    return 0u;
}

}   // namespace

TEST(JobSyncTest, CounterAndFenceNominalOperationsAndErrorValidation)
{
    JobCounter counter{};
    EXPECT_EQ(counter.getCount(), 0u);
    EXPECT_EQ(counter.getWaiterCount(), 0u);
    EXPECT_FALSE(counter.hasThresholdWaiters());
    EXPECT_TRUE(counter.isComplete());
    EXPECT_TRUE(counter.isAtOrBelow(0u));
    EXPECT_TRUE(counter.tryWait(0u));
    EXPECT_EQ(counter.wait(0u), JobSyncResult::Success);

    // Invalid zero-delta arguments and underflow/overflow checks
    EXPECT_EQ(counter.add(0u), JobSyncResult::InvalidArguments);
    EXPECT_EQ(counter.decrement(0u), JobSyncResult::InvalidArguments);
    EXPECT_EQ(counter.decrement(1u), JobSyncResult::CounterUnderflow);

    JobCounterValue newCount = 0u;
    EXPECT_EQ(counter.increment(&newCount), JobSyncResult::Success);
    EXPECT_EQ(newCount, 1u);
    EXPECT_EQ(counter.add(4u, &newCount), JobSyncResult::Success);
    EXPECT_EQ(newCount, 5u);
    EXPECT_EQ(counter.getCount(), 5u);
    EXPECT_FALSE(counter.isComplete());
    EXPECT_FALSE(counter.tryWait(0u));
    EXPECT_TRUE(counter.tryWait(5u));
    EXPECT_TRUE(counter.isAtOrBelow(5u));
    EXPECT_FALSE(counter.isAtOrBelow(4u));
    EXPECT_EQ(counter.decrement(6u), JobSyncResult::CounterUnderflow);
    EXPECT_EQ(counter.getCount(), 5u);

    // Overflow protection near kMaxJobCounterValue
    EXPECT_EQ(counter.reset(kMaxJobCounterValue - 1u), JobSyncResult::Success);
    EXPECT_EQ(counter.add(2u), JobSyncResult::CounterOverflow);
    EXPECT_EQ(counter.increment(&newCount), JobSyncResult::Success);
    EXPECT_EQ(newCount, kMaxJobCounterValue);
    EXPECT_EQ(counter.increment(), JobSyncResult::CounterOverflow);

    // Reset and decrement down to zero
    EXPECT_EQ(counter.reset(3u), JobSyncResult::Success);
    EXPECT_EQ(counter.decrement(2u, &newCount), JobSyncResult::Success);
    EXPECT_EQ(newCount, 1u);
    EXPECT_EQ(counter.decrement(1u, &newCount), JobSyncResult::Success);
    EXPECT_EQ(newCount, 0u);
    EXPECT_TRUE(counter.isComplete());

    // CacheAlignedJobCounter forwarding verification
    CacheAlignedJobCounter alignedCounter(2u);
    EXPECT_EQ(alignedCounter.getCount(), 2u);
    EXPECT_EQ(alignedCounter.getWaiterCount(), 0u);
    EXPECT_FALSE(alignedCounter.hasThresholdWaiters());
    EXPECT_FALSE(alignedCounter.isComplete());
    EXPECT_FALSE(alignedCounter.tryWait(0u));
    EXPECT_TRUE(alignedCounter.tryWait(2u));
    EXPECT_TRUE(alignedCounter.isAtOrBelow(2u));
    EXPECT_EQ(alignedCounter.increment(&newCount), JobSyncResult::Success);
    EXPECT_EQ(newCount, 3u);
    EXPECT_EQ(alignedCounter.decrement(3u, &newCount), JobSyncResult::Success);
    EXPECT_EQ(newCount, 0u);
    EXPECT_TRUE(alignedCounter.isComplete());
    EXPECT_TRUE(alignedCounter.tryWait(0u));
    EXPECT_EQ(alignedCounter.wait(0u), JobSyncResult::Success);
    EXPECT_EQ(alignedCounter.reset(0u), JobSyncResult::Success);

    // JobFence nominal state transitions
    JobFence fence{};
    EXPECT_FALSE(fence.isSignaled());
    EXPECT_FALSE(fence.tryWait());
    EXPECT_FALSE(fence.hasWaiters());

    fence.signal();
    EXPECT_TRUE(fence.isSignaled());
    EXPECT_TRUE(fence.tryWait());
    // Idempotent second signal
    fence.signal();
    EXPECT_TRUE(fence.isSignaled());
    EXPECT_EQ(fence.wait(), JobSyncResult::Success);

    EXPECT_EQ(fence.reset(), JobSyncResult::Success);
    EXPECT_FALSE(fence.isSignaled());
    EXPECT_FALSE(fence.tryWait());

    JobFence initiallySignaledFence(true);
    EXPECT_TRUE(initiallySignaledFence.isSignaled());
    EXPECT_TRUE(initiallySignaledFence.tryWait());
    EXPECT_EQ(initiallySignaledFence.wait(), JobSyncResult::Success);
}

TEST(JobSyncTest, ScopedJobFiberParkerNestingAndRestoration)
{
    EXPECT_FALSE(JobFiberParker::hasThreadParker());

    MiniSchedulerContext ctxA{};
    JobFiberParker parkerA{};
    parkerA.yieldCallback = &miniSchedulerYieldCallback;
    parkerA.wakeCallback = &miniSchedulerWakeCallback;
    parkerA.userData = &ctxA;

    {
        ScopedJobFiberParker guardA(parkerA);
        EXPECT_TRUE(JobFiberParker::hasThreadParker());
        EXPECT_EQ(JobFiberParker::getThreadParker().userData, &ctxA);
        EXPECT_FALSE(guardA.getPreviousParker().isValid());

        MiniSchedulerContext ctxB{};
        JobFiberParker parkerB = parkerA;
        parkerB.userData = &ctxB;

        {
            ScopedJobFiberParker guardB(parkerB);
            EXPECT_TRUE(JobFiberParker::hasThreadParker());
            EXPECT_EQ(JobFiberParker::getThreadParker().userData, &ctxB);
            EXPECT_EQ(guardB.getPreviousParker().userData, &ctxA);
        }

        EXPECT_TRUE(JobFiberParker::hasThreadParker());
        EXPECT_EQ(JobFiberParker::getThreadParker().userData, &ctxA);
    }

    EXPECT_FALSE(JobFiberParker::hasThreadParker());
}

TEST(JobSyncTest, StandaloneFiberReturnAnchorParkingAndSpuriousResumeProtection)
{
    LifecycleTracker::reset();

    ScopedThreadFiber anchorGuard(100u, "StandaloneSyncAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    JobCounter counter(2u);
    JobFence fence(false);

    StandaloneCounterFiberState state{};
    state.counter = &counter;
    state.fence = &fence;

    Fiber childFiber{};
    FiberDesc desc{};
    desc.name = "StandaloneWaitingFiber";
    desc.id = 1u;
    desc.entry = &standaloneCounterAndFenceFiberEntry;
    desc.userData = &state;
    desc.returnFiber = &anchor;
    ASSERT_EQ(childFiber.create(desc), FiberResult::Success);

    // Enter childFiber -> blocks on counter.wait(0) and yields back to anchor
    ASSERT_EQ(anchor.switchTo(childFiber), FiberResult::Success);
    EXPECT_EQ(state.stage, 1u);
    EXPECT_TRUE(childFiber.isSuspended());
    EXPECT_EQ(counter.getWaiterCount(), 1u);
    EXPECT_FALSE(counter.hasThresholdWaiters());

    // Calling reset() while a waiter is parked must return BusyWaiting
    EXPECT_EQ(counter.reset(0u), JobSyncResult::BusyWaiting);

    // Intermediate decrement (2 -> 1): because hasThresholdWaiters() is false, decrement(1) takes the lock-free
    // single-instruction fast path without waking childFiber!
    EXPECT_EQ(counter.decrement(1u), JobSyncResult::Success);
    EXPECT_EQ(counter.getCount(), 1u);
    EXPECT_EQ(counter.getWaiterCount(), 1u);

    // Spurious resume before counter reaches 0: childFiber must re-yield without unwinding its stack waitNode!
    ASSERT_EQ(anchor.switchTo(childFiber), FiberResult::Success);
    EXPECT_EQ(state.stage, 1u);
    EXPECT_TRUE(childFiber.isSuspended());
    EXPECT_EQ(counter.getWaiterCount(), 1u);

    // Decrement counter to 0 (1 -> 0) -> detaches waitNode and marks it Signaled
    EXPECT_EQ(counter.decrement(1u), JobSyncResult::Success);
    EXPECT_EQ(counter.getWaiterCount(), 0u);

    // Resume childFiber -> advances to stage 2 and blocks on fence.wait()
    ASSERT_EQ(anchor.switchTo(childFiber), FiberResult::Success);
    EXPECT_EQ(state.stage, 2u);
    EXPECT_TRUE(childFiber.isSuspended());
    EXPECT_TRUE(fence.hasWaiters());

    // Calling fence.reset() while a waiter is parked must return BusyWaiting
    EXPECT_EQ(fence.reset(), JobSyncResult::BusyWaiting);

    // Spurious resume on unsignaled fence: childFiber must re-yield safely
    ASSERT_EQ(anchor.switchTo(childFiber), FiberResult::Success);
    EXPECT_EQ(state.stage, 2u);
    EXPECT_TRUE(childFiber.isSuspended());
    EXPECT_TRUE(fence.hasWaiters());

    // Signal fence and resume childFiber to completion
    fence.signal();
    EXPECT_FALSE(fence.hasWaiters());
    ASSERT_EQ(anchor.switchTo(childFiber), FiberResult::Success);
    EXPECT_EQ(state.stage, 3u);
    EXPECT_TRUE(childFiber.isCompleted());

    EXPECT_EQ(
        LifecycleTracker::s_constructCount.load(gp::MemoryOrder::Acquire),
        LifecycleTracker::s_destructCount.load(gp::MemoryOrder::Acquire)
    );
    EXPECT_EQ(childFiber.destroy(), FiberResult::Success);
}

TEST(JobSyncTest, CustomParkerFifoWakeOrderAndPartialThresholdWaits)
{
    ASSERT_TRUE(gp::memory::FiberScratchArena::initializePool(4096u, 16u));

    ScopedThreadFiber anchorGuard(200u, "MiniSchedAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    MiniSchedulerContext sched{};
    sched.anchorFiber = &anchor;

    JobFiberParker parker{};
    parker.yieldCallback = &miniSchedulerYieldCallback;
    parker.wakeCallback = &miniSchedulerWakeCallback;
    parker.userData = &sched;
    ScopedJobFiberParker parkerGuard(parker);

    JobCounter counter(3u);

    constexpr gp::UInt32 kWaiterCount = 4u;
    // Fiber 0 & Fiber 1 both wait for targetCount == 0 (testing FIFO wake order between equal-threshold waiters).
    // Fiber 2 waits for targetCount == 2.
    // Fiber 3 waits for targetCount == 1.
    const JobCounterValue targets[kWaiterCount] = { 0u, 0u, 2u, 1u };

    Fiber fibers[kWaiterCount]{};
    ThresholdWaiterTaskArg args[kWaiterCount]{};

    for (gp::UInt32 i = 0u; i < kWaiterCount; ++i)
    {
        args[i].counter = &counter;
        args[i].targetCount = targets[i];
        args[i].fiberSlot = i;

        FiberDesc desc{};
        desc.name = "ThresholdWaiterFiber";
        desc.id = 10u + i;
        desc.entry = &thresholdWaiterFiberEntry;
        desc.userData = &args[i];
        desc.returnFiber = &anchor;
        ASSERT_EQ(fibers[i].create(desc), FiberResult::Success);

        // Start each fiber so it allocates from FiberScratchAllocator and parks on counter
        ASSERT_EQ(anchor.switchTo(fibers[i]), FiberResult::Success);
        EXPECT_TRUE(fibers[i].isSuspended());
        if (i < 2u)
        {
            EXPECT_FALSE(counter.hasThresholdWaiters());
        }
        else
        {
            EXPECT_TRUE(counter.hasThresholdWaiters());
        }
    }

    EXPECT_EQ(counter.getWaiterCount(), 4u);
    EXPECT_TRUE(counter.hasThresholdWaiters());
    EXPECT_EQ(sched.readyCount, 0u);

    // Step 1: Decrement counter 3 -> 2. Only Fiber 2 (id = 12, targetCount = 2) should be woken!
    ASSERT_EQ(counter.decrement(1u), JobSyncResult::Success);
    EXPECT_EQ(counter.getWaiterCount(), 3u);
    EXPECT_TRUE(counter.hasThresholdWaiters());
    ASSERT_EQ(sched.readyCount, 1u);
    EXPECT_EQ(sched.readyQueue[0], &fibers[2]);
    EXPECT_EQ(sched.wakeOrderIds[0], 12u);

    // Step 2: Decrement counter 2 -> 1. Only Fiber 3 (id = 13, targetCount = 1) should be woken, and
    // hasThresholdWaiters() must automatically clear to false because only targetCount == 0 waiters remain!
    ASSERT_EQ(counter.decrement(1u), JobSyncResult::Success);
    EXPECT_EQ(counter.getWaiterCount(), 2u);
    EXPECT_FALSE(counter.hasThresholdWaiters());
    ASSERT_EQ(sched.readyCount, 2u);
    EXPECT_EQ(sched.readyQueue[1], &fibers[3]);
    EXPECT_EQ(sched.wakeOrderIds[1], 13u);

    // Step 3: Decrement counter 1 -> 0. Fiber 0 (id = 10) and Fiber 1 (id = 11) must be woken in FIFO arrival order!
    ASSERT_EQ(counter.decrement(1u), JobSyncResult::Success);
    EXPECT_EQ(counter.getWaiterCount(), 0u);
    EXPECT_FALSE(counter.hasThresholdWaiters());
    ASSERT_EQ(sched.readyCount, 4u);
    EXPECT_EQ(sched.wakeOrderIds[2], 10u);
    EXPECT_EQ(sched.wakeOrderIds[3], 11u);

    // Drain the ready queue and run all woken fibers to completion
    for (gp::UInt32 i = 0u; i < sched.readyCount; ++i)
    {
        ASSERT_EQ(anchor.switchTo(*sched.readyQueue[i]), FiberResult::Success);
        EXPECT_TRUE(sched.readyQueue[i]->isCompleted());
    }

    for (gp::UInt32 i = 0u; i < kWaiterCount; ++i)
    {
        EXPECT_TRUE(args[i].completed);
        EXPECT_TRUE(args[i].scratchVerified);
        EXPECT_LE(args[i].observedCountUponWake, args[i].targetCount);
        EXPECT_EQ(fibers[i].destroy(), FiberResult::Success);
    }

    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);
    gp::memory::FiberScratchArena::shutdownPool();
}

TEST(JobSyncTest, YieldFailureUnlinksWaitNodeCleanlyAndSpinOnlyModeWorksInFiber)
{
    ScopedThreadFiber anchorGuard(300u, "YieldFailAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    MiniSchedulerContext sched{};
    sched.anchorFiber = &anchor;
    sched.forceYieldFailure = true;

    JobFiberParker failingParker{};
    failingParker.yieldCallback = &miniSchedulerYieldCallback;
    failingParker.wakeCallback = &miniSchedulerWakeCallback;
    failingParker.userData = &sched;
    ScopedJobFiberParker parkerGuard(failingParker);

    JobCounter counter(1u);
    JobFence fence(false);

    YieldFailFiberArg arg{};
    arg.counter = &counter;
    arg.fence = &fence;

    Fiber childFiber{};
    FiberDesc desc{};
    desc.name = "YieldFailFiber";
    desc.id = 55u;
    desc.entry = &yieldFailureFiberEntry;
    desc.userData = &arg;
    desc.returnFiber = &anchor;
    ASSERT_EQ(childFiber.create(desc), FiberResult::Success);

    ASSERT_EQ(anchor.switchTo(childFiber), FiberResult::Success);
    EXPECT_TRUE(childFiber.isCompleted());

    EXPECT_EQ(arg.counterWaitResult, JobSyncResult::YieldFailed);
    EXPECT_EQ(arg.fenceWaitResult, JobSyncResult::YieldFailed);
    EXPECT_EQ(arg.spinCounterWaitResult, JobSyncResult::Success);
    EXPECT_EQ(arg.spinFenceWaitResult, JobSyncResult::Success);
    EXPECT_EQ(counter.getWaiterCount(), 0u);
    EXPECT_FALSE(fence.hasWaiters());

    EXPECT_EQ(childFiber.destroy(), FiberResult::Success);
}

TEST(JobSyncTest, SmpLostWakeupAndDualCoreStackHandoffStressWithCrossThreadMigration)
{
    ASSERT_TRUE(gp::memory::FiberScratchArena::initializePool(4096u, 32u));

    SmpMigrationStressState state{};

    FiberDesc fiberDesc{};
    fiberDesc.name = "SmpMigratingJobFiber";
    fiberDesc.id = 77u;
    fiberDesc.entry = &smpMigratingFiberEntry;
    fiberDesc.userData = &state;
    ASSERT_EQ(state.migratingFiber.create(fiberDesc), FiberResult::Success);

    gp::platform::Thread workerB{};
    gp::platform::ThreadDesc workerBDesc{};
    workerBDesc.name = "GP_SmpSyncWorkerB";
    workerBDesc.entry = &smpWorkerBEntry;
    workerBDesc.userData = &state;
    ASSERT_EQ(workerB.start(workerBDesc), gp::platform::ThreadResult::Success);

    while (state.workerBReady.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    {
        ScopedThreadFiber anchorA(3001u, "SmpWorkerAAnchor");
        ASSERT_TRUE(anchorA.isValid());

        JobFiberParker parker{};
        parker.yieldCallback = &smpYieldCallback;
        parker.wakeCallback = &smpWakeCallback;
        parker.userData = &state;
        ScopedJobFiberParker parkerGuard(parker);

        // Kick off migratingFiber on Worker A; Fiber::switchTo automatically binds migratingFiber's returnFiber to
        // anchorA after acquiring m_isStackActive.
        ASSERT_EQ(anchorA.getFiber().switchTo(state.migratingFiber), FiberResult::Success);

        // Both Worker A and Worker B participate in resuming migratingFiber whenever it wakes from iterCounter.
        while (!state.allIterationsDone.load(gp::MemoryOrder::Acquire))
        {
            Fiber* const wokenFiber = state.readyFiberSlot.exchange(nullptr, gp::MemoryOrder::AcqRel);
            if (wokenFiber != nullptr)
            {
                ASSERT_EQ(anchorA.getFiber().switchTo(*wokenFiber), FiberResult::Success);
            }
            else
            {
                gp::platform::Thread::spinWait();
            }
        }
    }

    gp::UInt32 exitB = 1u;
    EXPECT_EQ(workerB.join(&exitB), gp::platform::ThreadResult::Success);
    EXPECT_EQ(exitB, 0u);

    EXPECT_EQ(state.completedIterations, SmpMigrationStressState::kTotalIterations);
    EXPECT_TRUE(state.allCanariesValid);
    EXPECT_TRUE(state.migratingFiber.isCompleted());
    EXPECT_FALSE(state.migratingFiber.isStackActive());
    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);

    EXPECT_EQ(state.migratingFiber.destroy(), FiberResult::Success);
    gp::memory::FiberScratchArena::shutdownPool();
}

TEST(JobSyncTest, MultiThreadedContendedCounterDecrementAndFenceBarrierStress)
{
    MultiThreadCounterStressContext ctx{};
    gp::platform::Thread workers[MultiThreadCounterStressContext::kWorkerCount]{};

    for (gp::UInt32 i = 0u; i < MultiThreadCounterStressContext::kWorkerCount; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_CounterStressWorker";
        desc.entry = &multiThreadDecrementWorkerEntry;
        desc.userData = &ctx;
        ASSERT_EQ(workers[i].start(desc), gp::platform::ThreadResult::Success);
    }

    while (ctx.readyThreads.load(gp::MemoryOrder::Acquire) < MultiThreadCounterStressContext::kWorkerCount)
    {
        gp::platform::Thread::spinWait();
    }

    // Release all 4 worker threads simultaneously from the JobFence barrier
    ctx.goFence.signal();

    // Main thread waits on sharedCounter reaching 0
    EXPECT_EQ(ctx.sharedCounter.wait(0u, JobWaitMode::Auto), JobSyncResult::Success);
    EXPECT_TRUE(ctx.sharedCounter.isComplete());
    EXPECT_EQ(ctx.sharedCounter.getCount(), 0u);
    EXPECT_EQ(ctx.sharedCounter.getWaiterCount(), 0u);

    for (gp::UInt32 i = 0u; i < MultiThreadCounterStressContext::kWorkerCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(workers[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }
}

}   // namespace gp::thread::tests
