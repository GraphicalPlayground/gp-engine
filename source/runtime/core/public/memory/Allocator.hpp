// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreMinimal.hpp"
#include <cstring>

namespace gp::memory
{

/// @brief Base interface for memory allocators.
/// @details
/// Defines the common virtual interface for allocating, deallocating, and querying memory.
/// All concrete allocators derive from this class.
/// GP is compiled with -fno-exceptions. All allocation methods are noexcept and return nullptr on failure.
/// Constructors invoked via construct() must not throw; doing so calls std::terminate.
class Allocator
{
public:
    Allocator() = default;
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;
    virtual ~Allocator() = default;

public:
    /// @brief Allocates a block of memory with the specified size and alignment.
    /// @param[in] size The number of bytes to allocate.
    /// @param[in] alignment The required alignment of the allocated memory.
    /// @return A void pointer to the allocated memory block, or nullptr if allocation fails.
    GP_NODISCARD virtual void* allocate(USize size, USize alignment = alignof(gp::MaxAlign)) noexcept = 0;

    /// @brief Deallocates a previously allocated memory block.
    /// @param[in] ptr Pointer to the memory block that should be freed.
    /// @param[in] size The size in bytes of the block being freed.
    virtual void deallocate(void* ptr, USize size) noexcept = 0;

    /// @brief Reallocates an existing memory block to a new size, preserving existing contents.
    /// @param[in] ptr Pointer to the existing memory block. May be nullptr.
    /// @param[in] oldSize The original size of the memory block. Ignored when ptr is nullptr.
    /// @param[in] newSize The requested new size for the memory block.
    /// @param[in] alignment The required alignment of the reallocated memory.
    /// @return A void pointer to the reallocated memory block, or nullptr if reallocation fails.
    GP_NODISCARD virtual void*
        reallocate(void* ptr, USize oldSize, USize newSize, USize alignment = alignof(gp::MaxAlign)) noexcept
    {
        void* newPtr = allocate(newSize, alignment);
        if (newPtr && ptr && oldSize > 0)
        {
            std::memcpy(newPtr, ptr, oldSize < newSize ? oldSize : newSize);
            deallocate(ptr, oldSize);
        }
        return newPtr;
    }

    /// @brief Resets the allocator to its initial state, freeing all allocations.
    /// @details
    /// Not all allocators support reset. The default implementation is a no-op. Allocators that support bulk
    /// deallocation (linear, stack, arena) override this.
    virtual void reset() noexcept {}

    /// @brief Queries whether this allocator owns the given pointer.
    /// @details
    /// Used by composable allocators (proxy, fallback chains) to determine which allocator should service a
    /// deallocation. The default returns false.
    /// @param[in] ptr Pointer to query ownership of.
    /// @return True if this allocator owns the memory at ptr, false otherwise.
    GP_NODISCARD virtual bool owns(GP_MAYBE_UNUSED const void* ptr) const noexcept { return false; }

    /// @brief Allocates an array of elements of type T without constructing them.
    /// @tparam T Type of elements in the array to allocate memory for.
    /// @param[in] count Number of elements to allocate memory for.
    /// @return Pointer to the allocated memory block aligned for type T, or nullptr if allocation fails.
    template <typename T>
    GP_NODISCARD T* allocateN(USize count) noexcept
    {
        return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
    }

    /// @brief Deallocates a memory block previously allocated for an array of elements.
    /// @tparam T Type of elements in the array.
    /// @param[in] ptr Pointer to the memory block containing the unconstructed elements to deallocate.
    /// @param[in] count Number of elements in the array to deallocate memory for.
    template <typename T>
    void deallocateN(T* ptr, USize count) noexcept
    {
        deallocate(ptr, count * sizeof(T));
    }

    /// @brief Allocates memory and constructs an object of type T in-place using the provided arguments.
    /// @details
    /// GP is compiled with -fno-exceptions. If T's constructor would throw, std::terminate is called. Callers must
    /// ensure T's constructor is noexcept or otherwise safe.
    /// @tparam T Type of the object to construct.
    /// @tparam Args Parameter pack for arguments passed to T's constructor.
    /// @param[in] args Arguments to forward to T's constructor.
    /// @return Pointer to the newly constructed object, or nullptr if memory allocation fails.
    template <typename T, typename... Args>
    GP_NODISCARD T* construct(Args&&... args) noexcept
    {
        void* memory = allocate(sizeof(T), alignof(T));
        if (!memory) { return nullptr; }
        return ::new (memory) T(static_cast<Args&&>(args)...);
    }

    /// @brief Destructs an object and deallocates its underlying memory.
    /// @tparam T Type of the object being destructed.
    /// @param[in] ptr Pointer to the object to destruct and deallocate.
    template <typename T>
    void destruct(T* ptr) noexcept
    {
        if (ptr)
        {
            ptr->~T();
            deallocate(ptr, sizeof(T));
        }
    }

    /// @brief Gets the total amount of memory currently allocated by this allocator.
    /// @return The number of bytes currently allocated, or 0 if tracking is unsupported.
    GP_NODISCARD virtual USize getAllocatedSize() const noexcept { return 0; }

    /// @brief Gets the total number of active allocations managed by this allocator.
    /// @return The number of active allocation instances, or 0 if tracking is unsupported.
    GP_NODISCARD virtual USize getAllocationCount() const noexcept { return 0; }

    /// @brief Retrieves a human-readable name for this allocator, useful for debugging.
    /// @return A null-terminated C-string representing the debug name.
    GP_NODISCARD virtual const char* getDebugName() const noexcept { return "Allocator"; }
};

}   // namespace gp::memory
