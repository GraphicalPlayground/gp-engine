// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/backends/MemoryStatistics.hpp"
#include "memory/backends/SystemMallocObject.hpp"
#include "memory/MemoryBase.hpp"

namespace gp::memory
{

/// @brief A memory allocator wrapper interface for malloc-based memory allocation. This class provides an interface for
/// memory allocation and deallocation using the standard C library's malloc, realloc, and free functions. It also
/// includes additional functionality for zero-initialized allocations, adjusting allocation sizes based on alignment
/// requirements, and retrieving allocation statistics. This class is designed
class Malloc : public SystemMallocObject
{
public:
    /// @brief Allocates a block of memory of the specified size and alignment.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    virtual void* malloc(gp::USize size, gp::UInt32 alignment = kDefaultAlignment) = 0;

    /// @brief Attempts to allocate a block of memory of the specified size and alignment.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    virtual void* tryMalloc(gp::USize size, gp::UInt32 alignment = kDefaultAlignment);

    /// @brief Reallocates a block of memory to a new size and alignment.
    /// @param[in] ptr A pointer to the previously allocated memory block to be reallocated.
    /// @param[in] newSize The new size of the memory block, in bytes.
    /// @param[in] alignment The alignment requirement for the reallocated memory block, in bytes.
    /// @return A pointer to the reallocated memory block, or nullptr if the reallocation fails.
    virtual void* realloc(void* ptr, gp::USize newSize, gp::UInt32 alignment = kDefaultAlignment) = 0;

    /// @brief Attempts to reallocate a block of memory to a new size and alignment.
    /// @param[in] ptr A pointer to the previously allocated memory block to be reallocated.
    /// @param[in] newSize The new size of the memory block, in bytes.
    /// @param[in] alignment The alignment requirement for the reallocated memory block, in bytes.
    /// @return A pointer to the reallocated memory block, or nullptr if the reallocation fails.
    virtual void* tryRealloc(void* ptr, gp::USize newSize, gp::UInt32 alignment = kDefaultAlignment);

    /// @brief Deallocates a previously allocated block of memory.
    /// @param[in] ptr A pointer to the memory block to be deallocated. The pointer must have been returned by a
    /// previous call to malloc, tryMalloc, realloc, or tryRealloc.
    virtual void free(void* ptr) = 0;

    /// @brief Allocates a block of memory of the specified size and alignment, and initializes it to zero.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated and zero-initialized memory block, or nullptr if the allocation fails.
    virtual void* mallocZeroed(gp::USize size, gp::UInt32 alignment = kDefaultAlignment);

    /// @brief Attempts to allocate a block of memory of the specified size and alignment, and initializes it to zero.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated and zero-initialized memory block, or nullptr if the allocation fails.
    virtual void* tryMallocZeroed(gp::USize size, gp::UInt32 alignment = kDefaultAlignment);

    /// @brief Get the size that should be requested to eliminate internal fragmentation for an allocation of the given
    /// size and alignment. The return value will always be greater than or equal to the requested size, and can be used
    /// to grow and shrink allocations to optimal sizes for the underlying allocator.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return The adjusted size that should be requested to eliminate internal fragmentation for an allocation of the
    /// given size and alignment.
    virtual gp::USize adjustSize(gp::USize size, gp::UInt32 alignment) const;

    /// @brief Retrieves the size of a previously allocated memory block. This function may not be supported by all
    /// allocators, and may return false if the allocator cannot determine the size of the allocation.
    /// @param[in] ptr A pointer to the memory block for which to retrieve the size.
    /// @param[out] outSize The size of the allocated memory block, in bytes. This parameter is only valid if the
    /// function returns true.
    /// @return true if the size of the allocated memory block was successfully retrieved, or false if the allocator
    /// cannot determine the size of the allocation.
    virtual bool getAllocationSize(void* ptr, gp::USize& outSize) const;

    /// @brief Releases unused memory back to the system. This function may be a no-op for some allocators, but can be
    /// used by allocators that manage their own memory pools to release unused memory back to the operating system.
    virtual void trim();

    /// @brief Updates the allocator's statistics.
    /// @note This is called once per frame by the memory manager to allow the allocator to update any internal
    /// statistics it maintains. This must be implemented thread-safely, as it may be called from a different thread
    /// than the one(s) used for allocation and deallocation.
    virtual void updateStatistics();

    /// @brief Retrieves the allocator's statistics.
    /// @return A MemoryStatistics structure containing the allocator's statistics.
    virtual MemoryStatistics getAllocatorStatistics() const;

    /// @brief Retrieves the allocator's statistics by filling in the provided MemoryStatistics structure.
    /// @param[out] outStats A reference to a MemoryStatistics structure that will be filled in with the allocator's
    /// statistics.
    virtual void getAllocatorStatistics(MemoryStatistics& outStats) const;

    /// @brief Indicates whether the allocator is internally thread-safe. If this function returns true, the memory
    /// manager may allow concurrent allocations and deallocations from multiple threads without additional
    /// synchronization.
    /// @return true if the allocator is internally thread-safe, or false if it is not. If this function returns false.
    virtual bool isInternallyThreadSafe() const;

    /// @brief Validates the integrity of the allocator's internal heap.
    /// @return true if the heap is valid, or false if it is not.
    virtual bool validateHeap() const;

    /// @brief Retrieves a human-readable name for the allocator.
    /// @return A null-terminated c-string containing the human-readable name for the allocator.
    virtual const char* getDisplayName() const;
};

}   // namespace gp::memory
