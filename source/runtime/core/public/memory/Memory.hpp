// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/PlatformMemory.hpp"
#include <algorithm>

namespace gp::memory
{

/// @brief Allocation hints that can be used to provide additional information about the intended usage of a memory
/// allocation. These hints can help the memory allocator make informed decisions about how to manage memory, such as
/// choosing the appropriate memory pool or optimizing for specific access patterns. The specific meaning and handling
/// of these hints may vary depending on the platform and memory allocator implementation.
enum class AllocationHints
{
    None = -1,
    Default,
    Temporary,
    SmallPool,
    COUNT
};

/// @brief Moves a block of memory from the source to the destination, handling overlapping regions correctly.
/// @note The behavior is similar to `memmove` in C/C++.
/// @param[in] destination The pointer to the destination memory block where the content will be moved.
/// @param[in] source The pointer to the source memory block from which the content will be moved.
/// @param[in] numBytes The number of bytes to move from the source to the destination.
/// @return A pointer to the destination memory block after the move operation is complete.
GP_FORCEINLINE_HINT void* moveMemory(void* destination, const void* source, USize numBytes)
{
    return gp::platform::Memory::moveMemory(destination, source, numBytes);
}

/// @brief Compares two blocks of memory.
/// @note The behavior is similar to `memcmp` in C/C++.
/// @param[in] ptr1 The pointer to the first memory block to compare.
/// @param[in] ptr2 The pointer to the second memory block to compare.
/// @param[in] numBytes The number of bytes to compare.
/// @return An integer indicating the result of the comparison.
GP_FORCEINLINE_HINT int compareMemory(const void* ptr1, const void* ptr2, USize numBytes)
{
    return gp::platform::Memory::compareMemory(ptr1, ptr2, numBytes);
}

/// @brief Sets a block of memory to a specified value. This function fills the first numBytes bytes of the memory
/// area pointed to by destination with the constant byte value.
/// @note The behavior is similar to `memset` in C/C++.
/// @param[in] destination The pointer to the destination memory block where the content will be set.
/// @param[in] value The value to set each byte of the memory block to.
/// @param[in] numBytes The number of bytes to set.
/// @return A pointer to the destination memory block after the set operation is complete.
GP_FORCEINLINE_HINT void* setMemory(void* destination, int value, USize numBytes)
{
    return gp::platform::Memory::setMemory(destination, value, numBytes);
}

/// @brief Template overload of setMemory for non-pointer types. This function sets the memory of a non-pointer type to
/// a specified value.
/// @note This function is a convenience overload that allows setting the memory of non-pointer types without needing to
/// take the address of the variable. It uses the size of the type to determine how many bytes to set.
/// @tparam T The type of the variable whose memory is to be set. This type must not be a pointer type, as indicated by
/// the concept constraint.
/// @param[in] source The reference to the variable whose memory will be set.
/// @param[in] value The value to set each byte of the memory block to.
template <class T>
requires(!concepts::IsPointer<T>) inline void setMemory(T& source, UInt8 value)
{
    setMemory(&source, value, sizeof(T));
}

/// @brief Sets a block of memory to zero.
/// @note The behavior is similar to `memset` with a value of 0 in C/C++.
/// @param[in] destination The pointer to the destination memory block where the content will be set to zero.
/// @param[in] numBytes The number of bytes to set to zero.
/// @return A pointer to the destination memory block after the zeroing operation is complete.
GP_FORCEINLINE_HINT void* zeroMemory(void* destination, USize numBytes)
{
    return gp::platform::Memory::zeroMemory(destination, numBytes);
}

/// @brief Template overload of zeroMemory for non-pointer types. This function sets the memory of a non-pointer type to
/// zero.
/// @note This function is a convenience overload that allows zeroing the memory of non-pointer types without needing to
/// take the address of the variable. It uses the size of the type to determine how many bytes to set to zero.
/// @tparam T The type of the variable whose memory is to be zeroed. This type must not be a pointer type, as indicated
/// by the concept constraint.
/// @param[in] source The reference to the variable whose memory will be zeroed.
template <class T>
requires(!concepts::IsPointer<T>) inline void zeroMemory(T& source)
{
    zeroMemory(&source, sizeof(T));
}

/// @brief Checks if a block of memory is zeroed (i.e., all bytes are set to zero).
/// @param[in] ptr The pointer to the memory block to check.
/// @param[in] numBytes The number of bytes to check in the memory block.
/// @return `true` if all bytes in the specified memory block are zero, otherwise `false`.
inline bool isMemoryZeroed(const void* ptr, USize numBytes)
{
    if (GP_UNLIKELY(ptr == nullptr || numBytes == 0))
    {
        return true;
    }

    auto* bytePtr = static_cast<const UInt8*>(ptr);
    return std::ranges::all_of(
        bytePtr,
        bytePtr + numBytes,
        [](UInt8 byte)
    {
        return byte == 0;
    }
    );
}

/// @brief Copies a block of memory from the source to the destination. The behavior is undefined if the source and
/// destination overlap.
/// @note The behavior is similar to `memcpy` in C/C++.
/// @param[in] destination The pointer to the destination memory block where the content will be copied.
/// @param[in] source The pointer to the source memory block from which the content will be copied.
/// @param[in] numBytes The number of bytes to copy from the source to the destination.
/// @return A pointer to the destination memory block after the copy operation is complete.
GP_FORCEINLINE_HINT void* copyMemory(void* destination, const void* source, USize numBytes)
{
    return gp::platform::Memory::copyMemory(destination, source, numBytes);
}

/// @brief Template overload of copyMemory for non-pointer types. This function copies the memory of a non-pointer type
/// from the source to the destination.
/// @note This function is a convenience overload that allows copying the memory of non-pointer types without needing to
/// take the address of the variables. It uses the size of the type to determine how many bytes to copy.
/// @tparam T The type of the variables whose memory is to be copied. This type must not be a pointer type, as indicated
/// by the concept constraint.
/// @param[in] destination The reference to the variable where the content will be copied to.
/// @param[in] source The reference to the variable from which the content will be copied.
template <class T>
requires(!concepts::IsPointer<T>) inline void copyMemory(T& destination, const T& source)
{
    copyMemory(&destination, &source, sizeof(T));
}

/// @brief Copies a block of memory from the source to the destination, optimized for large blocks of memory.
/// @note The behavior is similar to `memcpy` in C/C++, but may be optimized for larger blocks of memory.
/// @param[in] destination The pointer to the destination memory block where the content will be copied.
/// @param[in] source The pointer to the source memory block from which the content will be copied.
/// @param[in] numBytes The number of bytes to copy from the source to the destination.
/// @return A pointer to the destination memory block after the copy operation is complete.
GP_FORCEINLINE_HINT void* copyBigBlockMemory(void* destination, const void* source, USize numBytes)
{
    return gp::platform::Memory::copyBigBlockMemory(destination, source, numBytes);
}

/// @brief Copies a block of memory from the source to the destination, optimized for streaming data. On some
/// platforms, this is optimized for big blocks that avoid L2 cache pollution.
/// @note The behavior is similar to `memcpy` in C/C++, but may be optimized for streaming data or large blocks of
/// memory.
/// @param[in] destination The pointer to the destination memory block where the content will be copied.
/// @param[in] source The pointer to the source memory block from which the content will be copied.
/// @param[in] numBytes The number of bytes to copy from the source to the destination.
/// @return A pointer to the destination memory block after the copy operation is complete.
GP_FORCEINLINE_HINT void* copyStreamingMemory(void* destination, const void* source, gp::USize numBytes)
{
    return gp::platform::Memory::copyStreamingMemory(destination, source, numBytes);
}

/// @brief Copies a block of memory from the source to the destination, optimized for parallel execution. On some
/// platforms, memory copy operations can be distributed across multiple threads or hardware units.
/// @note The behavior is similar to `memcpy` in C/C++, but may be optimized for parallel execution on some
/// platforms.
/// @param[in] destination The pointer to the destination memory block where the content will be copied.
/// @param[in] source The pointer to the source memory block from which the content will be copied.
/// @param[in] numBytes The number of bytes to copy from the source to the destination.
/// @param[in] policy The cache policy to use for the memory copy operation. This parameter is optional and may be
/// ignored on some platforms.
/// @return A pointer to the destination memory block after the copy operation is complete.
GP_FORCEINLINE_HINT void* copyMemoryParallel(
    void* destination, const void* source, gp::USize numBytes
    /* MemoryCopyCachePolicy policy = MemoryCopyCachePolicy::StoreCached */
)
{
    return gp::platform::Memory::copyMemoryParallel(destination, source, numBytes /*, policy*/);
}

/// @brief Allocates a block of memory of the specified size. C style memory allocation stubs to fall back to the
/// standard library's `malloc` function.
/// @param[in] size The size of the memory block to allocate, in bytes.
/// @return A pointer to the allocated memory block, or `nullptr` if the allocation fails.
GP_ALLOCATION_FUNCTION(1) inline void* systemAllocate(USize size)
{
    return ::malloc(size);
}

/// @brief Deallocates a block of memory that was previously allocated. C style memory deallocation stubs to fall back
/// to the standard library's `free` function.
/// @param[in] ptr The pointer to the memory block to deallocate.
inline void systemDeallocate(void* ptr)
{
    ::free(ptr);
}

}   // namespace gp::memory
