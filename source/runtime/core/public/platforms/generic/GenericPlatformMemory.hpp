// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/Backends/Malloc.hpp"
#include "memory/MemoryConstants.hpp"
#include <cstring>

namespace gp::platform::generic
{

/// @brief Generic implementation of the memory management platform for unsupported platforms.
class Memory
{
private:
    /// @brief Deleted constructor prevent instantiation of the Memory class, as it is intended to be used.
    Memory() = delete;

    /// @brief Deleted destructor prevent instantiation of the Memory class, as it is intended to be used.
    ~Memory() = delete;

public:
    /// @brief Moves a block of memory from the source to the destination, handling overlapping regions correctly.
    /// @note The behavior is similar to `memmove` in C/C++.
    /// @param[in] destination The pointer to the destination memory block where the content will be moved.
    /// @param[in] source The pointer to the source memory block from which the content will be moved.
    /// @param[in] numBytes The number of bytes to move from the source to the destination.
    /// @return A pointer to the destination memory block after the move operation is complete.
    static GP_FORCEINLINE_HINT void* moveMemory(void* destination, const void* source, gp::USize numBytes)
    {
        return std::memmove(destination, source, numBytes);
    }

    /// @brief Copies a block of memory from the source to the destination. The behavior is undefined if the source and
    /// destination overlap.
    /// @note The behavior is similar to `memcpy` in C/C++.
    /// @param[in] destination The pointer to the destination memory block where the content will be copied.
    /// @param[in] source The pointer to the source memory block from which the content will be copied.
    /// @param[in] numBytes The number of bytes to copy from the source to the destination.
    /// @return A pointer to the destination memory block after the copy operation is complete.
    static GP_FORCEINLINE_HINT void* copyMemory(void* destination, const void* source, gp::USize numBytes)
    {
        return std::memcpy(destination, source, numBytes);
    }

    /// @brief Sets a block of memory to a specified value. This function fills the first numBytes bytes of the memory
    /// area pointed to by destination with the constant byte value.
    /// @note The behavior is similar to `memset` in C/C++.
    /// @param[in] destination The pointer to the destination memory block where the content will be set.
    /// @param[in] value The value to set each byte of the memory block to.
    /// @param[in] numBytes The number of bytes to set.
    /// @return A pointer to the destination memory block after the set operation is complete.
    static GP_FORCEINLINE_HINT void* setMemory(void* destination, int value, gp::USize numBytes)
    {
        return std::memset(destination, value, numBytes);
    }

    /// @brief Sets a block of memory to zero.
    /// @note The behavior is similar to `memset` with a value of 0 in C/C++.
    /// @param[in] destination The pointer to the destination memory block where the content will be set to zero.
    /// @param[in] numBytes The number of bytes to set to zero.
    /// @return A pointer to the destination memory block after the zeroing operation is complete.
    static GP_FORCEINLINE_HINT void* zeroMemory(void* destination, gp::USize numBytes)
    {
        return std::memset(destination, 0, numBytes);
    }

    /// @brief Compares two blocks of memory.
    /// @note The behavior is similar to `memcmp` in C/C++.
    /// @param[in] ptr1 The pointer to the first memory block to compare.
    /// @param[in] ptr2 The pointer to the second memory block to compare.
    /// @param[in] numBytes The number of bytes to compare.
    /// @return An integer indicating the result of the comparison.
    static GP_FORCEINLINE_HINT int compareMemory(const void* ptr1, const void* ptr2, gp::USize numBytes)
    {
        return std::memcmp(ptr1, ptr2, numBytes);
    }

    /// @brief Copies a block of memory from the source to the destination, optimized for large blocks of memory.
    /// @note The behavior is similar to `memcpy` in C/C++, but may be optimized for larger blocks of memory.
    /// @param[in] destination The pointer to the destination memory block where the content will be copied.
    /// @param[in] source The pointer to the source memory block from which the content will be copied.
    /// @param[in] numBytes The number of bytes to copy from the source to the destination.
    /// @return A pointer to the destination memory block after the copy operation is complete.
    static GP_FORCEINLINE_HINT void* copyBigBlockMemory(void* destination, const void* source, gp::USize numBytes)
    {
        return std::memcpy(destination, source, numBytes);
    }

    /// @brief Copies a block of memory from the source to the destination, optimized for streaming data. On some
    /// platforms, this is optimized for big blocks that avoid L2 cache pollution.
    /// @note The behavior is similar to `memcpy` in C/C++, but may be optimized for streaming data or large blocks of
    /// memory.
    /// @param[in] destination The pointer to the destination memory block where the content will be copied.
    /// @param[in] source The pointer to the source memory block from which the content will be copied.
    /// @param[in] numBytes The number of bytes to copy from the source to the destination.
    /// @return A pointer to the destination memory block after the copy operation is complete.
    static GP_FORCEINLINE_HINT void* copyStreamingMemory(void* destination, const void* source, gp::USize numBytes)
    {
        return std::memcpy(destination, source, numBytes);
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
    static GP_FORCEINLINE_HINT void* copyMemoryParallel(
        void* destination, const void* source, gp::USize numBytes
        /* MemoryCopyCachePolicy policy = MemoryCopyCachePolicy::StoreCached */
    )
    {
        return std::memcpy(destination, source, numBytes);
    }

    /// @brief Get the default memory allocator for the platform.
    /// @return A pointer to the default memory allocator for the platform.
    static GP_CORE_API memory::Malloc* getDefaultAllocator();

    /// @brief Get the platform-specific memory constants, such as page size, and other relevant parameters.
    /// @return The platform-specific memory constants.
    static GP_CORE_API memory::PlatformConstants getPlatformConstants();
};

}   // namespace gp::platform::generic
