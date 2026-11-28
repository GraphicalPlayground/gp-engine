// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/HeapDescriptorAllocator.hpp"
#include "diagnostics/AssertionMacros.hpp"

namespace gp::rhi::allocator
{

HeapDescriptorAllocator::HeapDescriptorAllocator(
    const view::DescriptorTypeMask typeMask,
    const gp::UInt32 descriptorCount,
    const gp::ConstVectorView<DescriptorStatCounter*> stats
) noexcept
    : DescriptorAllocator(descriptorCount, stats)
    , m_typeMask(typeMask)
{}

view::DescriptorHandle HeapDescriptorAllocator::allocate(const view::DescriptorType type) noexcept
{
    return DescriptorAllocator::allocate(type);
}

void HeapDescriptorAllocator::free(const gp::ConstVectorView<view::DescriptorHandle> handles) noexcept
{
    if (!handles.isEmpty())
    {
#if GP_DO_GUARD_SLOW
        for (const view::DescriptorHandle& handle: handles)
        {
            if (handle.isValid())
            {
                GP_CHECK_SLOW(handlesAllocation(handle.getType()));
            }
        }
#endif

        DescriptorAllocator::free(handles);
    }
}

void HeapDescriptorAllocator::free(const view::DescriptorHandle handle) noexcept
{
    if (handle.isValid())
    {
        GP_CHECK(handlesAllocation(handle.getType()));
        DescriptorAllocator::free(handle);
    }
}

gp::Optional<DescriptorAllocation> HeapDescriptorAllocator::allocate(const gp::UInt32 numDescriptors) noexcept
{
    return DescriptorAllocator::allocate(numDescriptors);
}

void HeapDescriptorAllocator::free(const DescriptorAllocation allocation) noexcept
{
    DescriptorAllocator::free(allocation);
}

}   // namespace gp::rhi::allocator
