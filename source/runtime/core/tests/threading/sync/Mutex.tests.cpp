// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/Mutex.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/Fiber.hpp"
#include "threading/sync/JobFence.hpp"
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace
{

struct NonLockableDummy
{
    gp::Int32 value{ 0 };
};

struct BasicOnlyLockableMock
{
    gp::UInt32 lockCount{ 0u };

    constexpr void lock() noexcept
    {
        ++lockCount;
    }

    constexpr void unlock() noexcept
    {
        --lockCount;
    }
};

static_assert(sizeof(Mutex) == 4u, "Mutex must be 4 bytes.");
static_assert(alignof(Mutex) == 4u, "Mutex must be 4-byte aligned.");
static_assert(
    sizeof(CacheAlignedMutex) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedMutex must match cache line size."
);
static_assert(sizeof(RecursiveMutex) == 16u, "RecursiveMutex must be 16 bytes.");
static_assert(alignof(RecursiveMutex) == 8u, "RecursiveMutex must be 8-byte aligned.");
static_assert(
    sizeof(CacheAlignedRecursiveMutex) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedRecursiveMutex must match cache line size."
);
static_assert(sizeof(SharedMutex) == 8u, "SharedMutex must be 8 bytes.");
static_assert(alignof(SharedMutex) == 4u, "SharedMutex must be 4-byte aligned.");
static_assert(
    sizeof(CacheAlignedSharedMutex) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedSharedMutex must match cache line size."
);
static_assert(sizeof(OnceFlag) == 4u, "OnceFlag must be 4 bytes.");
static_assert(alignof(OnceFlag) == 4u, "OnceFlag must be 4-byte aligned.");

static_assert(gp::concepts::IsLockable<Mutex>, "Mutex must satisfy IsLockable.");
static_assert(gp::concepts::IsLockable<CacheAlignedMutex>, "CacheAlignedMutex must satisfy IsLockable.");
static_assert(gp::concepts::IsLockable<RecursiveMutex>, "RecursiveMutex must satisfy IsLockable.");
static_assert(gp::concepts::IsLockable<CriticalSection>, "CriticalSection must satisfy IsLockable.");
static_assert(gp::concepts::IsSharedLockable<SharedMutex>, "SharedMutex must satisfy IsSharedLockable.");
static_assert(
    gp::concepts::IsSharedLockable<CacheAlignedSharedMutex>, "CacheAlignedSharedMutex must satisfy IsSharedLockable."
);
static_assert(
    gp::concepts::IsBasicLockable<BasicOnlyLockableMock>, "BasicOnlyLockableMock must satisfy IsBasicLockable."
);
static_assert(!gp::concepts::IsLockable<BasicOnlyLockableMock>, "BasicOnlyLockableMock must not satisfy IsLockable.");
static_assert(!gp::concepts::IsBasicLockable<NonLockableDummy>, "NonLockableDummy must not satisfy IsBasicLockable.");
static_assert(!gp::concepts::IsSharedLockable<Mutex>, "Mutex must not satisfy IsSharedLockable.");

constexpr bool testConstexprMutexAndGuards() noexcept
{
    Mutex mtx{};
    if (mtx.isLocked() || mtx.mayHaveWaiters())
    {
        return false;
    }

    if (!mtx.tryLock() || !mtx.isLocked() || mtx.tryLock())
    {
        return false;
    }

    {
        UniqueLock<Mutex> adoptedGuard(mtx, AdoptLock);
        if (!adoptedGuard.ownsLock() || !static_cast<bool>(adoptedGuard) || adoptedGuard.mutex() != &mtx)
        {
            return false;
        }
    }

    if (mtx.isLocked())
    {
        return false;
    }

    Mutex mtxB{};
    if (tryLockMultiple(mtx, mtxB) != -1 || !mtx.isLocked() || !mtxB.isLocked())
    {
        return false;
    }
    mtx.unlock();
    mtxB.unlock();

    SharedMutex rwMtx{};
    if (rwMtx.isLocked() || !rwMtx.tryLockShared() || !rwMtx.isLockedShared())
    {
        return false;
    }

    {
        SharedLock<SharedMutex> adoptedShared(rwMtx, AdoptLock);
        UniqueLock<SharedMutex> upgraded = adoptedShared.tryUpgradeToUnique();
        if (!upgraded.ownsLock() || adoptedShared.ownsLock() || !rwMtx.isLockedExclusive())
        {
            return false;
        }

        SharedLock<SharedMutex> downgraded(std::move(upgraded));
        if (!downgraded.ownsLock() || upgraded.ownsLock() || !rwMtx.isLockedShared())
        {
            return false;
        }
    }

    if (rwMtx.isLocked())
    {
        return false;
    }

    OnceFlag once{};
    if (once.isCompleted() || once.getState() != OnceState::NotStarted)
    {
        return false;
    }

    gp::Int32 value = 0;
    if (tryCallOnce(
            once,
            [&value]() constexpr noexcept -> bool
    {
        value = 10;
        return false;
    }
        ))
    {
        return false;
    }
    if (once.isCompleted() || value != 10)
    {
        return false;
    }

    callOnce(
        once,
        [&value]() constexpr noexcept -> void
    {
        value = 42;
    }
    );
    if (!once.isCompleted() || value != 42)
    {
        return false;
    }

    return true;
}

static_assert(
    testConstexprMutexAndGuards(), "Constexpr evaluation of Mutex, SharedMutex, OnceFlag, and Guards failed."
);

struct MutexContentionContext
{
    static constexpr gp::UInt32 kThreadCount = 6u;
    static constexpr gp::UInt32 kIterationsPerThread = 5000u;

    CacheAlignedMutex mutex{};
    JobFence startFence{};
    Atomic<gp::UInt32> readyThreads{ 0u };
    gp::UInt64 sharedAccumulator{ 0ULL };
    gp::UInt32 activeWriters{ 0u };
    bool mutualExclusionViolated{ false };
};

static gp::UInt32 mutexContentionWorkerEntry(void* const userData) noexcept
{
    MutexContentionContext* const ctx = static_cast<MutexContentionContext*>(userData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::Auto);

    for (gp::UInt32 i = 0u; i < MutexContentionContext::kIterationsPerThread; ++i)
    {
        ScopedLock<CacheAlignedMutex> lock(ctx->mutex);
        ++ctx->activeWriters;
        if (ctx->activeWriters != 1u)
        {
            ctx->mutualExclusionViolated = true;
        }
        ++ctx->sharedAccumulator;
        --ctx->activeWriters;
    }

    return 0u;
}

struct RecursiveMutexContentionContext
{
    static constexpr gp::UInt32 kThreadCount = 4u;
    static constexpr gp::UInt32 kIterationsPerThread = 3000u;

    CacheAlignedCriticalSection criticalSection{};
    JobFence startFence{};
    Atomic<gp::UInt32> readyThreads{ 0u };
    gp::UInt64 sharedTotal{ 0ULL };
};

static gp::UInt32 recursiveMutexWorkerEntry(void* const userData) noexcept
{
    RecursiveMutexContentionContext* const ctx = static_cast<RecursiveMutexContentionContext*>(userData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::Auto);

    for (gp::UInt32 i = 0u; i < RecursiveMutexContentionContext::kIterationsPerThread; ++i)
    {
        UniqueLock<CacheAlignedCriticalSection> outerLock(ctx->criticalSection);
        EXPECT_TRUE(ctx->criticalSection.isOwnedByCurrentContext());
        EXPECT_EQ(ctx->criticalSection.getRecursionDepth(), 1u);

        {
            UniqueLock<CacheAlignedCriticalSection> innerLock(ctx->criticalSection);
            EXPECT_EQ(ctx->criticalSection.getRecursionDepth(), 2u);
            ++ctx->sharedTotal;
        }

        EXPECT_EQ(ctx->criticalSection.getRecursionDepth(), 1u);
    }

    return 0u;
}

struct FiberRecursiveIsolationState
{
    RecursiveMutex* sharedRecursiveMutex{ nullptr };
    Fiber* fiberA{ nullptr };
    Fiber* fiberB{ nullptr };
    gp::UInt64 fiberAToken{ 0ULL };
    gp::UInt64 fiberBToken{ 0ULL };
    bool fiberBTryLockWhileFiberAHeldSucceeded{ true };
    bool fiberBReacquiredAfterFiberAUlock{ false };
};

static void fiberAIsolationEntry(void* const userData) noexcept
{
    FiberRecursiveIsolationState* const state = static_cast<FiberRecursiveIsolationState*>(userData);
    state->fiberAToken = RecursiveMutex::currentExecutionOwnerToken();

    // Fiber A acquires the recursive mutex twice on this worker thread
    state->sharedRecursiveMutex->lock();
    state->sharedRecursiveMutex->lock();
    EXPECT_TRUE(state->sharedRecursiveMutex->isOwnedByCurrentContext());
    EXPECT_EQ(state->sharedRecursiveMutex->getRecursionDepth(), 2u);

    // Temporarily adjust the non-yielding lock counter around the intentional test context switch so we can verify
    // that Fiber B on the same OS worker thread cannot falsely re-enter Fiber A's RecursiveMutex.
#if GP_DO_CHECK
    Fiber::adjustActiveNonYieldingLockCount(-1);
#endif
    ASSERT_NE(state->fiberA, nullptr);
    ASSERT_NE(state->fiberB, nullptr);
    ASSERT_EQ(state->fiberA->switchTo(*state->fiberB), FiberResult::Success);
#if GP_DO_CHECK
    Fiber::adjustActiveNonYieldingLockCount(1);
#endif

    // Resumed in Fiber A: unlock both recursive acquisitions
    EXPECT_TRUE(state->sharedRecursiveMutex->isOwnedByCurrentContext());
    EXPECT_EQ(state->sharedRecursiveMutex->getRecursionDepth(), 2u);
    state->sharedRecursiveMutex->unlock();
    EXPECT_EQ(state->sharedRecursiveMutex->getRecursionDepth(), 1u);
    state->sharedRecursiveMutex->unlock();
    EXPECT_FALSE(state->sharedRecursiveMutex->isLocked());

    // Switch back to Fiber B so Fiber B can verify it can now acquire the lock
    ASSERT_EQ(state->fiberA->switchTo(*state->fiberB), FiberResult::Success);
}

static void fiberBIsolationEntry(void* const userData) noexcept
{
    FiberRecursiveIsolationState* const state = static_cast<FiberRecursiveIsolationState*>(userData);
    state->fiberBToken = RecursiveMutex::currentExecutionOwnerToken();

    // Even though Fiber B runs on the exact same OS thread as Fiber A, it has a distinct FiberId owner token!
    // Therefore, isOwnedByCurrentContext() and tryLock() MUST return false while Fiber A holds the lock.
    EXPECT_FALSE(state->sharedRecursiveMutex->isOwnedByCurrentContext());
    state->fiberBTryLockWhileFiberAHeldSucceeded = state->sharedRecursiveMutex->tryLock();

    ASSERT_NE(state->fiberA, nullptr);
    ASSERT_NE(state->fiberB, nullptr);
    ASSERT_EQ(state->fiberB->switchTo(*state->fiberA), FiberResult::Success);

    // Now Fiber A has unlocked the recursive mutex; Fiber B must be able to lock and unlock it cleanly.
    if (state->sharedRecursiveMutex->tryLock())
    {
        state->fiberBReacquiredAfterFiberAUlock = true;
        EXPECT_TRUE(state->sharedRecursiveMutex->isOwnedByCurrentContext());
        EXPECT_EQ(state->sharedRecursiveMutex->getRecursionDepth(), 1u);
        state->sharedRecursiveMutex->unlock();
    }
}

struct SharedMutexStressContext
{
    static constexpr gp::UInt32 kReaderThreadCount = 4u;
    static constexpr gp::UInt32 kWriterThreadCount = 2u;
    static constexpr gp::UInt32 kWriterIterations = 1500u;
    static constexpr gp::UInt32 kReaderIterations = 3000u;

    CacheAlignedSharedMutex rwMutex{};
    JobFence startFence{};
    Atomic<gp::UInt32> readyThreads{ 0u };

    gp::UInt64 slotA{ 0ULL };
    gp::UInt64 slotB{ 0ULL };
    Atomic<gp::UInt32> activeReaders{ 0u };
    Atomic<gp::UInt32> activeWriters{ 0u };
    Atomic<bool> invariantViolated{ false };
};

static gp::UInt32 sharedMutexWriterWorkerEntry(void* const userData) noexcept
{
    SharedMutexStressContext* const ctx = static_cast<SharedMutexStressContext*>(userData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::Auto);

    for (gp::UInt32 i = 0u; i < SharedMutexStressContext::kWriterIterations; ++i)
    {
        UniqueLock<CacheAlignedSharedMutex> writeLock(ctx->rwMutex);
        const gp::UInt32 prevWriters = ctx->activeWriters.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        const gp::UInt32 curReaders = ctx->activeReaders.load(gp::MemoryOrder::Acquire);
        if (prevWriters != 0u || curReaders != 0u)
        {
            ctx->invariantViolated.store(true, gp::MemoryOrder::Release);
        }

        const gp::UInt64 nextValue = ctx->slotA + 1ULL;
        ctx->slotA = nextValue;
        ctx->slotB = nextValue * 3ULL;

        ctx->activeWriters.fetchSub(1u, gp::MemoryOrder::AcqRel);
    }

    return 0u;
}

static gp::UInt32 sharedMutexReaderWorkerEntry(void* const userData) noexcept
{
    SharedMutexStressContext* const ctx = static_cast<SharedMutexStressContext*>(userData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::Auto);

    for (gp::UInt32 i = 0u; i < SharedMutexStressContext::kReaderIterations; ++i)
    {
        SharedLock<CacheAlignedSharedMutex> readLock(ctx->rwMutex);
        ctx->activeReaders.fetchAdd(1u, gp::MemoryOrder::AcqRel);

        if (ctx->activeWriters.load(gp::MemoryOrder::Acquire) != 0u)
        {
            ctx->invariantViolated.store(true, gp::MemoryOrder::Release);
        }

        const gp::UInt64 observedA = ctx->slotA;
        const gp::UInt64 observedB = ctx->slotB;
        if (observedB != observedA * 3ULL)
        {
            ctx->invariantViolated.store(true, gp::MemoryOrder::Release);
        }

        ctx->activeReaders.fetchSub(1u, gp::MemoryOrder::AcqRel);
    }

    return 0u;
}

struct OnceFlagStressContext
{
    static constexpr gp::UInt32 kThreadCount = 8u;

    OnceFlag onceFlag{};
    JobFence startFence{};
    Atomic<gp::UInt32> readyThreads{ 0u };
    Atomic<gp::UInt32> executionCount{ 0u };
    gp::UInt64 initializedPayload{ 0ULL };
    Atomic<gp::UInt32> verifiedReaders{ 0u };
};

static gp::UInt32 onceFlagStressWorkerEntry(void* const userData) noexcept
{
    OnceFlagStressContext* const ctx = static_cast<OnceFlagStressContext*>(userData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::Auto);

    GP_CALL_ONCE(
        ctx->onceFlag,
        [ctx]() noexcept -> void
    {
        ctx->executionCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        // Brief sleep (1 ms) to force contending threads into OnceState::RunningWithWaiters futex parking
        gp::platform::Thread::sleep(1u);
        ctx->initializedPayload = 0xDEADBEEFCAFEBABEULL;
    }
    );

    if (ctx->onceFlag.isCompleted() && ctx->initializedPayload == 0xDEADBEEFCAFEBABEULL)
    {
        ctx->verifiedReaders.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }

    return 0u;
}

struct LongHoldMutexParkContext
{
    static constexpr gp::UInt32 kWaiterCount = 4u;

    Mutex mutex{};
    RecursiveMutex recursiveMutex{};
    SharedMutex sharedMutex{};
    Atomic<gp::UInt32> enteredCount{ 0u };
    Atomic<gp::UInt32> completedCount{ 0u };
};

static gp::UInt32 longHoldMutexWaiterEntry(void* const userData) noexcept
{
    LongHoldMutexParkContext* const ctx = static_cast<LongHoldMutexParkContext*>(userData);
    ctx->enteredCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);

    {
        UniqueLock<Mutex> lock(ctx->mutex);
        gp::platform::Thread::sleep(1u);
    }

    {
        UniqueLock<RecursiveMutex> rlock(ctx->recursiveMutex);
        gp::platform::Thread::sleep(1u);
    }

    {
        UniqueLock<SharedMutex> wlock(ctx->sharedMutex);
        gp::platform::Thread::sleep(1u);
    }

    ctx->completedCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    return 0u;
}

class ThreadSafeCounterBox final
{
private:
    mutable Mutex m_mutex{};
    gp::Int32 m_value GP_GUARDED_BY(m_mutex){ 0 };

public:
    constexpr ThreadSafeCounterBox() noexcept = default;

    void add(const gp::Int32 delta) noexcept GP_EXCLUDES_CAPABILITY(m_mutex)
    {
        UniqueLock<Mutex> lock(m_mutex);
        addLocked(delta);
    }

    [[nodiscard]] gp::Int32 get() const noexcept GP_EXCLUDES_CAPABILITY(m_mutex)
    {
        UniqueLock<Mutex> lock(m_mutex);
        return m_value;
    }

private:
    void addLocked(const gp::Int32 delta) noexcept GP_REQUIRES_CAPABILITY(m_mutex)
    {
        m_value += delta;
    }
};

struct DowngradeBroadcastContext
{
    static constexpr gp::UInt32 kParkedReaders = 3u;

    SharedMutex rwMutex{};
    Atomic<gp::UInt32> enteredReaders{ 0u };
    Atomic<gp::UInt32> activeSharedReaders{ 0u };
    JobFence releaseReadersFence{};
};

static gp::UInt32 downgradeParkedReaderEntry(void* const userData) noexcept
{
    DowngradeBroadcastContext* const ctx = static_cast<DowngradeBroadcastContext*>(userData);
    ctx->enteredReaders.fetchAdd(1u, gp::MemoryOrder::AcqRel);

    SharedLock<SharedMutex> readGuard(ctx->rwMutex);
    ctx->activeSharedReaders.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->releaseReadersFence.wait(JobWaitMode::Auto);
    return 0u;
}

struct FallibleOnceRetryContext
{
    static constexpr gp::UInt32 kThreadCount = 4u;

    OnceFlag onceFlag{};
    JobFence startFence{};
    Atomic<gp::UInt32> readyThreads{ 0u };
    Atomic<gp::UInt32> attemptCount{ 0u };
    Atomic<gp::UInt32> successCount{ 0u };
    gp::UInt64 initializedValue{ 0ULL };
};

static gp::UInt32 fallibleOnceRetryWorkerEntry(void* const userData) noexcept
{
    FallibleOnceRetryContext* const ctx = static_cast<FallibleOnceRetryContext*>(userData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::Auto);

    while (!tryCallOnce(
        ctx->onceFlag,
        [ctx]() noexcept -> bool
    {
        const gp::UInt32 attempt = ctx->attemptCount.fetchAdd(1u, gp::MemoryOrder::AcqRel) + 1u;
        gp::platform::Thread::sleep(1u);
        if (attempt == 1u)
        {
            // First attempt deliberately fails -> wakes parked waiters so one of them retries and succeeds!
            return false;
        }
        ctx->initializedValue = 0x123456789ABCDEF0ULL;
        ctx->successCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        return true;
    }
    ))
    {
        gp::platform::Thread::spinWait();
    }

    EXPECT_EQ(ctx->initializedValue, 0x123456789ABCDEF0ULL);
    return 0u;
}

struct FiberMutexContentionState
{
    Mutex* sharedMutex{ nullptr };
    SharedMutex* sharedRwMutex{ nullptr };
    OnceFlag* sharedOnceFlag{ nullptr };
    Atomic<gp::UInt32>* childFiberReady{ nullptr };
    gp::UInt32 fiberObservedValue{ 0u };
    gp::UInt32 onceObservedValue{ 0u };
};

static void contendingChildFiberEntry(void* const userData) noexcept
{
    FiberMutexContentionState* const state = static_cast<FiberMutexContentionState*>(userData);
    state->childFiberReady->store(2u, gp::MemoryOrder::Release);

    {
        UniqueLock<Mutex> lock(*state->sharedMutex);
        state->fiberObservedValue = 777u;
    }

    {
        SharedLock<SharedMutex> readLock(*state->sharedRwMutex);
        state->fiberObservedValue += 111u;
    }

    {
        UniqueLock<SharedMutex> writeLock(*state->sharedRwMutex);
        state->fiberObservedValue += 111u;
    }

    GP_CALL_ONCE(
        *state->sharedOnceFlag,
        [state]() noexcept -> void
    {
        state->onceObservedValue = 999u;
    }
    );
}

static gp::UInt32 fiberContentionHelperThreadEntry(void* const userData) noexcept
{
    FiberMutexContentionState* const state = static_cast<FiberMutexContentionState*>(userData);
    state->sharedMutex->lock();
    state->sharedRwMutex->lock();
    state->childFiberReady->store(1u, gp::MemoryOrder::Release);

    while (state->childFiberReady->load(gp::MemoryOrder::Acquire) < 2u)
    {
        gp::platform::Thread::spinWait();
    }

    // Hold locks briefly (~1 ms) while the child fiber on the main thread spins/yields in user space
    gp::platform::Thread::sleep(1u);
    state->sharedMutex->unlock();

    gp::platform::Thread::sleep(1u);
    state->sharedRwMutex->unlock();

    return 0u;
}

}   // namespace

TEST(MutexTest, UniqueLockTagDispatchMoveSwapAndReleaseSemantics)
{
    Mutex mtx{};
    EXPECT_FALSE(mtx.isLocked());

    // 1. Immediate lock & scope release
    {
        UniqueLock<Mutex> guard(mtx);
        EXPECT_TRUE(guard.ownsLock());
        EXPECT_TRUE(static_cast<bool>(guard));
        EXPECT_EQ(guard.mutex(), &mtx);
        EXPECT_TRUE(mtx.isLocked());
        EXPECT_FALSE(mtx.tryLock());
    }
    EXPECT_FALSE(mtx.isLocked());

    // 2. DeferLock + tryLock + unlock + lock
    {
        UniqueLock<Mutex> deferred(mtx, DeferLock);
        EXPECT_FALSE(deferred.ownsLock());
        EXPECT_FALSE(mtx.isLocked());

        EXPECT_TRUE(deferred.tryLock());
        EXPECT_TRUE(deferred.ownsLock());
        EXPECT_TRUE(mtx.isLocked());

        deferred.unlock();
        EXPECT_FALSE(deferred.ownsLock());
        EXPECT_FALSE(mtx.isLocked());

        deferred.lock();
        EXPECT_TRUE(deferred.ownsLock());
        EXPECT_TRUE(mtx.isLocked());
    }
    EXPECT_FALSE(mtx.isLocked());

    // 3. TryToLock when already held vs unlocked
    mtx.lock();
    {
        UniqueLock<Mutex> failedTry(mtx, TryToLock);
        EXPECT_FALSE(failedTry.ownsLock());
        EXPECT_EQ(failedTry.mutex(), &mtx);
    }
    mtx.unlock();

    {
        UniqueLock<Mutex> succeededTry(mtx, TryToLock);
        EXPECT_TRUE(succeededTry.ownsLock());
        EXPECT_TRUE(mtx.isLocked());

        // Move construction
        UniqueLock<Mutex> movedGuard(std::move(succeededTry));
        EXPECT_FALSE(succeededTry.ownsLock());
        EXPECT_EQ(succeededTry.mutex(), nullptr);
        EXPECT_TRUE(movedGuard.ownsLock());
        EXPECT_EQ(movedGuard.mutex(), &mtx);

        // Swap with empty guard
        UniqueLock<Mutex> emptyGuard{};
        movedGuard.swap(emptyGuard);
        EXPECT_FALSE(movedGuard.ownsLock());
        EXPECT_TRUE(emptyGuard.ownsLock());
        EXPECT_EQ(emptyGuard.mutex(), &mtx);

        // Move assignment
        movedGuard = std::move(emptyGuard);
        EXPECT_TRUE(movedGuard.ownsLock());
        EXPECT_FALSE(emptyGuard.ownsLock());

        // Release without unlocking, then AdoptLock
        Mutex* const rawMtx = movedGuard.release();
        EXPECT_EQ(rawMtx, &mtx);
        EXPECT_FALSE(movedGuard.ownsLock());
        EXPECT_TRUE(mtx.isLocked());

        UniqueLock<Mutex> adoptedGuard(mtx, AdoptLock);
        EXPECT_TRUE(adoptedGuard.ownsLock());
    }
    EXPECT_FALSE(mtx.isLocked());
}

TEST(MutexTest, RecursiveMutexReentrancyAndFiberOwnerTokenIsolation)
{
    CriticalSection cs{};
    EXPECT_FALSE(cs.isLocked());
    EXPECT_FALSE(cs.isOwnedByCurrentContext());
    EXPECT_EQ(cs.getRecursionDepth(), 0u);
    EXPECT_EQ(cs.getOwnerToken(), kInvalidExecutionOwnerToken);

    cs.lock();
    EXPECT_TRUE(cs.isLocked());
    EXPECT_TRUE(cs.isOwnedByCurrentContext());
    EXPECT_EQ(cs.getRecursionDepth(), 1u);
    EXPECT_NE(cs.getOwnerToken(), kInvalidExecutionOwnerToken);

    EXPECT_TRUE(cs.tryLock());
    EXPECT_EQ(cs.getRecursionDepth(), 2u);

    cs.lock();
    EXPECT_EQ(cs.getRecursionDepth(), 3u);

    cs.unlock();
    EXPECT_EQ(cs.getRecursionDepth(), 2u);
    EXPECT_TRUE(cs.isLocked());

    cs.unlock();
    EXPECT_EQ(cs.getRecursionDepth(), 1u);
    EXPECT_TRUE(cs.isLocked());

    cs.unlock();
    EXPECT_EQ(cs.getRecursionDepth(), 0u);
    EXPECT_FALSE(cs.isLocked());
    EXPECT_FALSE(cs.isOwnedByCurrentContext());
    EXPECT_EQ(cs.getOwnerToken(), kInvalidExecutionOwnerToken);

    // Verify fiber isolation: two child fibers on the SAME OS thread must have distinct owner tokens and cannot
    // falsely re-enter each other's RecursiveMutex!
    ScopedThreadFiber anchorGuard(900u, "RecursiveFiberAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    Fiber fiberA{};
    Fiber fiberB{};
    FiberRecursiveIsolationState state{};
    state.sharedRecursiveMutex = &cs;

    FiberDesc descA{};
    descA.name = "RecursiveFiberA";
    descA.id = 41u;
    descA.entry = &fiberAIsolationEntry;
    descA.userData = &state;
    descA.returnFiber = &anchor;
    ASSERT_EQ(fiberA.create(descA), FiberResult::Success);

    FiberDesc descB{};
    descB.name = "RecursiveFiberB";
    descB.id = 42u;
    descB.entry = &fiberBIsolationEntry;
    descB.userData = &state;
    descB.returnFiber = &fiberA;
    ASSERT_EQ(fiberB.create(descB), FiberResult::Success);

    state.fiberA = &fiberA;
    state.fiberB = &fiberB;
    ASSERT_EQ(anchor.switchTo(fiberA), FiberResult::Success);

    EXPECT_NE(state.fiberAToken, state.fiberBToken);
    EXPECT_NE(state.fiberAToken & kRecursiveMutexFiberOwnerBit, 0ULL);
    EXPECT_NE(state.fiberBToken & kRecursiveMutexFiberOwnerBit, 0ULL);
    EXPECT_FALSE(state.fiberBTryLockWhileFiberAHeldSucceeded);
    EXPECT_TRUE(state.fiberBReacquiredAfterFiberAUlock);
    EXPECT_FALSE(cs.isLocked());

    EXPECT_EQ(fiberA.destroy(), FiberResult::Success);
    EXPECT_EQ(fiberB.destroy(), FiberResult::Success);
}

TEST(MutexTest, SharedMutexAndSharedLockReaderWriterSemantics)
{
    SharedMutex rwMutex{};
    EXPECT_FALSE(rwMutex.isLocked());
    EXPECT_FALSE(rwMutex.isLockedExclusive());
    EXPECT_FALSE(rwMutex.isLockedShared());
    EXPECT_EQ(rwMutex.getSharedReaderCount(), 0u);

    // Multiple concurrent SharedLock guards on the same thread when no writer is waiting
    {
        SharedLock<SharedMutex> reader1(rwMutex);
        EXPECT_TRUE(reader1.ownsLock());
        EXPECT_TRUE(rwMutex.isLocked());
        EXPECT_TRUE(rwMutex.isLockedShared());
        EXPECT_FALSE(rwMutex.isLockedExclusive());
        EXPECT_EQ(rwMutex.getSharedReaderCount(), 1u);

        SharedLock<SharedMutex> reader2(rwMutex, TryToLock);
        EXPECT_TRUE(reader2.ownsLock());
        EXPECT_EQ(rwMutex.getSharedReaderCount(), 2u);

        // Writer tryLock must fail while shared readers are active
        EXPECT_FALSE(rwMutex.tryLock());

        // Move and swap SharedLock
        SharedLock<SharedMutex> reader3(std::move(reader2));
        EXPECT_FALSE(reader2.ownsLock());
        EXPECT_TRUE(reader3.ownsLock());
        EXPECT_EQ(rwMutex.getSharedReaderCount(), 2u);

        SharedLock<SharedMutex> deferredReader(rwMutex, DeferLock);
        EXPECT_FALSE(deferredReader.ownsLock());
        EXPECT_TRUE(deferredReader.tryLock());
        EXPECT_EQ(rwMutex.getSharedReaderCount(), 3u);
        deferredReader.unlock();
        EXPECT_EQ(rwMutex.getSharedReaderCount(), 2u);
    }

    EXPECT_FALSE(rwMutex.isLocked());
    EXPECT_EQ(rwMutex.getSharedReaderCount(), 0u);

    // Exclusive writer blocks shared readers
    {
        UniqueLock<SharedMutex> writer(rwMutex);
        EXPECT_TRUE(rwMutex.isLockedExclusive());
        EXPECT_FALSE(rwMutex.isLockedShared());
        EXPECT_FALSE(rwMutex.tryLock());
        EXPECT_FALSE(rwMutex.tryLockShared());

        SharedLock<SharedMutex> failedReader(rwMutex, TryToLock);
        EXPECT_FALSE(failedReader.ownsLock());
    }

    EXPECT_FALSE(rwMutex.isLocked());
}

TEST(MutexTest, OnceFlagAndCallOnceSingleAndMultiThreadedExecution)
{
    OnceFlag flag{};
    EXPECT_FALSE(flag.isCompleted());
    EXPECT_EQ(flag.getState(), OnceState::NotStarted);

    gp::UInt32 callCount = 0u;
    gp::Int32 capturedSum = 0;

    callOnce(
        flag,
        [&callCount, &capturedSum](const gp::Int32 a, const gp::Int32 b) noexcept -> void
    {
        ++callCount;
        capturedSum = a + b;
    },
        17,
        25
    );

    EXPECT_TRUE(flag.isCompleted());
    EXPECT_EQ(flag.getState(), OnceState::Completed);
    EXPECT_EQ(callCount, 1u);
    EXPECT_EQ(capturedSum, 42);

    // Subsequent callOnce and GP_CALL_ONCE must not invoke the callable
    GP_CALL_ONCE(
        flag,
        [&callCount]() noexcept -> void
    {
        ++callCount;
    }
    );
    EXPECT_EQ(callCount, 1u);

    // Resetting the flag allows re-execution during teardown/reinit
    flag.reset();
    EXPECT_FALSE(flag.isCompleted());
    GP_CALL_ONCE(
        flag,
        [&callCount]() noexcept -> void
    {
        ++callCount;
    }
    );
    EXPECT_TRUE(flag.isCompleted());
    EXPECT_EQ(callCount, 2u);

    // Multi-threaded contention on OnceFlag
    OnceFlagStressContext stressCtx{};
    gp::platform::Thread workers[OnceFlagStressContext::kThreadCount]{};

    for (gp::UInt32 i = 0u; i < OnceFlagStressContext::kThreadCount; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_OnceFlagStressWorker";
        desc.entry = &onceFlagStressWorkerEntry;
        desc.userData = &stressCtx;
        ASSERT_EQ(workers[i].start(desc), gp::platform::ThreadResult::Success);
    }

    while (stressCtx.readyThreads.load(gp::MemoryOrder::Acquire) < OnceFlagStressContext::kThreadCount)
    {
        gp::platform::Thread::spinWait();
    }

    stressCtx.startFence.signal();

    for (gp::UInt32 i = 0u; i < OnceFlagStressContext::kThreadCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(workers[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_EQ(stressCtx.executionCount.load(gp::MemoryOrder::Acquire), 1u);
    EXPECT_EQ(stressCtx.verifiedReaders.load(gp::MemoryOrder::Acquire), OnceFlagStressContext::kThreadCount);
}

TEST(MutexTest, MultiThreadedMutexContentionStress)
{
    MutexContentionContext ctx{};
    gp::platform::Thread workers[MutexContentionContext::kThreadCount]{};

    for (gp::UInt32 i = 0u; i < MutexContentionContext::kThreadCount; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_MutexStressWorker";
        desc.entry = &mutexContentionWorkerEntry;
        desc.userData = &ctx;
        ASSERT_EQ(workers[i].start(desc), gp::platform::ThreadResult::Success);
    }

    while (ctx.readyThreads.load(gp::MemoryOrder::Acquire) < MutexContentionContext::kThreadCount)
    {
        gp::platform::Thread::spinWait();
    }

    ctx.startFence.signal();

    for (gp::UInt32 i = 0u; i < MutexContentionContext::kThreadCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(workers[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_FALSE(ctx.mutualExclusionViolated);
    EXPECT_EQ(
        ctx.sharedAccumulator,
        static_cast<gp::UInt64>(MutexContentionContext::kThreadCount) *
            static_cast<gp::UInt64>(MutexContentionContext::kIterationsPerThread)
    );
    EXPECT_FALSE(ctx.mutex.isLocked());
}

TEST(MutexTest, MultiThreadedRecursiveMutexContentionStress)
{
    RecursiveMutexContentionContext ctx{};
    gp::platform::Thread workers[RecursiveMutexContentionContext::kThreadCount]{};

    for (gp::UInt32 i = 0u; i < RecursiveMutexContentionContext::kThreadCount; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_RecursiveMutexWorker";
        desc.entry = &recursiveMutexWorkerEntry;
        desc.userData = &ctx;
        ASSERT_EQ(workers[i].start(desc), gp::platform::ThreadResult::Success);
    }

    while (ctx.readyThreads.load(gp::MemoryOrder::Acquire) < RecursiveMutexContentionContext::kThreadCount)
    {
        gp::platform::Thread::spinWait();
    }

    ctx.startFence.signal();

    for (gp::UInt32 i = 0u; i < RecursiveMutexContentionContext::kThreadCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(workers[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_EQ(
        ctx.sharedTotal,
        static_cast<gp::UInt64>(RecursiveMutexContentionContext::kThreadCount) *
            static_cast<gp::UInt64>(RecursiveMutexContentionContext::kIterationsPerThread)
    );
    EXPECT_FALSE(ctx.criticalSection.isLocked());
    EXPECT_EQ(ctx.criticalSection.getRecursionDepth(), 0u);
}

TEST(MutexTest, MultiThreadedSharedMutexReaderWriterContentionStress)
{
    SharedMutexStressContext ctx{};
    gp::platform::Thread readers[SharedMutexStressContext::kReaderThreadCount]{};
    gp::platform::Thread writers[SharedMutexStressContext::kWriterThreadCount]{};

    for (gp::UInt32 i = 0u; i < SharedMutexStressContext::kReaderThreadCount; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_SharedMutexReader";
        desc.entry = &sharedMutexReaderWorkerEntry;
        desc.userData = &ctx;
        ASSERT_EQ(readers[i].start(desc), gp::platform::ThreadResult::Success);
    }

    for (gp::UInt32 i = 0u; i < SharedMutexStressContext::kWriterThreadCount; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_SharedMutexWriter";
        desc.entry = &sharedMutexWriterWorkerEntry;
        desc.userData = &ctx;
        ASSERT_EQ(writers[i].start(desc), gp::platform::ThreadResult::Success);
    }

    const gp::UInt32 totalWorkers =
        SharedMutexStressContext::kReaderThreadCount + SharedMutexStressContext::kWriterThreadCount;
    while (ctx.readyThreads.load(gp::MemoryOrder::Acquire) < totalWorkers)
    {
        gp::platform::Thread::spinWait();
    }

    ctx.startFence.signal();

    for (gp::UInt32 i = 0u; i < SharedMutexStressContext::kReaderThreadCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(readers[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    for (gp::UInt32 i = 0u; i < SharedMutexStressContext::kWriterThreadCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(writers[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_FALSE(ctx.invariantViolated.load(gp::MemoryOrder::Acquire));
    const gp::UInt64 expectedWrites = static_cast<gp::UInt64>(SharedMutexStressContext::kWriterThreadCount) *
                                      static_cast<gp::UInt64>(SharedMutexStressContext::kWriterIterations);
    EXPECT_EQ(ctx.slotA, expectedWrites);
    EXPECT_EQ(ctx.slotB, expectedWrites * 3ULL);
    EXPECT_FALSE(ctx.rwMutex.isLocked());
    EXPECT_EQ(ctx.rwMutex.getSharedReaderCount(), 0u);
}

TEST(MutexTest, MultiWaiterFutexParkAndCascadeWakeupWithoutLostWakeups)
{
    LongHoldMutexParkContext ctx{};
    ctx.mutex.lock();
    ctx.recursiveMutex.lock();
    ctx.sharedMutex.lock();

    gp::platform::Thread waiters[LongHoldMutexParkContext::kWaiterCount]{};
    for (gp::UInt32 i = 0u; i < LongHoldMutexParkContext::kWaiterCount; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_LongHoldWaiter";
        desc.entry = &longHoldMutexWaiterEntry;
        desc.userData = &ctx;
        ASSERT_EQ(waiters[i].start(desc), gp::platform::ThreadResult::Success);
    }

    while (ctx.enteredCount.load(gp::MemoryOrder::Acquire) < LongHoldMutexParkContext::kWaiterCount)
    {
        gp::platform::Thread::spinWait();
    }

    // Sleep 2 ms so all 4 waiter threads exhaust their 32-iteration spin budget and park inside Futex::waitOnAddress
    gp::platform::Thread::sleep(2u);
    EXPECT_TRUE(ctx.mutex.mayHaveWaiters());

    ctx.mutex.unlock();
    ctx.recursiveMutex.unlock();
    ctx.sharedMutex.unlock();

    for (gp::UInt32 i = 0u; i < LongHoldMutexParkContext::kWaiterCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(waiters[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_EQ(ctx.completedCount.load(gp::MemoryOrder::Acquire), LongHoldMutexParkContext::kWaiterCount);
    EXPECT_FALSE(ctx.mutex.isLocked());
    EXPECT_FALSE(ctx.recursiveMutex.isLocked());
    EXPECT_FALSE(ctx.sharedMutex.isLocked());
}

TEST(MutexTest, SharedMutexAtomicUpgradeDowngradeAndFiberLockDepthTracking)
{
    SharedMutex rwMutex{};
    const gp::UInt32 baseLockDepth = Fiber::getActiveNonYieldingLockCount();

    {
        SharedLock<SharedMutex> soleReader(rwMutex);
        EXPECT_TRUE(soleReader.ownsLock());
        EXPECT_TRUE(rwMutex.isLockedShared());
        EXPECT_FALSE(rwMutex.isLockedExclusive());
#if GP_DO_CHECK
        EXPECT_EQ(Fiber::getActiveNonYieldingLockCount(), baseLockDepth + 1u);
#endif

        // Add a second reader -> tryUpgradeToUnique() must fail and leave soleReader intact!
        {
            SharedLock<SharedMutex> secondReader(rwMutex);
            EXPECT_EQ(rwMutex.getSharedReaderCount(), 2u);
#if GP_DO_CHECK
            EXPECT_EQ(Fiber::getActiveNonYieldingLockCount(), baseLockDepth + 2u);
#endif

            UniqueLock<SharedMutex> failedUpgrade = soleReader.tryUpgradeToUnique();
            EXPECT_FALSE(failedUpgrade.ownsLock());
            EXPECT_TRUE(soleReader.ownsLock());
            EXPECT_EQ(rwMutex.getSharedReaderCount(), 2u);
        }

        // Now soleReader is the only active reader -> tryUpgradeToUnique() succeeds in 1 CAS!
        UniqueLock<SharedMutex> upgradedWriter = soleReader.tryUpgradeToUnique();
        EXPECT_TRUE(upgradedWriter.ownsLock());
        EXPECT_FALSE(soleReader.ownsLock());
        EXPECT_EQ(soleReader.mutex(), nullptr);
        EXPECT_TRUE(rwMutex.isLockedExclusive());
        EXPECT_FALSE(rwMutex.isLockedShared());
        EXPECT_EQ(rwMutex.getSharedReaderCount(), 0u);
#if GP_DO_CHECK
        EXPECT_EQ(Fiber::getActiveNonYieldingLockCount(), baseLockDepth + 1u);
#endif

        // Atomically downgrade upgradedWriter back into a SharedLock via SharedLock(UniqueLock&&)
        SharedLock<SharedMutex> downgradedReader(std::move(upgradedWriter));
        EXPECT_TRUE(downgradedReader.ownsLock());
        EXPECT_FALSE(upgradedWriter.ownsLock());
        EXPECT_EQ(upgradedWriter.mutex(), nullptr);
        EXPECT_FALSE(rwMutex.isLockedExclusive());
        EXPECT_TRUE(rwMutex.isLockedShared());
        EXPECT_EQ(rwMutex.getSharedReaderCount(), 1u);
#if GP_DO_CHECK
        EXPECT_EQ(Fiber::getActiveNonYieldingLockCount(), baseLockDepth + 1u);
#endif
    }

    EXPECT_FALSE(rwMutex.isLocked());
#if GP_DO_CHECK
    EXPECT_EQ(Fiber::getActiveNonYieldingLockCount(), baseLockDepth);
#endif
}

TEST(MutexTest, DeadlockFreeMultiLockAndTryLockMultipleAndFallibleTryCallOnce)
{
    Mutex m1{};
    Mutex m2{};
    RecursiveMutex m3{};

    // 1. All unlocked -> tryLockMultiple returns -1 and locks all three
    EXPECT_EQ(tryLockMultiple(m1, m2, m3), -1);
    EXPECT_TRUE(m1.isLocked());
    EXPECT_TRUE(m2.isLocked());
    EXPECT_TRUE(m3.isLocked());
    m1.unlock();
    m2.unlock();
    m3.unlock();

    // 2. Second mutex already held -> tryLockMultiple unlocks m1 and returns index 1
    Mutex heldMutex{};
    heldMutex.lock();
    Mutex freeA{};
    Mutex freeB{};
    EXPECT_FALSE(heldMutex.tryLock());

    // Simulate failed middle lock with a non-reentrant Mutex on the same thread by testing a custom lockable that
    // rejects tryLock
    struct RejectingLockable
    {
        bool allowTryLock{ false };
        gp::UInt32 lockCount{ 0u };

        void lock() noexcept
        {
            ++lockCount;
        }

        bool tryLock() noexcept
        {
            if (!allowTryLock)
            {
                return false;
            }
            ++lockCount;
            return true;
        }

        void unlock() noexcept
        {
            --lockCount;
        }
    };

    RejectingLockable rejector{};
    EXPECT_EQ(tryLockMultiple(freeA, rejector, freeB), 1);
    EXPECT_FALSE(freeA.isLocked());
    EXPECT_FALSE(freeB.isLocked());
    EXPECT_EQ(rejector.lockCount, 0u);
    heldMutex.unlock();

    // 3. lockMultiple on two Mutex instances in both (m1, m2) and (m2, m1) orders without AB/BA deadlock
    lockMultiple(m1, m2);
    EXPECT_TRUE(m1.isLocked());
    EXPECT_TRUE(m2.isLocked());
    m1.unlock();
    m2.unlock();

    lockMultiple(m2, m1);
    EXPECT_TRUE(m1.isLocked());
    EXPECT_TRUE(m2.isLocked());
    m1.unlock();
    m2.unlock();

    // 4. Heterogeneous 3-mutex lockMultiple
    lockMultiple(m1, m2, m3);
    EXPECT_TRUE(m1.isLocked());
    EXPECT_TRUE(m2.isLocked());
    EXPECT_TRUE(m3.isLocked());
    m1.unlock();
    m2.unlock();
    m3.unlock();

    // 5. Fallible tryCallOnce: failed attempt leaves OnceFlag in NotStarted so next call succeeds
    OnceFlag fallibleFlag{};
    gp::UInt32 attempts = 0u;

    const bool firstAttempt = tryCallOnce(
        fallibleFlag,
        [&attempts]() noexcept -> bool
    {
        ++attempts;
        return false;
    }
    );
    EXPECT_FALSE(firstAttempt);
    EXPECT_FALSE(fallibleFlag.isCompleted());
    EXPECT_EQ(fallibleFlag.getState(), OnceState::NotStarted);
    EXPECT_EQ(attempts, 1u);

    const bool secondAttempt = tryCallOnce(
        fallibleFlag,
        [&attempts]() noexcept -> bool
    {
        ++attempts;
        return true;
    }
    );
    EXPECT_TRUE(secondAttempt);
    EXPECT_TRUE(fallibleFlag.isCompleted());
    EXPECT_EQ(attempts, 2u);

    const bool thirdAttempt = tryCallOnce(
        fallibleFlag,
        [&attempts]() noexcept -> bool
    {
        ++attempts;
        return true;
    }
    );
    EXPECT_TRUE(thirdAttempt);
    EXPECT_EQ(attempts, 2u);

    // 6. Verify Clang thread-safety capability annotated helper class
    ThreadSafeCounterBox safeBox{};
    safeBox.add(15);
    safeBox.add(27);
    EXPECT_EQ(safeBox.get(), 42);
}

TEST(MutexTest, WriterToReaderDowngradeWakesParkedReadersAndMultiThreadedFallibleOnceRetry)
{
    // Part 1: Writer holds SharedMutex while 3 readers park on m_readerWakeEpoch; writer downgrades via
    // unlockAndLockShared() and all 3 parked readers wake immediately and run concurrently with the downgraded reader!
    DowngradeBroadcastContext downgradeCtx{};
    UniqueLock<SharedMutex> writerLock(downgradeCtx.rwMutex);

    gp::platform::Thread readerThreads[DowngradeBroadcastContext::kParkedReaders]{};
    for (gp::UInt32 i = 0u; i < DowngradeBroadcastContext::kParkedReaders; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_DowngradeWaitReader";
        desc.entry = &downgradeParkedReaderEntry;
        desc.userData = &downgradeCtx;
        ASSERT_EQ(readerThreads[i].start(desc), gp::platform::ThreadResult::Success);
    }

    while (downgradeCtx.enteredReaders.load(gp::MemoryOrder::Acquire) < DowngradeBroadcastContext::kParkedReaders)
    {
        gp::platform::Thread::spinWait();
    }

    // Allow the 3 reader threads to exhaust their spin budget and park on m_readerWakeEpoch
    gp::platform::Thread::sleep(2u);
    EXPECT_EQ(downgradeCtx.activeSharedReaders.load(gp::MemoryOrder::Acquire), 0u);

    // Atomically downgrade writerLock to SharedLock -> wakes all 3 parked readers while keeping 1 shared lock on this
    // thread (total active shared readers == 4)
    SharedLock<SharedMutex> downgradedLock(std::move(writerLock));
    EXPECT_TRUE(downgradedLock.ownsLock());

    while (downgradeCtx.activeSharedReaders.load(gp::MemoryOrder::Acquire) < DowngradeBroadcastContext::kParkedReaders)
    {
        gp::platform::Thread::spinWait();
    }

    EXPECT_EQ(downgradeCtx.rwMutex.getSharedReaderCount(), DowngradeBroadcastContext::kParkedReaders + 1u);
    downgradeCtx.releaseReadersFence.signal();
    downgradedLock.unlock();

    for (gp::UInt32 i = 0u; i < DowngradeBroadcastContext::kParkedReaders; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(readerThreads[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }
    EXPECT_FALSE(downgradeCtx.rwMutex.isLocked());

    // Part 2: Multi-threaded contended tryCallOnce where the first thread fails after parking waiters, waking them to
    // retry and succeed
    FallibleOnceRetryContext retryCtx{};
    gp::platform::Thread retryWorkers[FallibleOnceRetryContext::kThreadCount]{};
    for (gp::UInt32 i = 0u; i < FallibleOnceRetryContext::kThreadCount; ++i)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_FallibleOnceRetry";
        desc.entry = &fallibleOnceRetryWorkerEntry;
        desc.userData = &retryCtx;
        ASSERT_EQ(retryWorkers[i].start(desc), gp::platform::ThreadResult::Success);
    }

    while (retryCtx.readyThreads.load(gp::MemoryOrder::Acquire) < FallibleOnceRetryContext::kThreadCount)
    {
        gp::platform::Thread::spinWait();
    }

    retryCtx.startFence.signal();

    for (gp::UInt32 i = 0u; i < FallibleOnceRetryContext::kThreadCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(retryWorkers[i].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_TRUE(retryCtx.onceFlag.isCompleted());
    EXPECT_EQ(retryCtx.attemptCount.load(gp::MemoryOrder::Acquire), 2u);
    EXPECT_EQ(retryCtx.successCount.load(gp::MemoryOrder::Acquire), 1u);
    EXPECT_EQ(retryCtx.initializedValue, 0x123456789ABCDEF0ULL);
}

TEST(MutexTest, ChildFiberContentionUserSpaceSpinYieldPath)
{
    Mutex sharedMutex{};
    SharedMutex sharedRwMutex{};
    OnceFlag sharedOnceFlag{};
    Atomic<gp::UInt32> childFiberReady{ 0u };

    FiberMutexContentionState state{};
    state.sharedMutex = &sharedMutex;
    state.sharedRwMutex = &sharedRwMutex;
    state.sharedOnceFlag = &sharedOnceFlag;
    state.childFiberReady = &childFiberReady;

    gp::platform::Thread helperThread{};
    gp::platform::ThreadDesc threadDesc{};
    threadDesc.name = "GP_FiberContentionUnlocker";
    threadDesc.entry = &fiberContentionHelperThreadEntry;
    threadDesc.userData = &state;
    ASSERT_EQ(helperThread.start(threadDesc), gp::platform::ThreadResult::Success);

    while (childFiberReady.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    ScopedThreadFiber anchorGuard(950u, "FiberContentionAnchor");
    ASSERT_TRUE(anchorGuard.isValid());
    Fiber& anchor = anchorGuard.getFiber();

    Fiber childFiber{};
    FiberDesc fiberDesc{};
    fiberDesc.name = "ContendingChildFiber";
    fiberDesc.id = 95u;
    fiberDesc.entry = &contendingChildFiberEntry;
    fiberDesc.userData = &state;
    fiberDesc.returnFiber = &anchor;
    ASSERT_EQ(childFiber.create(fiberDesc), FiberResult::Success);

    ASSERT_EQ(anchor.switchTo(childFiber), FiberResult::Success);

    gp::UInt32 exitCode = 1u;
    EXPECT_EQ(helperThread.join(&exitCode), gp::platform::ThreadResult::Success);
    EXPECT_EQ(exitCode, 0u);

    EXPECT_EQ(state.fiberObservedValue, 999u);
    EXPECT_EQ(state.onceObservedValue, 999u);
    EXPECT_EQ(childFiber.destroy(), FiberResult::Success);
}

}   // namespace gp::thread::tests
