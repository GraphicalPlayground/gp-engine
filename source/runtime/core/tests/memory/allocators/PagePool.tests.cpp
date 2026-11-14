// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/PagePool.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::memory::tests
{

TEST(PagePoolTest, DefaultUninitializedStateAndSafeNoOps)
{
    PagePool pool{};

    EXPECT_FALSE(pool.isValid());
    EXPECT_EQ(pool.getPageSize(), 0u);
    EXPECT_EQ(pool.getPageCount(), 0u);
    EXPECT_EQ(pool.getCapacity(), 0u);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
    EXPECT_EQ(pool.getAvailablePageCount(), 0u);
    EXPECT_EQ(pool.getPeakLeasedPageCount(), 0u);

    EXPECT_EQ(pool.acquirePage(), nullptr);
    EXPECT_FALSE(pool.owns(nullptr));
    EXPECT_FALSE(pool.ownsPage(nullptr));
    EXPECT_EQ(pool.getPageIndex(nullptr), PagePool::kInvalidPageIndex);

    pool.releasePage(nullptr);
    pool.releasePageChain(nullptr, nullptr, 0u);
    pool.reset();
    pool.shutdown();
}

TEST(PagePoolTest, HeapBackedInitializationAcquireReleaseAndExhaustion)
{
    constexpr gp::USize kTestPageSize = 4096u;
    constexpr gp::UInt32 kTestPageCount = 4u;

    PagePool pool(kTestPageSize, kTestPageCount, nullptr, "PagePool.UnitTest");
    ASSERT_TRUE(pool.isValid());
    EXPECT_EQ(pool.getPageSize(), kTestPageSize);
    EXPECT_EQ(pool.getPageCount(), kTestPageCount);
    EXPECT_EQ(pool.getCapacity(), kTestPageSize * kTestPageCount);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
    EXPECT_EQ(pool.getAvailablePageCount(), kTestPageCount);
    EXPECT_STREQ(pool.getName(), "PagePool.UnitTest");

    void* pages[kTestPageCount]{};
    for (gp::UInt32 i = 0u; i < kTestPageCount; ++i)
    {
        pages[i] = pool.acquirePage();
        ASSERT_NE(pages[i], nullptr);
        EXPECT_TRUE(gp::memory::isAligned(pages[i], PagePool::kPageAlignment));
        EXPECT_TRUE(pool.owns(pages[i]));
        EXPECT_TRUE(pool.ownsPage(pages[i]));
        EXPECT_FALSE(pool.ownsPage(static_cast<gp::UInt8*>(pages[i]) + 64u));
        EXPECT_EQ(pool.getPageIndex(pages[i]), i);
    }

    EXPECT_EQ(pool.getLeasedPageCount(), kTestPageCount);
    EXPECT_EQ(pool.getAvailablePageCount(), 0u);
    EXPECT_EQ(pool.getPeakLeasedPageCount(), kTestPageCount);

    // Exhausted pool must return nullptr
    EXPECT_EQ(pool.acquirePage(), nullptr);

    // Release two pages and verify LIFO reuse from the ABA-tagged Treiber stack
    pool.releasePage(pages[1]);
    pool.releasePage(pages[3]);
    EXPECT_EQ(pool.getLeasedPageCount(), 2u);
    EXPECT_EQ(pool.getAvailablePageCount(), 2u);

    void* const poppedFirst = pool.acquirePage();
    void* const poppedSecond = pool.acquirePage();
    EXPECT_EQ(poppedFirst, pages[3]);
    EXPECT_EQ(poppedSecond, pages[1]);
    EXPECT_EQ(pool.getLeasedPageCount(), kTestPageCount);

    // Reset reclaims all pages in O(1) time while preserving peak leased count
    pool.reset();
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
    EXPECT_EQ(pool.getAvailablePageCount(), kTestPageCount);
    EXPECT_EQ(pool.getPeakLeasedPageCount(), kTestPageCount);

    pool.shutdown();
    EXPECT_FALSE(pool.isValid());
}

TEST(PagePoolTest, ExternalBufferInitializationAndAlignmentAdjustment)
{
    constexpr gp::USize kPageSize = 1024u;
    alignas(PagePool::kPageAlignment) gp::UInt8 rawBuffer[(kPageSize * 4u) + 128u]{};

    // Pass an intentionally misaligned pointer (+19 bytes) to verify upward cache-line alignment
    PagePool pool(rawBuffer + 19u, sizeof(rawBuffer) - 19u, kPageSize, "PagePool.ExternalTest");
    ASSERT_TRUE(pool.isValid());
    EXPECT_EQ(pool.getPageSize(), kPageSize);
    EXPECT_EQ(pool.getPageCount(), 4u);

    void* const page0 = pool.acquirePage();
    ASSERT_NE(page0, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(page0, PagePool::kPageAlignment));
    EXPECT_TRUE(pool.ownsPage(page0));

    pool.releasePage(page0);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
}

TEST(PagePoolTest, BulkReleasePageChainSingleAtomicSplice)
{
    PagePool pool(1024u, 6u);
    ASSERT_TRUE(pool.isValid());

    void* pages[4]{};
    for (gp::UInt32 i = 0u; i < 4u; ++i)
    {
        pages[i] = pool.acquirePage();
        ASSERT_NE(pages[i], nullptr);
    }
    EXPECT_EQ(pool.getLeasedPageCount(), 4u);

    // Link pages[0] -> pages[1] -> pages[2] -> pages[3] and release the 4-page chain in one call
    PagePool::linkPageForChainRelease(pages[0], pool.getPageIndex(pages[1]));
    PagePool::linkPageForChainRelease(pages[1], pool.getPageIndex(pages[2]));
    PagePool::linkPageForChainRelease(pages[2], pool.getPageIndex(pages[3]));

    pool.releasePageChain(pages[0], pages[3], 4u);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
    EXPECT_EQ(pool.getAvailablePageCount(), 6u);

    // Subsequent acquires must pop pages[0], pages[1], pages[2], pages[3] in exact chain order
    for (gp::UInt32 i = 0u; i < 4u; ++i)
    {
        EXPECT_EQ(pool.acquirePage(), pages[i]);
    }
}

TEST(PagePoolTest, MultiThreadedLockFreeContentionStressTest)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kPagesPerThread = 4u;
    constexpr gp::UInt32 kTotalPages = kThreadCount * kPagesPerThread;
    constexpr gp::USize kPageSize = 4096u;
    constexpr gp::UInt32 kIterationsPerThread = 1500u;

    PagePool pool(kPageSize, kTotalPages, nullptr, "PagePool.ConcurrencyStress");
    ASSERT_TRUE(pool.isValid());

    gp::thread::Atomic<gp::UInt32> readyCount(0u);
    gp::thread::Atomic<bool> startLatch(false);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 threadIndex = 0u; threadIndex < kThreadCount; ++threadIndex)
    {
        workers.emplace_back([threadIndex, &pool, &readyCount, &startLatch]()
        {
            readyCount.fetchAdd(1u, gp::MemoryOrder::Release);
            while (!startLatch.load(gp::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            void* localPages[kPagesPerThread]{};
            for (gp::UInt32 iter = 0u; iter < kIterationsPerThread; ++iter)
            {
                gp::UInt32 acquiredCount = 0u;
                for (gp::UInt32 p = 0u; p < kPagesPerThread; ++p)
                {
                    void* const page = pool.acquirePage();
                    ASSERT_NE(page, nullptr);
                    localPages[acquiredCount++] = page;

                    const gp::UInt8 stamp = static_cast<gp::UInt8>(((threadIndex + 1u) * 37u) ^ (iter & 0xFFu));
                    gp::UInt8* const bytes = static_cast<gp::UInt8*>(page);
                    bytes[0] = stamp;
                    bytes[kPageSize / 2u] = stamp;
                    bytes[kPageSize - 1u] = stamp;
                }

                // Verify no concurrent thread acquired the same pages while held by this worker
                for (gp::UInt32 p = 0u; p < acquiredCount; ++p)
                {
                    const gp::UInt8 expectedStamp = static_cast<gp::UInt8>(((threadIndex + 1u) * 37u) ^ (iter & 0xFFu));
                    const gp::UInt8* const bytes = static_cast<const gp::UInt8*>(localPages[p]);
                    EXPECT_EQ(bytes[0], expectedStamp);
                    EXPECT_EQ(bytes[kPageSize / 2u], expectedStamp);
                    EXPECT_EQ(bytes[kPageSize - 1u], expectedStamp);
                }

                // Alternate between single-page releases and bulk chain releases
                if ((iter & 1u) == 0u)
                {
                    for (gp::UInt32 p = 0u; p < acquiredCount; ++p)
                    {
                        pool.releasePage(localPages[p]);
                    }
                }
                else
                {
                    for (gp::UInt32 p = 0u; p + 1u < acquiredCount; ++p)
                    {
                        PagePool::linkPageForChainRelease(localPages[p], pool.getPageIndex(localPages[p + 1u]));
                    }
                    pool.releasePageChain(localPages[0], localPages[acquiredCount - 1u], acquiredCount);
                }
            }
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

    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
    EXPECT_EQ(pool.getAvailablePageCount(), kTotalPages);
    EXPECT_GT(pool.getPeakLeasedPageCount(), 0u);
    EXPECT_LE(pool.getPeakLeasedPageCount(), kTotalPages);
}

}   // namespace gp::memory::tests
