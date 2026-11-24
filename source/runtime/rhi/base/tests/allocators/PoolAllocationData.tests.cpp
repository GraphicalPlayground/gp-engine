// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/PoolAllocationData.hpp"
#include "allocators/MemoryPool.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "allocators/PoolResource.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::allocator::tests
{

namespace
{

class DummyPoolResource final : public PoolResource
{
public:
    gp::UInt32 id{ 0u };
};

}   // namespace

TEST(PoolAllocationDataTest, DefaultAndResetState)
{
    PoolAllocationData data;
    data.reset();

    EXPECT_FALSE(data.isHead());
    EXPECT_FALSE(data.isFree());
    EXPECT_FALSE(data.isAllocated());
    EXPECT_FALSE(data.isLocked());
    EXPECT_EQ(data.getSize(), 0u);
    EXPECT_EQ(data.getAlignment(), 0u);
    EXPECT_EQ(data.getOffset(), 0u);
    EXPECT_EQ(data.getPoolIndex(), MemoryPool::kInvalidPoolIndex);
    EXPECT_EQ(data.getNext(), nullptr);
    EXPECT_EQ(data.getPrev(), nullptr);
    EXPECT_EQ(data.getFirstAlias(), nullptr);
    EXPECT_EQ(data.getOwner(), nullptr);
}

TEST(PoolAllocationDataTest, InitAsHeadAndFree)
{
    PoolAllocationData head;
    head.initAsHead(3);

    EXPECT_TRUE(head.isHead());
    EXPECT_FALSE(head.isFree());
    EXPECT_FALSE(head.isAllocated());
    EXPECT_EQ(head.getPoolIndex(), 3);
    EXPECT_EQ(head.getNext(), &head);
    EXPECT_EQ(head.getPrev(), &head);

    PoolAllocationData freeBlock;
    freeBlock.initAsFree(3, 4096u, 256u, 512u);

    EXPECT_TRUE(freeBlock.isFree());
    EXPECT_FALSE(freeBlock.isHead());
    EXPECT_FALSE(freeBlock.isAllocated());
    EXPECT_EQ(freeBlock.getPoolIndex(), 3);
    EXPECT_EQ(freeBlock.getSize(), 4096u);
    EXPECT_EQ(freeBlock.getAlignment(), 256u);
    EXPECT_EQ(freeBlock.getOffset(), 512u);
}

TEST(PoolAllocationDataTest, InitAsAllocatedUnlockMoveFromAndMarkFree)
{
    MemoryPool pool(1, 1024u, 64u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortBySize);
    pool.init();

    PoolAllocationData alloc;
    ASSERT_TRUE(pool.tryAllocate(100u, 16u, PoolResourceTypes::Buffers, alloc));

    EXPECT_TRUE(alloc.isAllocated());
    EXPECT_TRUE(alloc.isLocked());
    EXPECT_EQ(alloc.getSize(), 100u);
    EXPECT_EQ(alloc.getAlignment(), 16u);
    EXPECT_EQ(alloc.getOffset(), 0u);
    EXPECT_EQ(alloc.getPoolIndex(), 1);

    alloc.unlock();
    EXPECT_FALSE(alloc.isLocked());

    PoolAllocationData moved;
    moved.moveFrom(alloc, true);
    EXPECT_FALSE(alloc.isAllocated());
    EXPECT_TRUE(moved.isAllocated());
    EXPECT_TRUE(moved.isLocked());
    EXPECT_EQ(moved.getSize(), 100u);
    EXPECT_EQ(moved.getOffset(), 0u);

    pool.deallocate(moved);
    EXPECT_TRUE(pool.isEmpty());
}

TEST(PoolAllocationDataTest, MarkFreeSnapsOffsetDownAndSizeUpDirectly)
{
    // Build a standalone two-node circular list (head <-> freeBlock) to exercise initAsAllocated + markFree directly.
    PoolAllocationData head;
    head.initAsHead(0);

    // Non-divisor alignment: poolAlignment = 256, allocationAlignment = 48.
    // Free block starts at offset 256 -> aligned offset becomes 288.
    PoolAllocationData freeBlock;
    freeBlock.initAsFree(0, 512u, 256u, 256u);

    // Wire head <-> freeBlock using a temporary MemoryPool to verify aligned offset and markFree snapping.
    MemoryPool pool(0, 4096u, 256u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortByOffset);
    pool.init();

    PoolAllocationData first;
    ASSERT_TRUE(pool.tryAllocate(100u, 48u, PoolResourceTypes::Buffers, first));
    EXPECT_EQ(first.getOffset(), 0u);

    PoolAllocationData second;
    ASSERT_TRUE(pool.tryAllocate(100u, 48u, PoolResourceTypes::Buffers, second));
    EXPECT_EQ(second.getOffset(), 288u);

    // Directly verify markFree() on an isolated allocation block obtained from a local pool.
    {
        MemoryPool scratchPool(0, 1024u, 256u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortByOffset);
        scratchPool.init();

        PoolAllocationData pad;
        PoolAllocationData target;
        ASSERT_TRUE(scratchPool.tryAllocate(100u, 48u, PoolResourceTypes::Buffers, pad));
        ASSERT_TRUE(scratchPool.tryAllocate(100u, 48u, PoolResourceTypes::Buffers, target));
        ASSERT_EQ(target.getOffset(), 288u);

        target.markFree(256u, 48u);
        EXPECT_TRUE(target.isFree());
        EXPECT_EQ(target.getSize(), 256u);
        EXPECT_EQ(target.getOffset(), 256u);
        EXPECT_EQ(target.getAlignment(), 256u);

        // Leave scratchPool destruction clean by not enabling linked-list validation on the manually mutated node.
        scratchPool.deallocate(pad);
    }

    pool.deallocate(second);
    pool.deallocate(first);
    EXPECT_TRUE(pool.isEmpty());
    EXPECT_EQ(pool.getAlignmentWaste(), 0u);
}

TEST(PoolAllocationDataTest, OwnerLifecycleSetTransferClear)
{
    PoolAllocationData data;
    data.reset();

    DummyPoolResource ownerA;
    ownerA.id = 10u;
    DummyPoolResource ownerB;
    ownerB.id = 20u;

    EXPECT_EQ(data.getOwner(), nullptr);

    data.setOwner(&ownerA);
    EXPECT_EQ(data.getOwner(), &ownerA);

    data.transferOwner(&ownerB);
    EXPECT_EQ(data.getOwner(), &ownerB);

    data.clearOwner();
    EXPECT_EQ(data.getOwner(), nullptr);
}

TEST(PoolAllocationDataTest, AliasAddRemoveHeadMiddleTailAndMoveFromPrimaryAndAlias)
{
    MemoryPool pool(2, 1024u, 64u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortBySize);
    pool.init();

    PoolAllocationData primary;
    ASSERT_TRUE(pool.tryAllocate(128u, 64u, PoolResourceTypes::Buffers, primary));

    PoolAllocationData aliasA;
    PoolAllocationData aliasB;
    PoolAllocationData aliasC;
    aliasA.reset();
    aliasB.reset();
    aliasC.reset();

    // No-op when removing a detached node.
    aliasA.removeAlias();
    EXPECT_EQ(aliasA.getFirstAlias(), nullptr);

    // Prepend order: primary -> aliasC -> aliasB -> aliasA.
    primary.addAlias(&aliasA);
    primary.addAlias(&aliasB);
    primary.addAlias(&aliasC);

    EXPECT_EQ(primary.getFirstAlias(), &aliasC);
    EXPECT_EQ(aliasC.getPrev(), nullptr);
    EXPECT_EQ(aliasC.getNext(), &aliasB);
    EXPECT_EQ(aliasB.getPrev(), &aliasC);
    EXPECT_EQ(aliasB.getNext(), &aliasA);
    EXPECT_EQ(aliasA.getPrev(), &aliasB);
    EXPECT_EQ(aliasA.getNext(), nullptr);

    // Moving primary updates all attached aliases to point at newPrimary.
    PoolAllocationData newPrimary;
    newPrimary.moveFrom(primary, true);
    EXPECT_EQ(newPrimary.getFirstAlias(), &aliasC);
    EXPECT_EQ(primary.getFirstAlias(), nullptr);

    // Remove middle alias (aliasB): list becomes aliasC -> aliasA.
    aliasB.removeAlias();
    EXPECT_EQ(aliasC.getNext(), &aliasA);
    EXPECT_EQ(aliasA.getPrev(), &aliasC);
    EXPECT_EQ(aliasB.getNext(), nullptr);
    EXPECT_EQ(aliasB.getPrev(), nullptr);

    // Remove tail alias (aliasA): list becomes aliasC.
    aliasA.removeAlias();
    EXPECT_EQ(newPrimary.getFirstAlias(), &aliasC);
    EXPECT_EQ(aliasC.getNext(), nullptr);

    // Remove head alias (aliasC): list becomes empty.
    aliasC.removeAlias();
    EXPECT_EQ(newPrimary.getFirstAlias(), nullptr);

    // Exercise the alias-node branch of moveFrom(): allocated node with m_previousAllocation == nullptr and valid
    // m_aliasAllocation pointing to its parent primary.
    MemoryPool tempPool(2, 1024u, 64u, PoolResourceTypes::Buffers, PoolFreeListOrder::SortBySize);
    tempPool.init();

    PoolAllocationData allocatedAlias;
    ASSERT_TRUE(tempPool.tryAllocate(128u, 64u, PoolResourceTypes::Buffers, allocatedAlias));
    // Attach allocatedAlias as an alias of newPrimary; addAlias sets m_previousAllocation = nullptr and
    // m_aliasAllocation = &newPrimary while keeping AllocationType::Allocated.
    newPrimary.addAlias(&allocatedAlias);
    ASSERT_EQ(newPrimary.getFirstAlias(), &allocatedAlias);
    ASSERT_EQ(allocatedAlias.getPrev(), nullptr);

    PoolAllocationData relocatedAlias;
    relocatedAlias.moveFrom(allocatedAlias, false);

    EXPECT_TRUE(relocatedAlias.isAllocated());
    EXPECT_FALSE(relocatedAlias.isLocked());
    EXPECT_EQ(newPrimary.getFirstAlias(), &relocatedAlias);
    EXPECT_FALSE(allocatedAlias.isAllocated());
    EXPECT_EQ(allocatedAlias.getFirstAlias(), nullptr);

    relocatedAlias.removeAlias();
    EXPECT_EQ(newPrimary.getFirstAlias(), nullptr);

    pool.deallocate(newPrimary);
    EXPECT_TRUE(pool.isEmpty());
}

TEST(PoolAllocationDataTest, PoolMoveRequestBindsContextsAndDestination)
{
    const ContextArray contexts{};
    PoolAllocationData dest;
    dest.initAsFree(0, 256u, 64u, 0u);

    const PoolMoveRequest request{ contexts, &dest };
    EXPECT_EQ(&request.contexts, &contexts);
    EXPECT_EQ(request.newData, &dest);
}

}   // namespace gp::rhi::allocator::tests
