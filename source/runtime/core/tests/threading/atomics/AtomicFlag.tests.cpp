// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "threading/atomics/AtomicFlag.hpp"
#include "concepts/Concepts.hpp"
#include "memory/MemoryOrder.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::thread::tests
{

constexpr bool testConstexprAtomicFlag()
{
    AtomicFlag flag;
    if (flag.test())
    {
        return false;
    }
    if (flag.testAndSet() || !flag.test())
    {
        return false;
    }
    if (!flag.testAndSet())
    {
        return false;
    }
    flag.clear();
    if (flag.test())
    {
        return false;
    }
    return true;
}

static_assert(testConstexprAtomicFlag(), "Constexpr AtomicFlag evaluation failed.");

TEST(AtomicFlagTest, NonCopyableAndNonMovable)
{
    EXPECT_FALSE(gp::concepts::IsCopyConstructible<AtomicFlag>);
    EXPECT_FALSE(gp::concepts::IsCopyAssignable<AtomicFlag>);
    EXPECT_FALSE(gp::concepts::IsMoveConstructible<AtomicFlag>);
    EXPECT_FALSE(gp::concepts::IsMoveAssignable<AtomicFlag>);
}

TEST(AtomicFlagTest, Lifecycle)
{
    AtomicFlag flag;
    EXPECT_FALSE(flag.test(MemoryOrder::Relaxed));

    EXPECT_FALSE(flag.testAndSet(MemoryOrder::Acquire));
    EXPECT_TRUE(flag.test(MemoryOrder::Acquire));

    EXPECT_TRUE(flag.testAndSet(MemoryOrder::AcqRel));

    flag.clear(MemoryOrder::Release);
    EXPECT_FALSE(flag.test(MemoryOrder::SeqCst));
}

TEST(AtomicFlagTest, MultiThreadedSpinlockContention)
{
    constexpr gp::UInt32 kThreadCount = 6u;
    constexpr gp::UInt32 kIterationsPerThread = 10000u;

    AtomicFlag spinlock;
    gp::UInt64 nonAtomicProtectedCounter = 0ULL;

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([&spinlock, &nonAtomicProtectedCounter]()
        {
            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                while (spinlock.testAndSet(MemoryOrder::Acquire))
                {
                }

                ++nonAtomicProtectedCounter;

                spinlock.clear(MemoryOrder::Release);
            }
        });
    }

    for (auto& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(nonAtomicProtectedCounter, static_cast<gp::UInt64>(kThreadCount) * kIterationsPerThread);
}

}   // namespace gp::thread::tests
