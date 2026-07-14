// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"

namespace gp::memory
{

static constexpr UInt32 kDefaultAlignment = 0;
static constexpr UInt32 kMinimumAlignment = 8;

/// @brief Aligns a value to the nearest higher multiple of the specified alignment.
/// @tparam T Must be an integral or pointer type.
/// @param[in] value The value to be aligned.
/// @param[in] alignment The alignment boundary. Must be a power of two.
/// @return The aligned value.
template <typename T>
requires concepts::IsIntegral<T> || concepts::IsPointer<T>
[[nodiscard]] inline constexpr T align(T value, UInt64 alignment)
{
    if constexpr (concepts::IsPointer<T>)
    {
        return reinterpret_cast<T>((reinterpret_cast<UInt64>(value) + alignment - 1) & ~(alignment - 1));
    }
    else
    {
        return static_cast<T>((static_cast<UInt64>(value) + alignment - 1) & ~(alignment - 1));
    }
}

/// @brief Aligns a value to the nearest lower multiple of the specified alignment.
/// @tparam T Must be an integral or pointer type.
/// @param[in] value The value to be aligned.
/// @param[in] alignment The alignment boundary. Must be a power of two.
/// @return The aligned value.
template <typename T>
requires concepts::IsIntegral<T> || concepts::IsPointer<T>
[[nodiscard]] inline constexpr T alignDown(T value, UInt64 alignment)
{
    if constexpr (concepts::IsPointer<T>)
    {
        return reinterpret_cast<T>(reinterpret_cast<UInt64>(value) & ~(alignment - 1));
    }
    else
    {
        return static_cast<T>(static_cast<UInt64>(value) & ~(alignment - 1));
    }
}

/// @brief Checks if a value is already aligned to the specified alignment boundary.
/// @tparam T Must be an integral or pointer type.
/// @param[in] value The value to check for alignment.
/// @param[in] alignment The alignment boundary. Must be a power of two.
/// @return true if the value is aligned, false otherwise.
template <typename T>
requires concepts::IsIntegral<T> || concepts::IsPointer<T>
[[nodiscard]] inline constexpr bool isAligned(T value, UInt64 alignment)
{
    if constexpr (concepts::IsPointer<T>)
    {
        return (reinterpret_cast<UInt64>(value) & (alignment - 1)) == 0;
    }
    else
    {
        return (static_cast<UInt64>(value) & (alignment - 1)) == 0;
    }
}

/// @brief Aligns a value to the nearest higher multiple of the specified alignment without requiring the alignment to
///        be a power of two.
/// @tparam T Must be an integral or pointer type.
/// @param[in] value The value to be aligned.
/// @param[in] alignment The alignment boundary.
/// @return The aligned value.
template <typename T>
requires concepts::IsIntegral<T> || concepts::IsPointer<T>
[[nodiscard]] inline constexpr T alignArbitrary(T value, UInt64 alignment)
{
    if constexpr (concepts::IsPointer<T>)
    {
        return reinterpret_cast<T>(((reinterpret_cast<UInt64>(value) + alignment - 1) / alignment) * alignment);
    }
    else
    {
        return static_cast<T>(((static_cast<UInt64>(value) + alignment - 1) / alignment) * alignment);
    }
}

}   // namespace gp::memory
