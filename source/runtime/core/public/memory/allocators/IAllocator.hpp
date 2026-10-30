// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryBase.hpp"

namespace gp::memory
{

/// @brief Pure abstract runtime interface for polymorphic memory allocators and lifetime-scoped arenas.
/// @details Allows subsystems and containers parameterized with `PolymorphicAllocator` to accept any concrete arena
///          (`LinearAllocator`, `StackAllocator`, `PoolAllocator`) at runtime without templating the consumer on the
///          concrete arena type.
class GP_CORE_API IAllocator
{
public:
    /// @brief Default constructor.
    constexpr IAllocator() noexcept = default;

    /// @brief Virtual destructor for safe polymorphic cleanup.
    virtual ~IAllocator() noexcept = default;

    /// @brief Allocates a contiguous block of memory of the specified size and alignment.
    /// @param[in] sizeInBytes Requested allocation size in bytes. Returns `nullptr` if `0`.
    /// @param[in] alignment Byte alignment constraint (must be a power of two, or `kDefaultAlignment`).
    /// @return Pointer to the aligned memory block, or `nullptr` if capacity is exhausted.
    [[nodiscard]] virtual void*
        allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept = 0;

    /// @brief Resizes an existing memory allocation, preserving up to `min(oldSizeInBytes, newSizeInBytes)` bytes.
    /// @param[in] ptr Pointer to the previously allocated memory block, or `nullptr` to perform a fresh allocation.
    /// @param[in] oldSizeInBytes Exact size in bytes of the existing allocation at `ptr` (0 if `ptr == nullptr`).
    /// @param[in] newSizeInBytes Requested new allocation size in bytes (0 to free `ptr` and return `nullptr`).
    /// @param[in] alignment Byte alignment constraint (must be a power of two, or `kDefaultAlignment`).
    /// @return Pointer to the resized memory block, or `nullptr` if `newSizeInBytes == 0` or reallocation failed.
    [[nodiscard]] virtual void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept = 0;

    /// @brief Releases a memory block previously allocated by this allocator.
    /// @param[in] ptr Pointer to the memory block to deallocate (safe no-op if `nullptr`).
    /// @param[in] oldSizeInBytes Size in bytes of the allocation being freed (enables O(1) top-of-bumper reclaim).
    /// @param[in] alignment Alignment used when the block was allocated.
    virtual void deallocate(
        void* ptr, const gp::USize oldSizeInBytes = 0u, const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept = 0;

    /// @brief Computes the actual usable block size for a given request to eliminate internal slack waste.
    /// @param[in] requestedSize Minimum requested size in bytes.
    /// @param[in] alignment Alignment constraint in bytes.
    /// @return Actual allocation size in bytes (`>= requestedSize`).
    [[nodiscard]] virtual gp::USize getActualAllocationSize(
        const gp::USize requestedSize, [[maybe_unused]] const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        return requestedSize;
    }

    /// @brief Checks whether the given memory address belongs to or is managed by this allocator.
    /// @param[in] ptr Memory address to query.
    /// @return True if `ptr` is non-null and owned by this allocator; false otherwise.
    [[nodiscard]] virtual bool owns(const void* ptr) const noexcept
    {
        return ptr != nullptr;
    }

    /// @brief Retrieves the human-readable debug/profiler name associated with this allocator.
    /// @return Null-terminated ASCII string identifying the allocator pool.
    [[nodiscard]] virtual const char* getName() const noexcept
    {
        return "IAllocator";
    }
};

}   // namespace gp::memory
