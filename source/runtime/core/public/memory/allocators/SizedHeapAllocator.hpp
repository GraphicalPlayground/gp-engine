// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/allocators/SizedAllocatorBase.hpp"

namespace gp::memory
{

/// @todo Implement the SizeHeapAllocator class, which is a sized allocator that uses a heap-based memory allocation.
template <gp::Int32 IndexSize, typename BaseMallocType>
class SizedHeapAllocator : public SizedAllocatorBase<IndexSize, SizedHeapAllocator<IndexSize, BaseMallocType>>
{
public:
    using MyClass = SizedHeapAllocator<IndexSize, BaseMallocType>;
    using SuperClass = SizedAllocatorBase<IndexSize, MyClass>;
    using BaseMalloc = BaseMallocType;
    using SizeType = SuperClass::SizeType;
};

}   // namespace gp::memory
