// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/OffsetHeapDescriptorAllocator.hpp"
#include "templates/NumericLimits.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::allocator::tests
{

TEST(OffsetHeapDescriptorAllocatorTest, ShiftsHandlesAndSpansByHeapOffset)
{
    constexpr gp::UInt32 kHeapOffset = 1000u;
    constexpr gp::UInt32 kCapacity = 8u;

    DescriptorStatCounter statCounter{ 0 };
    DescriptorStatCounter* const statPtrs[1] = { &statCounter };

    OffsetHeapDescriptorAllocator allocator(view::DescriptorTypeMask::Sampler, kCapacity, kHeapOffset, statPtrs);

    EXPECT_EQ(allocator.getCapacity(), kCapacity);
    EXPECT_EQ(allocator.getHeapOffset(), kHeapOffset);
    EXPECT_EQ(allocator.getTypeMask(), view::DescriptorTypeMask::Sampler);
    EXPECT_TRUE(allocator.handlesAllocation(view::DescriptorType::Sampler));
    EXPECT_FALSE(allocator.handlesAllocation(view::DescriptorType::TextureSRV));
    EXPECT_TRUE(allocator.handlesAllocations(view::DescriptorTypeMask::Sampler));

    // Invalid handle and invalid/empty span frees are safe no-ops without underflow.
    allocator.free(view::DescriptorHandle{});
    allocator.free(DescriptorAllocation(gp::NumericLimits<gp::UInt32>::max(), 1u));
    allocator.free(DescriptorAllocation(kHeapOffset, 0u));

    const view::DescriptorHandle h0 = allocator.allocate(view::DescriptorType::Sampler);
    ASSERT_TRUE(h0.isValid());
    EXPECT_EQ(h0.getIndex(), kHeapOffset + 0u);
    EXPECT_EQ(h0.getType(), view::DescriptorType::Sampler);

    const gp::Optional<DescriptorAllocation> span = allocator.allocate(4u);
    ASSERT_TRUE(span.hasValue());
    EXPECT_EQ(*span, DescriptorAllocation(kHeapOffset + 1u, 4u));

    const gp::Optional<DescriptorAllocation> remaining = allocator.allocate(3u);
    ASSERT_TRUE(remaining.hasValue());
    EXPECT_EQ(*remaining, DescriptorAllocation(kHeapOffset + 5u, 3u));

    // Exhausted allocator returns invalid handle and nullopt.
    EXPECT_FALSE(allocator.allocate(view::DescriptorType::Sampler).isValid());
    EXPECT_FALSE(allocator.allocate(1u).hasValue());
    EXPECT_EQ(statCounter.load(gp::MemoryOrder::Relaxed), 8);

    // Freeing shifts indices back down cleanly and coalesces.
    allocator.free(h0);
    allocator.free(*span);
    allocator.free(*remaining);
    EXPECT_EQ(statCounter.load(gp::MemoryOrder::Relaxed), 0);

    // Full capacity is available again starting at kHeapOffset.
    const gp::Optional<DescriptorAllocation> full = allocator.allocate(kCapacity);
    ASSERT_TRUE(full.hasValue());
    EXPECT_EQ(*full, DescriptorAllocation(kHeapOffset, kCapacity));

    allocator.free(*full);
    EXPECT_EQ(statCounter.load(gp::MemoryOrder::Relaxed), 0);
}

}   // namespace gp::rhi::allocator::tests
