// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/fibers/Fiber.hpp"
#include "memory/allocators/FiberScratchAllocator.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/FiberContext.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <new>   // IWYU pragma: keep
#include <utility>

namespace gp::thread::tests
{

namespace
{

static_assert(
    Fiber::resolveStackTierBytes(FiberStackTier::Standard64KiB) == (64u * 1024u),
    "Standard64KiB tier must resolve to 64 KiB."
);
static_assert(
    Fiber::resolveStackTierBytes(FiberStackTier::Large512KiB) == (512u * 1024u),
    "Large512KiB tier must resolve to 512 KiB."
);
static_assert(
    Fiber::resolveStackTierBytes(FiberStackTier::Custom, 1024u) == gp::platform::kMinFiberStackSize,
    "Custom stack tier below kMinFiberStackSize must clamp to 16 KiB."
);

struct LifecycleTracker
{
    static inline gp::thread::Atomic<gp::Int32> s_constructCount{ 0 };
    static inline gp::thread::Atomic<gp::Int32> s_destructCount{ 0 };

    static void reset() noexcept
    {
        s_constructCount.store(0, gp::MemoryOrder::Release);
        s_destructCount.store(0, gp::MemoryOrder::Release);
    }

    gp::UInt32 id{ 0u };
    gp::UInt32 payload{ 0u };

    LifecycleTracker(const gp::UInt32 inId, const gp::UInt32 inPayload) noexcept
        : id(inId)
        , payload(inPayload)
    {
        s_constructCount.fetchAdd(1, gp::MemoryOrder::Relaxed);
    }

    ~LifecycleTracker() noexcept
    {
        s_destructCount.fetchAdd(1, gp::MemoryOrder::Relaxed);
    }
};

struct RawContextPingPongState
{
    gp::platform::FiberContext* anchor{ nullptr };
    gp::platform::FiberContext* child{ nullptr };
    gp::UInt32 stepCounter{ 0u };
    gp::Float64 accumulatedFp{ 0.0 };
    gp::platform::FiberContextResult activeDestroyResult{ gp::platform::FiberContextResult::Success };
    gp::platform::FiberContextResult inactiveAnchorRestoreResult{ gp::platform::FiberContextResult::Success };
};

static void rawContextPingPongEntry(void* const rawData) noexcept
{
    RawContextPingPongState* const state = static_cast<RawContextPingPongState*>(rawData);

    // Attempting to destroy the active child context or restore the suspended thread anchor from inside the child
    // context must be rejected safely with CannotDestroyActiveContext.
    state->activeDestroyResult = state->child->destroy();
    state->inactiveAnchorRestoreResult = state->anchor->restoreCurrentThread();

    // Preserve volatile/non-volatile floating-point values across context switches
    gp::Float64 localFpA = 123.456;
    gp::Float64 localFpB = 789.012;

    state->stepCounter = 1u;
    (void)state->child->switchTo(*state->anchor);

    localFpA *= 2.5;
    localFpB -= 100.0;
    state->accumulatedFp = localFpA + localFpB;
    state->stepCounter = 3u;

    (void)state->child->switchTo(*state->anchor);
}

struct RearmCounterState
{
    gp::UInt32 totalExecutions{ 0u };
    gp::UInt32 lastScratchCheckSum{ 0u };
};

static void rearmableTaskEntry(void* const rawData) noexcept
{
    RearmCounterState* const state = static_cast<RearmCounterState*>(rawData);
    ++state->totalExecutions;

    gp::memory::FiberScratchAllocator scratch{};
    gp::UInt32* const tempValues = static_cast<gp::UInt32*>(scratch.allocate(16u * sizeof(gp::UInt32), 16u));
    ASSERT_NE(tempValues, nullptr);

    gp::UInt32 sum = 0u;
    for (gp::UInt32 i = 0u; i < 16u; ++i)
    {
        tempValues[i] = state->totalExecutions + i;
        sum += tempValues[i];
    }
    state->lastScratchCheckSum = sum;
}

struct OutOfOrderScratchState
{
    Fiber* anchor{ nullptr };
    Fiber* fiberA{ nullptr };
    Fiber* fiberB{ nullptr };
    bool fiberAVerified{ false };
    bool fiberBVerified{ false };
};

static void fiberAOutOfOrderTask(void* const rawData) noexcept
{
    OutOfOrderScratchState* const state = static_cast<OutOfOrderScratchState*>(rawData);
    gp::memory::FiberScratchAllocator scratch{};

    {
        gp::memory::ScopedFiberScratchMark scopeA{};
        gp::UInt8* const bufA = static_cast<gp::UInt8*>(scratch.allocate(2048u, 64u));
        ASSERT_NE(bufA, nullptr);
        gp::memory::setMemory(bufA, 0xA5, 2048u);

        // Yield directly from Fiber A to Fiber B while scopeA is active on Fiber A's stack
        ASSERT_EQ(state->fiberA->switchTo(*state->fiberB), FiberResult::Success);

        // Resumed from Fiber B while Fiber B is still suspended with its own active allocation!
        for (gp::USize i = 0u; i < 2048u; ++i)
        {
            if (bufA[i] != 0xA5u)
            {
                state->fiberAVerified = false;
                return;
            }
        }
        state->fiberAVerified = true;
    }

    // Fiber A's ScopedFiberScratchMark has now unwound Fiber A's PagedLinearAllocator back to 0 leased pages.
    EXPECT_EQ(state->fiberA->getScratchArena().getLeasedPageCount(), 0u);

    // Resume Fiber B so it can verify its own scratch allocation was not clobbered by Fiber A's unwind.
    ASSERT_EQ(state->fiberA->switchTo(*state->fiberB), FiberResult::Success);
}

static void fiberBOutOfOrderTask(void* const rawData) noexcept
{
    OutOfOrderScratchState* const state = static_cast<OutOfOrderScratchState*>(rawData);
    gp::memory::FiberScratchAllocator scratch{};

    {
        gp::memory::ScopedFiberScratchMark scopeB{};
        gp::UInt8* const bufB = static_cast<gp::UInt8*>(scratch.allocate(2048u, 64u));
        ASSERT_NE(bufB, nullptr);
        gp::memory::setMemory(bufB, 0x5A, 2048u);

        // Switch back to Fiber A, which will finish its scope and unwind its arena first
        ASSERT_EQ(state->fiberB->switchTo(*state->fiberA), FiberResult::Success);

        // Verify Fiber B's scratch memory is completely untouched
        bool intact = true;
        for (gp::USize i = 0u; i < 2048u; ++i)
        {
            if (bufB[i] != 0x5Au)
            {
                intact = false;
                break;
            }
        }
        state->fiberBVerified = intact;
    }

    EXPECT_EQ(state->fiberB->getScratchArena().getLeasedPageCount(), 0u);
}

struct DeepStackTaskState
{
    gp::UInt32 depth{ 0u };
    gp::UInt64 checksum{ 0ULL };
};

static gp::UInt64 consumeStackRecursively(const gp::UInt32 remainingDepth, const gp::UInt8 seed) noexcept
{
    alignas(16) volatile gp::UInt8 stackFrameBuffer[4096];
    for (gp::USize i = 0u; i < sizeof(stackFrameBuffer); i += 64u)
    {
        stackFrameBuffer[i] = static_cast<gp::UInt8>(seed + remainingDepth);
    }

    if (remainingDepth <= 1u)
    {
        return static_cast<gp::UInt64>(stackFrameBuffer[0]);
    }

    return static_cast<gp::UInt64>(stackFrameBuffer[0]) +
           consumeStackRecursively(remainingDepth - 1u, static_cast<gp::UInt8>(seed + 1u));
}

static void deepStackFiberTask(void* const rawData) noexcept
{
    DeepStackTaskState* const state = static_cast<DeepStackTaskState*>(rawData);
    // 48 frames * 4 KiB = ~192 KiB of stack usage (exceeds 64 KiB standard stack, fits easily in 512 KiB large stack)
    state->checksum = consumeStackRecursively(state->depth, 7u);
}

struct CrossThreadMigrationState
{
    Fiber migratingFiber{};
    Fiber* currentReturnAnchor{ nullptr };
    gp::thread::Atomic<gp::UInt32> phase{ 0u };
    gp::platform::ThreadId threadIdAtStart{ 0ULL };
    gp::platform::ThreadId threadIdAfterResume{ 0ULL };
    bool scratchVerifiedAfterMigration{ false };
};

static void migratingFiberEntry(void* const rawData) noexcept
{
    CrossThreadMigrationState* const state = static_cast<CrossThreadMigrationState*>(rawData);
    gp::memory::FiberScratchAllocator scratch{};

    state->threadIdAtStart = gp::platform::Thread::currentThreadId();

    {
        gp::memory::ScopedFiberScratchMark migrationScope{};
        ASSERT_TRUE(migrationScope.isArenaBoundToCurrentContext());

        constexpr gp::UInt32 kInitialCount = 16u;
        LifecycleTracker* const items = static_cast<LifecycleTracker*>(
            scratch.allocate(kInitialCount * sizeof(LifecycleTracker), alignof(LifecycleTracker))
        );
        ASSERT_NE(items, nullptr);

        for (gp::UInt32 i = 0u; i < kInitialCount; ++i)
        {
            new (&items[i]) LifecycleTracker(100u + i, (i + 1u) * 11u);
        }

        // Suspend on Worker Thread A and return to Worker Thread A's anchor fiber
        ASSERT_NE(state->currentReturnAnchor, nullptr);
        ASSERT_EQ(state->migratingFiber.switchTo(*state->currentReturnAnchor), FiberResult::Success);

        // We have now been stolen and resumed on Worker Thread B!
        state->threadIdAfterResume = gp::platform::Thread::currentThreadId();

        EXPECT_EQ(Fiber::getCurrentFiber(), &state->migratingFiber);
        EXPECT_EQ(gp::memory::FiberScratchArena::getBoundFiberArena(), &state->migratingFiber.getScratchArena());
        EXPECT_TRUE(migrationScope.isArenaBoundToCurrentContext());

        // Grow the allocation in-place on Worker Thread B
        constexpr gp::UInt32 kExpandedCount = 32u;
        LifecycleTracker* const grownItems = static_cast<LifecycleTracker*>(scratch.reallocate(
            items,
            kInitialCount * sizeof(LifecycleTracker),
            kExpandedCount * sizeof(LifecycleTracker),
            alignof(LifecycleTracker)
        ));
        ASSERT_EQ(grownItems, items);

        for (gp::UInt32 i = kInitialCount; i < kExpandedCount; ++i)
        {
            new (&grownItems[i]) LifecycleTracker(100u + i, (i + 1u) * 11u);
        }

        bool valid = true;
        for (gp::UInt32 i = 0u; i < kExpandedCount; ++i)
        {
            if (grownItems[i].id != (100u + i) || grownItems[i].payload != ((i + 1u) * 11u))
            {
                valid = false;
            }
            grownItems[i].~LifecycleTracker();
        }

        state->scratchVerifiedAfterMigration = valid;
    }

    // Update the fiber's returnFiber so completion yields back to Worker Thread B's anchor!
    state->migratingFiber.setReturnFiber(state->currentReturnAnchor);
}

static gp::UInt32 workerThreadAEntry(void* const rawData) noexcept
{
    CrossThreadMigrationState* const state = static_cast<CrossThreadMigrationState*>(rawData);

    ScopedThreadFiber workerAAnchor(1001u, "WorkerA_Anchor");
    if (!workerAAnchor.isValid())
    {
        state->phase.store(99u, gp::MemoryOrder::Release);
        return 1u;
    }

    state->currentReturnAnchor = &workerAAnchor.getFiber();
    state->migratingFiber.setReturnFiber(&workerAAnchor.getFiber());

    // Switch from Worker A into migratingFiber; migratingFiber will allocate scratch and yield back here
    const FiberResult switchRes = workerAAnchor.getFiber().switchTo(state->migratingFiber);
    EXPECT_EQ(switchRes, FiberResult::Success);
    EXPECT_EQ(state->migratingFiber.getState(), FiberState::Suspended);
    EXPECT_EQ(gp::memory::FiberScratchArena::getBoundFiberArena(), nullptr);

    // Publish the suspended fiber so Worker Thread B can steal and resume it
    state->phase.store(1u, gp::MemoryOrder::Release);
    return 0u;
}

static gp::UInt32 workerThreadBEntry(void* const rawData) noexcept
{
    CrossThreadMigrationState* const state = static_cast<CrossThreadMigrationState*>(rawData);

    while (state->phase.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    if (state->phase.load(gp::MemoryOrder::Acquire) != 1u)
    {
        return 1u;
    }

    ScopedThreadFiber workerBAnchor(1002u, "WorkerB_Anchor");
    if (!workerBAnchor.isValid())
    {
        return 1u;
    }

    state->currentReturnAnchor = &workerBAnchor.getFiber();

    // Resume the stolen fiber on Worker Thread B!
    const FiberResult switchRes = workerBAnchor.getFiber().switchTo(state->migratingFiber);
    EXPECT_EQ(switchRes, FiberResult::Success);
    EXPECT_EQ(state->migratingFiber.getState(), FiberState::Completed);
    EXPECT_EQ(gp::memory::FiberScratchArena::getBoundFiberArena(), nullptr);

    state->phase.store(2u, gp::MemoryOrder::Release);
    return 0u;
}

}   // namespace

TEST(FiberContextTest, DefaultStateAndInvalidArgumentsValidation)
{
    gp::platform::FiberContext context{};
    EXPECT_FALSE(context.isValid());
    EXPECT_FALSE(context.isThreadAnchor());
    EXPECT_FALSE(context.isCurrentlyExecuting());
    EXPECT_EQ(context.getStackSize(), 0u);
    EXPECT_EQ(context.getNativeHandle(), nullptr);

    EXPECT_EQ(context.destroy(), gp::platform::FiberContextResult::NotInitialized);
    EXPECT_EQ(context.restoreCurrentThread(), gp::platform::FiberContextResult::NotInitialized);
    EXPECT_EQ(context.switchTo(context), gp::platform::FiberContextResult::NotInitialized);

    gp::platform::FiberContextDesc invalidDesc{};
    invalidDesc.entry = nullptr;
    EXPECT_EQ(context.create(invalidDesc), gp::platform::FiberContextResult::InvalidArguments);

    invalidDesc.entry = &rawContextPingPongEntry;
    invalidDesc.stackReserveSize = gp::platform::kMaxFiberStackSize + 4096u;
    EXPECT_EQ(context.create(invalidDesc), gp::platform::FiberContextResult::InvalidArguments);

    invalidDesc.stackReserveSize = gp::platform::kDefaultFiberStackSize;
    invalidDesc.stackCommitSize = gp::platform::kDefaultFiberStackSize * 2u;
    EXPECT_EQ(context.create(invalidDesc), gp::platform::FiberContextResult::InvalidArguments);

    // Verify 0 reserve size defaults to kDefaultFiberStackSize and sub-minimum clamps to kMinFiberStackSize
    gp::platform::FiberContextDesc minDesc{};
    minDesc.entry = &rawContextPingPongEntry;
    minDesc.stackReserveSize = 1024u;
    minDesc.stackCommitSize = 0u;
    ASSERT_EQ(context.create(minDesc), gp::platform::FiberContextResult::Success);
    EXPECT_GE(context.getStackSize(), gp::platform::kMinFiberStackSize);

    // Double create / convertCurrentThread / restoreCurrentThread on an initialized child context
    EXPECT_EQ(context.create(minDesc), gp::platform::FiberContextResult::AlreadyInitialized);
    EXPECT_EQ(context.convertCurrentThread(), gp::platform::FiberContextResult::AlreadyInitialized);
    EXPECT_EQ(context.restoreCurrentThread(), gp::platform::FiberContextResult::NotAThreadFiber);
    EXPECT_EQ(context.destroy(), gp::platform::FiberContextResult::Success);
}

TEST(FiberContextTest, ThreadConversionPingPongSwitchAndMoveSemantics)
{
    gp::platform::ScopedThreadFiberContext threadAnchorGuard{};
    ASSERT_TRUE(threadAnchorGuard.isValid());
    EXPECT_EQ(threadAnchorGuard.getResult(), gp::platform::FiberContextResult::Success);
    EXPECT_TRUE(gp::platform::FiberContext::isCurrentThreadConverted());

    gp::platform::FiberContext& anchor = threadAnchorGuard.getContext();
    EXPECT_TRUE(anchor.isThreadAnchor());
    EXPECT_TRUE(anchor.isCurrentlyExecuting());
    EXPECT_EQ(anchor.switchTo(anchor), gp::platform::FiberContextResult::SelfSwitchError);

    // Attempting a second thread-anchor conversion on the same OS thread must fail deterministically
    gp::platform::FiberContext duplicateAnchor{};
    EXPECT_EQ(duplicateAnchor.convertCurrentThread(), gp::platform::FiberContextResult::AlreadyThreadFiber);

    RawContextPingPongState state{};
    state.anchor = &anchor;

    gp::platform::FiberContext initialChild{};
    gp::platform::FiberContextDesc desc{};
    desc.entry = &rawContextPingPongEntry;
    desc.userData = &state;
    desc.stackReserveSize = gp::platform::kDefaultFiberStackSize;

    ASSERT_EQ(initialChild.create(desc), gp::platform::FiberContextResult::Success);
    EXPECT_TRUE(initialChild.isValid());
    EXPECT_GE(initialChild.getStackSize(), gp::platform::kDefaultFiberStackSize);

    // Calling switchTo from a non-executing child context must return SwitchFailed
    EXPECT_EQ(initialChild.switchTo(anchor), gp::platform::FiberContextResult::SwitchFailed);

    // Move-construct and move-assign the child context prior to its first switch to verify move-safe trampoline handoff
    gp::platform::FiberContext tempMoved(std::move(initialChild));
    EXPECT_FALSE(initialChild.isValid());
    EXPECT_TRUE(tempMoved.isValid());

    gp::platform::FiberContext movedChild{};
    movedChild = std::move(tempMoved);
    EXPECT_FALSE(tempMoved.isValid());
    EXPECT_TRUE(movedChild.isValid());
    state.child = &movedChild;

    ASSERT_EQ(anchor.switchTo(movedChild), gp::platform::FiberContextResult::Success);
    EXPECT_EQ(state.stepCounter, 1u);
    EXPECT_EQ(state.activeDestroyResult, gp::platform::FiberContextResult::CannotDestroyActiveContext);
    EXPECT_EQ(state.inactiveAnchorRestoreResult, gp::platform::FiberContextResult::CannotDestroyActiveContext);
    EXPECT_TRUE(anchor.isCurrentlyExecuting());

    state.stepCounter = 2u;
    ASSERT_EQ(anchor.switchTo(movedChild), gp::platform::FiberContextResult::Success);
    EXPECT_EQ(state.stepCounter, 3u);
    EXPECT_DOUBLE_EQ(state.accumulatedFp, (123.456 * 2.5) + (789.012 - 100.0));

    EXPECT_EQ(movedChild.destroy(), gp::platform::FiberContextResult::Success);
    EXPECT_FALSE(movedChild.isValid());
}

TEST(FiberTest, LifecycleIdleCreationAndPersistentTaskRearmingWithoutReallocation)
{
    ASSERT_TRUE(gp::memory::FiberScratchArena::initializePool(4096u, 16u));

    ScopedThreadFiber anchorGuard(100u, "MainTestAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    EXPECT_TRUE(Fiber::isExecutingInFiber());
    EXPECT_FALSE(Fiber::isExecutingInChildFiber());
    EXPECT_EQ(Fiber::getCurrentFiber(), &anchor);
    EXPECT_EQ(Fiber::getCurrentFiberId(), 100u);

    // Create a child fiber initially in FiberState::Idle (no task bound yet)
    Fiber workerFiber{};
    FiberDesc desc{};
    desc.name = "PooledWorkerFiber_0";
    desc.id = 1u;
    desc.entry = nullptr;
    desc.returnFiber = &anchor;
    desc.stackTier = FiberStackTier::Standard64KiB;

    ASSERT_EQ(workerFiber.create(desc), FiberResult::Success);
    EXPECT_EQ(workerFiber.getState(), FiberState::Idle);
    EXPECT_EQ(workerFiber.getScratchArena().getLeasedPageCount(), 0u);

    // Switching into an Idle fiber without a bound task must be rejected with NoTaskBound
    EXPECT_EQ(anchor.switchTo(workerFiber), FiberResult::NoTaskBound);

    const void* const originalNativeHandle = workerFiber.getPlatformContext().getNativeHandle();
    RearmCounterState rearmState{};

    // Re-arm and execute 64 consecutive tasks on the exact same Fiber without recreating its stack context
    constexpr gp::UInt32 kTaskCount = 64u;
    for (gp::UInt32 i = 1u; i <= kTaskCount; ++i)
    {
        ASSERT_EQ(workerFiber.bindTask(&rearmableTaskEntry, &rearmState, &anchor), FiberResult::Success);
        EXPECT_EQ(workerFiber.getState(), FiberState::Ready);

        ASSERT_EQ(anchor.switchTo(workerFiber), FiberResult::Success);

        EXPECT_EQ(workerFiber.getState(), FiberState::Completed);
        EXPECT_EQ(workerFiber.getCompletedTaskCount(), static_cast<gp::UInt64>(i));
        EXPECT_EQ(rearmState.totalExecutions, i);
        EXPECT_EQ(workerFiber.getScratchArena().getLeasedPageCount(), 0u);
        EXPECT_EQ(workerFiber.getPlatformContext().getNativeHandle(), originalNativeHandle);
    }

    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);
    EXPECT_EQ(workerFiber.destroy(), FiberResult::Success);

    gp::memory::FiberScratchArena::shutdownPool();
}

TEST(FiberTest, CooperativeYieldingUpdatesEpochAndPreservesOutOfOrderFiberScratchArenas)
{
    ASSERT_TRUE(gp::memory::FiberScratchArena::initializePool(4096u, 16u));

    ScopedThreadFiber anchorGuard(200u, "SchedulerAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    OutOfOrderScratchState state{};
    state.anchor = &anchor;

    Fiber fiberA{};
    Fiber fiberB{};
    state.fiberA = &fiberA;
    state.fiberB = &fiberB;

    FiberDesc descA{};
    descA.name = "FiberA";
    descA.id = 10u;
    descA.entry = &fiberAOutOfOrderTask;
    descA.userData = &state;
    descA.returnFiber = &anchor;

    FiberDesc descB{};
    descB.name = "FiberB";
    descB.id = 11u;
    descB.entry = &fiberBOutOfOrderTask;
    descB.userData = &state;
    descB.returnFiber = &anchor;

    ASSERT_EQ(fiberA.create(descA), FiberResult::Success);
    ASSERT_EQ(fiberB.create(descB), FiberResult::Success);

    const gp::UInt64 epochBefore = gp::memory::FiberScratchArena::getThreadSwitchEpoch();

    // Start Fiber A -> switches to Fiber B -> switches to Fiber A (unwinds A) -> switches to Fiber B (unwinds B) ->
    // returns to anchor
    ASSERT_EQ(anchor.switchTo(fiberA), FiberResult::Success);

    // Fiber B completed and returned to anchor, while Fiber A is at the end of its task waiting to complete
    EXPECT_EQ(fiberB.getState(), FiberState::Completed);
    EXPECT_EQ(fiberA.getState(), FiberState::Suspended);

    // Resume Fiber A so it completes its final return to anchor
    ASSERT_EQ(anchor.switchTo(fiberA), FiberResult::Success);
    EXPECT_EQ(fiberA.getState(), FiberState::Completed);

    const gp::UInt64 epochAfter = gp::memory::FiberScratchArena::getThreadSwitchEpoch();
    EXPECT_GE(epochAfter - epochBefore, 6ULL);
    EXPECT_TRUE(state.fiberAVerified);
    EXPECT_TRUE(state.fiberBVerified);
    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);

    EXPECT_EQ(fiberA.destroy(), FiberResult::Success);
    EXPECT_EQ(fiberB.destroy(), FiberResult::Success);
    gp::memory::FiberScratchArena::shutdownPool();
}

TEST(FiberTest, Large512KiBStackTierSupportsDeepRecursiveCallTrees)
{
    ScopedThreadFiber anchorGuard(300u, "LargeStackAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    DeepStackTaskState deepState{};
    deepState.depth = 48u;   // ~192 KiB of stack frames

    Fiber largeFiber{};
    FiberDesc desc{};
    desc.name = "DeepRecursionFiber";
    desc.id = 42u;
    desc.entry = &deepStackFiberTask;
    desc.userData = &deepState;
    desc.returnFiber = &anchor;
    desc.stackTier = FiberStackTier::Large512KiB;

    ASSERT_EQ(largeFiber.create(desc), FiberResult::Success);
    EXPECT_GE(largeFiber.getStackSize(), kLargeFiberStackBytes);

    ASSERT_EQ(anchor.switchTo(largeFiber), FiberResult::Success);
    EXPECT_EQ(largeFiber.getState(), FiberState::Completed);
    EXPECT_GT(deepState.checksum, 0ULL);

    EXPECT_EQ(largeFiber.destroy(), FiberResult::Success);
}

TEST(FiberTest, CrossThreadWorkStealingFiberMigrationPreservesStackAndScratchArena)
{
    ASSERT_TRUE(gp::memory::FiberScratchArena::initializePool(4096u, 32u));
    LifecycleTracker::reset();

    CrossThreadMigrationState migrationState{};

    FiberDesc desc{};
    desc.name = "MigratingJobFiber";
    desc.id = 77u;
    desc.entry = &migratingFiberEntry;
    desc.userData = &migrationState;
    desc.stackTier = FiberStackTier::Standard64KiB;

    ASSERT_EQ(migrationState.migratingFiber.create(desc), FiberResult::Success);

    gp::platform::Thread workerA{};
    gp::platform::Thread workerB{};

    gp::platform::ThreadDesc descA{};
    descA.name = "GP_FiberWorkerA";
    descA.entry = &workerThreadAEntry;
    descA.userData = &migrationState;

    gp::platform::ThreadDesc descB{};
    descB.name = "GP_FiberWorkerB";
    descB.entry = &workerThreadBEntry;
    descB.userData = &migrationState;

    ASSERT_EQ(workerA.start(descA), gp::platform::ThreadResult::Success);
    ASSERT_EQ(workerB.start(descB), gp::platform::ThreadResult::Success);

    gp::UInt32 exitCodeA = 1u;
    gp::UInt32 exitCodeB = 1u;
    EXPECT_EQ(workerA.join(&exitCodeA), gp::platform::ThreadResult::Success);
    EXPECT_EQ(workerB.join(&exitCodeB), gp::platform::ThreadResult::Success);
    EXPECT_EQ(exitCodeA, 0u);
    EXPECT_EQ(exitCodeB, 0u);

    EXPECT_EQ(migrationState.phase.load(gp::MemoryOrder::Acquire), 2u);
    EXPECT_NE(migrationState.threadIdAtStart, 0ULL);
    EXPECT_NE(migrationState.threadIdAfterResume, 0ULL);
    EXPECT_NE(migrationState.threadIdAtStart, migrationState.threadIdAfterResume);
    EXPECT_TRUE(migrationState.scratchVerifiedAfterMigration);

    EXPECT_EQ(LifecycleTracker::s_constructCount.load(gp::MemoryOrder::Acquire), 32);
    EXPECT_EQ(LifecycleTracker::s_destructCount.load(gp::MemoryOrder::Acquire), 32);
    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);

    EXPECT_EQ(migrationState.migratingFiber.destroy(), FiberResult::Success);
    gp::memory::FiberScratchArena::shutdownPool();
}

namespace
{

struct ErrorAndMoveInspectState
{
    Fiber* anchor{ nullptr };
    Fiber* self{ nullptr };
    FiberResult selfDestroyResult{ FiberResult::Success };
    FiberResult anchorRestoreWhileInChildResult{ FiberResult::Success };
    FiberResult rebindWhileRunningResult{ FiberResult::Success };
    FiberResult selfSwitchResult{ FiberResult::Success };
};

static void errorInspectFiberTask(void* const rawData) noexcept
{
    ErrorAndMoveInspectState* const state = static_cast<ErrorAndMoveInspectState*>(rawData);
    EXPECT_TRUE(state->self->isRunning());
    EXPECT_TRUE(state->self->hasBoundTask());
    EXPECT_EQ(state->self->getUserData(), state);

    state->selfDestroyResult = state->self->destroy();
    state->anchorRestoreWhileInChildResult = state->anchor->restoreCurrentThread();
    state->rebindWhileRunningResult = state->self->bindTask(&errorInspectFiberTask, state);
    state->selfSwitchResult = state->self->switchTo(*state->self);

    // Yield mid-task so the caller can test Suspended-state invariants and move semantics
    EXPECT_EQ(state->self->yieldToReturnFiber(), FiberResult::Success);
}

struct PersistentScratchTaskState
{
    gp::UInt32* persistentBuffer{ nullptr };
    gp::UInt32 taskPass{ 0u };
    bool secondPassVerified{ false };
};

static void persistentScratchTaskEntry(void* const rawData) noexcept
{
    PersistentScratchTaskState* const state = static_cast<PersistentScratchTaskState*>(rawData);
    gp::memory::FiberScratchAllocator scratch{};

    ++state->taskPass;
    if (state->taskPass == 1u)
    {
        state->persistentBuffer = static_cast<gp::UInt32*>(scratch.allocate(8u * sizeof(gp::UInt32), 16u));
        ASSERT_NE(state->persistentBuffer, nullptr);
        for (gp::UInt32 i = 0u; i < 8u; ++i)
        {
            state->persistentBuffer[i] = 0x1000u + i;
        }
    }
    else if (state->taskPass == 2u)
    {
        ASSERT_NE(state->persistentBuffer, nullptr);
        bool matches = true;
        for (gp::UInt32 i = 0u; i < 8u; ++i)
        {
            if (state->persistentBuffer[i] != (0x1000u + i))
            {
                matches = false;
            }
        }
        state->secondPassVerified = matches;
    }
}

struct RingFiberState
{
    static constexpr gp::UInt32 kRingSize = 4u;
    Fiber* anchor{ nullptr };
    Fiber fibers[kRingSize]{};
    gp::UInt32 index[kRingSize]{ 0u, 1u, 2u, 3u };
    gp::Float64 results[kRingSize]{ 0.0, 0.0, 0.0, 0.0 };
    gp::UInt32 visitCount[kRingSize]{ 0u, 0u, 0u, 0u };
};

struct RingFiberNodeArg
{
    RingFiberState* ring{ nullptr };
    gp::UInt32 slot{ 0u };
};

static void ringFiberEntry(void* const rawData) noexcept
{
    const RingFiberNodeArg* const arg = static_cast<const RingFiberNodeArg*>(rawData);
    RingFiberState* const ring = arg->ring;
    const gp::UInt32 idx = arg->slot;
    const gp::UInt32 nextIdx = (idx + 1u) % RingFiberState::kRingSize;

    gp::memory::FiberScratchAllocator scratch{};
    gp::memory::ScopedFiberScratchMark mark{};

    gp::UInt32* const scratchSlot = static_cast<gp::UInt32*>(scratch.allocate(64u * sizeof(gp::UInt32), 16u));
    ASSERT_NE(scratchSlot, nullptr);
    for (gp::UInt32 i = 0u; i < 64u; ++i)
    {
        scratchSlot[i] = (idx + 1u) * 100u + i;
    }

    gp::Float64 localAccumulator = static_cast<gp::Float64>(idx + 1u) * 10.5;
    ++ring->visitCount[idx];

    // First hop around the ring: F0 -> F1 -> F2 -> F3 -> F0
    ASSERT_EQ(ring->fibers[idx].switchTo(ring->fibers[nextIdx]), FiberResult::Success);

    // Verify stack local and scratch buffer survived full ring traversal
    for (gp::UInt32 i = 0u; i < 64u; ++i)
    {
        EXPECT_EQ(scratchSlot[i], (idx + 1u) * 100u + i);
    }

    localAccumulator += static_cast<gp::Float64>(scratchSlot[idx]);
    ring->results[idx] = localAccumulator;
    ++ring->visitCount[idx];

    // If not the last fiber in the ring, resume the next fiber so it can finish too
    if (idx + 1u < RingFiberState::kRingSize)
    {
        ASSERT_EQ(ring->fibers[idx].switchTo(ring->fibers[nextIdx]), FiberResult::Success);
    }
}

struct MultiHopBounceState
{
    static constexpr gp::UInt32 kTotalBounces = 24u;
    Fiber bouncingFiber{};
    Fiber* activeReturnAnchor{ nullptr };
    gp::thread::Atomic<gp::UInt32> turn{ 0u };   // 0 = ThreadA, 1 = ThreadB, 2 = Done
    gp::UInt32 completedBounces{ 0u };
    bool allScratchChecksPassed{ true };
};

static void multiHopBounceFiberEntry(void* const rawData) noexcept
{
    MultiHopBounceState* const state = static_cast<MultiHopBounceState*>(rawData);
    gp::memory::FiberScratchAllocator scratch{};
    gp::memory::ScopedFiberScratchMark mark{};

    gp::UInt64* const bounceLedger =
        static_cast<gp::UInt64*>(scratch.allocate(MultiHopBounceState::kTotalBounces * sizeof(gp::UInt64), 16u));
    ASSERT_NE(bounceLedger, nullptr);

    for (gp::UInt32 hop = 0u; hop < MultiHopBounceState::kTotalBounces; ++hop)
    {
        bounceLedger[hop] = gp::platform::Thread::currentThreadId() ^ static_cast<gp::UInt64>(hop * 0x9E3779B9u);
        state->completedBounces = hop + 1u;

        // Verify all previous hops recorded in the fiber's scratch arena
        for (gp::UInt32 prev = 0u; prev <= hop; ++prev)
        {
            if (bounceLedger[prev] == 0ULL)
            {
                state->allScratchChecksPassed = false;
            }
        }

        if (hop + 1u < MultiHopBounceState::kTotalBounces)
        {
            ASSERT_NE(state->activeReturnAnchor, nullptr);
            ASSERT_EQ(state->bouncingFiber.switchTo(*state->activeReturnAnchor), FiberResult::Success);
        }
    }

    state->bouncingFiber.setReturnFiber(state->activeReturnAnchor);
}

static gp::UInt32 bounceWorkerA(void* const rawData) noexcept
{
    MultiHopBounceState* const state = static_cast<MultiHopBounceState*>(rawData);
    ScopedThreadFiber anchorA(2001u, "BounceWorkerA");
    if (!anchorA.isValid())
    {
        state->turn.store(99u, gp::MemoryOrder::Release);
        return 1u;
    }

    for (;;)
    {
        const gp::UInt32 t = state->turn.load(gp::MemoryOrder::Acquire);
        if (t == 2u || t == 99u)
        {
            break;
        }
        if (t != 0u)
        {
            gp::platform::Thread::spinWait();
            continue;
        }

        state->activeReturnAnchor = &anchorA.getFiber();
        state->bouncingFiber.setReturnFiber(&anchorA.getFiber());
        EXPECT_EQ(anchorA.getFiber().switchTo(state->bouncingFiber), FiberResult::Success);

        if (state->bouncingFiber.isCompleted())
        {
            state->turn.store(2u, gp::MemoryOrder::Release);
            break;
        }

        state->turn.store(1u, gp::MemoryOrder::Release);
    }

    return 0u;
}

static gp::UInt32 bounceWorkerB(void* const rawData) noexcept
{
    MultiHopBounceState* const state = static_cast<MultiHopBounceState*>(rawData);
    ScopedThreadFiber anchorB(2002u, "BounceWorkerB");
    if (!anchorB.isValid())
    {
        state->turn.store(99u, gp::MemoryOrder::Release);
        return 1u;
    }

    for (;;)
    {
        const gp::UInt32 t = state->turn.load(gp::MemoryOrder::Acquire);
        if (t == 2u || t == 99u)
        {
            break;
        }
        if (t != 1u)
        {
            gp::platform::Thread::spinWait();
            continue;
        }

        state->activeReturnAnchor = &anchorB.getFiber();
        state->bouncingFiber.setReturnFiber(&anchorB.getFiber());
        EXPECT_EQ(anchorB.getFiber().switchTo(state->bouncingFiber), FiberResult::Success);

        if (state->bouncingFiber.isCompleted())
        {
            state->turn.store(2u, gp::MemoryOrder::Release);
            break;
        }

        state->turn.store(0u, gp::MemoryOrder::Release);
    }

    return 0u;
}

}   // namespace

TEST(FiberTest, DefaultStateErrorPathsAndMoveSemantics)
{
    Fiber uninitFiber{};
    EXPECT_FALSE(uninitFiber.isValid());
    EXPECT_FALSE(uninitFiber.isThreadAnchor());
    EXPECT_FALSE(uninitFiber.isIdle());
    EXPECT_FALSE(uninitFiber.isReady());
    EXPECT_FALSE(uninitFiber.isRunning());
    EXPECT_FALSE(uninitFiber.isSuspended());
    EXPECT_FALSE(uninitFiber.isCompleted());
    EXPECT_FALSE(uninitFiber.hasBoundTask());
    EXPECT_FALSE(uninitFiber.isCurrentlyExecuting());
    EXPECT_EQ(uninitFiber.getState(), FiberState::Uninitialized);
    EXPECT_EQ(uninitFiber.getId(), kInvalidFiberId);
    EXPECT_EQ(uninitFiber.getUserData(), nullptr);
    EXPECT_EQ(uninitFiber.getReturnFiber(), nullptr);
    EXPECT_EQ(uninitFiber.getCompletedTaskCount(), 0ULL);
    EXPECT_FALSE(Fiber::isExecutingInFiber());
    EXPECT_FALSE(Fiber::isExecutingInChildFiber());
    EXPECT_EQ(Fiber::getCurrentFiber(), nullptr);
    EXPECT_EQ(Fiber::getCurrentFiberId(), kInvalidFiberId);

    // Uninitialized error paths
    EXPECT_EQ(uninitFiber.destroy(), FiberResult::NotInitialized);
    EXPECT_EQ(uninitFiber.restoreCurrentThread(), FiberResult::NotInitialized);
    EXPECT_EQ(uninitFiber.bindTask(&errorInspectFiberTask, nullptr), FiberResult::NotInitialized);
    EXPECT_EQ(uninitFiber.switchTo(uninitFiber), FiberResult::NotInitialized);
    EXPECT_EQ(uninitFiber.yieldToReturnFiber(), FiberResult::InvalidArguments);
    EXPECT_EQ(uninitFiber.convertCurrentThread(kInvalidFiberId), FiberResult::InvalidArguments);

    // Invalid creation descriptors
    FiberDesc badDesc{};
    badDesc.id = kInvalidFiberId;
    EXPECT_EQ(uninitFiber.create(badDesc), FiberResult::InvalidArguments);

    badDesc.id = 1u;
    badDesc.stackTier = FiberStackTier::Custom;
    badDesc.customStackSizeBytes = gp::platform::kMaxFiberStackSize + 4096u;
    EXPECT_EQ(uninitFiber.create(badDesc), FiberResult::InvalidArguments);

    // Convert current thread and test move semantics of an active ThreadAnchor Fiber
    Fiber rawAnchor{};
    ASSERT_EQ(rawAnchor.convertCurrentThread(500u, "MovableAnchor"), FiberResult::Success);
    EXPECT_TRUE(rawAnchor.isThreadAnchor());
    EXPECT_EQ(Fiber::getCurrentFiber(), &rawAnchor);

    Fiber movedAnchor(std::move(rawAnchor));
    EXPECT_FALSE(rawAnchor.isValid());
    EXPECT_TRUE(movedAnchor.isValid());
    EXPECT_TRUE(movedAnchor.isThreadAnchor());
    EXPECT_EQ(Fiber::getCurrentFiber(), &movedAnchor);

    // Duplicate thread conversion and invalid bindTask/restoreCurrentThread on anchor
    Fiber duplicateAnchor{};
    EXPECT_EQ(duplicateAnchor.convertCurrentThread(501u), FiberResult::AlreadyThreadFiber);
    EXPECT_EQ(movedAnchor.bindTask(&errorInspectFiberTask, nullptr), FiberResult::InvalidArguments);
    EXPECT_EQ(movedAnchor.create(FiberDesc{}), FiberResult::AlreadyInitialized);
    EXPECT_EQ(movedAnchor.convertCurrentThread(502u), FiberResult::AlreadyInitialized);

    ErrorAndMoveInspectState inspectState{};
    inspectState.anchor = &movedAnchor;

    Fiber childFiber{};
    FiberDesc childDesc{};
    childDesc.name = "ErrorInspectChild";
    childDesc.id = 12u;
    childDesc.entry = &errorInspectFiberTask;
    childDesc.userData = &inspectState;
    childDesc.returnFiber = &movedAnchor;

    ASSERT_EQ(childFiber.create(childDesc), FiberResult::Success);
    EXPECT_TRUE(childFiber.isReady());
    EXPECT_TRUE(childFiber.hasBoundTask());
    EXPECT_EQ(childFiber.getUserData(), &inspectState);
    EXPECT_EQ(childFiber.getReturnFiber(), &movedAnchor);
    EXPECT_EQ(childFiber.create(childDesc), FiberResult::AlreadyInitialized);
    EXPECT_EQ(childFiber.restoreCurrentThread(), FiberResult::NotAThreadFiber);
    EXPECT_EQ(childFiber.bindTask(nullptr, nullptr), FiberResult::InvalidArguments);

    // Calling switchTo from a non-executing fiber must return CallerNotInFiber
    EXPECT_EQ(childFiber.switchTo(movedAnchor), FiberResult::CallerNotInFiber);

    // Move-construct and move-assign childFiber in Ready state before starting it
    Fiber movedChild(std::move(childFiber));
    EXPECT_FALSE(childFiber.isValid());
    EXPECT_EQ(childFiber.getState(), FiberState::Uninitialized);
    EXPECT_TRUE(movedChild.isReady());

    Fiber assignedChild{};
    assignedChild = std::move(movedChild);
    EXPECT_FALSE(movedChild.isValid());
    EXPECT_TRUE(assignedChild.isReady());
    inspectState.self = &assignedChild;

    // Enter assignedChild -> runs until mid-task yieldToReturnFiber()
    ASSERT_EQ(movedAnchor.switchTo(assignedChild), FiberResult::Success);
    EXPECT_TRUE(assignedChild.isSuspended());
    EXPECT_EQ(inspectState.selfDestroyResult, FiberResult::CannotDestroyActiveFiber);
    EXPECT_EQ(inspectState.anchorRestoreWhileInChildResult, FiberResult::CannotDestroyActiveFiber);
    EXPECT_EQ(inspectState.rebindWhileRunningResult, FiberResult::BusyRunningOrSuspended);
    EXPECT_EQ(inspectState.selfSwitchResult, FiberResult::SelfSwitchError);

    // Calling bindTask on a Suspended fiber must fail with BusyRunningOrSuspended
    EXPECT_EQ(assignedChild.bindTask(&errorInspectFiberTask, &inspectState), FiberResult::BusyRunningOrSuspended);

    // Resume assignedChild to completion
    ASSERT_EQ(movedAnchor.switchTo(assignedChild), FiberResult::Success);
    EXPECT_TRUE(assignedChild.isCompleted());
    EXPECT_FALSE(assignedChild.hasBoundTask());
    EXPECT_EQ(assignedChild.getCompletedTaskCount(), 1ULL);

    EXPECT_EQ(assignedChild.destroy(), FiberResult::Success);
    EXPECT_EQ(movedAnchor.restoreCurrentThread(), FiberResult::Success);
    EXPECT_FALSE(Fiber::isExecutingInFiber());
}

TEST(FiberTest, CustomPagePoolAndManualScratchRetentionAcrossTasks)
{
    gp::memory::PagePool customPool{};
    ASSERT_TRUE(customPool.initialize(4096u, 8u, nullptr, "CustomTestFiberPagePool"));

    ScopedThreadFiber anchorGuard(600u, "CustomPoolAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    PersistentScratchTaskState state{};

    Fiber customFiber{};
    FiberDesc desc{};
    desc.name = "ManualScratchFiber";
    desc.id = 25u;
    desc.entry = &persistentScratchTaskEntry;
    desc.userData = &state;
    desc.returnFiber = &anchor;
    desc.scratchPagePool = &customPool;
    desc.resetScratchOnTaskCompletion = false;

    ASSERT_EQ(customFiber.create(desc), FiberResult::Success);
    EXPECT_FALSE(customFiber.shouldResetScratchOnCompletion());
    EXPECT_EQ(customFiber.getScratchArena().getPagePool(), &customPool);

    // Run Pass 1: allocates from customPool and retains leased page on completion because resetScratchOnTaskCompletion
    // is false
    ASSERT_EQ(anchor.switchTo(customFiber), FiberResult::Success);
    EXPECT_TRUE(customFiber.isCompleted());
    EXPECT_EQ(customPool.getLeasedPageCount(), 1u);
    EXPECT_EQ(customFiber.getScratchArena().getLeasedPageCount(), 1u);

    // Enable automatic reset for Pass 2 and re-arm the fiber
    customFiber.setResetScratchOnCompletion(true);
    EXPECT_TRUE(customFiber.shouldResetScratchOnCompletion());
    ASSERT_EQ(customFiber.bindTask(&persistentScratchTaskEntry, &state, &anchor), FiberResult::Success);

    // Run Pass 2: verifies Pass 1's data is still intact in the retained page, then auto-resets on completion
    ASSERT_EQ(anchor.switchTo(customFiber), FiberResult::Success);
    EXPECT_TRUE(customFiber.isCompleted());
    EXPECT_TRUE(state.secondPassVerified);
    EXPECT_EQ(customFiber.getScratchArena().getLeasedPageCount(), 0u);
    EXPECT_EQ(customPool.getLeasedPageCount(), 0u);

    EXPECT_EQ(customFiber.destroy(), FiberResult::Success);
    customPool.shutdown();
}

TEST(FiberTest, MultiFiberRingSwitchPreservesIndependentStacksAndFloatingPointState)
{
    ASSERT_TRUE(gp::memory::FiberScratchArena::initializePool(4096u, 16u));

    ScopedThreadFiber anchorGuard(700u, "RingAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    RingFiberState ring{};
    ring.anchor = &anchor;
    RingFiberNodeArg args[RingFiberState::kRingSize]{};

    for (gp::UInt32 i = 0u; i < RingFiberState::kRingSize; ++i)
    {
        args[i].ring = &ring;
        args[i].slot = i;

        FiberDesc desc{};
        desc.name = "RingFiber";
        desc.id = 70u + i;
        desc.entry = &ringFiberEntry;
        desc.userData = &args[i];
        desc.returnFiber = &anchor;
        ASSERT_EQ(ring.fibers[i].create(desc), FiberResult::Success);
    }

    // Start ring at F0:
    // F0 -> F1 -> F2 -> F3 -> F0 (finishes, resumes F1) -> F1 (finishes, resumes F2) -> F2 (finishes, resumes F3)
    // -> F3 (finishes, returns to anchor) -> anchor resumes F2/F1/F0 to let their trampolines return to anchor.
    for (gp::UInt32 pass = 0u; pass < RingFiberState::kRingSize; ++pass)
    {
        for (gp::UInt32 i = 0u; i < RingFiberState::kRingSize; ++i)
        {
            if (ring.fibers[i].isReady() || ring.fibers[i].isSuspended())
            {
                ASSERT_EQ(anchor.switchTo(ring.fibers[i]), FiberResult::Success);
            }
        }
    }

    for (gp::UInt32 i = 0u; i < RingFiberState::kRingSize; ++i)
    {
        EXPECT_TRUE(ring.fibers[i].isCompleted());
        EXPECT_EQ(ring.visitCount[i], 2u);
        const gp::Float64 expected =
            (static_cast<gp::Float64>(i + 1u) * 10.5) + static_cast<gp::Float64>((i + 1u) * 100u + i);
        EXPECT_DOUBLE_EQ(ring.results[i], expected);
        EXPECT_EQ(ring.fibers[i].destroy(), FiberResult::Success);
    }

    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);
    gp::memory::FiberScratchArena::shutdownPool();
}

TEST(FiberTest, MultiWorkerCrossThreadPingPongMigrationStress)
{
    ASSERT_TRUE(gp::memory::FiberScratchArena::initializePool(4096u, 32u));

    MultiHopBounceState bounceState{};
    FiberDesc desc{};
    desc.name = "MultiHopBouncingFiber";
    desc.id = 88u;
    desc.entry = &multiHopBounceFiberEntry;
    desc.userData = &bounceState;
    ASSERT_EQ(bounceState.bouncingFiber.create(desc), FiberResult::Success);

    gp::platform::Thread workerA{};
    gp::platform::Thread workerB{};

    gp::platform::ThreadDesc descA{};
    descA.name = "GP_BounceWorkerA";
    descA.entry = &bounceWorkerA;
    descA.userData = &bounceState;

    gp::platform::ThreadDesc descB{};
    descB.name = "GP_BounceWorkerB";
    descB.entry = &bounceWorkerB;
    descB.userData = &bounceState;

    ASSERT_EQ(workerA.start(descA), gp::platform::ThreadResult::Success);
    ASSERT_EQ(workerB.start(descB), gp::platform::ThreadResult::Success);

    gp::UInt32 exitA = 1u;
    gp::UInt32 exitB = 1u;
    EXPECT_EQ(workerA.join(&exitA), gp::platform::ThreadResult::Success);
    EXPECT_EQ(workerB.join(&exitB), gp::platform::ThreadResult::Success);
    EXPECT_EQ(exitA, 0u);
    EXPECT_EQ(exitB, 0u);

    EXPECT_EQ(bounceState.turn.load(gp::MemoryOrder::Acquire), 2u);
    EXPECT_EQ(bounceState.completedBounces, MultiHopBounceState::kTotalBounces);
    EXPECT_TRUE(bounceState.allScratchChecksPassed);
    EXPECT_TRUE(bounceState.bouncingFiber.isCompleted());
    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);

    EXPECT_EQ(bounceState.bouncingFiber.destroy(), FiberResult::Success);
    gp::memory::FiberScratchArena::shutdownPool();
}

}   // namespace gp::thread::tests
