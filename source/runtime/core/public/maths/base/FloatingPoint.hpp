// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace gp::math
{

namespace detail
{

/// @brief Checks the IEEE-754 sign bit cleanly at both compile time and runtime.
/// @tparam T Floating-point type.
/// @param[in] value Floating-point operand.
/// @return @c true if the sign bit is set (including `-0.0` and `-NaN`), @c false otherwise.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr bool hasNegativeSignBit(const T value) noexcept
{
    if constexpr (sizeof(T) == 4 && std::numeric_limits<T>::is_iec559)
    {
        return (std::bit_cast<UInt32>(value) & 0x80000000u) != 0u;
    }
    else if constexpr (sizeof(T) == 8 && std::numeric_limits<T>::is_iec559)
    {
        return (std::bit_cast<UInt64>(value) & 0x8000000000000000ull) != 0ull;
    }
    else
    {
        return std::signbit(value);
    }
}

}   // namespace detail

/// @brief Checks whether a floating-point value is NaN (Not-a-Number).
/// @details Uses `std::bit_cast` for IEEE-754 binary32/binary64 so classification stays `constexpr` and immune to
///          `/fp:fast` / `-ffast-math` optimizations that assume finite math and elide `x != x`.
/// @tparam T Floating-point type.
/// @param[in] value The value to inspect.
/// @return @c true if @p value is quiet or signaling NaN, @c false otherwise.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr bool isNaN(const T value) noexcept
{
    if constexpr (sizeof(T) == 4 && std::numeric_limits<T>::is_iec559)
    {
        const UInt32 bits = std::bit_cast<UInt32>(value);
        return (bits & 0x7FFFFFFFu) > 0x7F800000u;
    }
    else if constexpr (sizeof(T) == 8 && std::numeric_limits<T>::is_iec559)
    {
        const UInt64 bits = std::bit_cast<UInt64>(value);
        return (bits & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull;
    }
    else
    {
        return std::isnan(value);
    }
}

/// @brief Checks whether a floating-point value is finite (neither NaN nor +/-infinity).
/// @details Inspects IEEE-754 exponent bits via `std::bit_cast` for binary32/binary64, avoiding `/fp:fast` elision.
/// @tparam T Floating-point type.
/// @param[in] value The value to inspect.
/// @return @c true if @p value is finite, @c false if infinite or NaN.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr bool isFinite(const T value) noexcept
{
    if constexpr (sizeof(T) == 4 && std::numeric_limits<T>::is_iec559)
    {
        const UInt32 bits = std::bit_cast<UInt32>(value);
        return (bits & 0x7F800000u) != 0x7F800000u;
    }
    else if constexpr (sizeof(T) == 8 && std::numeric_limits<T>::is_iec559)
    {
        const UInt64 bits = std::bit_cast<UInt64>(value);
        return (bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
    }
    else
    {
        return std::isfinite(value);
    }
}

/// @brief Checks whether a floating-point value is within a tolerance of zero.
/// @tparam T Floating-point type.
/// @param[in] value The value to test.
/// @param[in] tolerance Maximum absolute deviation from zero to be considered "nearly zero".
/// @return @c true if |@p value| <= @p tolerance, @c false otherwise.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr bool isNearlyZero(const T value, const T tolerance = Constants<T>::smallNumber) noexcept
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
[[nodiscard]] constexpr bool isNearlyEqual(const T a, const T b, const T tolerance = Constants<T>::smallNumber) noexcept
{
    return abs(a - b) <= tolerance;
}

/// @brief Computes the largest floating-point integer value not greater than @p value.
/// @tparam T Floating-point type.
/// @param[in] value The value to round toward negative infinity.
/// @return `floor(value)` as @c T.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T floor(const T value) noexcept
{
    if consteval
    {
        if (!isFinite(value))
        {
            return value;
        }
        constexpr T kMaxFractionalLimit = static_cast<T>(1ull << (std::numeric_limits<T>::digits - 1));
        if (abs(value) >= kMaxFractionalLimit || value == static_cast<T>(0))
        {
            return value;
        }
        const Int64 truncated = static_cast<Int64>(value);
        const T truncFloat = static_cast<T>(truncated);
        return (value < truncFloat) ? static_cast<T>(truncated - 1) : truncFloat;
    }
    else
    {
        return std::floor(value);
    }
}

/// @brief Converts a floating-point value to the smallest integer greater than or equal to @p value.
/// @tparam TInt Signed integer return type (defaults to `gp::Int32`).
/// @tparam TFloat Floating-point input type.
/// @param[in] value The finite floating-point value within representable range of @c TInt.
/// @return `ceil(value)` converted to @c TInt.
template <concepts::IsSignedIntegral TInt = Int32, concepts::IsFloatingPoint TFloat>
[[nodiscard]] constexpr TInt ceilToInt(const TFloat value) noexcept
{
    if consteval
    {
        const TInt truncated = static_cast<TInt>(value);
        return (value > static_cast<TFloat>(truncated)) ? static_cast<TInt>(truncated + static_cast<TInt>(1))
                                                        : truncated;
    }
    else
    {
        return static_cast<TInt>(std::ceil(value));
    }
}

/// @brief Truncates a floating-point value toward zero and converts it to a signed integer.
/// @tparam TInt Signed integer return type (defaults to `gp::Int32`).
/// @tparam TFloat Floating-point input type.
/// @param[in] value The finite floating-point value within representable range of @c TInt.
/// @return `trunc(value)` converted to @c TInt.
template <concepts::IsSignedIntegral TInt = Int32, concepts::IsFloatingPoint TFloat>
[[nodiscard]] constexpr TInt truncToInt(const TFloat value) noexcept
{
    return static_cast<TInt>(value);
}

/// @brief Splits a floating-point value into integral and signed fractional parts.
/// @tparam T Floating-point type.
/// @param[in] value Input value to decompose.
/// @param[out] outIntPart Optional output pointer receiving the integral part (truncated toward zero).
/// @return Signed fractional part with the same sign as @p value.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr T modf(const T value, T* outIntPart) noexcept
{
    if consteval
    {
        const bool isNeg = detail::hasNegativeSignBit(value);
        const T signedZero = isNeg ? static_cast<T>(-0.0) : static_cast<T>(0.0);

        if (!isFinite(value))
        {
            if (outIntPart != nullptr)
            {
                *outIntPart = value;
            }
            return isNaN(value) ? value : signedZero;
        }

        constexpr T kMaxFractionalLimit = static_cast<T>(1ull << (std::numeric_limits<T>::digits - 1));
        if (abs(value) >= kMaxFractionalLimit || value == static_cast<T>(0))
        {
            if (outIntPart != nullptr)
            {
                *outIntPart = value;
            }
            return signedZero;
        }

        const Int64 truncInt = static_cast<Int64>(value);
        const T intPart = (truncInt == 0) ? signedZero : static_cast<T>(truncInt);
        if (outIntPart != nullptr)
        {
            *outIntPart = intPart;
        }
        const T frac = value - intPart;
        return (frac == static_cast<T>(0)) ? signedZero : frac;
    }
    else
    {
        T intPart{};
        const T frac = std::modf(value, &intPart);
        if (outIntPart != nullptr)
        {
            *outIntPart = intPart;
        }
        return frac;
    }
}

}   // namespace gp::math
