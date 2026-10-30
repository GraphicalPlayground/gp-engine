// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/ThreadScratchAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::memory::tests
{

namespace
{

struct MockContainerWithThreadScratch
{
    void* data{ nullptr };
    gp::USize size{ 0u };
    gp::USize capacity{ 0u };
    GP_NO_UNIQUE_ADDRESS ThreadScratchAllocator allocator{};
};

static_assert(
    sizeof(MockContainerWithThreadScratch) == (sizeof(void*) + (2u * sizeof(gp::USize))),
    "Stateless ThreadScratchAllocator must occupy 0 bytes when embedded via GP_NO_UNIQUE_ADDRESS."
);

}   // namespace

TEST(ThreadScratchAllocatorTest, ExplicitInitializationResetAndNestedScopedScratchMarks)
{
    ASSERT_TRUE(ThreadScratchArena::initializeThreadScratch(64u * 1024u));
    EXPECT_EQ(ThreadScratchArena::get().getCapacity(), 64u * 1024u);
    EXPECT_EQ(ThreadScratchArena::get().getUsedBytes(), 0u);

    ThreadScratchAllocator scratch{};

    {
        ScopedScratchMark outerScope{};
        void* outerBlock = scratch.allocate(128u, 16u);
        ASSERT_NE(outerBlock, nullptr);
        EXPECT_TRUE(scratch.owns(outerBlock));
        EXPECT_EQ(ThreadScratchArena::get().getUsedBytes(), 128u);

        {
            ScopedScratchMark innerScope{};
            void* innerBlock = scratch.allocate(256u, 32u);
            ASSERT_NE(innerBlock, nullptr);
            EXPECT_TRUE(gp::memory::isAligned(innerBlock, 32u));
            EXPECT_GT(ThreadScratchArena::get().getUsedBytes(), 128u);

            // In-place top-of-scratch resize
            void* resizedInner = scratch.reallocate(innerBlock, 256u, 512u, 32u);
            EXPECT_EQ(resizedInner, innerBlock);
        }

        // Inner scope rewound back to 128 bytes
        EXPECT_EQ(ThreadScratchArena::get().getUsedBytes(), 128u);
    }

    // Outer scope rewound back to 0 bytes
    EXPECT_EQ(ThreadScratchArena::get().getUsedBytes(), 0u);

    void* manualBlock = scratch.allocate(512u, 16u);
    ASSERT_NE(manualBlock, nullptr);
    EXPECT_EQ(ThreadScratchArena::get().getUsedBytes(), 512u);

    ThreadScratchArena::resetThreadScratch();
    EXPECT_EQ(ThreadScratchArena::get().getUsedBytes(), 0u);

    ThreadScratchArena::shutdownThreadScratch();
}

TEST(ThreadScratchAllocatorTest, MultiThreadedConcurrentScratchIsolationStressTest)
{
    constexpr gp::UInt32 kThreadCount = 6u;
    constexpr gp::UInt32 kIterationsPerThread = 500u;

    gp::thread::Atomic<gp::UInt32> readyCount(0u);
    gp::thread::Atomic<bool> startLatch(false);
    gp::thread::Atomic<gp::UInt32> completedWorkers(0u);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 threadIndex = 0u; threadIndex < kThreadCount; ++threadIndex)
    {
        workers.emplace_back([threadIndex, &readyCount, &startLatch, &completedWorkers]()
        {
            ASSERT_TRUE(ThreadScratchArena::initializeThreadScratch(128u * 1024u));
            ThreadScratchAllocator workerScratch{};

            readyCount.fetchAdd(1u, gp::MemoryOrder::Release);
            while (!startLatch.load(gp::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 iter = 0u; iter < kIterationsPerThread; ++iter)
            {
                ScopedScratchMark iterScope{};

                void* block = workerScratch.allocate(64u, 32u);
                ASSERT_NE(block, nullptr);
                EXPECT_TRUE(gp::memory::isAligned(block, 32u));
                EXPECT_TRUE(workerScratch.owns(block));

                const gp::UInt8 expectedPattern = static_cast<gp::UInt8>(((threadIndex + 1u) * 41u) ^ (iter & 0xFFu));
                gp::UInt8* bytes = static_cast<gp::UInt8*>(block);
                for (gp::USize i = 0u; i < 64u; ++i)
                {
                    bytes[i] = expectedPattern;
                }

                // Grow in place on this worker's thread-local arena
                void* grown = workerScratch.reallocate(block, 64u, 192u, 32u);
                EXPECT_EQ(grown, block);

                const gp::UInt8* checkBytes = static_cast<const gp::UInt8*>(grown);
                for (gp::USize i = 0u; i < 64u; ++i)
                {
                    EXPECT_EQ(checkBytes[i], expectedPattern);
                }
            }

            EXPECT_EQ(ThreadScratchArena::get().getUsedBytes(), 0u);
            ThreadScratchArena::shutdownThreadScratch();
            completedWorkers.fetchAdd(1u, gp::MemoryOrder::Release);
        });
    }

    while (readyCount.load(gp::MemoryOrder::Acquire) < kThreadCount)
    {
        std::this_thread::yield();
    }
    startLatch.store(true, gp::MemoryOrder::Release);

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(completedWorkers.load(gp::MemoryOrder::Acquire), kThreadCount);
}

}   // namespace gp::memory::tests
