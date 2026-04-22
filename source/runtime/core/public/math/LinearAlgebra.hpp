// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include <type_traits>

namespace gp::math
{

/// @brief Returns the smaller of two values.
/// @tparam T The type of the values. Must be an arithmetic type.
/// @param a The first value.
/// @param b The second value.
/// @return The smaller of the two values.
template <typename T>
requires std::is_arithmetic_v<T> GP_NODISCARD GP_FORCEINLINE constexpr T min(T a, T b) noexcept
{
    return (a < b) ? a : b;
}

/// @brief Returns the smaller of three values.
/// @tparam T The type of the values. Must be an arithmetic type.
/// @param a The first value.
/// @param b The second value.
/// @param c The third value.
/// @return The smaller of the three values.
template <typename T>
requires std::is_arithmetic_v<T> GP_NODISCARD GP_FORCEINLINE constexpr T min(T a, T b, T c) noexcept
{
    return min(min(a, b), c);
}

/// @brief Returns the larger of two values.
/// @tparam T The type of the values. Must be an arithmetic type.
/// @param a The first value.
/// @param b The second value.
/// @return The larger of the two values.
template <typename T>
requires std::is_arithmetic_v<T> GP_NODISCARD GP_FORCEINLINE constexpr T max(T a, T b) noexcept
{
    return (a > b) ? a : b;
}

/// @brief Returns the larger of three values.
/// @tparam T The type of the values. Must be an arithmetic type.
/// @param a The first value.
/// @param b The second value.
/// @param c The third value.
/// @return The larger of the three values.
template <typename T>
requires std::is_arithmetic_v<T> GP_NODISCARD GP_FORCEINLINE constexpr T max(T a, T b, T c) noexcept
{
    return max(max(a, b), c);
}

/// @brief Clamps a value between a minimum and maximum value.
/// @tparam T The type of the values. Must be an arithmetic type.
/// @param value The value to clamp.
/// @param minimum The minimum value.
/// @param maximum The maximum value.
/// @return The clamped value.
template <typename T>
requires std::is_arithmetic_v<T> GP_NODISCARD GP_FORCEINLINE constexpr T clamp(T value, T minimum, T maximum) noexcept
{
    return max(minimum, min(value, maximum));
}

}   // namespace gp::math
