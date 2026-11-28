// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/DescriptorAllocator.hpp"
#include "containers/arrays/Array.hpp"
#include "templates/NumericLimits.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::rhi::allocator::tests
{

namespace
{

struct ScopedDescriptorTracking final
{
    bool previousState{ false };

    explicit ScopedDescriptorTracking(const bool enable) noexcept
        : previousState(DescriptorAllocator::isOutstandingAllocationTrackingEnabled())
    {
        DescriptorAllocator::setOutstandingAllocationTrackingEnabled(enable);
    }

    ~ScopedDescriptorTracking() noexcept
    {
        DescriptorAllocator::setOutstandingAllocationTrackingEnabled(previousState);
    }
};

}   // namespace

TEST(DescriptorAllocatorTest, DefaultInitShutdownZeroCapacityAndNullStats)
{
    DescriptorAllocator allocator;
    EXPECT_EQ(allocator.getCapacity(), 0u);
    EXPECT_FALSE(allocator.allocate(1u).hasValue());
    EXPECT_FALSE(allocator.allocate(view::DescriptorType::TextureSRV).isValid());

    DescriptorAllocatorRange range;
    EXPECT_FALSE(allocator.getAllocatedRange(range));

    // init(0) stays empty and safe.
    DescriptorStatCounter* const nullStats[1] = { nullptr };
    allocator.init(0u, nullStats);
    EXPECT_EQ(allocator.getCapacity(), 0u);
    EXPECT_FALSE(allocator.getAllocatedRange(range));

    // Growing from capacity 0 works cleanly.
    const gp::Optional<DescriptorAllocation> first = allocator.resizeGrowAndAllocate(4u, 2u);
    ASSERT_TRUE(first.hasValue());
    EXPECT_EQ(allocator.getCapacity(), 4u);
    EXPECT_EQ(*first, DescriptorAllocation(0u, 2u));
    allocator.free(*first);

    allocator.init(16u, nullStats);
    EXPECT_EQ(allocator.getCapacity(), 16u);
    EXPECT_FALSE(allocator.getAllocatedRange(range));

    allocator.shutdown();
    EXPECT_EQ(allocator.getCapacity(), 0u);
    EXPECT_FALSE(allocator.allocate(1u).hasValue());
}

TEST(DescriptorAllocatorTest, AllocateExhaustionInteriorRangeRemovalAndFirstFitSkip)
{
    const ScopedDescriptorTracking tracking(true);

    DescriptorStatCounter statCounter{ 0 };
    DescriptorStatCounter* const statPtrs[1] = { &statCounter };

    DescriptorAllocator allocator(12u, statPtrs);
    EXPECT_EQ(allocator.getCapacity(), 12u);

    const gp::Optional<DescriptorAllocation> a = allocator.allocate(2u);   // [0, 1]
    const gp::Optional<DescriptorAllocation> b = allocator.allocate(2u);   // [2, 3]
    const gp::Optional<DescriptorAllocation> c = allocator.allocate(4u);   // [4, 7]
    const gp::Optional<DescriptorAllocation> d = allocator.allocate(4u);   // [8, 11]

    ASSERT_TRUE(a.hasValue());
    ASSERT_TRUE(b.hasValue());
    ASSERT_TRUE(c.hasValue());
    ASSERT_TRUE(d.hasValue());
    EXPECT_EQ(statCounter.load(gp::MemoryOrder::Relaxed), 12);
    EXPECT_FALSE(allocator.allocate(1u).hasValue());

    // Free a ([0,1]) and c ([4,7]) -> two disjoint ranges: [0,1] (size 2) and [4,7] (size 4).
    allocator.free(*a);
    allocator.free(*c);

    // First-fit skips [0,1] because 3 > 2 and allocates from [4,7].
    const gp::Optional<DescriptorAllocation> fit3 = allocator.allocate(3u);
    ASSERT_TRUE(fit3.hasValue());
    EXPECT_EQ(*fit3, DescriptorAllocation(4u, 3u));

    // Exact-size allocation of leading range [0,1] while [7,7] follows removes interior range index 0.
    const gp::Optional<DescriptorAllocation> exact2 = allocator.allocate(2u);
    ASSERT_TRUE(exact2.hasValue());
    EXPECT_EQ(*exact2, DescriptorAllocation(0u, 2u));

    // Remaining single slot at index 7 is still allocatable.
    const gp::Optional<DescriptorAllocation> last1 = allocator.allocate(1u);
    ASSERT_TRUE(last1.hasValue());
    EXPECT_EQ(*last1, DescriptorAllocation(7u, 1u));
    EXPECT_FALSE(allocator.allocate(1u).hasValue());

    allocator.free(*b);
    allocator.free(*d);
    allocator.free(*fit3);
    allocator.free(*exact2);
    allocator.free(*last1);
    EXPECT_EQ(statCounter.load(gp::MemoryOrder::Relaxed), 0);
}

TEST(DescriptorAllocatorTest, FreeBinarySearchCoalescingAllDirectionsAndIsolatedInserts)
{
    const ScopedDescriptorTracking tracking(true);

    // 10 contiguous 2-slot blocks: [0..1], [2..3], ..., [18..19].
    DescriptorAllocator allocator(20u);
    gp::Array<DescriptorAllocation, 10u> blocks{};
    for (gp::UInt32 i = 0u; i < 10u; ++i)
    {
        const gp::Optional<DescriptorAllocation> alloc = allocator.allocate(2u);
        ASSERT_TRUE(alloc.hasValue());
        blocks[i] = *alloc;
    }

    // Create multiple disjoint free ranges so binary search narrows both left and right.
    // Free indices: 1 ([2..3]), 5 ([10..11]), 9 ([18..19]).
    allocator.free(blocks[1]);
    allocator.free(blocks[5]);
    allocator.free(blocks[9]);

    // Isolated insertion before first range (emplaceAt(0)): free 0? No, 0 neighbors 1!
    // Free 3 ([6..7]) -> isolated between [2..3] and [10..11], hits emplaceAt(index) / emplaceAt(index + 1).
    allocator.free(blocks[3]);

    // Left-only coalesce: free 2 ([4..5]) bridges [2..3] and [6..7] (both-sides coalesce).
    allocator.free(blocks[2]);

    // Right-only coalesce (offset < first, end == first): free 0 ([0..1]) grows [2..7] to [0..7].
    allocator.free(blocks[0]);

    // Left-only coalesce (offset > last, offset - 1 == last): free 6 ([12..13]) grows [10..11] to [10..13].
    allocator.free(blocks[6]);

    // Isolated insert between [10..13] and [18..19]? Free 7 ([14..15]) grows [10..13] to [10..15].
    allocator.free(blocks[7]);

    // Both-sides bridge: free 4 ([8..9]) merges [0..7] + [8..9] + [10..15] -> [0..15].
    allocator.free(blocks[4]);

    // Final bridge: free 8 ([16..17]) merges [0..15] + [16..17] + [18..19] -> [0..19].
    allocator.free(blocks[8]);

    // Verify full 20-slot contiguous allocation succeeds.
    const gp::Optional<DescriptorAllocation> full = allocator.allocate(20u);
    ASSERT_TRUE(full.hasValue());
    EXPECT_EQ(*full, DescriptorAllocation(0u, 20u));
    allocator.free(*full);
}

TEST(DescriptorAllocatorTest, FreeHandlesBatchEmptyAndIgnoreInvalid)
{
    const ScopedDescriptorTracking tracking(true);

    DescriptorAllocator allocator(8u);

    const view::DescriptorHandle h0 = allocator.allocate(view::DescriptorType::TextureSRV);
    const view::DescriptorHandle h1 = allocator.allocate(view::DescriptorType::BufferUAV);
    const view::DescriptorHandle h2 = allocator.allocate(view::DescriptorType::Sampler);
    const view::DescriptorHandle invalid{};

    EXPECT_TRUE(h0.isValid());
    EXPECT_EQ(h0.getIndex(), 0u);
    EXPECT_EQ(h0.getType(), view::DescriptorType::TextureSRV);
    EXPECT_EQ(h1.getIndex(), 1u);
    EXPECT_EQ(h2.getIndex(), 2u);

    // Empty batch, invalid handle, and zero-count/invalid-offset frees are safe no-ops.
    allocator.free(gp::ConstVectorView<view::DescriptorHandle>{});
    allocator.free(invalid);
    allocator.free(DescriptorAllocation(gp::NumericLimits<gp::UInt32>::max(), 1u));
    allocator.free(DescriptorAllocation(0u, 0u));

    const gp::Array<view::DescriptorHandle, 3u> batch{ h0, invalid, h2 };
    allocator.free(batch);
    allocator.free(h1);

    DescriptorAllocatorRange range;
    EXPECT_FALSE(allocator.getAllocatedRange(range));
}

TEST(DescriptorAllocatorTest, ResizeGrowAndAllocateMergesOrAppends)
{
    const ScopedDescriptorTracking tracking(true);

    // Case 1: Tail is free -> extends existing trailing range.
    {
        DescriptorAllocator allocator(8u);
        const gp::Optional<DescriptorAllocation> first = allocator.allocate(4u);
        ASSERT_TRUE(first.hasValue());

        const gp::Optional<DescriptorAllocation> grown = allocator.resizeGrowAndAllocate(16u, 8u);
        ASSERT_TRUE(grown.hasValue());
        EXPECT_EQ(allocator.getCapacity(), 16u);
        EXPECT_EQ(*grown, DescriptorAllocation(4u, 8u));

        allocator.free(*first);
        allocator.free(*grown);
    }

    // Case 2: Heap is completely full -> grows by extending the zero-size trailing range at end.
    {
        DescriptorAllocator allocator(4u);
        const gp::Optional<DescriptorAllocation> all = allocator.allocate(4u);
        ASSERT_TRUE(all.hasValue());

        const gp::Optional<DescriptorAllocation> next = allocator.resizeGrowAndAllocate(8u, 4u);
        ASSERT_TRUE(next.hasValue());
        EXPECT_EQ(allocator.getCapacity(), 8u);
        EXPECT_EQ(*next, DescriptorAllocation(4u, 4u));

        allocator.free(*all);
        allocator.free(*next);
    }

    // Case 3: Trailing slot is allocated while an earlier hole exists -> appends a new end range.
    {
        DescriptorAllocator allocator(8u);
        const gp::Optional<DescriptorAllocation> a = allocator.allocate(4u);
        const gp::Optional<DescriptorAllocation> b = allocator.allocate(4u);
        ASSERT_TRUE(a.hasValue());
        ASSERT_TRUE(b.hasValue());

        allocator.free(*a);

        const gp::Optional<DescriptorAllocation> big = allocator.resizeGrowAndAllocate(16u, 6u);
        ASSERT_TRUE(big.hasValue());
        EXPECT_EQ(allocator.getCapacity(), 16u);
        EXPECT_EQ(*big, DescriptorAllocation(8u, 6u));

        allocator.free(*b);
        allocator.free(*big);
    }
}

TEST(DescriptorAllocatorTest, GetAllocatedRangeTracksTightBoundsFullAndSingleSlot)
{
    // Full heap and single-slot capacity edge cases.
    {
        DescriptorAllocator single(1u);
        DescriptorAllocatorRange range;
        EXPECT_FALSE(single.getAllocatedRange(range));

        const gp::Optional<DescriptorAllocation> only = single.allocate(1u);
        ASSERT_TRUE(only.hasValue());
        ASSERT_TRUE(single.getAllocatedRange(range));
        EXPECT_EQ(range, DescriptorAllocatorRange(0u, 0u));

        single.free(*only);
        EXPECT_FALSE(single.getAllocatedRange(range));
    }

    {
        DescriptorAllocator allocator(16u);
        DescriptorAllocatorRange range;

        EXPECT_FALSE(allocator.getAllocatedRange(range));

        const gp::Optional<DescriptorAllocation> a = allocator.allocate(4u);   // [0, 3]
        const gp::Optional<DescriptorAllocation> b = allocator.allocate(4u);   // [4, 7]
        const gp::Optional<DescriptorAllocation> c = allocator.allocate(4u);   // [8, 11]
        const gp::Optional<DescriptorAllocation> d = allocator.allocate(4u);   // [12, 15]
        ASSERT_TRUE(a.hasValue());
        ASSERT_TRUE(b.hasValue());
        ASSERT_TRUE(c.hasValue());
        ASSERT_TRUE(d.hasValue());

        // 100% full heap -> [0, 15].
        ASSERT_TRUE(allocator.getAllocatedRange(range));
        EXPECT_EQ(range, DescriptorAllocatorRange(0u, 15u));

        allocator.free(*d);
        ASSERT_TRUE(allocator.getAllocatedRange(range));
        EXPECT_EQ(range, DescriptorAllocatorRange(0u, 11u));

        allocator.free(*a);
        ASSERT_TRUE(allocator.getAllocatedRange(range));
        EXPECT_EQ(range, DescriptorAllocatorRange(4u, 11u));

        allocator.free(*c);
        ASSERT_TRUE(allocator.getAllocatedRange(range));
        EXPECT_EQ(range, DescriptorAllocatorRange(4u, 7u));

        allocator.free(*b);
        EXPECT_FALSE(allocator.getAllocatedRange(range));
    }
}

TEST(DescriptorAllocatorTest, MultiThreadedConcurrentAllocateAndFree)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kSlotsPerThread = 64u;
    constexpr gp::UInt32 kIterations = 200u;

    DescriptorStatCounter statCounter{ 0 };
    DescriptorStatCounter* const statPtrs[1] = { &statCounter };

    DescriptorAllocator allocator(kThreadCount * kSlotsPerThread, statPtrs);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back(
            [&allocator]() -> void
        {
            std::vector<view::DescriptorHandle> localHandles;
            localHandles.reserve(kSlotsPerThread);

            for (gp::UInt32 iter = 0u; iter < kIterations; ++iter)
            {
                localHandles.clear();
                for (gp::UInt32 i = 0u; i < kSlotsPerThread; ++i)
                {
                    const view::DescriptorHandle handle = allocator.allocate(view::DescriptorType::BufferSRV);
                    ASSERT_TRUE(handle.isValid());
                    localHandles.push_back(handle);
                }

                for (const view::DescriptorHandle handle: localHandles)
                {
                    allocator.free(handle);
                }
            }
        }
        );
    }

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(statCounter.load(gp::MemoryOrder::Acquire), 0);

    DescriptorAllocatorRange range;
    EXPECT_FALSE(allocator.getAllocatedRange(range));
}

}   // namespace gp::rhi::allocator::tests
