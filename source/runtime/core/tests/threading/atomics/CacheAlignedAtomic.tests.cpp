// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "threading/atomics/CacheAlignedAtomic.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::thread::tests
{

TEST(CacheAlignedAtomicTest, LayoutAndAlignment)
{
    CacheAlignedAtomic<gp::UInt64> alignedCounter(100ULL);

    constexpr gp::USize kExpectedLineSize = gp::memory::HardwareConstants::cacheLineSize;
    EXPECT_EQ(sizeof(alignedCounter), kExpectedLineSize);
    EXPECT_EQ(alignof(decltype(alignedCounter)), kExpectedLineSize);
    EXPECT_EQ(reinterpret_cast<gp::UIntPtr>(&alignedCounter) % kExpectedLineSize, 0u);

    alignedCounter.value.fetchAdd(50ULL, MemoryOrder::Relaxed);
    EXPECT_EQ(alignedCounter.value.load(MemoryOrder::Relaxed), 150ULL);
}

TEST(CacheAlignedAtomicTest, MultiThreadedContention)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 25000u;

    CacheAlignedAtomic<gp::UInt64> sharedCounter(0ULL);
    Atomic<bool> startGate(false);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([&sharedCounter, &startGate]()
        {
            while (!startGate.load(MemoryOrder::Acquire))
            {
            }

            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                sharedCounter.value.fetchAdd(1ULL, MemoryOrder::Relaxed);
            }
        });
    }

    startGate.store(true, MemoryOrder::Release);

    for (auto& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(
        sharedCounter.value.load(MemoryOrder::Acquire), static_cast<gp::UInt64>(kThreadCount) * kIterationsPerThread
    );
}

}   // namespace gp::thread::tests
