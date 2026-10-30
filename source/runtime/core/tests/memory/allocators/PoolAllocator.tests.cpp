// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/PoolAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>
#include <utility>

namespace gp::memory::tests
{

TEST(PoolAllocatorTest, DefaultUninitializedState)
{
    const PoolAllocator uninitialized{};

    EXPECT_FALSE(uninitialized.isValid());
    EXPECT_EQ(uninitialized.getBlockSize(), 0u);
    EXPECT_EQ(uninitialized.getBlockCount(), 0u);
    EXPECT_EQ(uninitialized.getUsedBlockCount(), 0u);
    EXPECT_EQ(uninitialized.getFreeBlockCount(), 0u);
    EXPECT_EQ(uninitialized.getCapacity(), 0u);
    EXPECT_STREQ(uninitialized.getName(), "PoolAllocator");
    EXPECT_FALSE(uninitialized.owns(nullptr));
}

TEST(PoolAllocatorTest, SequentialAddressOrderOnColdStartAndLifoFreeListReuse)
{
    constexpr gp::USize kBlockSize = 64u;
    constexpr gp::USize kBlockCount = 8u;
    constexpr gp::UInt32 kBlockAlignment = 32u;

    PoolAllocator pool(kBlockSize, kBlockCount, kBlockAlignment, nullptr, "Test.PoolSlab");
    ASSERT_TRUE(pool.isValid());
    EXPECT_EQ(pool.getBlockSize(), 64u);
    EXPECT_EQ(pool.getBlockAlignment(), 32u);
    EXPECT_EQ(pool.getBlockCount(), kBlockCount);
    EXPECT_EQ(pool.getFreeBlockCount(), kBlockCount);
    EXPECT_EQ(pool.getCapacity(), kBlockSize * kBlockCount);

    void* blocks[kBlockCount] = {};
    for (gp::USize i = 0u; i < kBlockCount; ++i)
    {
        blocks[i] = pool.allocate(48u, 16u);
        ASSERT_NE(blocks[i], nullptr);
        EXPECT_TRUE(gp::memory::isAligned(blocks[i], kBlockAlignment));
        EXPECT_TRUE(pool.owns(blocks[i]));

        // Verify sequential forward address order on initial cold start for cache locality
        if (i > 0u)
        {
            const gp::UIntPtr prevAddr = reinterpret_cast<gp::UIntPtr>(blocks[i - 1u]);
            const gp::UIntPtr currAddr = reinterpret_cast<gp::UIntPtr>(blocks[i]);
            EXPECT_EQ(currAddr - prevAddr, kBlockSize);
        }
    }

    EXPECT_EQ(pool.getUsedBlockCount(), kBlockCount);
    EXPECT_EQ(pool.getFreeBlockCount(), 0u);

    // Pool is now exhausted; next allocation must return nullptr
    EXPECT_EQ(pool.allocate(32u, 16u), nullptr);

    // Free blocks[2] and blocks[6], then re-allocate and verify O(1) LIFO free-list head pop
    pool.deallocate(blocks[2], 48u);
    pool.deallocate(blocks[6], 48u);
    EXPECT_EQ(pool.getFreeBlockCount(), 2u);

    EXPECT_EQ(pool.allocate(32u, 16u), blocks[6]);
    EXPECT_EQ(pool.allocate(32u, 16u), blocks[2]);
    EXPECT_EQ(pool.getFreeBlockCount(), 0u);
}

TEST(PoolAllocatorTest, RejectsOversizedOrOverAlignedRequestsAndSupportsInPlaceReallocate)
{
    PoolAllocator pool(32u, 4u, 16u, nullptr, "Test.PoolBounds");
    ASSERT_TRUE(pool.isValid());

    EXPECT_EQ(pool.allocate(0u), nullptr);
    EXPECT_EQ(pool.allocate(33u, 16u), nullptr);
    EXPECT_EQ(pool.allocate(16u, 32u), nullptr);
    EXPECT_EQ(pool.getActualAllocationSize(0u, 16u), 0u);
    EXPECT_EQ(pool.getActualAllocationSize(33u, 16u), 0u);
    EXPECT_EQ(pool.getActualAllocationSize(16u, 32u), 0u);
    EXPECT_EQ(pool.getActualAllocationSize(12u, 8u), 32u);

    void* block = pool.allocate(16u, 8u);
    ASSERT_NE(block, nullptr);

    // Reallocate within block stride succeeds in place in O(1)
    EXPECT_EQ(pool.reallocate(block, 16u, 32u, 16u), block);
    // Reallocate exceeding block stride fails and returns nullptr
    EXPECT_EQ(pool.reallocate(block, 32u, 64u, 16u), nullptr);
    // Reallocate to 0 frees the block and returns nullptr
    EXPECT_EQ(pool.reallocate(block, 32u, 0u, 16u), nullptr);
    EXPECT_EQ(pool.getUsedBlockCount(), 0u);
}

TEST(PoolAllocatorTest, ExternalBufferResetAndMoveSemantics)
{
    alignas(64) gp::UInt8 rawSlab[512]{};
    // Pass an intentionally unaligned start pointer (rawSlab + 4) to verify internal alignment adjustment
    PoolAllocator pool(rawSlab + 4u, sizeof(rawSlab) - 4u, 64u, 64u, "Test.PoolExternal");
    ASSERT_TRUE(pool.isValid());
    EXPECT_EQ(pool.getBlockSize(), 64u);
    EXPECT_EQ(pool.getBlockAlignment(), 64u);
    EXPECT_EQ(pool.getBlockCount(), 7u);

    void* block = pool.allocate(64u, 64u);
    ASSERT_NE(block, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(block, 64u));
    EXPECT_EQ(pool.getUsedBlockCount(), 1u);

    PoolAllocator movedPool(std::move(pool));
    EXPECT_FALSE(pool.isValid());
    EXPECT_TRUE(movedPool.isValid());
    EXPECT_TRUE(movedPool.owns(block));

    movedPool.reset();
    EXPECT_EQ(movedPool.getUsedBlockCount(), 0u);
    EXPECT_EQ(movedPool.getFreeBlockCount(), 7u);
}

}   // namespace gp::memory::tests
