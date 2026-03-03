// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Core/Utility.hpp"
#include <cstring>

namespace GP
{

/// @brief Abstract base for all GP allocators.
///        All allocations are size+alignment aware so allocators can be completely decoupled from std::malloc
///        semantics.
class IAllocator
{
public:
    /// @brief Default constructor for the allocator interface.
    IAllocator() = default;

    /// @brief Delete copy constructor to prevent copying of allocators.
    IAllocator(const IAllocator&) = delete;

    /// @brief Default virtual destructor to allow proper cleanup of derived allocator.
    virtual ~IAllocator() = default;

public:
    /// @brief Delete copy assignment operator to prevent copying of allocators.
    /// @return Reference to the current allocator instance.
    IAllocator& operator=(const IAllocator&) = delete;

public:
    /// @brief Allocates a block of memory with the specified size and alignment.
    /// @param size The size of the memory block to allocate, in bytes.
    /// @param alignment The alignment requirement for the memory block. Must be a power of two and at least
    ///                  alignof(MaxAlignT).
    /// @return Pointer to the allocated memory block, or nullptr if allocation failed.
    GP_NODISCARD virtual void* Allocate(SizeT size, SizeT alignment = alignof(MaxAlignT)) noexcept = 0;

    /// @brief Deallocates a previously allocated memory block.
    /// @param ptr The pointer to the memory block to deallocate.
    /// @param size The size of the memory block being deallocated.
    virtual void Deallocate(void* ptr, SizeT size) noexcept = 0;

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
        Reallocate(void* ptr, SizeT oldSize, SizeT newSize, SizeT alignment = alignof(MaxAlignT)) noexcept
    {
        void* newPtr = Allocate(newSize, alignment);
        if (newPtr && ptr)
        {
            ::memcpy(newPtr, ptr, oldSize < newSize ? oldSize : newSize);
            Deallocate(ptr, oldSize);
        }
        return newPtr;
    }

    /// @brief Allocates an array of objects of type T without constructing them.
    /// @tparam T Element type.
    /// @param count Number of elements to allocate.
    /// @return Pointer to the first element of the allocated array, or nullptr if allocation failed.
    template <typename T>
    GP_NODISCARD T* AllocateN(SizeT count) noexcept
    {
        return static_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

    /// @brief Deallocates an array of objects previously allocated with AllocateN().
    /// @tparam T Element type.
    /// @param ptr Pointer to the first element of the array.
    /// @param count Number of elements in the array.
    template <typename T>
    void DeallocateN(T* ptr, SizeT count) noexcept
    {
        Deallocate(ptr, sizeof(T) * count);
    }

    /// @brief Allocates and constructs an object of type T with the given arguments.
    /// @tparam T Type of the object to create.
    /// @tparam Args Constructor argument types.
    /// @param args Constructor arguments.
    /// @return Pointer to the newly created object, or nullptr if allocation failed.
    template <typename T, typename... Args>
    GP_NODISCARD T* New(Args&&... args) noexcept(noexcept(T(GP::Forward<Args>(args)...)))
    {
        void* mem = Allocate(sizeof(T), alignof(T));
        if (!mem) { return nullptr; }
        return ::new (mem) T(GP::Forward<Args>(args)...);
    }

    /// @brief Destructs and deallocates an object created with New().
    /// @tparam T Type of the object to delete.
    /// @param ptr Pointer to the object to delete.
    template <typename T>
    void Delete(T* ptr) noexcept
    {
        if (ptr)
        {
            ptr->~T();
            Deallocate(ptr, sizeof(T));
        }
    }

    /// @brief Allocates a memory block and associates it with a profiling tag for engine memory tracking.
    ///        Profiling allocators override this to record which subsystem owns the allocation.
    ///        The default implementation ignores the tag and forwards to Allocate().
    /// @param size      Size of the block in bytes.
    /// @param alignment Alignment requirement; must be a power of two.
    /// @param tag       Null-terminated string identifying the owning subsystem (e.g. "TArray").
    ///                  May be nullptr; the allocator should treat that as an untagged allocation.
    /// @return Pointer to allocated memory, or nullptr on failure.
    GP_NODISCARD virtual void* AllocateTagged(SizeT size, SizeT alignment, const char* tag) noexcept
    {
        GP_UNUSED(tag);
        return Allocate(size, alignment);
    }

    /// @brief Gets the total number of bytes currently allocated (if tracked).
    /// @return Total allocated bytes.
    GP_NODISCARD virtual SizeT GetTotalAllocated() const noexcept { return 0; }

    /// @brief Gets the total number of active allocations (if tracked).
    /// @return Number of live allocations.
    GP_NODISCARD virtual SizeT GetAllocationCount() const noexcept { return 0; }

    /// @brief Gets a human-readable name for this allocator, useful for debugging and profiling.
    /// @return Null-terminated string literal.
    GP_NODISCARD virtual const char* GetDebugName() const noexcept { return "IAllocator"; }
};

}   // namespace GP
