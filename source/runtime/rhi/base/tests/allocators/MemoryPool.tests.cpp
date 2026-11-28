// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/MemoryPool.hpp"
#include "allocators/PoolAllocator.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::allocator::tests
{

namespace
{

struct ScopedLinkedListValidation final
{
    bool previousState{ false };

    explicit ScopedLinkedListValidation(const bool enable) noexcept
        : previousState(PoolAllocator::isLinkedListValidationEnabled())
    {
        PoolAllocator::setLinkedListValidationEnabled(enable);
    }

    ~ScopedLinkedListValidation() noexcept
    {
        PoolAllocator::setLinkedListValidationEnabled(previousState);
    }
};

}   // namespace

TEST(MemoryPoolTest, StaticAlignmentHelpers)
{
    // Divisible alignment: no extra padding beyond aligning to poolAlignment.
    EXPECT_EQ(MemoryPool::getAlignedSize(1u, 256u, 16u), 256u);
    EXPECT_EQ(MemoryPool::getAlignedSize(256u, 256u, 64u), 256u);
    EXPECT_EQ(MemoryPool::getAlignedSize(257u, 256u, 64u), 512u);
    EXPECT_EQ(MemoryPool::getAlignedOffset(0u, 256u, 64u), 0u);
    EXPECT_EQ(MemoryPool::getAlignedOffset(256u, 256u, 64u), 256u);

    // Non-divisor alignment: poolAlignment=256, allocationAlignment=48 (256 % 48 == 16 != 0).
    EXPECT_EQ(MemoryPool::getAlignedSize(100u, 256u, 48u), 256u);
    EXPECT_EQ(MemoryPool::getAlignedSize(240u, 256u, 48u), 512u);
    EXPECT_EQ(MemoryPool::getAlignedOffset(0u, 256u, 48u), 0u);
    EXPECT_EQ(MemoryPool::getAlignedOffset(256u, 256u, 48u), 288u);
    EXPECT_EQ(MemoryPool::getAlignedOffset(288u, 256u, 48u), 288u);
}

TEST(MemoryPoolTest, InitialStateAndResourceTypeSupport)
{
    const ScopedLinkedListValidation validation(true);

    MemoryPool pool(
        4, 4096u, 256u, PoolResourceTypes::Buffers | PoolResourceTypes::NonRTDSTextures, PoolFreeListOrder::SortBySize
    );
    pool.init();

    EXPECT_EQ(pool.getPoolIndex(), 4);
    EXPECT_EQ(pool.getPoolSize(), 4096u);
    EXPECT_EQ(pool.getFreeSize(), 4096u);
    EXPECT_EQ(pool.getUsedSize(), 0u);
    EXPECT_EQ(pool.getPoolAlignment(), 256u);
    EXPECT_EQ(pool.getAlignmentWaste(), 0u);
    EXPECT_EQ(pool.getAllocatedBlocks(), 0u);
    EXPECT_TRUE(pool.isEmpty());
    EXPECT_FALSE(pool.isFull());
    EXPECT_TRUE(pool.isResourceTypeSupported(PoolResourceTypes::Buffers));
    EXPECT_TRUE(pool.isResourceTypeSupported(PoolResourceTypes::NonRTDSTextures));
    EXPECT_FALSE(pool.isResourceTypeSupported(PoolResourceTypes::RTDSTextures));
    EXPECT_EQ(pool.getSupportedResourceTypes(), PoolResourceTypes::Buffers | PoolResourceTypes::NonRTDSTextures);
}

TEST(MemoryPoolTest, DeferredAsyncPoolIndexAndInitFreeBlocks)
{
    const ScopedLinkedListValidation validation(true);

    MemoryPool pool(
        MemoryPool::kInvalidPoolIndex, 1024u, 64u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortBySize
    );
    pool.init();

    EXPECT_EQ(pool.getPoolIndex(), MemoryPool::kInvalidPoolIndex);
    EXPECT_EQ(pool.getFreeSize(), 1024u);

    pool.setPoolIndex(7);
    pool.initFreeBlocks();
    EXPECT_EQ(pool.getPoolIndex(), 7);

    PoolAllocationData alloc;
    ASSERT_TRUE(pool.tryAllocate(128u, 64u, PoolResourceTypes::Buffers, alloc));
    EXPECT_EQ(alloc.getPoolIndex(), 7);
    EXPECT_EQ(alloc.getOffset(), 0u);

    pool.deallocate(alloc);
    EXPECT_TRUE(pool.isEmpty());
}

TEST(MemoryPoolTest, DestroyIsIdempotentWithValidationEnabled)
{
    const ScopedLinkedListValidation validation(true);

    MemoryPool pool(0, 1024u, 256u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortBySize);
    pool.init();
    pool.destroy();
    pool.destroy();
}

TEST(MemoryPoolTest, AllocateExhaustionAndExactFill)
{
    const ScopedLinkedListValidation validation(true);

    MemoryPool pool(0, 1024u, 256u, PoolResourceTypes::All, PoolFreeListOrder::SortBySize);
    pool.init();

    PoolAllocationData a;
    PoolAllocationData b;
    PoolAllocationData c;
    PoolAllocationData d;
    PoolAllocationData overflow;

    ASSERT_TRUE(pool.tryAllocate(200u, 64u, PoolResourceTypes::Buffers, a));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, b));
    ASSERT_TRUE(pool.tryAllocate(100u, 16u, PoolResourceTypes::Buffers, c));
    ASSERT_TRUE(pool.tryAllocate(256u, 128u, PoolResourceTypes::Buffers, d));

    EXPECT_TRUE(pool.isFull());
    EXPECT_FALSE(pool.isEmpty());
    EXPECT_EQ(pool.getFreeSize(), 0u);
    EXPECT_EQ(pool.getUsedSize(), 1024u);
    EXPECT_EQ(pool.getAllocatedBlocks(), 4u);
    EXPECT_EQ(pool.getAlignmentWaste(), (256u - 200u) + (256u - 100u));

    EXPECT_FALSE(pool.tryAllocate(64u, 64u, PoolResourceTypes::Buffers, overflow));

    pool.deallocate(b);
    pool.deallocate(c);
    pool.deallocate(a);
    pool.deallocate(d);

    EXPECT_TRUE(pool.isEmpty());
    EXPECT_FALSE(pool.isFull());
    EXPECT_EQ(pool.getFreeSize(), 1024u);
    EXPECT_EQ(pool.getAlignmentWaste(), 0u);
    EXPECT_EQ(pool.getAllocatedBlocks(), 0u);
}

TEST(MemoryPoolTest, CoalescingPrevNextAndBothSides)
{
    const ScopedLinkedListValidation validation(true);

    MemoryPool pool(0, 1024u, 256u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortBySize);
    pool.init();

    PoolAllocationData b0;
    PoolAllocationData b1;
    PoolAllocationData b2;
    PoolAllocationData b3;

    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, b0));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, b1));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, b2));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, b3));

    pool.deallocate(b0);
    pool.deallocate(b2);

    PoolAllocationData big;
    EXPECT_FALSE(pool.tryAllocate(512u, 256u, PoolResourceTypes::Buffers, big));

    pool.deallocate(b1);

    ASSERT_TRUE(pool.tryAllocate(768u, 256u, PoolResourceTypes::Buffers, big));
    EXPECT_EQ(big.getOffset(), 0u);
    EXPECT_TRUE(pool.isFull());

    pool.deallocate(big);
    pool.deallocate(b3);
    EXPECT_TRUE(pool.isEmpty());
}

TEST(MemoryPoolTest, SortBySizeBinarySearchSelectsBestFitBlock)
{
    const ScopedLinkedListValidation validation(true);

    MemoryPool pool(0, 4096u, 256u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortBySize);
    pool.init();

    PoolAllocationData h256;   // [0, 256)
    PoolAllocationData p0;     // [256, 512)
    PoolAllocationData h512;   // [512, 1024)
    PoolAllocationData p1;     // [1024, 1280)
    PoolAllocationData h768;   // [1280, 2048)
    PoolAllocationData p2;     // [2048, 2304)

    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, h256));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, p0));
    ASSERT_TRUE(pool.tryAllocate(512u, 256u, PoolResourceTypes::Buffers, h512));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, p1));
    ASSERT_TRUE(pool.tryAllocate(768u, 256u, PoolResourceTypes::Buffers, h768));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, p2));

    // Free blocks sorted by size: 256 (@0), 512 (@512), 768 (@1280), 1792 (@2304).
    pool.deallocate(h256);
    pool.deallocate(h512);
    pool.deallocate(h768);

    // Request 768 bytes: index 0 (256) is too small, forcing the binary search loop to pick 768 @1280.
    PoolAllocationData fit768;
    ASSERT_TRUE(pool.tryAllocate(768u, 256u, PoolResourceTypes::Buffers, fit768));
    EXPECT_EQ(fit768.getOffset(), 1280u);

    // Request 512 bytes: binary search picks 512 @512.
    PoolAllocationData fit512;
    ASSERT_TRUE(pool.tryAllocate(512u, 256u, PoolResourceTypes::Buffers, fit512));
    EXPECT_EQ(fit512.getOffset(), 512u);

    // Request 2048 bytes: total free is 2048 (256 + 1792), but no single block fits -> fails cleanly.
    PoolAllocationData tooBig;
    EXPECT_FALSE(pool.tryAllocate(2048u, 256u, PoolResourceTypes::Buffers, tooBig));

    pool.deallocate(p0);
    pool.deallocate(p1);
    pool.deallocate(p2);
    pool.deallocate(fit768);
    pool.deallocate(fit512);
    EXPECT_TRUE(pool.isEmpty());
}

TEST(MemoryPoolTest, SortByOffsetSelectsLowestOffsetAndHandlesFragmentationFailure)
{
    const ScopedLinkedListValidation validation(true);

    MemoryPool pool(0, 2048u, 256u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortByOffset);
    pool.init();

    PoolAllocationData a;   // [0, 512)
    PoolAllocationData b;   // [512, 768)
    PoolAllocationData c;   // [768, 1024)
    PoolAllocationData d;   // [1024, 1536)
    PoolAllocationData e;   // [1536, 2048)

    ASSERT_TRUE(pool.tryAllocate(512u, 256u, PoolResourceTypes::Buffers, a));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, b));
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, c));
    ASSERT_TRUE(pool.tryAllocate(512u, 256u, PoolResourceTypes::Buffers, d));
    ASSERT_TRUE(pool.tryAllocate(512u, 256u, PoolResourceTypes::Buffers, e));

    // Free a (512 @0) and c (256 @768) -> total free is 768, max contiguous is 512.
    pool.deallocate(a);
    pool.deallocate(c);

    // Fragmentation failure: 768 total free, but no single block >= 768.
    PoolAllocationData tooBig;
    EXPECT_FALSE(pool.tryAllocate(768u, 256u, PoolResourceTypes::Buffers, tooBig));

    // First-fit by offset for 256 bytes picks offset 0 even though a tighter 256-byte hole exists at 768.
    PoolAllocationData firstFit;
    ASSERT_TRUE(pool.tryAllocate(256u, 256u, PoolResourceTypes::Buffers, firstFit));
    EXPECT_EQ(firstFit.getOffset(), 0u);

    pool.deallocate(b);
    pool.deallocate(d);
    pool.deallocate(e);
    pool.deallocate(firstFit);
    EXPECT_TRUE(pool.isEmpty());
}

TEST(MemoryPoolTest, NodePoolOverflowAllocatesAndDeletesCleanly)
{
    const ScopedLinkedListValidation validation(true);

    // Create > 32 disjoint free holes simultaneously so getNewAllocationData()/releaseAllocationData() exceed the
    // 32-node recycled pool capacity.
    constexpr gp::UInt32 kBlockCount = 80u;
    MemoryPool pool(0, kBlockCount * 64u, 64u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortByOffset);
    pool.init();

    gp::Vector<PoolAllocationData> blocks;
    blocks.resize(kBlockCount);
    for (gp::UInt32 i = 0u; i < kBlockCount; ++i)
    {
        ASSERT_TRUE(pool.tryAllocate(64u, 64u, PoolResourceTypes::Buffers, blocks[i]));
    }

    // Free every even block -> 40 disjoint free nodes (> 32 recycled pool cap).
    for (gp::UInt32 i = 0u; i < kBlockCount; i += 2u)
    {
        pool.deallocate(blocks[i]);
    }

    // Free every odd block -> coalesces back to 1 block, releasing 40 nodes (> 32 cap -> deletes surplus).
    for (gp::UInt32 i = 1u; i < kBlockCount; i += 2u)
    {
        pool.deallocate(blocks[i]);
    }

    EXPECT_TRUE(pool.isEmpty());
    EXPECT_EQ(pool.getAllocatedBlocks(), 0u);
}

}   // namespace gp::rhi::allocator::tests
