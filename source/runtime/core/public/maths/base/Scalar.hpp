// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "maths/base/Constants.hpp"
#include <cmath>

namespace gp::math
{

/// @brief Returns the smaller of two values.
/// @tparam T Arithmetic type of the operands.
/// @param[in] a First value.
/// @param[in] b Second value.
/// @return The smaller of @p a and @p b.
template <concepts::IsArithmetic T>
GP_NODISCARD constexpr T min(const T a, const T b) noexcept
{
    return a < b ? a : b;
}

/// @brief Returns the smallest of three values.
/// @tparam T Arithmetic type of the operands.
/// @param[in] a First value.
/// @param[in] b Second value.
/// @param[in] c Third value.
/// @return The smallest of @p a, @p b, and @p c.
template <concepts::IsArithmetic T>
GP_NODISCARD constexpr T min(const T a, const T b, const T c) noexcept
{
    return min(min(a, b), c);
}

/// @brief Returns the larger of two values.
/// @tparam T Arithmetic type of the operands.
/// @param[in] a First value.
/// @param[in] b Second value.
/// @return The larger of @p a and @p b.
template <concepts::IsArithmetic T>
GP_NODISCARD constexpr T max(const T a, const T b) noexcept
{
    return a > b ? a : b;
}

/// @brief Returns the largest of three values.
/// @tparam T Arithmetic type of the operands.
/// @param[in] a First value.
/// @param[in] b Second value.
/// @param[in] c Third value.
/// @return The largest of @p a, @p b, and @p c.
template <concepts::IsArithmetic T>
GP_NODISCARD constexpr T max(const T a, const T b, const T c) noexcept
{
    return max(max(a, b), c);
}

/// @brief Clamps a value to the inclusive range [minValue, maxValue].
/// @tparam T Arithmetic type of the operands.
/// @param[in] value The value to clamp.
/// @param[in] minValue Lower bound of the clamping range.
/// @param[in] maxValue Upper bound of the clamping range.
/// @return @p value clamped to [@p minValue, @p maxValue].
template <concepts::IsArithmetic T>
GP_NODISCARD constexpr T clamp(const T value, const T minValue, const T maxValue) noexcept
{
    return max(minValue, min(value, maxValue));
}

/// @brief Returns the absolute value of a signed arithmetic value.
/// @tparam T Signed arithmetic type.
/// @param[in] value The value to take the absolute value of.
/// @return @p value if non-negative, otherwise -@p value.
template <concepts::IsArithmetic T>
requires concepts::IsSigned<T> GP_NODISCARD constexpr T abs(const T value) noexcept
{
    return value < static_cast<T>(0) ? -value : value;
}

/// @brief Returns the sign of a value as -1, 0, or 1.
/// @tparam T Signed arithmetic type.
/// @param[in] value The value whose sign is determined.
/// @return T(-1) if @p value < 0, T(0) if @p value == 0, T(1) if @p value > 0.
template <concepts::IsArithmetic T>
requires concepts::IsSigned<T> GP_NODISCARD constexpr T sign(const T value) noexcept
{
    if (value > static_cast<T>(0))
    {
        return static_cast<T>(1);
    }
    else if (value < static_cast<T>(0))
    {
        return static_cast<T>(-1);
    }
    else
    {
        return static_cast<T>(0);
    }
}

/// @brief Checks whether a floating-point value is within a tolerance of zero.
/// @tparam T Floating-point type.
/// @param[in] value The value to test.
/// @param[in] tolerance Maximum absolute deviation from zero to be considered "nearly zero".
/// @return @c true if |@p value| <= @p tolerance, @c false otherwise.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr bool isNearlyZero(const T value, const T tolerance = constants<T>::smallNumber) noexcept
{
    return abs(value) <= tolerance;
}

/// @brief Checks whether two floating-point values are within a tolerance of each other.
/// @tparam T Floating-point type.
/// @param[in] a First value.
/// @param[in] b Second value.
/// @param[in] tolerance Maximum absolute difference to be considered equal.
/// @return @c true if |@p a - @p b| <= @p tolerance, @c false otherwise.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr bool isNearlyEqual(const T a, const T b, const T tolerance = constants<T>::smallNumber) noexcept
{
    return abs(a - b) <= tolerance;
}

/// @brief Linearly interpolates between two values.
/// @tparam T Arithmetic type of the start and end values.
/// @tparam U Floating-point type of the interpolation factor.
/// @param[in] a Start value, returned when @p t == 0.
/// @param[in] b End value, returned when @p t == 1.
/// @param[in] t Interpolation factor, typically in [0, 1].
/// @return (1 - @p t) * @p a + @p t * @p b, cast to @c T.
template <concepts::IsArithmetic T, concepts::IsFloatingPoint U>
GP_NODISCARD constexpr T lerp(const T a, const T b, const U t) noexcept
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
GP_NODISCARD constexpr T inverseLerp(const T a, const T b, const T value) noexcept
{
    if (isNearlyEqual(a, b))
    {
        return static_cast<T>(0);
    }
    return clamp((value - a) / (b - a), static_cast<T>(0), static_cast<T>(1));
}

/// @brief Remaps a value from one range to another.
/// @tparam T Floating-point type.
/// @param[in] inMin Lower bound of the input range.
/// @param[in] inMax Upper bound of the input range.
/// @param[in] outMin Lower bound of the output range.
/// @param[in] outMax Upper bound of the output range.
/// @param[in] value The value to remap.
/// @return @p value mapped linearly into [@p outMin, @p outMax]; clamped if outside [@p inMin, @p inMax].
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T remap(const T inMin, const T inMax, const T outMin, const T outMax, const T value) noexcept
{
    return lerp(outMin, outMax, inverseLerp(inMin, inMax, value));
}

/// @brief Performs smooth Hermite interpolation between 0 and 1 over the range [@p edge0, @p edge1].
/// @tparam T Floating-point type.
/// @param[in] edge0 Lower edge of the interpolation range.
/// @param[in] edge1 Upper edge of the interpolation range. Must differ from @p edge0.
/// @param[in] x The value to evaluate.
/// @return 0 if @p x <= @p edge0, 1 if @p x >= @p edge1, smoothly interpolated otherwise.
/// @note Behavior is undefined (division by zero) when @p edge0 == @p edge1.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T smoothstep(const T edge0, const T edge1, const T x) noexcept
{
    const T t = clamp((x - edge0) / (edge1 - edge0), static_cast<T>(0), static_cast<T>(1));
    return t * t * (static_cast<T>(3) - static_cast<T>(2) * t);
}

/// @brief Converts an angle from degrees to radians.
/// @tparam T Floating-point type.
/// @param[in] degrees Angle in degrees.
/// @return Equivalent angle in radians.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T degreesToRadians(const T degrees) noexcept
{
    return degrees * constants<T>::degToRad;
}

/// @brief Converts an angle from radians to degrees.
/// @tparam T Floating-point type.
/// @param[in] radians Angle in radians.
/// @return Equivalent angle in degrees.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T radiansToDegrees(const T radians) noexcept
{
    return radians * constants<T>::radToDeg;
}

/// @brief Checks whether an integral value is a power of two.
/// @tparam T Integral type.
/// @param[in] value The value to test.
/// @return @c true if @p value is a strictly positive power of two, @c false otherwise.
template <concepts::IsIntegral T>
GP_NODISCARD constexpr bool isPowerOfTwo(const T value) noexcept
{
    return value > 0 && (value & (value - 1)) == 0;
}

/// @brief Returns the smallest power of two greater than or equal to @p value.
/// @tparam T Integral type.
/// @param[in] value The value to round up.
/// @return Smallest power of two >= @p value, or 1 if @p value is 0.
template <concepts::IsIntegral T>
GP_NODISCARD constexpr T nextPowerOfTwo(T value) noexcept
{
    if (value == 0)
    {
        return 1;
    }
    value--;
    for (size_t i = 1; i < sizeof(T) * 8; i *= 2)
    {
        value |= value >> i;
    }
    return value + 1;
}

/// @brief Returns the square root of a value.
/// @tparam T Floating-point type.
/// @param[in] value The value to compute the square root of.
/// @return The square root of @p value.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T sqrt(const T value) noexcept
{
    return std::sqrt(value);
}

/// @brief Returns the reciprocal of the square root of a value.
/// @tparam T Floating-point type.
/// @param[in] value The value to compute the inverse square root of.
/// @return The reciprocal of the square root of @p value.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T inverseSqrt(const T value) noexcept
{
    return static_cast<T>(1) / sqrt(value);
}

}   // namespace gp::math
