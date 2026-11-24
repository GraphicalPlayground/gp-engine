// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/OffsetHeapDescriptorAllocator.hpp"
#include "templates/NumericLimits.hpp"

namespace gp::rhi::allocator
{

OffsetHeapDescriptorAllocator::OffsetHeapDescriptorAllocator(
    const view::DescriptorTypeMask typeMask,
    const gp::UInt32 descriptorCount,
    const gp::UInt32 heapOffset,
    const gp::ConstVectorView<DescriptorStatCounter*> stats
) noexcept
    : HeapDescriptorAllocator(typeMask, descriptorCount, stats)
    , m_heapOffset(heapOffset)
{}

view::DescriptorHandle OffsetHeapDescriptorAllocator::allocate(const view::DescriptorType type) noexcept
{
    const view::DescriptorHandle allocatorHandle = HeapDescriptorAllocator::allocate(type);
    if (allocatorHandle.isValid())
    {
        return view::DescriptorHandle(allocatorHandle.getType(), allocatorHandle.getIndex() + m_heapOffset);
    }

    return view::DescriptorHandle();
}

void OffsetHeapDescriptorAllocator::free(const view::DescriptorHandle handle) noexcept
{
    if (handle.isValid())
    {
        const view::DescriptorHandle adjustedHandle(handle.getType(), handle.getIndex() - m_heapOffset);
        HeapDescriptorAllocator::free(adjustedHandle);
    }
}

gp::Optional<DescriptorAllocation> OffsetHeapDescriptorAllocator::allocate(const gp::UInt32 numDescriptors) noexcept
{
    gp::Optional<DescriptorAllocation> allocation = HeapDescriptorAllocator::allocate(numDescriptors);
    if (allocation.hasValue())
    {
        allocation->startIndex += m_heapOffset;
    }

    return allocation;
}

void OffsetHeapDescriptorAllocator::free(DescriptorAllocation allocation) noexcept
{
    if (allocation.startIndex == gp::NumericLimits<gp::UInt32>::max() || allocation.count == 0u)
    {
        return;
    }

    allocation.startIndex -= m_heapOffset;
    HeapDescriptorAllocator::free(allocation);
}

}   // namespace gp::rhi::allocator
