// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreMinimal.hpp"

namespace gp::memory
{

/// @brief Rounds a value up to the nearest multiple of the given alignment.
/// @details
/// Alignment must be a power of two. This is a branchless, constexpr operation suitable for hot-path usage.
/// @param[in] value The value to align upward.
/// @param[in] alignment The alignment boundary (must be a power of two).
/// @return The smallest value >= input that is a multiple of alignment.
GP_FORCEINLINE constexpr USize alignUp(USize value, USize alignment) noexcept
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

/// @brief Rounds a value down to the nearest multiple of the given alignment.
/// @param[in] value The value to align downward.
/// @param[in] alignment The alignment boundary (must be a power of two).
/// @return The largest value <= input that is a multiple of alignment.
GP_FORCEINLINE constexpr USize alignDown(USize value, USize alignment) noexcept { return value & ~(alignment - 1); }

/// @brief Checks whether a value is aligned to the given boundary.
/// @param[in] value The value to check.
/// @param[in] alignment The alignment boundary (must be a power of two).
/// @return True if value is a multiple of alignment.
GP_FORCEINLINE constexpr bool isAligned(USize value, USize alignment) noexcept
{
    return (value & (alignment - 1)) == 0;
}

/// @brief Checks whether a pointer is aligned to the given boundary.
/// @param[in] ptr The pointer to check.
/// @param[in] alignment The alignment boundary (must be a power of two).
/// @return True if the pointer address is a multiple of alignment.
GP_FORCEINLINE bool isAligned(const void* ptr, USize alignment) noexcept
{
    return isAligned(reinterpret_cast<USize>(ptr), alignment);
}

/// @brief Low-level cross-platform aligned memory allocation.
/// @details
/// This is the single point of entry for all raw allocation in the engine. On POSIX platforms, the size is rounded up
/// to a multiple of alignment to satisfy the aligned_alloc(3) contract. On Windows, _aligned_malloc has no such
/// constraint but we round anyway for consistency.
/// @param[in] size The number of bytes to allocate. Must be > 0.
/// @param[in] alignment The required alignment (must be a power of two).
/// @return Pointer to the allocated memory, or nullptr on failure.
GP_NODISCARD GP_FORCEINLINE void* malloc(USize size, USize alignment = alignof(gp::MaxAlign)) noexcept
{
    if (size == 0) { return nullptr; }
    const USize alignedSize = alignUp(size, alignment);
    return GP_ALIGNED_ALLOC(alignedSize, alignment);
}

/// @brief Low-level cross-platform aligned memory deallocation.
/// @details
/// Pairs with gp::memory::malloc. Passing nullptr is a safe no-op.
/// @param[in] ptr Pointer previously returned by gp::memory::malloc.
GP_FORCEINLINE void free(void* ptr) noexcept
{
    if (ptr) { GP_ALIGNED_FREE(ptr); }
}

}   // namespace gp::memory
