// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/backends/Malloc.hpp"

namespace gp::memory
{

/// @brief An implementation of the Malloc interface that uses the standard C library's malloc, realloc, and free
/// functions to manage memory. This allocator is designed to be a simple and portable implementation of the Malloc
/// interface, and may not provide the best performance or memory usage characteristics for all workloads. It is
/// intended to be used as a fallback allocator when more specialized allocators are not available or suitable for a
/// particular use case.
class MallocAnsi final : public Malloc
{
public:
    /// @brief Constructs a new MallocAnsi allocator instance.
    MallocAnsi();

public:
    /// @brief Allocates a block of memory of the specified size and alignment.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    virtual void* malloc(gp::USize size, gp::UInt32 alignment) override;

    /// @brief Attempts to allocate a block of memory of the specified size and alignment.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    virtual void* tryMalloc(gp::USize size, gp::UInt32 alignment) override;

    /// @brief Reallocates a block of memory to a new size and alignment.
    /// @param[in] ptr A pointer to the previously allocated memory block to be reallocated.
    /// @param[in] newSize The new size of the memory block, in bytes.
    /// @param[in] alignment The alignment requirement for the reallocated memory block, in bytes.
    /// @return A pointer to the reallocated memory block, or nullptr if the reallocation fails.
    virtual void* realloc(void* ptr, gp::USize newSize, gp::UInt32 alignment) override;

    /// @brief Attempts to reallocate a block of memory to a new size and alignment.
    /// @param[in] ptr A pointer to the previously allocated memory block to be reallocated.
    /// @param[in] newSize The new size of the memory block, in bytes.
    /// @param[in] alignment The alignment requirement for the reallocated memory block, in bytes.
    /// @return A pointer to the reallocated memory block, or nullptr if the reallocation fails.
    virtual void* tryRealloc(void* ptr, gp::USize newSize, gp::UInt32 alignment) override;

    /// @brief Deallocates a previously allocated block of memory.
    /// @param[in] ptr A pointer to the memory block to be deallocated. The pointer must have been returned by a
    /// previous call to malloc, tryMalloc, realloc, or tryRealloc.
    virtual void free(void* ptr) override;

    /// @brief Retrieves the size of a previously allocated memory block. This function may not be supported by all
    /// allocators, and may return false if the allocator cannot determine the size of the allocation.
    /// @param[in] ptr A pointer to the memory block for which to retrieve the size.
    /// @param[out] outSize The size of the allocated memory block, in bytes. This parameter is only valid if the
    /// function returns true.
    /// @return true if the size of the allocated memory block was successfully retrieved, or false if the allocator
    /// cannot determine the size of the allocation.
    virtual bool getAllocationSize(void* ptr, gp::USize& outSize) const override;

    /// @brief Indicates whether the allocator is internally thread-safe. If this function returns true, the memory
    /// manager may allow concurrent allocations and deallocations from multiple threads without additional
    /// synchronization.
    /// @return true if the allocator is internally thread-safe, or false if it is not. If this function returns false.
    virtual bool isInternallyThreadSafe() const override;

    /// @brief Validates the integrity of the allocator's internal heap.
    /// @return true if the heap is valid, or false if it is not.
    virtual bool validateHeap() const override;

    /// @brief Retrieves a human-readable name for the allocator.
    /// @return A null-terminated c-string containing the human-readable name for the allocator.
    virtual const char* getDisplayName() const override;
};

}   // namespace gp::memory
