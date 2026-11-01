// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/StackAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>
#include <utility>

namespace gp::memory::tests
{

TEST(StackAllocatorTest, DefaultUninitializedState)
{
    const StackAllocator uninitialized{};

    EXPECT_FALSE(uninitialized.isValid());
    EXPECT_EQ(uninitialized.getCapacity(), 0u);
    EXPECT_EQ(uninitialized.getUsedBytes(), 0u);
    EXPECT_EQ(uninitialized.getRemainingBytes(), 0u);
    EXPECT_STREQ(uninitialized.getName(), "StackAllocator");
    EXPECT_FALSE(uninitialized.owns(nullptr));
}

TEST(StackAllocatorTest, LifoAllocationAndDeallocationUnwindsExactOffsets)
{
    StackAllocator stack(4096u, nullptr, "Test.StackLifo");
    ASSERT_TRUE(stack.isValid());

    void* block1 = stack.allocate(32u, 8u);
    ASSERT_NE(block1, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(block1, 8u));
    const gp::USize offsetAfter1 = stack.getUsedBytes();
    EXPECT_GT(offsetAfter1, 32u);

    void* block2 = stack.allocate(128u, 32u);
    ASSERT_NE(block2, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(block2, 32u));
    const gp::USize offsetAfter2 = stack.getUsedBytes();
    EXPECT_GT(offsetAfter2, offsetAfter1);

    void* block3 = stack.allocate(64u, 64u);
    ASSERT_NE(block3, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(block3, 64u));
    EXPECT_GT(stack.getUsedBytes(), offsetAfter2);

    // Popping in strict LIFO order (block3 -> block2 -> block1) must unwind m_offset at each step
    stack.deallocate(block3, 64u, 64u);
    EXPECT_EQ(stack.getUsedBytes(), offsetAfter2);

    stack.deallocate(block2, 128u, 32u);
    EXPECT_EQ(stack.getUsedBytes(), offsetAfter1);

    stack.deallocate(block1, 32u, 8u);
    EXPECT_EQ(stack.getUsedBytes(), 0u);
    EXPECT_EQ(stack.getStats().activeAllocations, 0ull);
}

TEST(StackAllocatorTest, TopOfStackInPlaceReallocationUpdatesHeaderAndPreservesLifoUnwind)
{
    StackAllocator stack(2048u, nullptr, "Test.StackResize");
    ASSERT_TRUE(stack.isValid());

    void* bottom = stack.allocate(48u, 16u);
    ASSERT_NE(bottom, nullptr);
    const gp::USize offsetAfterBottom = stack.getUsedBytes();

    void* top = stack.allocate(64u, 16u);
    ASSERT_NE(top, nullptr);
    gp::UInt8* topBytes = static_cast<gp::UInt8*>(top);
    for (gp::USize i = 0u; i < 64u; ++i)
    {
        topBytes[i] = static_cast<gp::UInt8>(0x70u + i);
    }

    // Grow `top` in place at the top of the stack
    void* grownTop = stack.reallocate(top, 64u, 256u, 16u);
    EXPECT_EQ(grownTop, top);

    // Shrink `top` in place at the top of the stack
    void* shrunkTop = stack.reallocate(grownTop, 256u, 96u, 16u);
    EXPECT_EQ(shrunkTop, top);

    const gp::UInt8* verifyBytes = static_cast<const gp::UInt8*>(shrunkTop);
    for (gp::USize i = 0u; i < 64u; ++i)
    {
        EXPECT_EQ(verifyBytes[i], static_cast<gp::UInt8>(0x70u + i));
    }

    // Deallocating `shrunkTop` must still unwind back to `offsetAfterBottom` using its updated header!
    stack.deallocate(shrunkTop, 96u, 16u);
    EXPECT_EQ(stack.getUsedBytes(), offsetAfterBottom);

    stack.deallocate(bottom, 48u, 16u);
    EXPECT_EQ(stack.getUsedBytes(), 0u);
}

TEST(StackAllocatorTest, ExternalBufferScopedMemoryMarkAndMoveSemantics)
{
    alignas(64) gp::UInt8 externalBuffer[1024]{};
    StackAllocator stack(externalBuffer, sizeof(externalBuffer), "Test.StackExternal");
    ASSERT_TRUE(stack.isValid());

    void* base = stack.allocate(64u, 16u);
    ASSERT_NE(base, nullptr);
    const gp::USize baseOffset = stack.getUsedBytes();

    {
        ScopedMemoryMark<StackAllocator> guard(stack);
        EXPECT_NE(stack.allocate(128u, 16u), nullptr);
        EXPECT_NE(stack.allocate(128u, 32u), nullptr);
        EXPECT_GT(stack.getUsedBytes(), baseOffset);
    }

    EXPECT_EQ(stack.getUsedBytes(), baseOffset);

    // Move construction
    StackAllocator movedStack(std::move(stack));
    EXPECT_FALSE(stack.isValid());
    EXPECT_TRUE(movedStack.isValid());
    EXPECT_TRUE(movedStack.owns(base));
    EXPECT_TRUE(movedStack == movedStack);
    EXPECT_FALSE(movedStack == stack);

    movedStack.reset();
    EXPECT_EQ(movedStack.getUsedBytes(), 0u);
}

TEST(StackAllocatorTest, DefaultAlignmentGuaranteesEightByteMinimumAlignmentAfterOddSizedAllocation)
{
    StackAllocator stack(1024u, nullptr, "Test.StackDefaultAlignment");
    ASSERT_TRUE(stack.isValid());

    // Allocate 12 bytes with default alignment (kDefaultAlignment = 0) -> ends at a 4-byte-odd offset (20)
    void* oddBlock = stack.allocate(12u);
    ASSERT_NE(oddBlock, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(oddBlock, kMinimumAlignment));

    // Next allocation with default alignment MUST still be aligned to kMinimumAlignment (8 bytes), NOT 4 bytes!
    void* nextBlock = stack.allocate(16u);
    ASSERT_NE(nextBlock, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(nextBlock, kMinimumAlignment));

    stack.deallocate(nextBlock, 16u);
    stack.deallocate(oddBlock, 12u);
    EXPECT_EQ(stack.getUsedBytes(), 0u);
}

TEST(StackAllocatorTest, NonTopShrinkStaysInPlaceAndNonTopGrowRetiresOldAllocationStats)
{
    StackAllocator stack(2048u, nullptr, "Test.StackNonTopRealloc");
    ASSERT_TRUE(stack.isValid());

    void* block1 = stack.allocate(128u, 16u);
    void* block2 = stack.allocate(64u, 16u);
    ASSERT_NE(block1, nullptr);
    ASSERT_NE(block2, nullptr);
    EXPECT_EQ(stack.getStats().activeAllocations, 2ull);
    EXPECT_EQ(stack.getStats().allocatedBytes, 192u);

    const gp::USize usedBeforeShrink = stack.getUsedBytes();

    // Shrinking non-top block1 must stay in place without advancing cursor or allocating a new block
    void* shrunkBlock1 = stack.reallocate(block1, 128u, 64u, 16u);
    EXPECT_EQ(shrunkBlock1, block1);
    EXPECT_EQ(stack.getUsedBytes(), usedBeforeShrink);
    EXPECT_EQ(stack.getStats().activeAllocations, 2ull);
    EXPECT_EQ(stack.getStats().allocatedBytes, 128u);

    // Popping block2 then shrunkBlock1 in LIFO order must still unwind the stack all the way to 0!
    stack.deallocate(block2, 64u, 16u);
    stack.deallocate(shrunkBlock1, 64u, 16u);
    EXPECT_EQ(stack.getUsedBytes(), 0u);
    EXPECT_EQ(stack.getStats().activeAllocations, 0ull);
    EXPECT_EQ(stack.getStats().allocatedBytes, 0u);

    // Now test non-top grow: must relocate and retire old block1 from activeAllocations and allocatedBytes
    void* first = stack.allocate(64u, 16u);
    void* second = stack.allocate(32u, 16u);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(stack.getStats().activeAllocations, 2ull);
    EXPECT_EQ(stack.getStats().allocatedBytes, 96u);

    void* grownFirst = stack.reallocate(first, 64u, 256u, 16u);
    ASSERT_NE(grownFirst, nullptr);
    EXPECT_NE(grownFirst, first);
    EXPECT_EQ(stack.getStats().activeAllocations, 2ull);
    EXPECT_EQ(stack.getStats().allocatedBytes, 256u + 32u);
}

}   // namespace gp::memory::tests
