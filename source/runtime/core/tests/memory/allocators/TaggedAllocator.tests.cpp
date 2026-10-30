// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/TaggedAllocator.hpp"
#include "memory/allocators/AlignedHeapAllocator.hpp"
#include "memory/allocators/FixedBufferAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::memory::tests
{

TEST(TaggedAllocatorTest, DefaultNameAndZeroInitialStats)
{
    const TaggedAllocator<HeapAllocator> defaultTagged{};
    EXPECT_STREQ(defaultTagged.getName(), "TaggedAllocator");
    EXPECT_EQ(defaultTagged.getStats().allocatedBytes, 0u);
    EXPECT_EQ(defaultTagged.getStats().peakAllocatedBytes, 0u);
    EXPECT_EQ(defaultTagged.getStats().activeAllocations, 0ull);
    EXPECT_EQ(defaultTagged.getStats().totalAllocations, 0ull);
    EXPECT_EQ(defaultTagged.getStats().totalDeallocations, 0ull);

    const TaggedAllocator<HeapAllocator> nullNameTagged(nullptr);
    EXPECT_STREQ(nullNameTagged.getName(), "TaggedAllocator");
}

TEST(TaggedAllocatorTest, AccuratelyTracksAllocatedBytesPeakAndCountsAcrossLifecycle)
{
    TaggedAllocator<Simd32HeapAllocator> tagged("Physics.Broadphase");
    EXPECT_STREQ(tagged.getName(), "Physics.Broadphase");

    void* blockA = tagged.allocate(128u, 16u);
    void* blockB = tagged.allocate(256u, 16u);
    ASSERT_NE(blockA, nullptr);
    ASSERT_NE(blockB, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(blockA, 32u));
    EXPECT_TRUE(gp::memory::isAligned(blockB, 32u));
    EXPECT_TRUE(tagged.owns(blockA));

    EXPECT_EQ(tagged.getStats().allocatedBytes, 384u);
    EXPECT_EQ(tagged.getStats().peakAllocatedBytes, 384u);
    EXPECT_EQ(tagged.getStats().activeAllocations, 2ull);
    EXPECT_EQ(tagged.getStats().totalAllocations, 2ull);

    // Grow blockA from 128 to 512 bytes -> total allocated becomes 512 + 256 = 768 bytes
    blockA = tagged.reallocate(blockA, 128u, 512u, 16u);
    ASSERT_NE(blockA, nullptr);
    EXPECT_EQ(tagged.getStats().allocatedBytes, 768u);
    EXPECT_EQ(tagged.getStats().peakAllocatedBytes, 768u);
    EXPECT_EQ(tagged.getStats().activeAllocations, 2ull);
    EXPECT_EQ(tagged.getStats().totalAllocations, 3ull);

    // Shrink blockA from 512 to 64 bytes -> total allocated becomes 64 + 256 = 320 bytes, peak stays 768
    blockA = tagged.reallocate(blockA, 512u, 64u, 16u);
    ASSERT_NE(blockA, nullptr);
    EXPECT_EQ(tagged.getStats().allocatedBytes, 320u);
    EXPECT_EQ(tagged.getStats().peakAllocatedBytes, 768u);

    // Reallocate blockB to 0 bytes -> acts as deallocate
    EXPECT_EQ(tagged.reallocate(blockB, 256u, 0u, 16u), nullptr);
    EXPECT_EQ(tagged.getStats().allocatedBytes, 64u);
    EXPECT_EQ(tagged.getStats().activeAllocations, 1ull);
    EXPECT_EQ(tagged.getStats().totalDeallocations, 1ull);

    tagged.deallocate(blockA, 64u, 16u);
    EXPECT_EQ(tagged.getStats().allocatedBytes, 0u);
    EXPECT_EQ(tagged.getStats().activeAllocations, 0ull);
    EXPECT_EQ(tagged.getStats().totalDeallocations, 2ull);
    EXPECT_EQ(tagged.getStats().peakAllocatedBytes, 768u);
}

TEST(TaggedAllocatorTest, HandlesFailedAllocationAndReallocationGracefully)
{
    TaggedAllocator<FixedBufferAllocator<64u, 16u>> boundedTagged("Test.BoundedTag");

    // Request exceeding 64-byte inner fixed buffer fails and records 0 stats
    EXPECT_EQ(boundedTagged.allocate(128u, 16u), nullptr);
    EXPECT_EQ(boundedTagged.getStats().allocatedBytes, 0u);
    EXPECT_EQ(boundedTagged.getStats().activeAllocations, 0ull);

    void* validBlock = boundedTagged.allocate(32u, 16u);
    ASSERT_NE(validBlock, nullptr);
    EXPECT_EQ(boundedTagged.getStats().allocatedBytes, 32u);
    EXPECT_EQ(boundedTagged.getStats().activeAllocations, 1ull);

    // Reallocation exceeding 64-byte capacity fails; original 32-byte stats remain intact
    EXPECT_EQ(boundedTagged.reallocate(validBlock, 32u, 128u, 16u), nullptr);
    EXPECT_EQ(boundedTagged.getStats().allocatedBytes, 32u);
    EXPECT_EQ(boundedTagged.getStats().activeAllocations, 1ull);

    boundedTagged.deallocate(validBlock, 32u, 16u);
    EXPECT_EQ(boundedTagged.getStats().allocatedBytes, 0u);
}

}   // namespace gp::memory::tests
