// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Construction.hpp"
#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorTypes.hpp"
#include <type_traits>

namespace gp::concepts
{

/// @brief Concept verifying that a type satisfies the engine's first-class sized memory allocator contract.
/// @details Unlike C-style `realloc(ptr, newSize)` and `free(ptr)`, engine allocators receive `oldSizeInBytes` on
///          both reallocation and deallocation. Since containers (`Vector`, `String`, `List`, `Set`, `Map`) always
///          know their allocated capacity, passing `oldSizeInBytes` enables O(1) headerless in-place bump resizing
///          in `LinearAllocator` and `FixedBufferAllocator` as well as exact memory copies during relocation.
/// @tparam InAllocatorType Candidate allocator type to validate.
template <typename InAllocatorType>
concept IsAllocator = requires(
    InAllocatorType& allocator,
    const InAllocatorType& constAllocator,
    void* ptr,
    const void* constPtr,
    const gp::USize sizeInBytes,
    const gp::USize oldSizeInBytes,
    const gp::USize newSizeInBytes,
    const gp::UInt32 alignment
) {
    {
        allocator.allocate(sizeInBytes, alignment)
    } noexcept -> IsSameAs<void*>;
    {
        allocator.reallocate(ptr, oldSizeInBytes, newSizeInBytes, alignment)
    } noexcept -> IsSameAs<void*>;
    {
        allocator.deallocate(ptr, oldSizeInBytes, alignment)
    } noexcept -> IsSameAs<void>;
    {
        constAllocator.getActualAllocationSize(sizeInBytes, alignment)
    } noexcept -> IsSameAs<gp::USize>;
    {
        constAllocator.owns(constPtr)
    } noexcept -> IsSameAs<bool>;
};

/// @brief Concept verifying that an allocator is stateless and occupies zero bytes when stored with
/// `[[no_unique_address]]`.
/// @details Stateless allocators (`HeapAllocator`, `AlignedHeapAllocator`, `ThreadScratchAllocator`) route requests to
///          global or thread-local backing resources and carry no per-instance member variables.
/// @tparam InAllocatorType Candidate allocator type to validate.
template <typename InAllocatorType>
concept IsStatelessAllocator =
    IsAllocator<InAllocatorType> && std::is_empty_v<InAllocatorType> && IsDefaultConstructible<InAllocatorType>;

/// @brief Concept verifying that an arena allocator supports O(1) cursor checkpointing (`mark`), unwinding (`rewind`),
///        and bulk reclamation (`reset`).
/// @tparam InAllocatorType Candidate arena allocator type to validate.
template <typename InAllocatorType>
concept IsMarkableAllocator =
    IsAllocator<InAllocatorType> && requires(InAllocatorType& allocator, const gp::memory::MemoryMark& mark) {
        {
            allocator.mark()
        } noexcept -> IsSameAs<gp::memory::MemoryMark>;
        {
            allocator.rewind(mark)
        } noexcept -> IsSameAs<void>;
        {
            allocator.reset()
        } noexcept -> IsSameAs<void>;
    };

}   // namespace gp::concepts
