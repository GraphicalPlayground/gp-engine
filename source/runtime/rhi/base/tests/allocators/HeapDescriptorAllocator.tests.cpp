// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/HeapDescriptorAllocator.hpp"
#include "containers/arrays/Array.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::allocator::tests
{

TEST(HeapDescriptorAllocatorTest, TypeMaskFilteringAndHandlesQueries)
{
    const HeapDescriptorAllocator standardHeap(view::DescriptorTypeMask::Standard, 32u);

    EXPECT_EQ(standardHeap.getCapacity(), 32u);
    EXPECT_EQ(standardHeap.getTypeMask(), view::DescriptorTypeMask::Standard);
    EXPECT_TRUE(standardHeap.handlesAllocation(view::DescriptorType::BufferSRV));
    EXPECT_TRUE(standardHeap.handlesAllocation(view::DescriptorType::TextureUAV));
    EXPECT_TRUE(standardHeap.handlesAllocation(view::DescriptorType::CBV));
    EXPECT_FALSE(standardHeap.handlesAllocation(view::DescriptorType::Sampler));
    EXPECT_FALSE(standardHeap.handlesAllocation(view::DescriptorType::Invalid));

    EXPECT_TRUE(standardHeap.handlesAllocations(view::DescriptorTypeMask::None));
    EXPECT_TRUE(standardHeap.handlesAllocations(view::DescriptorTypeMask::SRV));
    EXPECT_TRUE(standardHeap.handlesAllocations(view::DescriptorTypeMask::SRV | view::DescriptorTypeMask::UAV));
    EXPECT_FALSE(standardHeap.handlesAllocations(view::DescriptorTypeMask::SRV | view::DescriptorTypeMask::Sampler));
}

TEST(HeapDescriptorAllocatorTest, AllocateFreeSingleBatchExhaustionRangeAndResize)
{
    DescriptorStatCounter statCounter{ 0 };
    DescriptorStatCounter* const statPtrs[1] = { &statCounter };

    HeapDescriptorAllocator heap(view::DescriptorTypeMask::SRV, 8u, statPtrs);

    const view::DescriptorHandle h0 = heap.allocate(view::DescriptorType::BufferSRV);
    const view::DescriptorHandle h1 = heap.allocate(view::DescriptorType::TextureSRV);
    const view::DescriptorHandle h2 = heap.allocate(view::DescriptorType::TypedBufferSRV);
    const gp::Optional<DescriptorAllocation> span = heap.allocate(5u);

    ASSERT_TRUE(h0.isValid());
    ASSERT_TRUE(h1.isValid());
    ASSERT_TRUE(h2.isValid());
    ASSERT_TRUE(span.hasValue());
    EXPECT_EQ(h0.getIndex(), 0u);
    EXPECT_EQ(h1.getIndex(), 1u);
    EXPECT_EQ(h2.getIndex(), 2u);
    EXPECT_EQ(*span, DescriptorAllocation(3u, 5u));
    EXPECT_EQ(statCounter.load(gp::MemoryOrder::Relaxed), 8);

    // Exhaustion returns invalid handle and nullopt.
    EXPECT_FALSE(heap.allocate(view::DescriptorType::BufferSRV).isValid());
    EXPECT_FALSE(heap.allocate(1u).hasValue());

    DescriptorAllocatorRange range;
    ASSERT_TRUE(heap.getAllocatedRange(range));
    EXPECT_EQ(range, DescriptorAllocatorRange(0u, 7u));

    const gp::Optional<DescriptorAllocation> grown = heap.resizeGrowAndAllocate(16u, 4u);
    ASSERT_TRUE(grown.hasValue());
    EXPECT_EQ(heap.getCapacity(), 16u);
    EXPECT_EQ(*grown, DescriptorAllocation(8u, 4u));

    // Empty batch and invalid single handle are safe no-ops.
    heap.free(gp::ConstVectorView<view::DescriptorHandle>{});
    heap.free(view::DescriptorHandle{});

    // Single-handle free + batch free + span free.
    heap.free(h2);
    const gp::Array<view::DescriptorHandle, 2u> handles{ h0, h1 };
    heap.free(handles);
    heap.free(*span);
    heap.free(*grown);

    EXPECT_EQ(statCounter.load(gp::MemoryOrder::Relaxed), 0);
    EXPECT_FALSE(heap.getAllocatedRange(range));
}

}   // namespace gp::rhi::allocator::tests
