// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/system/Thread.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>

namespace gp::platform::tests
{

static_assert(std::is_scoped_enum_v<ThreadResult>, "ThreadResult must be an enum class.");
static_assert(
    std::is_same_v<std::underlying_type_t<ThreadResult>, unsigned char>,
    "ThreadResult underlying type must be unsigned char."
);

static_assert(std::is_scoped_enum_v<ThreadPriority>, "ThreadPriority must be an enum class.");
static_assert(
    std::is_same_v<std::underlying_type_t<ThreadPriority>, unsigned char>,
    "ThreadPriority underlying type must be unsigned char."
);

static_assert(kDefaultStackSize == 0ULL, "kDefaultStackSize must be 0ULL.");
static_assert(kNoAffinityMask == ~0ULL, "kNoAffinityMask must be ~0ULL.");

namespace
{

/// @brief Context passed to worker thread verifying data passing and thread ID capture.
struct WorkerContext
{
    std::atomic<bool> executed{ false };
    ThreadId observedThreadId{ 0ULL };
    gp::UInt32 inputValue{ 0u };
};

/// @brief Entry point function executing a basic computation and capturing thread state.
/// @param[in] userData Pointer to WorkerContext.
/// @return Calculated status code (inputValue * 2).
gp::UInt32 basicWorkerEntry(void* userData) noexcept
{
    WorkerContext* const ctx = static_cast<WorkerContext*>(userData);
    if (ctx != nullptr)
    {
        ctx->observedThreadId = Thread::currentThreadId();
        ctx->executed.store(true, std::memory_order_release);
        return ctx->inputValue * 2u;
    }
    return 0u;
}

/// @brief Context passed to detached worker thread to coordinate lifecycle.
struct DetachContext
{
    std::atomic<bool> isStarted{ false };
    std::atomic<bool> canExit{ false };
    std::atomic<bool> isFinished{ false };
};

/// @brief Entry point function that waits for a release signal before terminating.
/// @param[in] userData Pointer to DetachContext.
/// @return Exit code 0.
gp::UInt32 detachWorkerEntry(void* userData) noexcept
{
    DetachContext* const ctx = static_cast<DetachContext*>(userData);
    if (ctx != nullptr)
    {
        ctx->isStarted.store(true, std::memory_order_release);
        while (!ctx->canExit.load(std::memory_order_acquire))
        {
            Thread::yieldExecution();
        }
        ctx->isFinished.store(true, std::memory_order_release);
    }
    return 0u;
}

/// @brief Context used to test self-join error detection.
struct SelfJoinContext
{
    Thread* threadInstance{ nullptr };
    ThreadResult observedResult{ ThreadResult::Success };
    std::atomic<bool> executed{ false };
};

/// @brief Entry point attempting to call join() on its own Thread object.
/// @param[in] userData Pointer to SelfJoinContext.
/// @return Exit code 1u.
gp::UInt32 selfJoinWorkerEntry(void* userData) noexcept
{
    SelfJoinContext* const ctx = static_cast<SelfJoinContext*>(userData);
    if (ctx != nullptr && ctx->threadInstance != nullptr)
    {
        ctx->observedResult = ctx->threadInstance->join();
        ctx->executed.store(true, std::memory_order_release);
    }
    return 1u;
}

}   // namespace

TEST(PlatformThreadTest, ThreadDescDefaultValues)
{
    const ThreadDesc desc;

    EXPECT_STREQ(desc.name, "GP_Worker");
    EXPECT_EQ(desc.entry, nullptr);
    EXPECT_EQ(desc.userData, nullptr);
    EXPECT_EQ(desc.stackSize, kDefaultStackSize);
    EXPECT_EQ(desc.affinityMask, kNoAffinityMask);
    EXPECT_EQ(desc.priority, ThreadPriority::Normal);
    EXPECT_FALSE(desc.failOnConfigError);
}

TEST(PlatformThreadTest, ConcurrencyQueriesReturnValidCounts)
{
    const gp::UInt32 hardwareCount = Thread::getHardwareConcurrency();
    EXPECT_GE(hardwareCount, 1u);

    const gp::UInt32 processCount = Thread::getProcessConcurrency();
    EXPECT_GE(processCount, 1u);
    EXPECT_LE(processCount, hardwareCount);
}

TEST(PlatformThreadTest, CurrentThreadIdReturnsNonZeroAndConsistent)
{
    const ThreadId id1 = Thread::currentThreadId();
    EXPECT_GT(id1, 0ULL);

    const ThreadId id2 = Thread::currentThreadId();
    EXPECT_EQ(id1, id2);
}

TEST(PlatformThreadTest, SleepYieldAndSpinWaitDoNotCrash)
{
    Thread::sleep(0u);
    Thread::sleep(1u);
    Thread::yieldExecution();
    Thread::spinWait();
}

TEST(PlatformThreadTest, DefaultConstructionIsUnstartedAndNotJoinable)
{
    Thread thread;

    EXPECT_FALSE(thread.isJoinable());
    EXPECT_EQ(thread.getId(), 0ULL);

    // Operations on unstarted thread must fail with NotJoinable
    EXPECT_EQ(thread.join(), ThreadResult::NotJoinable);
    EXPECT_EQ(thread.detach(), ThreadResult::NotJoinable);
    EXPECT_EQ(thread.setAffinity(1ULL), ThreadResult::NotJoinable);
    EXPECT_EQ(thread.setPriority(ThreadPriority::Normal), ThreadResult::NotJoinable);
}

TEST(PlatformThreadTest, StartRejectsNullEntryPoint)
{
    Thread thread;
    ThreadDesc desc;
    desc.entry = nullptr;

    const ThreadResult result = thread.start(desc);
    EXPECT_EQ(result, ThreadResult::InvalidArguments);
    EXPECT_FALSE(thread.isJoinable());
}

TEST(PlatformThreadTest, StartExecutionAndJoinLifecycle)
{
    WorkerContext ctx;
    ctx.inputValue = 21u;

    ThreadDesc desc;
    desc.name = "GP_TestWorker";
    desc.entry = &basicWorkerEntry;
    desc.userData = &ctx;

    Thread thread;
    const ThreadResult startRes = thread.start(desc);
    ASSERT_EQ(startRes, ThreadResult::Success);

    EXPECT_TRUE(thread.isJoinable());
    const ThreadId workerId = thread.getId();
    EXPECT_GT(workerId, 0ULL);
    EXPECT_NE(workerId, Thread::currentThreadId());

    // Calling start() again while active must return AlreadyRunning
    EXPECT_EQ(thread.start(desc), ThreadResult::AlreadyRunning);

    gp::UInt32 exitCode = 0u;
    const ThreadResult joinRes = thread.join(&exitCode);
    EXPECT_EQ(joinRes, ThreadResult::Success);
    EXPECT_EQ(exitCode, 42u);

    EXPECT_TRUE(ctx.executed.load(std::memory_order_acquire));
    EXPECT_EQ(ctx.observedThreadId, workerId);

    // Thread must no longer be joinable after join()
    EXPECT_FALSE(thread.isJoinable());
    EXPECT_EQ(thread.getId(), 0ULL);
}

TEST(PlatformThreadTest, DetachActiveThread)
{
    DetachContext ctx;

    ThreadDesc desc;
    desc.name = "GP_DetachWorker";
    desc.entry = &detachWorkerEntry;
    desc.userData = &ctx;

    Thread thread;
    ASSERT_EQ(thread.start(desc), ThreadResult::Success);
    EXPECT_TRUE(thread.isJoinable());

    // Wait until child thread has started execution
    while (!ctx.isStarted.load(std::memory_order_acquire))
    {
        Thread::yieldExecution();
    }

    const ThreadResult detachRes = thread.detach();
    EXPECT_EQ(detachRes, ThreadResult::Success);
    EXPECT_FALSE(thread.isJoinable());
    EXPECT_EQ(thread.getId(), 0ULL);

    // Release child thread to cleanly terminate
    ctx.canExit.store(true, std::memory_order_release);
    while (!ctx.isFinished.load(std::memory_order_acquire))
    {
        Thread::yieldExecution();
    }
}

TEST(PlatformThreadTest, SetPriorityAndAffinityOnActiveThread)
{
    DetachContext ctx;

    ThreadDesc desc;
    desc.name = "GP_ConfigWorker";
    desc.entry = &detachWorkerEntry;
    desc.userData = &ctx;

    Thread thread;
    ASSERT_EQ(thread.start(desc), ThreadResult::Success);

    while (!ctx.isStarted.load(std::memory_order_acquire))
    {
        Thread::yieldExecution();
    }

    // Priority tests
    EXPECT_EQ(thread.setPriority(ThreadPriority::BelowNormal), ThreadResult::Success);
    EXPECT_EQ(thread.setPriority(ThreadPriority::AboveNormal), ThreadResult::Success);
    EXPECT_EQ(thread.setPriority(ThreadPriority::Normal), ThreadResult::Success);

    // Affinity tests
    EXPECT_EQ(thread.setAffinity(0ULL), ThreadResult::InvalidArguments);
    EXPECT_EQ(thread.setAffinity(kNoAffinityMask), ThreadResult::InvalidArguments);
    EXPECT_EQ(thread.setAffinity(1ULL), ThreadResult::Success);   // Bind to logical core 0

    // Clean up
    ctx.canExit.store(true, std::memory_order_release);
    EXPECT_EQ(thread.join(), ThreadResult::Success);
}

TEST(PlatformThreadTest, SelfJoinReturnsSelfJoinError)
{
    SelfJoinContext ctx;

    ThreadDesc desc;
    desc.name = "GP_SelfJoinWorker";
    desc.entry = &selfJoinWorkerEntry;
    desc.userData = &ctx;

    Thread thread;
    ctx.threadInstance = &thread;

    ASSERT_EQ(thread.start(desc), ThreadResult::Success);

    // Wait until worker executed self-join
    while (!ctx.executed.load(std::memory_order_acquire))
    {
        Thread::yieldExecution();
    }

    EXPECT_EQ(ctx.observedResult, ThreadResult::SelfJoinError);

    gp::UInt32 exitCode = 0u;
    EXPECT_EQ(thread.join(&exitCode), ThreadResult::Success);
    EXPECT_EQ(exitCode, 1u);
}

TEST(PlatformThreadTest, MoveConstructionAndAssignment)
{
    WorkerContext ctx;
    ctx.inputValue = 10u;

    ThreadDesc desc;
    desc.name = "GP_MoveWorker";
    desc.entry = &basicWorkerEntry;
    desc.userData = &ctx;

    Thread t1;
    ASSERT_EQ(t1.start(desc), ThreadResult::Success);
    const ThreadId origId = t1.getId();

    // Move construct t2 from t1
    Thread t2(std::move(t1));
    EXPECT_FALSE(t1.isJoinable());
    EXPECT_EQ(t1.getId(), 0ULL);
    EXPECT_TRUE(t2.isJoinable());
    EXPECT_EQ(t2.getId(), origId);

    // Move assign t3 from t2
    Thread t3;
    t3 = std::move(t2);
    EXPECT_FALSE(t2.isJoinable());
    EXPECT_EQ(t2.getId(), 0ULL);
    EXPECT_TRUE(t3.isJoinable());
    EXPECT_EQ(t3.getId(), origId);

    gp::UInt32 exitCode = 0u;
    EXPECT_EQ(t3.join(&exitCode), ThreadResult::Success);
    EXPECT_EQ(exitCode, 20u);
}

TEST(PlatformThreadTest, ConcurrentMultiThreadedExecution)
{
    constexpr gp::UInt32 kThreadCount = 4u;
    constexpr gp::UInt32 kIncrementsPerThread = 10'000u;

    struct SharedData
    {
        std::atomic<gp::UInt32> counter{ 0u };
    };

    const auto workerFunc = [](void* userData) noexcept -> gp::UInt32
    {
        SharedData* const data = static_cast<SharedData*>(userData);
        for (gp::UInt32 i = 0u; i < kIncrementsPerThread; ++i)
        {
            data->counter.fetch_add(1u, std::memory_order_relaxed);
        }
        return 0u;
    };

    SharedData sharedData{};
    std::vector<Thread> threads(kThreadCount);

    for (gp::UInt32 i = 0u; i < kThreadCount; ++i)
    {
        ThreadDesc desc;
        desc.name = "GP_ConcurrentWorker";
        desc.entry = workerFunc;
        desc.userData = &sharedData;

        ASSERT_EQ(threads[i].start(desc), ThreadResult::Success);
    }

    for (gp::UInt32 i = 0u; i < kThreadCount; ++i)
    {
        EXPECT_EQ(threads[i].join(), ThreadResult::Success);
    }

    EXPECT_EQ(sharedData.counter.load(std::memory_order_acquire), kThreadCount * kIncrementsPerThread);
}

}   // namespace gp::platform::tests
