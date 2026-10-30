// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/FixedBufferAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::memory::tests
{

TEST(FixedBufferAllocatorTest, InitialStateAndCompileTimeCapacity)
{
    FixedBufferAllocator<256u, 32u> arena{};

    static_assert(FixedBufferAllocator<256u, 32u>::getCapacity() == 256u);
    static_assert(FixedBufferAllocator<256u, 32u>::kBufferAlignment == 32u);

    EXPECT_EQ(arena.getCapacity(), 256u);
    EXPECT_EQ(arena.getUsedBytes(), 0u);
    EXPECT_EQ(arena.getRemainingBytes(), 256u);
    EXPECT_NE(arena.getData(), nullptr);
    EXPECT_TRUE(gp::memory::isAligned(arena.getData(), 32u));
    EXPECT_FALSE(arena.owns(nullptr));
    EXPECT_EQ(arena.allocate(0u), nullptr);
    EXPECT_EQ(arena.getActualAllocationSize(0u), 0u);
    EXPECT_EQ(arena.getActualAllocationSize(512u), 0u);
}

TEST(FixedBufferAllocatorTest, BumpAllocationTopOfBumperResizeAndDeallocation)
{
    FixedBufferAllocator<512u, 64u> arena{};

    void* block = arena.allocate(64u, 32u);
    ASSERT_NE(block, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(block, 32u));
    EXPECT_TRUE(arena.owns(block));
    EXPECT_EQ(arena.getUsedBytes(), 64u);

    gp::UInt8* bytes = static_cast<gp::UInt8*>(block);
    for (gp::USize i = 0u; i < 64u; ++i)
    {
        bytes[i] = static_cast<gp::UInt8>(0x20u + i);
    }

    // Grow in place at top of fixed buffer: must return the exact same pointer in O(1)
    void* grown = arena.reallocate(block, 64u, 192u, 32u);
    EXPECT_EQ(grown, block);
    EXPECT_EQ(arena.getUsedBytes(), 192u);

    // Shrink in place at top of fixed buffer
    void* shrunk = arena.reallocate(grown, 192u, 96u, 32u);
    EXPECT_EQ(shrunk, block);
    EXPECT_EQ(arena.getUsedBytes(), 96u);

    // Deallocate top block rewinds offset to 0
    arena.deallocate(shrunk, 96u, 32u);
    EXPECT_EQ(arena.getUsedBytes(), 0u);
    EXPECT_EQ(arena.getStats().activeAllocations, 0ull);
}

TEST(FixedBufferAllocatorTest, ScopedMemoryMarkRewindAndNoHeapFallbackOnExhaustion)
{
    FixedBufferAllocator<256u, 16u> arena{};

    void* persistent = arena.allocate(64u, 16u);
    ASSERT_NE(persistent, nullptr);

    {
        ScopedMemoryMark<FixedBufferAllocator<256u, 16u>> guard(arena);
        void* temp = arena.allocate(128u, 16u);
        ASSERT_NE(temp, nullptr);
        EXPECT_EQ(arena.getUsedBytes(), 192u);
        EXPECT_EQ(arena.getRemainingBytes(), 64u);

        // Requesting 128 bytes when only 64 bytes remain must return nullptr (never falls back to heap)
        EXPECT_EQ(arena.allocate(128u, 16u), nullptr);
        EXPECT_EQ(arena.getUsedBytes(), 192u);
    }

    EXPECT_EQ(arena.getUsedBytes(), 64u);
    EXPECT_EQ(arena.getStats().peakAllocatedBytes, 192u);

    arena.reset();
    EXPECT_EQ(arena.getUsedBytes(), 0u);
    EXPECT_EQ(arena.getRemainingBytes(), 256u);
}

}   // namespace gp::memory::tests
