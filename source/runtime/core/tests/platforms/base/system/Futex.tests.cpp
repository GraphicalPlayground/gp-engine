// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/system/Futex.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>

namespace gp::platform::tests
{

namespace
{

struct FutexWakeOneContext
{
    gp::thread::Atomic<gp::UInt32> futexWord{ 0u };
    gp::thread::Atomic<gp::UInt32> waiterParked{ 0u };
    FutexResult waitResult{ FutexResult::WaitFailed };
};

static gp::UInt32 futexWakeOneWorkerEntry(void* const userData) noexcept
{
    FutexWakeOneContext* const ctx = static_cast<FutexWakeOneContext*>(userData);
    ctx->waiterParked.store(1u, gp::MemoryOrder::Release);

    while (ctx->futexWord.load(gp::MemoryOrder::Acquire) == 0u)
    {
        ctx->waitResult = Futex::waitOnAddress(ctx->futexWord, 0u);
    }

    return 0u;
}

struct FutexWakeAllContext
{
    static constexpr gp::UInt32 kWaiterCount = 4u;

    gp::thread::Atomic<gp::UInt32> futexWord{ 10u };
    gp::thread::Atomic<gp::UInt32> parkedCount{ 0u };
    gp::thread::Atomic<gp::UInt32> wokenCount{ 0u };
};

static gp::UInt32 futexWakeAllWorkerEntry(void* const userData) noexcept
{
    FutexWakeAllContext* const ctx = static_cast<FutexWakeAllContext*>(userData);
    ctx->parkedCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);

    while (ctx->futexWord.load(gp::MemoryOrder::Acquire) == 10u)
    {
        (void)Futex::waitOnAddress(ctx->futexWord, 10u);
    }

    ctx->wokenCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    return 0u;
}

}   // namespace

TEST(FutexTest, ValueMismatchAndZeroTimeoutReturnImmediately)
{
    gp::thread::Atomic<gp::UInt32> state(42u);

    // Value differs from expectedValue (42 != 99) -> immediate ValueMismatch
    EXPECT_EQ(Futex::waitOnAddress(state, 99u), FutexResult::ValueMismatch);
    EXPECT_EQ(gp::platform::generic::Futex::waitOnAddress(state, 99u), FutexResult::ValueMismatch);

    // Value matches (42 == 42) with zero timeout -> immediate TimedOut
    EXPECT_EQ(Futex::waitOnAddress(state, 42u, 0ULL), FutexResult::TimedOut);
    EXPECT_EQ(gp::platform::generic::Futex::waitOnAddress(state, 42u, 0ULL), FutexResult::TimedOut);

    // Short bounded timeout (2 milliseconds = 2,000,000 ns) when value never changes -> TimedOut
    EXPECT_EQ(Futex::waitOnAddress(state, 42u, 2'000'000ULL), FutexResult::TimedOut);
    EXPECT_EQ(gp::platform::generic::Futex::waitOnAddress(state, 42u, 2'000'000ULL), FutexResult::TimedOut);
}

TEST(FutexTest, WakeOneUnblocksParkedThread)
{
    FutexWakeOneContext ctx{};
    Thread worker{};

    ThreadDesc desc{};
    desc.name = "GP_FutexWakeOneWorker";
    desc.entry = &futexWakeOneWorkerEntry;
    desc.userData = &ctx;
    ASSERT_EQ(worker.start(desc), ThreadResult::Success);

    while (ctx.waiterParked.load(gp::MemoryOrder::Acquire) == 0u)
    {
        Thread::spinWait();
    }

    ctx.futexWord.store(1u, gp::MemoryOrder::Release);
    Futex::wakeOne(ctx.futexWord);

    gp::UInt32 exitCode = 1u;
    EXPECT_EQ(worker.join(&exitCode), ThreadResult::Success);
    EXPECT_EQ(exitCode, 0u);
}

TEST(FutexTest, WakeAllUnblocksAllParkedThreads)
{
    FutexWakeAllContext ctx{};
    Thread workers[FutexWakeAllContext::kWaiterCount]{};

    for (gp::UInt32 i = 0u; i < FutexWakeAllContext::kWaiterCount; ++i)
    {
        ThreadDesc desc{};
        desc.name = "GP_FutexWakeAllWorker";
        desc.entry = &futexWakeAllWorkerEntry;
        desc.userData = &ctx;
        ASSERT_EQ(workers[i].start(desc), ThreadResult::Success);
    }

    while (ctx.parkedCount.load(gp::MemoryOrder::Acquire) < FutexWakeAllContext::kWaiterCount)
    {
        Thread::spinWait();
    }

    ctx.futexWord.store(20u, gp::MemoryOrder::Release);
    Futex::wakeAll(ctx.futexWord);

    for (gp::UInt32 i = 0u; i < FutexWakeAllContext::kWaiterCount; ++i)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(workers[i].join(&exitCode), ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_EQ(ctx.wokenCount.load(gp::MemoryOrder::Acquire), FutexWakeAllContext::kWaiterCount);
}

}   // namespace gp::platform::tests
