// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "allocators/DescriptorAllocator.hpp"
#include "allocators/DescriptorAllocatorTypes.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "templates/Optional.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ViewEnums.hpp"

namespace gp::rhi::allocator
{

/// @brief Typed descriptor heap allocator restricting allocations and frees to a fixed `view::DescriptorTypeMask`.
/// @details Inherits from `DescriptorAllocator` with protected inheritance so callers interact through type-checked
///          entry points while still exposing capacity, range query, and grow operations.
class GP_RHI_API HeapDescriptorAllocator : protected DescriptorAllocator
{
public:
    HeapDescriptorAllocator() = delete;

    /// @brief Constructs a heap descriptor allocator serving `typeMask` with `descriptorCount` slots.
    /// @param[in] typeMask Bitmask of `view::DescriptorType` categories permitted in this heap.
    /// @param[in] descriptorCount Initial slot capacity of the heap.
    /// @param[in] stats Optional list of atomic counters updated on allocation and free.
    HeapDescriptorAllocator(
        view::DescriptorTypeMask typeMask,
        gp::UInt32 descriptorCount,
        gp::ConstVectorView<DescriptorStatCounter*> stats = {}
    ) noexcept;

    HeapDescriptorAllocator(const HeapDescriptorAllocator&) = delete;
    HeapDescriptorAllocator& operator=(const HeapDescriptorAllocator&) = delete;
    HeapDescriptorAllocator(HeapDescriptorAllocator&&) = delete;
    HeapDescriptorAllocator& operator=(HeapDescriptorAllocator&&) = delete;

public:
    /// @brief Allocates a single descriptor slot tagged with `type`.
    /// @param[in] type Descriptor category for the returned handle.
    /// @return Valid `view::DescriptorHandle` on success, or an invalid handle if the heap is full.
    [[nodiscard]] view::DescriptorHandle allocate(view::DescriptorType type) noexcept;

    /// @brief Frees a batch of descriptor handles belonging to this heap.
    /// @param[in] handles Read-only view of descriptor handles to release.
    void free(gp::ConstVectorView<view::DescriptorHandle> handles) noexcept;

    /// @brief Frees a single descriptor handle belonging to this heap.
    /// @param[in] handle Descriptor handle to release; verified against `m_typeMask` when valid.
    void free(view::DescriptorHandle handle) noexcept;

    /// @brief Allocates a contiguous span of `numDescriptors` raw descriptor slots.
    /// @param[in] numDescriptors Number of contiguous slots requested.
    /// @return Populated `DescriptorAllocation` on success, or `gp::nullopt`.
    [[nodiscard]] gp::Optional<DescriptorAllocation> allocate(gp::UInt32 numDescriptors) noexcept;

    /// @brief Frees a contiguous span of raw descriptor slots.
    /// @param[in] allocation Allocation span to release.
    void free(DescriptorAllocation allocation) noexcept;

    using DescriptorAllocator::getAllocatedRange;
    using DescriptorAllocator::getCapacity;
    using DescriptorAllocator::resizeGrowAndAllocate;

    /// @brief Returns the bitmask of descriptor types supported by this heap.
    /// @return Configured `view::DescriptorTypeMask`.
    [[nodiscard]] GP_FORCEINLINE_HINT view::DescriptorTypeMask getTypeMask() const noexcept
    {
        return m_typeMask;
    }

    /// @brief Checks whether this heap supports allocating a single descriptor of `type`.
    /// @param[in] type Descriptor type to query.
    /// @return True when `type`'s bit is set in `m_typeMask`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool handlesAllocation(const view::DescriptorType type) const noexcept
    {
        return (getTypeMask() & view::descriptorTypeMaskFromType(type)) != view::DescriptorTypeMask::None;
    }

    /// @brief Checks whether this heap supports every descriptor type set in `typeMask`.
    /// @param[in] typeMask Bitmask of required descriptor types.
    /// @return True when all bits in `typeMask` are set in `m_typeMask`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool handlesAllocations(const view::DescriptorTypeMask typeMask) const noexcept
    {
        return (getTypeMask() & typeMask) == typeMask;
    }

private:
    const view::DescriptorTypeMask m_typeMask{ view::DescriptorTypeMask::None };
};

}   // namespace gp::rhi::allocator
