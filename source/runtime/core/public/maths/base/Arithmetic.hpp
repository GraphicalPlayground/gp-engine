// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <cmath>

namespace gp::math
{

/// @brief Returns the smaller of two values.
/// @tparam T Arithmetic type of the operands.
/// @param[in] a First value.
/// @param[in] b Second value.
/// @return The smaller of @p a and @p b.
template <concepts::IsArithmetic T>
[[nodiscard]] constexpr T min(const T a, const T b) noexcept
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
[[nodiscard]] constexpr T min(const T a, const T b, const T c) noexcept
{
    return min(min(a, b), c);
}

/// @brief Returns the larger of two values.
/// @tparam T Arithmetic type of the operands.
/// @param[in] a First value.
/// @param[in] b Second value.
/// @return The larger of @p a and @p b.
template <concepts::IsArithmetic T>
[[nodiscard]] constexpr T max(const T a, const T b) noexcept
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
[[nodiscard]] constexpr T max(const T a, const T b, const T c) noexcept
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
[[nodiscard]] constexpr T clamp(const T value, const T minValue, const T maxValue) noexcept
{
    return max(minValue, min(value, maxValue));
}

/// @brief Returns the absolute value of a signed arithmetic value.
/// @tparam T Signed arithmetic type.
/// @param[in] value The value to take the absolute value of.
/// @return @p value if non-negative, otherwise -@p value.
template <concepts::IsArithmetic T>
requires concepts::IsSigned<T> [[nodiscard]] constexpr T abs(const T value) noexcept
{
    return value < static_cast<T>(0) ? -value : value;
}

/// @brief Returns the sign of a value as -1, 0, or 1.
/// @tparam T Signed arithmetic type.
/// @param[in] value The value whose sign is determined.
/// @return T(-1) if @p value < 0, T(0) if @p value == 0, T(1) if @p value > 0.
template <concepts::IsArithmetic T>
requires concepts::IsSigned<T> [[nodiscard]] constexpr T sign(const T value) noexcept
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

/// @brief Divides two integers and rounds the quotient toward positive infinity without addition overflow.
/// @details Computes `(dividend / divisor)` and adjusts by `+1` when a non-zero remainder has the same sign as the
///          divisor, avoiding the `(dividend + divisor - 1) / divisor` wrap-around near integer limits.
/// @tparam T Integral type of the operands.
/// @param[in] dividend Numerator to divide.
/// @param[in] divisor Non-zero denominator.
/// @pre @p divisor != 0.
/// @return `ceil(dividend / divisor)` in integer arithmetic.
template <concepts::IsIntegral T>
[[nodiscard]] constexpr T divideAndRoundUp(const T dividend, const T divisor) noexcept
{
    const T quotient = static_cast<T>(dividend / divisor);
    const T remainder = static_cast<T>(dividend % divisor);
    if constexpr (concepts::IsUnsignedIntegral<T>)
    {
        return (remainder != static_cast<T>(0)) ? static_cast<T>(quotient + static_cast<T>(1)) : quotient;
    }
    else
    {
        const bool sameSign = (dividend > static_cast<T>(0)) == (divisor > static_cast<T>(0));
        return (remainder != static_cast<T>(0) && sameSign) ? static_cast<T>(quotient + static_cast<T>(1)) : quotient;
    }
}

/// @brief Returns the square root of a value.
/// @tparam T Floating-point type.
/// @param[in] value The value to compute the square root of.
/// @return The square root of @p value.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T sqrt(const T value) noexcept
{
    return std::sqrt(value);
}

/// @brief Returns the reciprocal of the square root of a value.
/// @tparam T Floating-point type.
/// @param[in] value The value to compute the inverse square root of.
/// @return The reciprocal of the square root of @p value.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T inverseSqrt(const T value) noexcept
{
    return static_cast<T>(1) / sqrt(value);
}

}   // namespace gp::math
