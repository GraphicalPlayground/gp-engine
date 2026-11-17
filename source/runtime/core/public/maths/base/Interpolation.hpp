// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/FloatingPoint.hpp"

namespace gp::math
{

/// @brief Linearly interpolates between two values.
/// @tparam T Arithmetic type of the start and end values.
/// @tparam U Floating-point type of the interpolation factor.
/// @param[in] a Start value, returned when @p t == 0.
/// @param[in] b End value, returned when @p t == 1.
/// @param[in] t Interpolation factor, typically in [0, 1].
/// @return (1 - @p t) * @p a + @p t * @p b, cast to @c T.
template <concepts::IsArithmetic T, concepts::IsFloatingPoint U>
[[nodiscard]] constexpr T lerp(const T a, const T b, const U t) noexcept
{
    return static_cast<T>((static_cast<U>(1) - t) * a + t * b);
}

/// @brief Returns the normalized position of a value within the range [@p a, @p b].
/// @tparam T Floating-point type.
/// @param[in] a Start of the range.
/// @param[in] b End of the range.
/// @param[in] value The value to normalize.
/// @return Interpolation factor in [0, 1] such that lerp(@p a, @p b, result) == @p value,
///         or 0 if @p a and @p b are nearly equal.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T inverseLerp(const T a, const T b, const T value) noexcept
{
    if (isNearlyEqual(a, b))
    {
        return static_cast<T>(0);
    }
    return clamp((value - a) / (b - a), static_cast<T>(0), static_cast<T>(1));
}

/// @brief Remaps a value from one range to another, clamping outside [@p inMin, @p inMax].
/// @tparam T Floating-point type.
/// @param[in] inMin Start of the input range.
/// @param[in] inMax End of the input range (may be smaller than @p inMin for reversed ranges).
/// @param[in] outMin Start of the output range.
/// @param[in] outMax End of the output range.
/// @param[in] value The value to remap.
/// @return @p value mapped linearly into [@p outMin, @p outMax]; clamped to output bounds if outside input range.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T remap(const T inMin, const T inMax, const T outMin, const T outMax, const T value) noexcept
{
    return lerp(outMin, outMax, inverseLerp(inMin, inMax, value));
}

/// @brief Maps a value from an input range to an output range, clamping to output bounds.
/// @details Supports both ascending (`inMin < inMax`) and descending (`inMin > inMax`) ranges and returns @p outMin
///          when the input range is degenerate.
/// @tparam T Floating-point type.
/// @param[in] inMin Start of the input range.
/// @param[in] inMax End of the input range.
/// @param[in] outMin Start of the output range.
/// @param[in] outMax End of the output range.
/// @param[in] value Input value to remap.
/// @return Remapped and clamped value in [@p outMin, @p outMax].
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T
    getMappedRangeValueClamped(const T inMin, const T inMax, const T outMin, const T outMax, const T value) noexcept
{
    return remap(inMin, inMax, outMin, outMax, value);
}

/// @brief Performs smooth Hermite interpolation between 0 and 1 over the range [@p edge0, @p edge1].
/// @tparam T Floating-point type.
/// @param[in] edge0 Lower edge of the interpolation range.
/// @param[in] edge1 Upper edge of the interpolation range. Must differ from @p edge0.
/// @param[in] x The value to evaluate.
/// @return 0 if @p x <= @p edge0, 1 if @p x >= @p edge1, smoothly interpolated otherwise.
/// @note Behavior is undefined (division by zero) when @p edge0 == @p edge1.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T smoothstep(const T edge0, const T edge1, const T x) noexcept
{
    const T t = clamp((x - edge0) / (edge1 - edge0), static_cast<T>(0), static_cast<T>(1));
    return t * t * (static_cast<T>(3) - static_cast<T>(2) * t);
}

/// @brief Converts an angle from degrees to radians.
/// @tparam T Floating-point type.
/// @param[in] degrees Angle in degrees.
/// @return Equivalent angle in radians.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T degreesToRadians(const T degrees) noexcept
{
    return degrees * Constants<T>::degToRad;
}

/// @brief Converts an angle from radians to degrees.
/// @tparam T Floating-point type.
/// @param[in] radians Angle in radians.
/// @return Equivalent angle in degrees.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T radiansToDegrees(const T radians) noexcept
{
    return radians * Constants<T>::radToDeg;
}

}   // namespace gp::math
