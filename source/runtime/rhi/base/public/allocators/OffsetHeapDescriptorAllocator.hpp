// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "allocators/DescriptorAllocatorTypes.hpp"
#include "allocators/HeapDescriptorAllocator.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "templates/Optional.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ViewEnums.hpp"

namespace gp::rhi::allocator
{

/// @brief Heap descriptor allocator managing a sub-range starting at a fixed base offset inside a larger heap.
/// @details Translates local zero-based allocator indices by `+m_heapOffset` on allocation and `-m_heapOffset` on free.
class GP_RHI_API OffsetHeapDescriptorAllocator : protected HeapDescriptorAllocator
{
public:
    OffsetHeapDescriptorAllocator() = delete;

    /// @brief Constructs an offset heap descriptor allocator covering `[heapOffset, heapOffset + descriptorCount - 1]`.
    /// @param[in] typeMask Bitmask of descriptor types permitted in this sub-heap.
    /// @param[in] descriptorCount Number of descriptor slots managed by this sub-heap.
    /// @param[in] heapOffset Constant index offset from the start of the parent descriptor heap.
    /// @param[in] stats Optional list of atomic counters updated on allocation and free.
    OffsetHeapDescriptorAllocator(
        view::DescriptorTypeMask typeMask,
        gp::UInt32 descriptorCount,
        gp::UInt32 heapOffset,
        gp::ConstVectorView<DescriptorStatCounter*> stats = {}
    ) noexcept;

    OffsetHeapDescriptorAllocator(const OffsetHeapDescriptorAllocator&) = delete;
    OffsetHeapDescriptorAllocator& operator=(const OffsetHeapDescriptorAllocator&) = delete;
    OffsetHeapDescriptorAllocator(OffsetHeapDescriptorAllocator&&) = delete;
    OffsetHeapDescriptorAllocator& operator=(OffsetHeapDescriptorAllocator&&) = delete;

public:
    /// @brief Allocates a single descriptor slot and shifts its index by `m_heapOffset`.
    /// @param[in] type Descriptor category for the returned handle.
    /// @return Valid shifted `view::DescriptorHandle` on success, or an invalid handle when exhausted.
    [[nodiscard]] view::DescriptorHandle allocate(view::DescriptorType type) noexcept;

    /// @brief Frees a previously allocated shifted descriptor handle.
    /// @param[in] handle Descriptor handle whose index includes `m_heapOffset`.
    void free(view::DescriptorHandle handle) noexcept;

    /// @brief Allocates a contiguous span of `numDescriptors` slots with `startIndex` shifted by `m_heapOffset`.
    /// @param[in] numDescriptors Number of contiguous descriptor slots requested.
    /// @return Shifted `DescriptorAllocation` on success, or `gp::nullopt`.
    [[nodiscard]] gp::Optional<DescriptorAllocation> allocate(gp::UInt32 numDescriptors) noexcept;

    /// @brief Frees a previously allocated shifted descriptor span.
    /// @param[in] allocation Allocation span whose `startIndex` includes `m_heapOffset`.
    void free(DescriptorAllocation allocation) noexcept;

    using HeapDescriptorAllocator::getCapacity;
    using HeapDescriptorAllocator::getTypeMask;
    using HeapDescriptorAllocator::handlesAllocation;
    using HeapDescriptorAllocator::handlesAllocations;

    /// @brief Returns the constant base index offset applied to allocations from this sub-heap.
    /// @return Heap base offset in descriptor slots.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getHeapOffset() const noexcept
    {
        return m_heapOffset;
    }

private:
    const gp::UInt32 m_heapOffset{ 0u };
};

}   // namespace gp::rhi::allocator
