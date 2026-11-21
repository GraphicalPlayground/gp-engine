// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/PagedLinearAllocator.hpp"
#include "memory/allocators/PagePool.hpp"
#include "memory/allocators/PolymorphicAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include "memory/utils/MemoryOps.hpp"
#include <gtest/gtest.h>
#include <new>   // IWYU pragma: keep
#include <utility>

namespace gp::memory::tests
{

namespace
{

struct LifecycleTracker
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_copyCount{ 0 };
    static inline gp::Int32 s_moveCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };

    static void reset() noexcept
    {
        s_constructCount = 0;
        s_copyCount = 0;
        s_moveCount = 0;
        s_destructCount = 0;
    }

    gp::Int32 id{ 0 };

    LifecycleTracker() noexcept
        : id(0)
    {
        ++s_constructCount;
    }

    explicit LifecycleTracker(const gp::Int32 inId) noexcept
        : id(inId)
    {
        ++s_constructCount;
    }

    LifecycleTracker(const LifecycleTracker& other) noexcept
        : id(other.id)
    {
        ++s_copyCount;
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : id(other.id)
    {
        ++s_moveCount;
        other.id = -1;
    }

    LifecycleTracker& operator=(const LifecycleTracker& other) noexcept
    {
        id = other.id;
        ++s_copyCount;
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        id = other.id;
        other.id = -1;
        ++s_moveCount;
        return *this;
    }

    ~LifecycleTracker() noexcept
    {
        ++s_destructCount;
    }

    bool operator==(const LifecycleTracker& other) const noexcept
    {
        return id == other.id;
    }
};

}   // namespace

TEST(PagedLinearAllocatorTest, ZeroIdleFootprintOnConstructionAndReset)
{
    PagePool pool(4096u, 8u);
    ASSERT_TRUE(pool.isValid());

    PagedLinearAllocator arena(&pool, nullptr, true, "PagedLinearAllocator.UnitTest");
    EXPECT_TRUE(arena.isValid());
    EXPECT_EQ(arena.getPagePool(), &pool);
    EXPECT_EQ(arena.getLeasedPageCount(), 0u);
    EXPECT_EQ(arena.getOversizedBlockCount(), 0u);
    EXPECT_EQ(arena.getTotalPageCount(), 0u);
    EXPECT_EQ(arena.getUsedBytes(), 0u);
    EXPECT_EQ(arena.getCapacity(), 0u);
    EXPECT_EQ(arena.getCurrentPageRemainingBytes(), 0u);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);

    // Zero-size allocation must not lease a page
    EXPECT_EQ(arena.allocate(0u, 16u), nullptr);
    EXPECT_EQ(arena.getLeasedPageCount(), 0u);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);

    // First real allocation leases exactly 1 page
    void* const block = arena.allocate(128u, 16u);
    ASSERT_NE(block, nullptr);
    EXPECT_TRUE(arena.owns(block));
    EXPECT_EQ(arena.getLeasedPageCount(), 1u);
    EXPECT_EQ(pool.getLeasedPageCount(), 1u);
    EXPECT_EQ(arena.getUsedBytes(), 128u);

    // Reset returns the page back to the pool, restoring a 0-byte footprint while preserving peakAllocatedBytes
    arena.reset();
    EXPECT_EQ(arena.getLeasedPageCount(), 0u);
    EXPECT_EQ(arena.getCapacity(), 0u);
    EXPECT_EQ(arena.getUsedBytes(), 0u);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
    EXPECT_EQ(arena.getStats().peakAllocatedBytes, 128u);
}

TEST(PagedLinearAllocatorTest, ElasticMultiPageChainingAndCrossPageScopedMemoryMarkRewind)
{
    constexpr gp::USize kPageSize = 4096u;
    PagePool pool(kPageSize, 16u);
    ASSERT_TRUE(pool.isValid());

    PagedLinearAllocator arena(&pool);

    // Outer mark taken when arena holds 0 pages (`pageCookie == nullptr`)
    {
        ScopedMemoryMark<PagedLinearAllocator> emptyRootScope(arena);
        EXPECT_EQ(emptyRootScope.getMark().pageCookie, nullptr);

        // Allocate 2048 bytes -> leases Page 1
        void* const p1a = arena.allocate(2048u, 32u);
        ASSERT_NE(p1a, nullptr);
        EXPECT_TRUE(gp::memory::isAligned(p1a, 32u));
        EXPECT_EQ(arena.getLeasedPageCount(), 1u);
        EXPECT_EQ(pool.getLeasedPageCount(), 1u);

        {
            ScopedMemoryMark<PagedLinearAllocator> page1Scope(arena);

            // Allocate 3000 bytes -> does not fit in remaining bytes of Page 1, leases Page 2
            void* const p2 = arena.allocate(3000u, 16u);
            ASSERT_NE(p2, nullptr);
            EXPECT_EQ(arena.getLeasedPageCount(), 2u);
            EXPECT_EQ(pool.getLeasedPageCount(), 2u);

            // Allocate another 3000 bytes -> leases Page 3
            void* const p3 = arena.allocate(3000u, 64u);
            ASSERT_NE(p3, nullptr);
            EXPECT_TRUE(gp::memory::isAligned(p3, 64u));
            EXPECT_EQ(arena.getLeasedPageCount(), 3u);
            EXPECT_EQ(pool.getLeasedPageCount(), 3u);

            EXPECT_TRUE(arena.owns(p1a));
            EXPECT_TRUE(arena.owns(p2));
            EXPECT_TRUE(arena.owns(p3));
        }

        // Exiting page1Scope must splice Pages 3 and 2 back to `pool` in one batch, leaving only Page 1
        EXPECT_EQ(arena.getLeasedPageCount(), 1u);
        EXPECT_EQ(pool.getLeasedPageCount(), 1u);
        EXPECT_EQ(arena.getUsedBytes(), 2048u);
        EXPECT_TRUE(arena.owns(p1a));
    }

    // Exiting emptyRootScope must release Page 1 back to `pool`, returning arena to 0 pages
    EXPECT_EQ(arena.getLeasedPageCount(), 0u);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
    EXPECT_EQ(arena.getUsedBytes(), 0u);
}

TEST(PagedLinearAllocatorTest, InPlaceTopOfPageReallocationAndCrossPageRelocation)
{
    PagePool pool(4096u, 8u);
    ASSERT_TRUE(pool.isValid());

    PagedLinearAllocator arena(&pool);

    // Reserve an anchor block of 1024 bytes so Page 1 has ~3008 bytes remaining
    void* const anchorBlock = arena.allocate(1024u, 32u);
    ASSERT_NE(anchorBlock, nullptr);

    gp::UInt8* const firstBlock = static_cast<gp::UInt8*>(arena.allocate(256u, 32u));
    ASSERT_NE(firstBlock, nullptr);
    for (gp::USize i = 0u; i < 256u; ++i)
    {
        firstBlock[i] = static_cast<gp::UInt8>(i & 0xFFu);
    }

    // 1. In-place growth at top of active page must return the exact same address without leasing a second page
    gp::UInt8* const grownInPlace = static_cast<gp::UInt8*>(arena.reallocate(firstBlock, 256u, 1024u, 32u));
    EXPECT_EQ(grownInPlace, firstBlock);
    EXPECT_EQ(arena.getLeasedPageCount(), 1u);
    EXPECT_EQ(arena.getUsedBytes(), 2048u);

    // 2. In-place shrink at top of active page
    gp::UInt8* const shrunkInPlace = static_cast<gp::UInt8*>(arena.reallocate(grownInPlace, 1024u, 512u, 32u));
    EXPECT_EQ(shrunkInPlace, firstBlock);
    EXPECT_EQ(arena.getUsedBytes(), 1536u);

    // 3. Cross-page growth (512 -> 3500 bytes exceeds remaining ~3008 bytes in Page 1, leasing Page 2)
    gp::UInt8* const relocatedBlock = static_cast<gp::UInt8*>(arena.reallocate(shrunkInPlace, 512u, 3500u, 32u));
    ASSERT_NE(relocatedBlock, nullptr);
    EXPECT_NE(relocatedBlock, firstBlock);
    EXPECT_EQ(arena.getLeasedPageCount(), 2u);
    EXPECT_EQ(arena.getUsedBytes(), 1024u + 3500u);

    for (gp::USize i = 0u; i < 256u; ++i)
    {
        EXPECT_EQ(relocatedBlock[i], static_cast<gp::UInt8>(i & 0xFFu));
    }

    // Top-of-page deallocate reclaims the 3500-byte payload immediately
    arena.deallocate(relocatedBlock, 3500u, 32u);
    EXPECT_EQ(arena.getUsedBytes(), 1024u);
}

TEST(PagedLinearAllocatorTest, OversizedHeapBlockFallbackAndDeterministicUnwind)
{
    PagePool pool(4096u, 4u);
    ASSERT_TRUE(pool.isValid());

    PagedLinearAllocator arena(&pool, nullptr, true, "PagedLinearAllocator.OversizedTest");

    {
        ScopedMemoryMark<PagedLinearAllocator> outerScope(arena);

        void* const smallFirst = arena.allocate(512u, 16u);
        ASSERT_NE(smallFirst, nullptr);
        EXPECT_EQ(arena.getLeasedPageCount(), 1u);
        EXPECT_EQ(arena.getOversizedBlockCount(), 0u);

        // Allocate 16 KiB (> 4 KiB pool page) with 128-byte alignment
        void* const hugeBlock1 = arena.allocate(16384u, 128u);
        ASSERT_NE(hugeBlock1, nullptr);
        EXPECT_TRUE(gp::memory::isAligned(hugeBlock1, 128u));
        EXPECT_TRUE(arena.owns(hugeBlock1));
        EXPECT_EQ(arena.getLeasedPageCount(), 1u);
        EXPECT_EQ(arena.getOversizedBlockCount(), 1u);
        EXPECT_EQ(arena.getTotalPageCount(), 2u);

        // Subsequent small allocation must continue bumping Page 1 without stranding its remaining ~3.5 KiB!
        void* const smallSecond = arena.allocate(256u, 16u);
        ASSERT_NE(smallSecond, nullptr);
        EXPECT_EQ(arena.getLeasedPageCount(), 1u);
        EXPECT_EQ(arena.getOversizedBlockCount(), 1u);

        {
            ScopedMemoryMark<PagedLinearAllocator> innerScope(arena);

            void* const hugeBlock2 = arena.allocate(8192u, 64u);
            ASSERT_NE(hugeBlock2, nullptr);
            EXPECT_TRUE(arena.owns(hugeBlock2));
            EXPECT_EQ(arena.getOversizedBlockCount(), 2u);

            // Reallocating top oversized block replaces and immediately frees hugeBlock2
            void* const hugeBlock2Grown = arena.reallocate(hugeBlock2, 8192u, 12288u, 64u);
            ASSERT_NE(hugeBlock2Grown, nullptr);
            EXPECT_EQ(arena.getOversizedBlockCount(), 2u);
            EXPECT_TRUE(arena.owns(hugeBlock2Grown));
        }

        // Exiting innerScope frees hugeBlock2Grown while preserving hugeBlock1 and Page 1
        EXPECT_EQ(arena.getOversizedBlockCount(), 1u);
        EXPECT_EQ(arena.getLeasedPageCount(), 1u);
        EXPECT_TRUE(arena.owns(hugeBlock1));
        EXPECT_TRUE(arena.owns(smallFirst));
        EXPECT_TRUE(arena.owns(smallSecond));
    }

    // Outer scope unwind frees both the pool page AND the remaining oversized heap block
    EXPECT_EQ(arena.getLeasedPageCount(), 0u);
    EXPECT_EQ(arena.getOversizedBlockCount(), 0u);
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
    EXPECT_EQ(arena.getUsedBytes(), 0u);
}

TEST(PagedLinearAllocatorTest, MoveSemanticsAndPolymorphicLifecycleTracking)
{
    LifecycleTracker::reset();

    PagePool pool(4096u, 8u);
    ASSERT_TRUE(pool.isValid());

    PagedLinearAllocator sourceArena(&pool);
    PolymorphicAllocator poly(&sourceArena);

    LifecycleTracker* const items =
        static_cast<LifecycleTracker*>(poly.allocate(sizeof(LifecycleTracker) * 8u, alignof(LifecycleTracker)));
    ASSERT_NE(items, nullptr);

    for (gp::Int32 i = 0; i < 8; ++i)
    {
        ::new (static_cast<void*>(&items[i])) LifecycleTracker(i + 100);
    }
    EXPECT_EQ(LifecycleTracker::s_constructCount, 8);

    // Move-construct arena into `movedArena` and verify ownership transfers cleanly
    PagedLinearAllocator movedArena(std::move(sourceArena));
    EXPECT_EQ(sourceArena.getLeasedPageCount(), 0u);
    EXPECT_EQ(movedArena.getLeasedPageCount(), 1u);
    EXPECT_TRUE(movedArena.owns(items));

    gp::memory::destructItems(items, 8u);
    EXPECT_EQ(LifecycleTracker::s_destructCount, 8);

    movedArena.reset();
    EXPECT_EQ(pool.getLeasedPageCount(), 0u);
}

}   // namespace gp::memory::tests
