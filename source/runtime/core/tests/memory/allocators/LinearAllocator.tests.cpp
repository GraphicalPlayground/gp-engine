// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/LinearAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>
#include <utility>

namespace gp::memory::tests
{

TEST(LinearAllocatorTest, DefaultUninitializedState)
{
    const LinearAllocator uninitialized{};

    EXPECT_FALSE(uninitialized.isValid());
    EXPECT_EQ(uninitialized.getCapacity(), 0u);
    EXPECT_EQ(uninitialized.getUsedBytes(), 0u);
    EXPECT_EQ(uninitialized.getRemainingBytes(), 0u);
    EXPECT_STREQ(uninitialized.getName(), "LinearAllocator");
    EXPECT_FALSE(uninitialized.owns(nullptr));
}

TEST(LinearAllocatorTest, MallocBackedInitializationAndSequentialAlignedAllocations)
{
    LinearAllocator arena(4096u, nullptr, "Test.LinearMalloc");
    ASSERT_TRUE(arena.isValid());
    EXPECT_EQ(arena.getCapacity(), 4096u);
    EXPECT_EQ(arena.getRemainingBytes(), 4096u);
    EXPECT_STREQ(arena.getName(), "Test.LinearMalloc");

    // 1. Allocate 13 bytes (8-byte default alignment)
    void* ptr1 = arena.allocate(13u, 8u);
    ASSERT_NE(ptr1, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(ptr1, 8u));
    EXPECT_TRUE(arena.owns(ptr1));
    EXPECT_EQ(arena.getUsedBytes(), 13u);

    // 2. Allocate 64 bytes with 64-byte alignment -> must pad cursor up to 64-byte boundary
    void* ptr2 = arena.allocate(64u, 64u);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(ptr2, 64u));
    EXPECT_TRUE(arena.owns(ptr2));
    EXPECT_GE(arena.getUsedBytes(), 13u + 64u);
    EXPECT_EQ(arena.getStats().activeAllocations, 2ull);
    EXPECT_EQ(arena.getStats().totalAllocations, 2ull);
}

TEST(LinearAllocatorTest, TopOfBumperInPlaceReallocationAndDeallocation)
{
    LinearAllocator arena(2048u, nullptr, "Test.LinearInPlace");
    ASSERT_TRUE(arena.isValid());

    void* topBlock = arena.allocate(64u, 16u);
    ASSERT_NE(topBlock, nullptr);
    EXPECT_EQ(arena.getUsedBytes(), 64u);

    gp::UInt8* bytes = static_cast<gp::UInt8*>(topBlock);
    for (gp::USize i = 0u; i < 64u; ++i)
    {
        bytes[i] = static_cast<gp::UInt8>(0x50u + i);
    }

    // Grow in place at the top of the bump cursor: must return the EXACT same address in O(1)!
    void* grownInPlace = arena.reallocate(topBlock, 64u, 256u, 16u);
    EXPECT_EQ(grownInPlace, topBlock);
    EXPECT_EQ(arena.getUsedBytes(), 256u);

    // Shrink in place at the top of the bump cursor: must return the EXACT same address and rewind offset!
    void* shrunkInPlace = arena.reallocate(grownInPlace, 256u, 96u, 16u);
    EXPECT_EQ(shrunkInPlace, topBlock);
    EXPECT_EQ(arena.getUsedBytes(), 96u);

    // Verify original payload was untouched
    const gp::UInt8* verifyBytes = static_cast<const gp::UInt8*>(shrunkInPlace);
    for (gp::USize i = 0u; i < 64u; ++i)
    {
        EXPECT_EQ(verifyBytes[i], static_cast<gp::UInt8>(0x50u + i));
    }

    // Deallocating the block sitting at the exact top of the cursor rewinds offset back to 0!
    arena.deallocate(shrunkInPlace, 96u, 16u);
    EXPECT_EQ(arena.getUsedBytes(), 0u);
    EXPECT_EQ(arena.getStats().activeAllocations, 0ull);
}

TEST(LinearAllocatorTest, NonTopReallocationRelocatesAndPreservesPayload)
{
    LinearAllocator arena(2048u, nullptr, "Test.LinearRelocate");
    ASSERT_TRUE(arena.isValid());

    void* first = arena.allocate(32u, 16u);
    void* second = arena.allocate(32u, 16u);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    gp::UInt8* firstBytes = static_cast<gp::UInt8*>(first);
    for (gp::USize i = 0u; i < 32u; ++i)
    {
        firstBytes[i] = static_cast<gp::UInt8>(i + 100u);
    }

    // Shrinking a non-top allocation returns the same pointer without advancing cursor
    const gp::USize usedBeforeShrink = arena.getUsedBytes();
    EXPECT_EQ(arena.reallocate(first, 32u, 16u, 16u), first);
    EXPECT_EQ(arena.getUsedBytes(), usedBeforeShrink);

    // Growing a non-top allocation allocates a new block at the top and copies existing bytes
    void* relocatedFirst = arena.reallocate(first, 32u, 128u, 16u);
    ASSERT_NE(relocatedFirst, nullptr);
    EXPECT_NE(relocatedFirst, first);

    const gp::UInt8* relocatedBytes = static_cast<const gp::UInt8*>(relocatedFirst);
    for (gp::USize i = 0u; i < 32u; ++i)
    {
        EXPECT_EQ(relocatedBytes[i], static_cast<gp::UInt8>(i + 100u));
    }
}

TEST(LinearAllocatorTest, ExternalBufferScopedMemoryMarkAndExhaustion)
{
    alignas(64) gp::UInt8 externalSlab[512]{};
    LinearAllocator arena(externalSlab, sizeof(externalSlab), "Test.LinearExternal");
    ASSERT_TRUE(arena.isValid());
    EXPECT_EQ(arena.getCapacity(), 512u);

    void* base = arena.allocate(128u, 16u);
    ASSERT_NE(base, nullptr);
    const gp::USize baseOffset = arena.getUsedBytes();

    {
        ScopedMemoryMark<LinearAllocator> markGuard(arena);
        void* temp = arena.allocate(256u, 32u);
        ASSERT_NE(temp, nullptr);
        EXPECT_EQ(arena.getUsedBytes(), baseOffset + 256u);

        // Request exceeding remaining capacity returns nullptr cleanly
        EXPECT_EQ(arena.allocate(256u, 16u), nullptr);
    }

    // Cursor rewound back to baseOffset upon scope exit
    EXPECT_EQ(arena.getUsedBytes(), baseOffset);
    EXPECT_EQ(arena.getStats().activeAllocations, 1ull);
    EXPECT_GE(arena.getStats().peakAllocatedBytes, baseOffset + 256u);

    arena.reset();
    EXPECT_EQ(arena.getUsedBytes(), 0u);
    EXPECT_EQ(arena.getStats().activeAllocations, 0ull);
    EXPECT_GE(arena.getStats().peakAllocatedBytes, baseOffset + 256u);
}

TEST(LinearAllocatorTest, MoveConstructionAndMoveAssignmentTransferOwnership)
{
    LinearAllocator source(1024u, nullptr, "Test.LinearMove");
    void* ptr = source.allocate(64u, 16u);
    ASSERT_NE(ptr, nullptr);

    LinearAllocator movedConstructed(std::move(source));
    EXPECT_FALSE(source.isValid());
    EXPECT_EQ(source.getCapacity(), 0u);
    EXPECT_TRUE(movedConstructed.isValid());
    EXPECT_EQ(movedConstructed.getCapacity(), 1024u);
    EXPECT_EQ(movedConstructed.getUsedBytes(), 64u);
    EXPECT_TRUE(movedConstructed.owns(ptr));

    LinearAllocator moveAssigned(512u, nullptr, "Test.LinearTarget");
    moveAssigned = std::move(movedConstructed);
    EXPECT_FALSE(movedConstructed.isValid());
    EXPECT_TRUE(moveAssigned.isValid());
    EXPECT_EQ(moveAssigned.getCapacity(), 1024u);
    EXPECT_TRUE(moveAssigned.owns(ptr));
}

}   // namespace gp::memory::tests
