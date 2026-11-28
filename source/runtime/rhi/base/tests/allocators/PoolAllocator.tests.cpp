// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/PoolAllocator.hpp"
#include "allocators/MemoryPool.hpp"
#include "allocators/PoolResource.hpp"
#include "maths/base/Scalar.hpp"
#include "platforms/base/system/Thread.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::allocator::tests
{

namespace
{

class TestPoolResource final : public PoolResource
{
public:
    PoolAllocationData* activeAllocation{ nullptr };
    gp::UInt32 moveCount{ 0u };
};

class TestPoolAllocator final : public PoolAllocator
{
public:
    TestPoolAllocator(
        const gp::UInt64 defaultPoolSize,
        const gp::UInt32 poolAlignment,
        const gp::UInt32 maxAllocationSize,
        const PoolFreeListOrder freeListOrder,
        const bool defragEnabled,
        const PoolAllocationStrategy allocationStrategy = PoolAllocationStrategy::Immediate,
        const gp::UInt16 overflowPoolsCount = 1u
    ) noexcept
        : PoolAllocator(
              defaultPoolSize,
              poolAlignment,
              maxAllocationSize,
              freeListOrder,
              defragEnabled,
              allocationStrategy,
              overflowPoolsCount
          )
    {}

    [[nodiscard]] bool allocate(
        const gp::UInt32 sizeInBytes,
        const gp::UInt32 alignment,
        const PoolResourceTypes resourceType,
        PoolAllocationData& outAllocation
    ) noexcept
    {
        return tryAllocateInternal(sizeInBytes, alignment, resourceType, outAllocation);
    }

    void free(PoolAllocationData& allocation) noexcept
    {
        deallocateInternal(allocation);
    }

    void triggerSortPools() noexcept
    {
        sortPools();
    }

    void releasePoolSlot(const gp::USize index) noexcept
    {
        if (index < m_pools.size() && m_pools[index] != nullptr)
        {
            m_pools[index]->destroy();
            delete m_pools[index];
            m_pools[index] = nullptr;
        }
    }

    [[nodiscard]] gp::USize getPoolCount() const noexcept
    {
        return m_pools.size();
    }

    [[nodiscard]] gp::USize getOverflowPoolCount() const noexcept
    {
        return m_overflowPools.size();
    }

    [[nodiscard]] gp::UInt32 getPoolAllocationOrderAt(const gp::USize index) const noexcept
    {
        return m_poolAllocationOrder[index];
    }

    [[nodiscard]] gp::UInt32 getCreatedPoolCount() const noexcept
    {
        return m_createdPoolCount.load(gp::MemoryOrder::Acquire);
    }

    [[nodiscard]] gp::UInt32 getDefragMoveCount() const noexcept
    {
        return m_defragMoveCount;
    }

    void setRejectDefragRequests(const bool reject) noexcept
    {
        m_rejectDefragRequests = reject;
    }

protected:
    [[nodiscard]] virtual MemoryPool* createNewPool(
        const gp::Int16 poolIndex,
        const gp::UInt32 minimumAllocationSize,
        const PoolResourceTypes allocationResourceType
    ) noexcept override
    {
        m_createdPoolCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        const gp::UInt64 poolSize = gp::math::max(m_defaultPoolSize, static_cast<gp::UInt64>(minimumAllocationSize));
        MemoryPool* const pool =
            new MemoryPool(poolIndex, poolSize, m_poolAlignment, allocationResourceType, m_freeListOrder);
        pool->init();
        return pool;
    }

    [[nodiscard]] virtual bool handleDefragRequest(
        const ContextArray& contexts, PoolAllocationData* const sourceBlock, PoolAllocationData& tmpTargetBlock
    ) noexcept override
    {
        static_cast<void>(contexts);
        if (m_rejectDefragRequests)
        {
            m_pools[static_cast<gp::USize>(tmpTargetBlock.getPoolIndex())]->deallocate(tmpTargetBlock);
            return false;
        }

        TestPoolResource* const owner = static_cast<TestPoolResource*>(sourceBlock->getOwner());
        if (owner == nullptr)
        {
            m_pools[static_cast<gp::USize>(tmpTargetBlock.getPoolIndex())]->deallocate(tmpTargetBlock);
            return false;
        }

        PoolAllocationData oldBlock;
        oldBlock.moveFrom(*sourceBlock, true);
        m_pools[static_cast<gp::USize>(oldBlock.getPoolIndex())]->deallocate(oldBlock);

        sourceBlock->moveFrom(tmpTargetBlock, false);
        sourceBlock->setOwner(owner);
        owner->activeAllocation = sourceBlock;
        ++owner->moveCount;
        ++m_defragMoveCount;
        return true;
    }

private:
    gp::thread::Atomic<gp::UInt32> m_createdPoolCount{ 0u };
    gp::UInt32 m_defragMoveCount{ 0u };
    bool m_rejectDefragRequests{ false };
};

struct ScopedPoolAllocatorSettings final
{
    gp::Float32 prevFraction{ 0.9f };
    gp::Int32 prevMaxPools{ 1 };
    bool prevValidate{ false };

    ScopedPoolAllocatorSettings() noexcept
        : prevFraction(PoolAllocator::getDefragSizeFraction())
        , prevMaxPools(PoolAllocator::getDefragMaxPoolsToClear())
        , prevValidate(PoolAllocator::isLinkedListValidationEnabled())
    {}

    ~ScopedPoolAllocatorSettings() noexcept
    {
        PoolAllocator::setDefragSizeFraction(prevFraction);
        PoolAllocator::setDefragMaxPoolsToClear(prevMaxPools);
        PoolAllocator::setLinkedListValidationEnabled(prevValidate);
    }
};

}   // namespace

TEST(PoolAllocatorTest, ImmediateAllocateGrowAndStatsAccumulation)
{
    const ScopedPoolAllocatorSettings guard;
    PoolAllocator::setLinkedListValidationEnabled(true);

    TestPoolAllocator allocator(1024u, 256u, 1024u, PoolFreeListOrder::SortBySize, false);
    allocator.initialize(PoolResourceTypes::Buffers);

    PoolAllocationData a;
    PoolAllocationData b;
    PoolAllocationData c;

    ASSERT_TRUE(allocator.allocate(500u, 256u, PoolResourceTypes::Buffers, a));
    ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, b));
    EXPECT_EQ(allocator.getPoolCount(), 1u);

    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, c));
    EXPECT_EQ(allocator.getPoolCount(), 2u);
    EXPECT_EQ(c.getPoolIndex(), 1);

    // Seed non-zero incoming values to verify accumulation semantics.
    gp::UInt32 allocated = 100u;
    gp::UInt32 used = 10u;
    gp::UInt32 freeBytes = 20u;
    gp::UInt32 endFree = 30u;
    gp::UInt32 waste = 5u;
    gp::UInt32 pageCount = 1u;
    gp::UInt32 fullPageCount = 2u;

    allocator.updateMemoryStats(allocated, used, freeBytes, endFree, waste, pageCount, fullPageCount);

    EXPECT_EQ(allocated, 100u + 2048u);
    EXPECT_EQ(used, 10u + 500u + 512u + 256u);
    EXPECT_EQ(freeBytes, 20u + 768u);
    EXPECT_EQ(endFree, 30u + 768u);
    EXPECT_EQ(waste, 5u + 12u);
    EXPECT_EQ(pageCount, 1u + 2u);
    EXPECT_EQ(fullPageCount, 2u + 1u);

    allocator.free(a);
    allocator.free(b);
    allocator.free(c);
}

TEST(PoolAllocatorTest, SortPoolsAndNullPoolSlotReuse)
{
    // Defrag enabled: fullest pool first, nullptr slots sorted first as UINT32_MAX used.
    {
        TestPoolAllocator allocator(1024u, 256u, 1024u, PoolFreeListOrder::SortBySize, true);
        PoolAllocationData p0a;
        PoolAllocationData p0b;
        PoolAllocationData p1a;
        PoolAllocationData p1b;
        PoolAllocationData p1c;

        ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, p0a));
        ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, p0b));
        ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p1a));
        ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p1b));
        ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p1c));

        allocator.free(p0a);
        allocator.free(p0b);

        allocator.triggerSortPools();
        EXPECT_EQ(allocator.getPoolAllocationOrderAt(0u), 1u);
        EXPECT_EQ(allocator.getPoolAllocationOrderAt(1u), 0u);

        // Release pool 0 leaving a nullptr slot; verify updateMemoryStats skips nullptr and next overflow reuses slot
        // 0.
        allocator.releasePoolSlot(0u);
        allocator.triggerSortPools();

        gp::UInt32 allocated = 0u;
        gp::UInt32 used = 0u;
        gp::UInt32 freeBytes = 0u;
        gp::UInt32 endFree = 0u;
        gp::UInt32 waste = 0u;
        gp::UInt32 pageCount = 0u;
        gp::UInt32 fullPageCount = 0u;
        allocator.updateMemoryStats(allocated, used, freeBytes, endFree, waste, pageCount, fullPageCount);
        EXPECT_EQ(pageCount, 1u);
        EXPECT_EQ(allocated, 1024u);

        // Fill pool 1 and request one more block -> should reuse nullptr slot 0 without growing m_pools.
        PoolAllocationData p1d;
        PoolAllocationData reused;
        ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p1d));
        ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, reused));
        EXPECT_EQ(allocator.getPoolCount(), 2u);
        EXPECT_EQ(reused.getPoolIndex(), 0);

        allocator.free(p1a);
        allocator.free(p1b);
        allocator.free(p1c);
        allocator.free(p1d);
        allocator.free(reused);
    }

    // Defrag disabled: smaller pool size first, then equal-size index tie-breaking.
    {
        TestPoolAllocator allocator(1024u, 256u, 2048u, PoolFreeListOrder::SortBySize, false);
        PoolAllocationData big;
        PoolAllocationData small1;
        PoolAllocationData small2;

        ASSERT_TRUE(allocator.allocate(2048u, 256u, PoolResourceTypes::Buffers, big));      // pool 0 (2048)
        ASSERT_TRUE(allocator.allocate(1024u, 256u, PoolResourceTypes::Buffers, small1));   // pool 1 (1024)
        ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, small2));    // pool 2 (1024)

        allocator.triggerSortPools();
        EXPECT_EQ(allocator.getPoolAllocationOrderAt(0u), 1u);
        EXPECT_EQ(allocator.getPoolAllocationOrderAt(1u), 2u);
        EXPECT_EQ(allocator.getPoolAllocationOrderAt(2u), 0u);

        allocator.free(big);
        allocator.free(small1);
        allocator.free(small2);
    }
}

TEST(PoolAllocatorTest, DefragEarlyOutBranches)
{
    const ScopedPoolAllocatorSettings guard;
    const ContextArray contexts{};

    // 1. Defrag disabled -> immediate return.
    {
        TestPoolAllocator allocator(1024u, 256u, 1024u, PoolFreeListOrder::SortBySize, false);
        PoolAllocationData a;
        ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, a));
        gp::UInt32 copySize = 0u;
        allocator.defrag(contexts, 1024u, copySize);
        EXPECT_EQ(copySize, 0u);
        allocator.free(a);
    }

    // 2. Fewer than 2 non-full pools -> no defrag.
    {
        TestPoolAllocator allocator(1024u, 256u, 1024u, PoolFreeListOrder::SortBySize, true);
        PoolAllocationData a;
        ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, a));
        TestPoolResource res;
        a.setOwner(&res);
        a.unlock();

        gp::UInt32 copySize = 0u;
        allocator.defrag(contexts, 1024u, copySize);
        EXPECT_EQ(copySize, 0u);

        PoolAllocationData aLocked;
        aLocked.moveFrom(a, true);
        allocator.free(aLocked);
    }

    // 3. All pools above DefragSizeFraction -> maxPoolsToDefrag == 0 -> no defrag.
    {
        PoolAllocator::setDefragSizeFraction(0.2f);
        TestPoolAllocator allocator(1024u, 256u, 1024u, PoolFreeListOrder::SortBySize, true);
        PoolAllocationData p0a;
        PoolAllocationData p0b;
        PoolAllocationData p1a;

        ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, p0a));
        ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, p0b));
        ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, p1a));
        allocator.free(p0b);   // Pool 0 = 50% (> 0.2), Pool 1 = 50% (> 0.2).

        TestPoolResource res;
        p1a.setOwner(&res);
        p1a.unlock();

        gp::UInt32 copySize = 0u;
        allocator.defrag(contexts, 1024u, copySize);
        EXPECT_EQ(copySize, 0u);
        EXPECT_EQ(p1a.getPoolIndex(), 1);

        PoolAllocationData p1aLocked;
        p1aLocked.moveFrom(p1a, true);
        allocator.free(p0a);
        allocator.free(p1aLocked);
    }
}

TEST(PoolAllocatorTest, DefragMigratesUnlockedOwnedBlocksRespectsBudgetAndTimeSlicing)
{
    const ScopedPoolAllocatorSettings guard;
    PoolAllocator::setLinkedListValidationEnabled(true);
    PoolAllocator::setDefragSizeFraction(0.9f);
    PoolAllocator::setDefragMaxPoolsToClear(1);

    TestPoolAllocator allocator(1024u, 256u, 1024u, PoolFreeListOrder::SortBySize, true);

    PoolAllocationData p0a;
    PoolAllocationData p0b;
    PoolAllocationData p0c;
    PoolAllocationData p1a;
    PoolAllocationData p1b;
    PoolAllocationData p1c;

    ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, p0a));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p0b));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p0c));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p1a));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p1b));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p1c));

    // Free p0b and p0c so pool 0 has 512 free (50% used), pool 1 has 768 used (75% used).
    // Swap roles by keeping pool 0 fuller: instead free p0b (pool 0 = 768 used, 256 free) and p1c (pool 1 = 512 used).
    allocator.free(p0b);
    allocator.free(p1c);

    TestPoolResource res1a;
    TestPoolResource res1b;
    p1a.setOwner(&res1a);
    p1b.setOwner(&res1b);
    p1b.unlock();   // p1a stays locked so it must be skipped.

    const ContextArray contexts{};
    gp::UInt32 currentCopySize = 0u;

    // Budget cap of 256 stops after moving p1b (256 bytes).
    allocator.defrag(contexts, 256u, currentCopySize);

    EXPECT_EQ(currentCopySize, 256u);
    EXPECT_EQ(allocator.getDefragMoveCount(), 1u);
    EXPECT_EQ(p1b.getPoolIndex(), 0);
    EXPECT_EQ(p1a.getPoolIndex(), 1);

    // Calling defrag again exercises m_lastDefragPoolIndex progression/wrap-around cleanly.
    allocator.defrag(contexts, 512u, currentCopySize);
    EXPECT_EQ(currentCopySize, 256u);

    PoolAllocationData p1bLocked;
    p1bLocked.moveFrom(p1b, true);

    allocator.free(p0a);
    allocator.free(p0c);
    allocator.free(p1a);
    allocator.free(p1bLocked);
}

TEST(PoolAllocatorTest, DefragAllPoolsModeAndRejectedMovesDoNotChargeBudget)
{
    const ScopedPoolAllocatorSettings guard;
    PoolAllocator::setLinkedListValidationEnabled(true);
    PoolAllocator::setDefragSizeFraction(0.9f);
    PoolAllocator::setDefragMaxPoolsToClear(-1);

    TestPoolAllocator allocator(1024u, 256u, 1024u, PoolFreeListOrder::SortBySize, true);

    PoolAllocationData p0a;
    PoolAllocationData p0b;
    PoolAllocationData p1a;

    ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, p0a));
    ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, p0b));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, p1a));

    allocator.free(p0b);

    TestPoolResource res;
    p1a.setOwner(&res);
    p1a.unlock();

    allocator.setRejectDefragRequests(true);
    const ContextArray contexts{};
    gp::UInt32 currentCopySize = 0u;
    allocator.defrag(contexts, 1024u, currentCopySize);
    EXPECT_EQ(currentCopySize, 0u);
    EXPECT_EQ(p1a.getPoolIndex(), 1);

    allocator.setRejectDefragRequests(false);
    allocator.defrag(contexts, 1024u, currentCopySize);
    EXPECT_EQ(currentCopySize, 256u);
    EXPECT_EQ(p1a.getPoolIndex(), 0);

    PoolAllocationData p1aLocked;
    p1aLocked.moveFrom(p1a, true);
    allocator.free(p0a);
    allocator.free(p1aLocked);
}

TEST(PoolAllocatorTest, AsyncStrategyPromotesReplenishesAndFallsBackInline)
{
    const ScopedPoolAllocatorSettings guard;
    PoolAllocator::setLinkedListValidationEnabled(true);

    TestPoolAllocator allocator(
        1024u, 256u, 2048u, PoolFreeListOrder::SortBySize, false, PoolAllocationStrategy::Async, 1u
    );
    allocator.initialize(PoolResourceTypes::Buffers);

    // Wait for initial background overflow pool creation.
    for (gp::UInt32 spin = 0u; spin < 200000u && allocator.getCreatedPoolCount() < 2u; ++spin)
    {
        gp::platform::Thread::yieldExecution();
    }

    EXPECT_EQ(allocator.getPoolCount(), 1u);

    PoolAllocationData a;
    PoolAllocationData b;
    PoolAllocationData c;
    PoolAllocationData oversized;

    ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, a));
    ASSERT_TRUE(allocator.allocate(512u, 256u, PoolResourceTypes::Buffers, b));

    // Promotes overflow pool into slot 1 and schedules async replenishment.
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, c));
    EXPECT_EQ(allocator.getPoolCount(), 2u);
    EXPECT_EQ(c.getPoolIndex(), 1);

    // Oversized allocation (2048 > 1024 overflow pool) triggers inline fallback creation.
    ASSERT_TRUE(allocator.allocate(2048u, 256u, PoolResourceTypes::Buffers, oversized));
    EXPECT_EQ(allocator.getPoolCount(), 3u);
    EXPECT_EQ(oversized.getPoolIndex(), 2);

    // Free pool 0, release slot 0 to nullptr, and verify async path reuses the nullptr slot cleanly.
    allocator.free(a);
    allocator.free(b);
    allocator.releasePoolSlot(0u);

    PoolAllocationData fill1a;
    PoolAllocationData fill1b;
    PoolAllocationData fill1c;
    PoolAllocationData reuseSlot0;
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, fill1a));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, fill1b));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, fill1c));
    ASSERT_TRUE(allocator.allocate(256u, 256u, PoolResourceTypes::Buffers, reuseSlot0));
    EXPECT_EQ(reuseSlot0.getPoolIndex(), 0);

    allocator.free(c);
    allocator.free(oversized);
    allocator.free(fill1a);
    allocator.free(fill1b);
    allocator.free(fill1c);
    allocator.free(reuseSlot0);

    allocator.destroy();
    EXPECT_EQ(allocator.getPoolCount(), 0u);
    EXPECT_EQ(allocator.getOverflowPoolCount(), 0u);
}

}   // namespace gp::rhi::allocator::tests
