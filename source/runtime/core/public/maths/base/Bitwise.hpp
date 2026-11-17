// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <bit>

namespace gp::math
{

/// @brief Checks whether an integral value is a power of two.
/// @tparam T Integral type.
/// @param[in] value The value to test.
/// @return @c true if @p value is a strictly positive power of two, @c false otherwise.
template <concepts::IsIntegral T>
[[nodiscard]] constexpr bool isPowerOfTwo(const T value) noexcept
{
    return value > 0 && (value & (value - 1)) == 0;
}

/// @brief Returns the smallest power of two greater than or equal to @p value.
/// @tparam T Integral type.
/// @param[in] value The value to round up.
/// @return Smallest power of two >= @p value, or 1 if @p value is 0.
template <concepts::IsIntegral T>
[[nodiscard]] constexpr T nextPowerOfTwo(T value) noexcept
{
    if (value == 0)
    {
        return 1;
    }
    --value;
    for (USize i = 1; i < sizeof(T) * 8; i *= 2)
    {
        value = static_cast<T>(value | (value >> i));
    }
    return static_cast<T>(value + 1);
}

/// @brief Computes the base 2 logarithm for an unsigned integer value.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to compute the logarithm for.
/// @return The base 2 logarithm of @p value, rounded down to the nearest integer. Returns 0 if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T floorLog2(const T value) noexcept
{
    if (value == 0)
    {
        return 0;
    }
    return static_cast<T>(std::bit_width(value) - 1);
}

/// @brief Returns the smallest N such that (1 << N) >= value.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to compute the ceiling log2 for.
/// @return The ceiling base 2 logarithm. Returns 0 if @p value is 0 or 1.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T ceilLog2(const T value) noexcept
{
    if (value <= 1)
    {
        return 0;
    }
    return static_cast<T>(std::bit_width(static_cast<T>(value - 1)));
}

/// @brief Counts the number of leading zeros in an unsigned integer value.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to count leading zeros for.
/// @return The number of leading zero bits in @p value. Returns the bit width of T if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T countLeadingZeros(const T value) noexcept
{
    return static_cast<T>(std::countl_zero(value));
}

/// @brief Counts the number of trailing zeros in an unsigned integer value.
/// @details Uses C++20 `std::countr_zero`, guaranteeing well-defined bit-width results even when @p value is 0.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to count trailing zeros for.
/// @return The number of trailing zero bits starting from bit 0, or the bit width of @c T if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T countTrailingZeros(const T value) noexcept
{
    return static_cast<T>(std::countr_zero(value));
}

/// @brief Counts the number of set bits (population count) in an unsigned integer value.
/// @tparam T An unsigned integer type.
/// @param[in] value The value whose set bits are counted.
/// @return Number of bits set to 1 in @p value.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T countBits(const T value) noexcept
{
    return static_cast<T>(std::popcount(value));
}

/// @brief Rounds the given number up to the next highest power of two.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to round up.
/// @return The next power of two. Returns 1 if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T roundUpToPowerOfTwo(const T value) noexcept
{
    return std::bit_ceil(value);
}

/// @brief Rounds the given number down to the nearest power of two.
/// @tparam T An unsigned integer type.
/// @param[in] value The value to round down.
/// @return The nearest power of two below the value. Returns 0 if @p value is 0.
template <concepts::IsUnsignedIntegral T>
[[nodiscard]] constexpr T roundDownToPowerOfTwo(const T value) noexcept
{
    return std::bit_floor(value);
}

}   // namespace gp::math
