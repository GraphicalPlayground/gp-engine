// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Memory/Allocators/IAllocator.hpp"

namespace GP
{

/// @brief General-purpose allocator backed by aligned_alloc / _aligned_malloc.
///        Thread-safe via the OS heap. Tracks allocation count and total bytes in debug builds.
class FDefaultAllocator final : public IAllocator
{
private:
    SizeT m_totalAllocated{ 0 };    //<! Total bytes currently allocated.
    SizeT m_allocationCount{ 0 };   //<! Total number of active allocations.

public:
    /// @brief Allocates a block of memory with the specified size and alignment.
    /// @param size The size of the memory block to allocate, in bytes.
    /// @param alignment The alignment requirement for the memory block. Must be a power of two and at least
    ///                  alignof(MaxAlignT).
    /// @return Pointer to the allocated memory block, or nullptr if allocation failed.
    GP_NODISCARD virtual void* Allocate(SizeT size, SizeT alignment = alignof(MaxAlignT)) noexcept override;

    /// @brief Deallocates a previously allocated memory block.
    /// @param ptr The pointer to the memory block to deallocate.
    /// @param size The size of the memory block being deallocated.
    virtual void Deallocate(void* ptr, SizeT size) noexcept override;

    /// @brief Default implementation of Reallocate() that allocates new memory, copies the old data, and deallocates
    ///        the old memory.
    /// @param ptr The pointer to the existing memory block to be reallocated. Can be nullptr if no existing block.
    /// @param oldSize The size of the existing memory block pointed to by ptr. Must be 0 if ptr is nullptr.
    /// @param newSize The size of the new memory block to allocate. Must be greater than 0.
    /// @param alignment The alignment requirement for the new memory block. Defaults to alignof(MaxAlignT) if not
    ///                  specified.
    /// @return Pointer to the newly allocated memory block with the requested size and alignment, or nullptr if
    ///         allocation failed.
    GP_NODISCARD virtual void*
        Reallocate(void* ptr, SizeT oldSize, SizeT newSize, SizeT alignment = alignof(MaxAlignT)) noexcept override;

    /// @brief Gets the total number of bytes currently allocated (if tracked).
    /// @return Total allocated bytes.
    GP_NODISCARD virtual SizeT GetTotalAllocated() const noexcept override;

    /// @brief Gets the total number of active allocations (if tracked).
    /// @return Number of live allocations.
    GP_NODISCARD virtual SizeT GetAllocationCount() const noexcept override;

    /// @brief Gets a human-readable name for this allocator, useful for debugging and profiling.
    /// @return Null-terminated string literal.
    GP_NODISCARD virtual const char* GetDebugName() const noexcept override;

public:
    /// @brief Returns the process-wide singleton default allocator instance.
    /// @return Reference to the default allocator.
    GP_NODISCARD static FDefaultAllocator& Get() noexcept;
};

}   // namespace GP
