// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/GlobalMemory.hpp"
#include "memory/MemoryBase.hpp"

namespace gp::memory
{

namespace detail
{

/// @brief Resolves the active global `Malloc` backend, preferring the zero-IAT inline shadow pointer when populated.
/// @return Non-null pointer to the active `Malloc` instance.
[[nodiscard]] GP_FORCEINLINE_HINT Malloc* resolveActiveMalloc() noexcept
{
    Malloc* inlineMalloc = gp::memory::getInlineMalloc();
    if (inlineMalloc != nullptr) [[likely]]
    {
        return inlineMalloc;
    }

    return gp::memory::getGlobalMalloc();
}

}   // namespace detail

/// @brief Stateless general-purpose heap allocator backed by the engine's global `Malloc` instance.
/// @details Because `HeapAllocator` carries no member variables (`std::is_empty_v<HeapAllocator> == true`), containers
///          storing it with `[[no_unique_address]]` pay zero bytes of storage overhead. Serves as `DefaultAllocator`
///          across all engine containers (`Vector`, `String`, `List`, `Set`, `Map`).
class HeapAllocator
{
public:
    /// @brief Default constructor.
    constexpr HeapAllocator() noexcept = default;

    /// @brief Allocates a block of memory from the global `Malloc` backend.
    /// @param[in] sizeInBytes Number of bytes to allocate. Returns `nullptr` if `0`.
    /// @param[in] alignment Byte alignment requirement (defaults to `kDefaultAlignment`).
    /// @return Pointer to the allocated memory block, or `nullptr` if `sizeInBytes == 0` or allocation fails.
    [[nodiscard]] GP_FORCEINLINE_HINT void*
        allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        if (sizeInBytes == 0u) [[unlikely]]
        {
            return nullptr;
        }

        return detail::resolveActiveMalloc()->tryAllocate(sizeInBytes, alignment);
    }

    /// @brief Resizes an existing heap allocation via the global `Malloc` backend.
    /// @param[in] ptr Existing memory block to resize, or `nullptr` to allocate a new block.
    /// @param[in] oldSizeInBytes Previous allocation size in bytes (unused by the global heap backend).
    /// @param[in] newSizeInBytes Requested new size in bytes. If `0`, frees `ptr` and returns `nullptr`.
    /// @param[in] alignment Byte alignment requirement (defaults to `kDefaultAlignment`).
    /// @return Pointer to the reallocated block, or `nullptr` if freed or out of memory.
    [[nodiscard]] GP_FORCEINLINE_HINT void* reallocate(
        void* ptr,
        [[maybe_unused]] const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        if (ptr == nullptr && newSizeInBytes == 0u) [[unlikely]]
        {
            return nullptr;
        }

        if (newSizeInBytes == 0u)
        {
            detail::resolveActiveMalloc()->deallocate(ptr);
            return nullptr;
        }

        return detail::resolveActiveMalloc()->tryReallocate(ptr, newSizeInBytes, alignment);
    }

    /// @brief Deallocates a memory block previously obtained from `HeapAllocator`.
    /// @param[in] ptr Pointer to the memory block to free (safe no-op if `nullptr`).
    /// @param[in] oldSizeInBytes Size of the allocation in bytes (unused by the global heap backend).
    /// @param[in] alignment Alignment used during allocation (unused by the global heap backend).
    GP_FORCEINLINE_HINT void deallocate(
        void* ptr,
        [[maybe_unused]] const gp::USize oldSizeInBytes = 0u,
        [[maybe_unused]] const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        if (ptr != nullptr) [[likely]]
        {
            detail::resolveActiveMalloc()->deallocate(ptr);
        }
    }

    /// @brief Queries the underlying `Malloc` backend for the quantized block size matching `requestedSize`.
    /// @param[in] requestedSize Minimum requested size in bytes.
    /// @param[in] alignment Alignment constraint in bytes.
    /// @return Quantized size in bytes (`>= requestedSize`).
    [[nodiscard]] GP_FORCEINLINE_HINT gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        if (requestedSize == 0u) [[unlikely]]
        {
            return 0u;
        }

        return detail::resolveActiveMalloc()->getActualAllocationSize(requestedSize, alignment);
    }

    /// @brief Checks whether a pointer could have been allocated by the heap allocator.
    /// @param[in] ptr Memory address to check.
    /// @return True if `ptr != nullptr`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool owns(const void* ptr) const noexcept
    {
        return ptr != nullptr;
    }

    /// @brief Typed convenience helper to allocate uninitialized storage for `count` elements of `InElementType`.
    /// @tparam InElementType Element type whose size and alignment govern the allocation.
    /// @param[in] count Number of elements to allocate storage for.
    /// @return Typed pointer to uninitialized storage, or `nullptr` if `count == 0`.
    template <typename InElementType>
    [[nodiscard]] GP_FORCEINLINE_HINT InElementType* allocateElements(const gp::USize count) noexcept
    {
        if (count == 0u) [[unlikely]]
        {
            return nullptr;
        }

        const gp::UInt32 alignment = static_cast<gp::UInt32>(alignof(InElementType));
        return static_cast<InElementType*>(allocate(count * sizeof(InElementType), alignment));
    }

    /// @brief Typed convenience helper to free storage for `count` elements of `InElementType`.
    /// @tparam InElementType Element type whose size and alignment govern the allocation.
    /// @param[in] ptr Typed pointer to the element buffer to free.
    /// @param[in] count Number of elements previously allocated.
    template <typename InElementType>
    GP_FORCEINLINE_HINT void deallocateElements(InElementType* ptr, const gp::USize count = 1u) noexcept
    {
        const gp::UInt32 alignment = static_cast<gp::UInt32>(alignof(InElementType));
        deallocate(static_cast<void*>(ptr), count * sizeof(InElementType), alignment);
    }

    /// @brief Equality comparison; all stateless `HeapAllocator` instances are interchangeable.
    /// @param[in] rhs Other `HeapAllocator` instance.
    /// @return Always true.
    [[nodiscard]] constexpr bool operator==([[maybe_unused]] const HeapAllocator& rhs) const noexcept
    {
        return true;
    }
};

static_assert(
    gp::concepts::IsStatelessAllocator<HeapAllocator>, "HeapAllocator must satisfy gp::concepts::IsStatelessAllocator."
);

/// @brief Canonical default allocator alias used across `gp-engine` containers.
using DefaultAllocator = HeapAllocator;

}   // namespace gp::memory
