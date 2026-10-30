// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/InlineAllocator.hpp"
#include "memory/allocators/TaggedAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::memory::tests
{

TEST(InlineAllocatorTest, ServesSmallAllocationsFromInlineBufferWithZeroHeapTraffic)
{
    using TrackedInlineAllocator = InlineAllocator<128u, 16u, TaggedAllocator<HeapAllocator>>;
    TrackedInlineAllocator allocator(TaggedAllocator<HeapAllocator>("Test.InlineFallback"));

    EXPECT_EQ(allocator.getInlineUsedBytes(), 0u);
    EXPECT_EQ(allocator.getInlineRemainingBytes(), 128u);
    EXPECT_EQ(allocator.allocate(0u), nullptr);
    EXPECT_FALSE(allocator.isInlinePointer(nullptr));

    void* ptr = allocator.allocate(64u, 16u);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(allocator.isInlinePointer(ptr));
    EXPECT_TRUE(allocator.owns(ptr));
    EXPECT_EQ(allocator.getInlineUsedBytes(), 64u);
    EXPECT_EQ(allocator.getFallbackAllocator().getStats().totalAllocations, 0ull);

    // Grow in place inside the 128-byte inline buffer
    void* grownInline = allocator.reallocate(ptr, 64u, 112u, 16u);
    EXPECT_EQ(grownInline, ptr);
    EXPECT_TRUE(allocator.isInlinePointer(grownInline));
    EXPECT_EQ(allocator.getInlineUsedBytes(), 112u);
    EXPECT_EQ(allocator.getFallbackAllocator().getStats().totalAllocations, 0ull);

    // Shrink in place inside the inline buffer
    void* shrunkInline = allocator.reallocate(grownInline, 112u, 48u, 16u);
    EXPECT_EQ(shrunkInline, ptr);
    EXPECT_EQ(allocator.getInlineUsedBytes(), 48u);

    // Deallocating top inline block reclaims the inline buffer
    allocator.deallocate(shrunkInline, 48u, 16u);
    EXPECT_EQ(allocator.getInlineUsedBytes(), 0u);
}

TEST(InlineAllocatorTest, SeamlesslySpillsOverToFallbackAllocatorAndPreservesPayload)
{
    using TrackedInlineAllocator = InlineAllocator<128u, 16u, TaggedAllocator<HeapAllocator>>;
    TrackedInlineAllocator allocator(TaggedAllocator<HeapAllocator>("Test.InlineSpillover"));

    void* inlineBlock = allocator.allocate(80u, 16u);
    ASSERT_NE(inlineBlock, nullptr);
    EXPECT_TRUE(allocator.isInlinePointer(inlineBlock));

    gp::UInt8* inlineBytes = static_cast<gp::UInt8*>(inlineBlock);
    for (gp::USize i = 0u; i < 80u; ++i)
    {
        inlineBytes[i] = static_cast<gp::UInt8>(0x90u + (i & 0x3Fu));
    }

    // Grow from 80 bytes to 512 bytes -> exceeds 128-byte inline capacity and spills over to fallback!
    void* heapBlock = allocator.reallocate(inlineBlock, 80u, 512u, 16u);
    ASSERT_NE(heapBlock, nullptr);
    EXPECT_FALSE(allocator.isInlinePointer(heapBlock));
    EXPECT_TRUE(allocator.owns(heapBlock));

    // Inline buffer must be automatically reclaimed when the top inline block spills over to fallback
    EXPECT_EQ(allocator.getInlineUsedBytes(), 0u);
    EXPECT_EQ(allocator.getFallbackAllocator().getStats().activeAllocations, 1ull);
    EXPECT_EQ(allocator.getFallbackAllocator().getStats().allocatedBytes, 512u);

    const gp::UInt8* heapBytes = static_cast<const gp::UInt8*>(heapBlock);
    for (gp::USize i = 0u; i < 80u; ++i)
    {
        EXPECT_EQ(heapBytes[i], static_cast<gp::UInt8>(0x90u + (i & 0x3Fu)));
    }

    // Subsequent reallocate on the spilled block routes directly to the fallback allocator
    heapBlock = allocator.reallocate(heapBlock, 512u, 1024u, 16u);
    ASSERT_NE(heapBlock, nullptr);
    EXPECT_EQ(allocator.getFallbackAllocator().getStats().allocatedBytes, 1024u);

    allocator.deallocate(heapBlock, 1024u, 16u);
    EXPECT_EQ(allocator.getFallbackAllocator().getStats().activeAllocations, 0ull);
    EXPECT_EQ(allocator.getFallbackAllocator().getStats().allocatedBytes, 0u);
}

}   // namespace gp::memory::tests
