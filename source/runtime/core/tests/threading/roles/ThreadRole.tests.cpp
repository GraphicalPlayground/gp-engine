// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/roles/ThreadRole.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "threading/roles/ThreadRoleTypes.hpp"
#include <cstring>
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace
{

/// @brief Out-of-range value equal to the `Count` sentinel.
constexpr NamedThread kSentinelNamedThread = NamedThread::Count;

/// @brief Out-of-range value far past the sentinel, as a corrupted byte would produce.
constexpr NamedThread kGarbageNamedThread = static_cast<NamedThread>(0xFFu);

// Enum layout. The dispatcher and the ownership table index arrays with these values.
static_assert(static_cast<gp::UInt8>(NamedThread::Game) == 0u, "Game must be index 0.");
static_assert(static_cast<gp::UInt8>(NamedThread::Render) == 1u, "Render must be index 1.");
static_assert(static_cast<gp::UInt8>(NamedThread::RHI) == 2u, "RHI must be index 2.");
static_assert(kNamedThreadCount == 3u, "There must be three named threads.");
static_assert(sizeof(ThreadRole) == 1u, "ThreadRole must fit in one byte of TLS.");

// Role masks.
static_assert(kExclusiveThreadRoles == (ThreadRole::Game | ThreadRole::Render | ThreadRole::RHI), "Exclusive mask.");
static_assert(kScopedThreadRoles == ThreadRole::ParallelRender, "Scoped mask.");
static_assert((kExclusiveThreadRoles & kScopedThreadRoles) == ThreadRole::None, "Masks must not overlap.");

// isValidNamedThread.
static_assert(isValidNamedThread(NamedThread::Game), "Game is valid.");
static_assert(isValidNamedThread(NamedThread::Render), "Render is valid.");
static_assert(isValidNamedThread(NamedThread::RHI), "RHI is valid.");
static_assert(!isValidNamedThread(kSentinelNamedThread), "Count is not a thread.");
static_assert(!isValidNamedThread(kGarbageNamedThread), "0xFF is not a thread.");

// toThreadRole.
static_assert(toThreadRole(NamedThread::Game) == ThreadRole::Game, "Game maps to the Game bit.");
static_assert(toThreadRole(NamedThread::Render) == ThreadRole::Render, "Render maps to the Render bit.");
static_assert(toThreadRole(NamedThread::RHI) == ThreadRole::RHI, "RHI maps to the RHI bit.");
static_assert(toThreadRole(kSentinelNamedThread) == ThreadRole::None, "Count maps to no role.");
static_assert(toThreadRole(kGarbageNamedThread) == ThreadRole::None, "Garbage maps to no role.");

// hasAnyThreadRole uses any-of semantics and never matches an empty query.
static_assert(hasAnyThreadRole(ThreadRole::Game, ThreadRole::Game), "Exact match.");
static_assert(hasAnyThreadRole(ThreadRole::Render, ThreadRole::Game | ThreadRole::Render), "Any-of match.");
static_assert(!hasAnyThreadRole(ThreadRole::Game, ThreadRole::Render), "Disjoint masks.");
static_assert(!hasAnyThreadRole(ThreadRole::Game, ThreadRole::None), "Empty query never matches.");
static_assert(!hasAnyThreadRole(ThreadRole::None, kExclusiveThreadRoles | kScopedThreadRoles), "Empty mask.");

// NamedThreadTask.
static_assert(!NamedThreadTask{}.isValid(), "A default task has no entry.");
static_assert(kUnclaimedThreadId == 0ULL, "The unclaimed marker is zero.");
static_assert(kPumpAllTasks == 0xFFFFFFFFu, "kPumpAllTasks is the largest UInt32.");

/// @brief Starts a platform thread on `entry`, joins it, and reports whether it ran and returned `0`.
/// @param[in] entry Thread entry point.
/// @param[in] userData Context forwarded to `entry`.
/// @return True if the thread started, joined, and exited with code `0`.
[[nodiscard]] bool runOnWorkerThread(const gp::platform::ThreadEntryFunction entry, void* const userData) noexcept
{
    gp::platform::Thread worker{};
    gp::platform::ThreadDesc desc{};
    desc.name = "GP_ThreadRoleTestWorker";
    desc.entry = entry;
    desc.userData = userData;
    if (worker.start(desc) != gp::platform::ThreadResult::Success)
    {
        return false;
    }

    gp::UInt32 exitCode = 1u;
    return worker.join(&exitCode) == gp::platform::ThreadResult::Success && exitCode == 0u;
}

/// @brief Checks that the calling thread holds nothing and that no named thread is claimed.
void expectCleanRegistry()
{
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::None);
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(NamedThread::Game));
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(NamedThread::Render));
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(NamedThread::RHI));
}

/// @brief What a worker thread saw when it started, and what happened when it tried to claim `target`.
struct WorkerProbe
{
    NamedThread target{ NamedThread::Count };
    ThreadRole observedRoles{ ThreadRole::Game };
    bool observedGame{ true };
    bool observedRender{ true };
    bool observedRHI{ true };
    bool observedParallelRender{ true };
    gp::platform::ThreadId selfId{ kUnclaimedThreadId };
    ThreadRoleResult attachResult{ ThreadRoleResult::InvalidArguments };
    bool holdsTargetAfterAttach{ false };
    gp::platform::ThreadId ownerSeenAfterAttach{ kUnclaimedThreadId };
    ThreadRoleResult detachResult{ ThreadRoleResult::InvalidArguments };
};

/// @brief Worker entry that fills a `WorkerProbe`.
gp::UInt32 workerProbeEntry(void* rawProbe)
{
    WorkerProbe& probe = *static_cast<WorkerProbe*>(rawProbe);

    probe.observedRoles = ThreadRoleRegistry::getCurrentRoles();
    probe.observedGame = isInGameThread();
    probe.observedRender = isInRenderThread();
    probe.observedRHI = isInRHIThread();
    probe.observedParallelRender = isInParallelRenderThread();
    probe.selfId = gp::platform::Thread::currentThreadId();

    probe.attachResult = ThreadRoleRegistry::attachCurrentThread(probe.target);
    probe.holdsTargetAfterAttach = isInThreadRole(probe.target);
    probe.ownerSeenAfterAttach = ThreadRoleRegistry::getOwnerThreadId(probe.target);
    if (probe.attachResult == ThreadRoleResult::Success)
    {
        probe.detachResult = ThreadRoleRegistry::detachCurrentThread(probe.target);
    }
    return 0u;
}

/// @brief Handshake for a worker that claims a named thread and holds it while the main thread looks.
struct HoldingWorkerContext
{
    NamedThread target{ NamedThread::RHI };
    gp::platform::ThreadId workerId{ kUnclaimedThreadId };
    ThreadRoleResult attachResult{ ThreadRoleResult::InvalidArguments };
    ThreadRoleResult detachResult{ ThreadRoleResult::InvalidArguments };
    Atomic<gp::UInt32> isHolding{ 0u };
    Atomic<gp::UInt32> mayRelease{ 0u };
};

/// @brief Worker entry that attaches, signals, waits for permission, then detaches.
gp::UInt32 holdingWorkerEntry(void* rawContext)
{
    HoldingWorkerContext& context = *static_cast<HoldingWorkerContext*>(rawContext);
    context.workerId = gp::platform::Thread::currentThreadId();
    context.attachResult = ThreadRoleRegistry::attachCurrentThread(context.target);
    context.isHolding.store(1u, gp::MemoryOrder::Release);

    while (context.mayRelease.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    if (context.attachResult == ThreadRoleResult::Success)
    {
        context.detachResult = ThreadRoleRegistry::detachCurrentThread(context.target);
    }
    return 0u;
}

/// @brief Shared state for the attach race between many threads.
struct AttachRaceContext
{
    static constexpr gp::UInt32 kThreadCount = 8u;

    Atomic<gp::UInt32> readyCount{ 0u };
    Atomic<gp::UInt32> attemptedCount{ 0u };
    Atomic<gp::UInt32> startSignal{ 0u };
    Atomic<gp::UInt32> successCount{ 0u };
    Atomic<gp::UInt32> claimedByOtherCount{ 0u };
    Atomic<gp::UInt32> unexpectedResultCount{ 0u };
    Atomic<gp::UInt32> failedDetachCount{ 0u };
};

/// @brief Racer entry: waits for the start signal, tries to claim `Game`, and releases it only after every racer has
///        tried, so exactly one success is possible.
gp::UInt32 attachRaceEntry(void* rawContext)
{
    AttachRaceContext& context = *static_cast<AttachRaceContext*>(rawContext);
    (void)context.readyCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    while (context.startSignal.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    const ThreadRoleResult result = ThreadRoleRegistry::attachCurrentThread(NamedThread::Game);
    (void)context.attemptedCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);

    if (result == ThreadRoleResult::Success)
    {
        (void)context.successCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        while (context.attemptedCount.load(gp::MemoryOrder::Acquire) < AttachRaceContext::kThreadCount)
        {
            gp::platform::Thread::spinWait();
        }
        if (ThreadRoleRegistry::detachCurrentThread(NamedThread::Game) != ThreadRoleResult::Success)
        {
            (void)context.failedDetachCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        }
    }
    else if (result == ThreadRoleResult::RoleClaimedByOtherThread)
    {
        (void)context.claimedByOtherCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }
    else
    {
        (void)context.unexpectedResultCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }
    return 0u;
}

}   // namespace

TEST(ThreadRoleTypesTest, GetNamedThreadNameReturnsStableNames)
{
    EXPECT_EQ(std::strcmp(getNamedThreadName(NamedThread::Game), "Game"), 0);
    EXPECT_EQ(std::strcmp(getNamedThreadName(NamedThread::Render), "Render"), 0);
    EXPECT_EQ(std::strcmp(getNamedThreadName(NamedThread::RHI), "RHI"), 0);
    EXPECT_EQ(std::strcmp(getNamedThreadName(kSentinelNamedThread), "Invalid"), 0);
    EXPECT_EQ(std::strcmp(getNamedThreadName(kGarbageNamedThread), "Invalid"), 0);
}

TEST(ThreadRoleTypesTest, NamedThreadTaskIsValidRequiresEntry)
{
    NamedThreadTask task{};
    EXPECT_FALSE(task.isValid());

    task.entry = [](void*)
    {
    };
    EXPECT_TRUE(task.isValid());
}

TEST(ThreadRoleRegistryTest, FreshThreadHasNoRoles)
{
    expectCleanRegistry();
    EXPECT_FALSE(isInGameThread());
    EXPECT_FALSE(isInRenderThread());
    EXPECT_FALSE(isInRHIThread());
    EXPECT_FALSE(isInParallelRenderThread());
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::Game), kUnclaimedThreadId);
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::Render), kUnclaimedThreadId);
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::RHI), kUnclaimedThreadId);
}

TEST(ThreadRoleRegistryTest, AttachAndDetachGameThread)
{
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);

    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::Game);
    EXPECT_TRUE(isInGameThread());
    EXPECT_FALSE(isInRenderThread());
    EXPECT_FALSE(isInRHIThread());
    EXPECT_FALSE(isInParallelRenderThread());
    EXPECT_TRUE(ThreadRoleRegistry::isAttached(NamedThread::Game));
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(NamedThread::Render));
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::Game), gp::platform::Thread::currentThreadId());

    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);
    expectCleanRegistry();
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::Game), kUnclaimedThreadId);
}

TEST(ThreadRoleRegistryTest, AttachAndDetachRenderAndRHIThreads)
{
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    EXPECT_TRUE(isInRenderThread());
    EXPECT_TRUE(isInParallelRenderThread());
    EXPECT_FALSE(isInRHIThread());
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    EXPECT_FALSE(isInRenderThread());
    EXPECT_FALSE(isInParallelRenderThread());

    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::RHI), ThreadRoleResult::Success);
    EXPECT_TRUE(isInRHIThread());
    EXPECT_FALSE(isInRenderThread());
    EXPECT_FALSE(isInParallelRenderThread());
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::RHI), ThreadRoleResult::Success);

    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, AttachTwiceReportsAlreadyAttached)
{
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    EXPECT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Render), ThreadRoleResult::AlreadyAttached);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::Render);

    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, DetachWithoutAttachReportsNotAttached)
{
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Game), ThreadRoleResult::NotAttached);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Render), ThreadRoleResult::NotAttached);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::RHI), ThreadRoleResult::NotAttached);

    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Game), ThreadRoleResult::NotAttached);

    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, InvalidNamedThreadIsRejectedEverywhere)
{
    EXPECT_EQ(ThreadRoleRegistry::attachCurrentThread(kSentinelNamedThread), ThreadRoleResult::InvalidArguments);
    EXPECT_EQ(ThreadRoleRegistry::attachCurrentThread(kGarbageNamedThread), ThreadRoleResult::InvalidArguments);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(kSentinelNamedThread), ThreadRoleResult::InvalidArguments);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(kGarbageNamedThread), ThreadRoleResult::InvalidArguments);
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(kSentinelNamedThread));
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(kGarbageNamedThread));
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(kSentinelNamedThread), kUnclaimedThreadId);
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(kGarbageNamedThread), kUnclaimedThreadId);
    EXPECT_FALSE(isInThreadRole(kSentinelNamedThread));
    EXPECT_FALSE(isInThreadRole(kGarbageNamedThread));

    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, OneThreadCanHoldEveryNamedRole)
{
    // Single-threaded configuration: the main thread plays game, render, and RHI at once.
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::RHI), ThreadRoleResult::Success);

    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), kExclusiveThreadRoles);
    EXPECT_TRUE(isInGameThread());
    EXPECT_TRUE(isInRenderThread());
    EXPECT_TRUE(isInRHIThread());
    EXPECT_TRUE(isInParallelRenderThread());

    const gp::platform::ThreadId selfId = gp::platform::Thread::currentThreadId();
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::Game), selfId);
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::Render), selfId);
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::RHI), selfId);

    // Release out of order; each detach clears only its own bit.
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::Game | ThreadRole::RHI);
    EXPECT_FALSE(isInParallelRenderThread());
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::RHI);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::RHI), ThreadRoleResult::Success);

    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, IsInRoleUsesAnyOfSemantics)
{
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);

    EXPECT_TRUE(ThreadRoleRegistry::isInRole(ThreadRole::Render));
    EXPECT_TRUE(ThreadRoleRegistry::isInRole(ThreadRole::Game | ThreadRole::Render));
    EXPECT_FALSE(ThreadRoleRegistry::isInRole(ThreadRole::Game | ThreadRole::RHI));
    EXPECT_FALSE(ThreadRoleRegistry::isInRole(ThreadRole::None));

    EXPECT_TRUE(isInThreadRole(ThreadRole::Render | ThreadRole::ParallelRender));
    EXPECT_FALSE(isInThreadRole(ThreadRole::None));
    EXPECT_TRUE(isInThreadRole(NamedThread::Render));
    EXPECT_FALSE(isInThreadRole(NamedThread::Game));
    EXPECT_FALSE(isInThreadRole(NamedThread::RHI));

    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, PushAndPopParallelRenderRole)
{
    ThreadRole previousRoles = ThreadRole::Game;
    ASSERT_EQ(
        ThreadRoleRegistry::pushScopedRoles(ThreadRole::ParallelRender, previousRoles), ThreadRoleResult::Success
    );
    EXPECT_EQ(previousRoles, ThreadRole::None);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::ParallelRender);
    EXPECT_TRUE(isInParallelRenderThread());
    EXPECT_FALSE(isInRenderThread());
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(NamedThread::Render));

    ThreadRoleRegistry::popScopedRoles(previousRoles);
    EXPECT_FALSE(isInParallelRenderThread());
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, PushRejectsEmptyExclusiveAndUnknownBits)
{
    ThreadRole previousRoles = ThreadRole::Game;

    EXPECT_EQ(ThreadRoleRegistry::pushScopedRoles(ThreadRole::None, previousRoles), ThreadRoleResult::InvalidArguments);
    EXPECT_EQ(previousRoles, ThreadRole::None);

    EXPECT_EQ(ThreadRoleRegistry::pushScopedRoles(ThreadRole::Game, previousRoles), ThreadRoleResult::InvalidArguments);
    EXPECT_EQ(
        ThreadRoleRegistry::pushScopedRoles(ThreadRole::Game | ThreadRole::Render, previousRoles),
        ThreadRoleResult::InvalidArguments
    );
    EXPECT_EQ(
        ThreadRoleRegistry::pushScopedRoles(ThreadRole::Render | ThreadRole::ParallelRender, previousRoles),
        ThreadRoleResult::InvalidArguments
    );
    EXPECT_EQ(
        ThreadRoleRegistry::pushScopedRoles(static_cast<ThreadRole>(0x80u), previousRoles),
        ThreadRoleResult::InvalidArguments
    );

    // A rejected push must not grant anything, in particular not the exclusive role it asked for.
    EXPECT_FALSE(isInRenderThread());
    EXPECT_FALSE(isInGameThread());
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, PushReportsPreviousMaskIncludingNamedRoles)
{
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);

    ThreadRole previousRoles = ThreadRole::None;
    ASSERT_EQ(
        ThreadRoleRegistry::pushScopedRoles(ThreadRole::ParallelRender, previousRoles), ThreadRoleResult::Success
    );
    EXPECT_EQ(previousRoles, ThreadRole::Game);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::Game | ThreadRole::ParallelRender);

    ThreadRoleRegistry::popScopedRoles(previousRoles);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::Game);

    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, PopNeverChangesExclusiveRoles)
{
    // A stale snapshot taken before an attach must not revoke the named role.
    ThreadRole previousRoles = ThreadRole::Game;
    ASSERT_EQ(
        ThreadRoleRegistry::pushScopedRoles(ThreadRole::ParallelRender, previousRoles), ThreadRoleResult::Success
    );
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);

    ThreadRoleRegistry::popScopedRoles(previousRoles);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::Render);
    EXPECT_TRUE(isInRenderThread());
    EXPECT_TRUE(ThreadRoleRegistry::isAttached(NamedThread::Render));

    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);

    // A forged snapshot cannot grant a named role that was never claimed.
    ThreadRoleRegistry::popScopedRoles(kExclusiveThreadRoles | ThreadRole::ParallelRender);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::ParallelRender);
    EXPECT_FALSE(isInGameThread());
    EXPECT_FALSE(isInRenderThread());
    EXPECT_FALSE(isInRHIThread());

    ThreadRoleRegistry::popScopedRoles(ThreadRole::None);
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, RolesAreThreadLocal)
{
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);
    ThreadRole previousRoles = ThreadRole::None;
    ASSERT_EQ(
        ThreadRoleRegistry::pushScopedRoles(ThreadRole::ParallelRender, previousRoles), ThreadRoleResult::Success
    );

    WorkerProbe probe{};
    probe.target = NamedThread::RHI;
    ASSERT_TRUE(runOnWorkerThread(&workerProbeEntry, &probe));

    EXPECT_EQ(probe.observedRoles, ThreadRole::None);
    EXPECT_FALSE(probe.observedGame);
    EXPECT_FALSE(probe.observedRender);
    EXPECT_FALSE(probe.observedRHI);
    EXPECT_FALSE(probe.observedParallelRender);

    // The worker's own claim and release did not touch the main thread's mask.
    EXPECT_EQ(probe.attachResult, ThreadRoleResult::Success);
    EXPECT_EQ(probe.detachResult, ThreadRoleResult::Success);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::Game | ThreadRole::ParallelRender);

    ThreadRoleRegistry::popScopedRoles(previousRoles);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Game), ThreadRoleResult::Success);
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, OtherThreadCannotClaimOwnedRole)
{
    ASSERT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    const gp::platform::ThreadId mainId = gp::platform::Thread::currentThreadId();

    WorkerProbe probe{};
    probe.target = NamedThread::Render;
    ASSERT_TRUE(runOnWorkerThread(&workerProbeEntry, &probe));

    EXPECT_EQ(probe.attachResult, ThreadRoleResult::RoleClaimedByOtherThread);
    EXPECT_FALSE(probe.holdsTargetAfterAttach);
    EXPECT_EQ(probe.ownerSeenAfterAttach, mainId);
    EXPECT_NE(probe.selfId, mainId);

    // The failed attempt left the main thread's claim intact.
    EXPECT_TRUE(isInRenderThread());
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::Render), mainId);

    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::Render), ThreadRoleResult::Success);
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, ClaimByOtherThreadIsVisibleButNotHeld)
{
    HoldingWorkerContext context{};
    context.target = NamedThread::RHI;

    gp::platform::Thread worker{};
    gp::platform::ThreadDesc desc{};
    desc.name = "GP_ThreadRoleHoldingWorker";
    desc.entry = &holdingWorkerEntry;
    desc.userData = &context;
    ASSERT_EQ(worker.start(desc), gp::platform::ThreadResult::Success);

    while (context.isHolding.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    EXPECT_EQ(context.attachResult, ThreadRoleResult::Success);
    EXPECT_TRUE(ThreadRoleRegistry::isAttached(NamedThread::RHI));
    EXPECT_EQ(ThreadRoleRegistry::getOwnerThreadId(NamedThread::RHI), context.workerId);
    EXPECT_FALSE(isInRHIThread());
    EXPECT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::RHI), ThreadRoleResult::RoleClaimedByOtherThread);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::RHI), ThreadRoleResult::NotAttached);

    context.mayRelease.store(1u, gp::MemoryOrder::Release);
    gp::UInt32 exitCode = 1u;
    EXPECT_EQ(worker.join(&exitCode), gp::platform::ThreadResult::Success);
    EXPECT_EQ(exitCode, 0u);
    EXPECT_EQ(context.detachResult, ThreadRoleResult::Success);

    // Once released, the role can be claimed by the main thread.
    EXPECT_EQ(ThreadRoleRegistry::attachCurrentThread(NamedThread::RHI), ThreadRoleResult::Success);
    EXPECT_EQ(ThreadRoleRegistry::detachCurrentThread(NamedThread::RHI), ThreadRoleResult::Success);
    expectCleanRegistry();
}

TEST(ThreadRoleRegistryTest, ConcurrentAttachHasExactlyOneWinner)
{
    AttachRaceContext context{};
    gp::platform::Thread racers[AttachRaceContext::kThreadCount]{};

    for (gp::UInt32 index = 0u; index < AttachRaceContext::kThreadCount; ++index)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_ThreadRoleAttachRacer";
        desc.entry = &attachRaceEntry;
        desc.userData = &context;
        ASSERT_EQ(racers[index].start(desc), gp::platform::ThreadResult::Success);
    }

    while (context.readyCount.load(gp::MemoryOrder::Acquire) < AttachRaceContext::kThreadCount)
    {
        gp::platform::Thread::spinWait();
    }
    context.startSignal.store(1u, gp::MemoryOrder::Release);

    for (gp::platform::Thread& racer: racers)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(racer.join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_EQ(context.successCount.load(gp::MemoryOrder::Acquire), 1u);
    EXPECT_EQ(context.claimedByOtherCount.load(gp::MemoryOrder::Acquire), AttachRaceContext::kThreadCount - 1u);
    EXPECT_EQ(context.unexpectedResultCount.load(gp::MemoryOrder::Acquire), 0u);
    EXPECT_EQ(context.failedDetachCount.load(gp::MemoryOrder::Acquire), 0u);
    expectCleanRegistry();
}

}   // namespace gp::thread::tests
