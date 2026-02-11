// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/Utils/Constants.hpp"
#include "Templates/Concepts.hpp"
#include <cmath>

namespace GP::Math
{

/// \brief Calculate the square root of a value.
/// \tparam T The type of the value.
/// \param value The value to calculate the square root of.
/// \return The square root of the value.
template <Concepts::IsArithmetic T>
constexpr T Sqrt(T value) noexcept
{
    return std::sqrt(value);
}

/// \brief Calculate the inverse square root of a value (1/sqrt(value)).
/// \tparam T The type of the value.
/// \param value The value to calculate the inverse square root of.
/// \return The inverse square root of the value.
template <Concepts::IsArithmetic T>
constexpr T InvSqrt(T value) noexcept
{
    return static_cast<T>(1.0) / std::sqrt(value);
}

/// \brief Calculate the square of a value.
/// \tparam T The type of the value.
/// \param value The value to square.
/// \return The square of the value.
template <Concepts::IsArithmetic T>
constexpr T Square(T value) noexcept
{
    return value * value;
}

/// \brief Calculate the cube of a value.
/// \tparam T The type of the value.
/// \param value The value to cube.
/// \return The cube of the value.
template <Concepts::IsArithmetic T>
constexpr T Cube(T value) noexcept
{
    return value * value * value;
}

/// \brief Calculate the absolute value of a value.
/// \tparam T The type of the value.
/// \param value The value to calculate the absolute value of.
/// \return The absolute value.
template <Concepts::IsArithmetic T>
constexpr T Abs(T value) noexcept
{
    return (value < static_cast<T>(0)) ? -value : value;
}

/// \brief Get the sign of a value (-1, 0, or 1).
/// \tparam T The type of the value.
/// \param value The value to get the sign of.
/// \return -1 if negative, 0 if zero, 1 if positive.
template <Concepts::IsArithmetic T>
constexpr T Sign(T value) noexcept
{
    return (value > static_cast<T>(0))   ? static_cast<T>(1)
           : (value < static_cast<T>(0)) ? static_cast<T>(-1)
                                         : static_cast<T>(0);
}

/// \brief Calculate the power of a base raised to an exponent.
/// \tparam T The type of the base and exponent.
/// \param base The base value.
/// \param exponent The exponent value.
/// \return The result of base raised to the power of exponent.
template <Concepts::IsArithmetic T>
constexpr T Pow(T base, T exponent) noexcept
{
    return std::pow(base, exponent);
}

/// \brief Calculate the natural logarithm of a value.
/// \tparam T The type of the value.
/// \param value The value to calculate the logarithm of.
/// \return The natural logarithm of the value.
template <Concepts::IsFloatingPoint T>
constexpr T Log(T value) noexcept
{
    return std::log(value);
}

/// \brief Calculate the base-10 logarithm of a value.
/// \tparam T The type of the value.
/// \param value The value to calculate the logarithm of.
/// \return The base-10 logarithm of the value.
template <Concepts::IsFloatingPoint T>
constexpr T Log10(T value) noexcept
{
    return std::log10(value);
}

/// \brief Calculate the base-2 logarithm of a value.
/// \tparam T The type of the value.
/// \param value The value to calculate the logarithm of.
/// \return The base-2 logarithm of the value.
template <Concepts::IsFloatingPoint T>
constexpr T Log2(T value) noexcept
{
    return std::log2(value);
}

/// \brief Calculate e raised to the power of a value.
/// \tparam T The type of the value.
/// \param value The exponent value.
/// \return e^value.
template <Concepts::IsFloatingPoint T>
constexpr T Exp(T value) noexcept
{
    return std::exp(value);
}

/// \brief Calculate the sine of an angle in radians.
/// \tparam T The type of the angle.
/// \param angle The angle in radians.
/// \return The sine of the angle.
template <Concepts::IsFloatingPoint T>
constexpr T Sin(T angle) noexcept
{
    return std::sin(angle);
}

/// \brief Calculate the cosine of an angle in radians.
/// \tparam T The type of the angle.
/// \param angle The angle in radians.
/// \return The cosine of the angle.
template <Concepts::IsFloatingPoint T>
constexpr T Cos(T angle) noexcept
{
    return std::cos(angle);
}

/// \brief Calculate the tangent of an angle in radians.
/// \tparam T The type of the angle.
/// \param angle The angle in radians.
/// \return The tangent of the angle.
template <Concepts::IsFloatingPoint T>
constexpr T Tan(T angle) noexcept
{
    return std::tan(angle);
}

/// \brief Calculate the arcsine of a value.
/// \tparam T The type of the value.
/// \param value The value to calculate the arcsine of.
/// \return The arcsine of the value in radians.
template <Concepts::IsFloatingPoint T>
constexpr T Asin(T value) noexcept
{
    return std::asin(value);
}

/// \brief Calculate the arccosine of a value.
/// \tparam T The type of the value.
/// \param value The value to calculate the arccosine of.
/// \return The arccosine of the value in radians.
template <Concepts::IsFloatingPoint T>
constexpr T Acos(T value) noexcept
{
    return std::acos(value);
}

/// \brief Calculate the arctangent of a value.
/// \tparam T The type of the value.
/// \param value The value to calculate the arctangent of.
/// \return The arctangent of the value in radians.
template <Concepts::IsFloatingPoint T>
constexpr T Atan(T value) noexcept
{
    return std::atan(value);
}

/// \brief Calculate the arctangent of y/x considering the signs of both arguments to determine the correct quadrant.
/// \tparam T The type of the values.
/// \param y The y coordinate.
/// \param x The x coordinate.
/// \return The arctangent of y/x in radians.
template <Concepts::IsFloatingPoint T>
constexpr T Atan2(T y, T x) noexcept
{
    return std::atan2(y, x);
}

/// \brief Calculate the hyperbolic sine of a value.
/// \tparam T The type of the value.
/// \param value The value.
/// \return The hyperbolic sine of the value.
template <Concepts::IsFloatingPoint T>
constexpr T Sinh(T value) noexcept
{
    return std::sinh(value);
}

/// \brief Calculate the hyperbolic cosine of a value.
/// \tparam T The type of the value.
/// \param value The value.
/// \return The hyperbolic cosine of the value.
template <Concepts::IsFloatingPoint T>
constexpr T Cosh(T value) noexcept
{
    return std::cosh(value);
}

/// \brief Calculate the hyperbolic tangent of a value.
/// \tparam T The type of the value.
/// \param value The value.
/// \return The hyperbolic tangent of the value.
template <Concepts::IsFloatingPoint T>
constexpr T Tanh(T value) noexcept
{
    return std::tanh(value);
}

/// \brief Round a value down to the nearest integer.
/// \tparam T The type of the value.
/// \param value The value to floor.
/// \return The floored value.
template <Concepts::IsFloatingPoint T>
constexpr T Floor(T value) noexcept
{
    return std::floor(value);
}

/// \brief Round a value up to the nearest integer.
/// \tparam T The type of the value.
/// \param value The value to ceil.
/// \return The ceiled value.
template <Concepts::IsFloatingPoint T>
constexpr T Ceil(T value) noexcept
{
    return std::ceil(value);
}

/// \brief Round a value to the nearest integer.
/// \tparam T The type of the value.
/// \param value The value to round.
/// \return The rounded value.
template <Concepts::IsFloatingPoint T>
constexpr T Round(T value) noexcept
{
    return std::round(value);
}

/// \brief Truncate a value towards zero.
/// \tparam T The type of the value.
/// \param value The value to truncate.
/// \return The truncated value.
template <Concepts::IsFloatingPoint T>
constexpr T Trunc(T value) noexcept
{
    return std::trunc(value);
}

/// \brief Get the fractional part of a value.
/// \tparam T The type of the value.
/// \param value The value.
/// \return The fractional part (value - floor(value)).
template <Concepts::IsFloatingPoint T>
constexpr T Frac(T value) noexcept
{
    return value - std::floor(value);
}

/// \brief Calculate the modulo of two values.
/// \tparam T The type of the values.
/// \param x The dividend.
/// \param y The divisor.
/// \return The remainder of x/y.
template <Concepts::IsFloatingPoint T>
constexpr T Mod(T x, T y) noexcept
{
    return std::fmod(x, y);
}

/// \brief Return the minimum of two values.
/// \tparam T The type of the values.
/// \param a The first value.
/// \param b The second value.
/// \return The smaller of the two values.
template <Concepts::IsArithmetic T>
constexpr T Min(T a, T b) noexcept
{
    return (a < b) ? a : b;
}

/// \brief Return the maximum of two values.
/// \tparam T The type of the values.
/// \param a The first value.
/// \param b The second value.
/// \return The larger of the two values.
template <Concepts::IsArithmetic T>
constexpr T Max(T a, T b) noexcept
{
    return (a > b) ? a : b;
}

/// \brief Return the minimum of three values.
/// \tparam T The type of the values.
/// \param a The first value.
/// \param b The second value.
/// \param c The third value.
/// \return The smallest of the three values.
template <Concepts::IsArithmetic T>
constexpr T Min3(T a, T b, T c) noexcept
{
    return Min(Min(a, b), c);
}

/// \brief Return the maximum of three values.
/// \tparam T The type of the values.
/// \param a The first value.
/// \param b The second value.
/// \param c The third value.
/// \return The largest of the three values.
template <Concepts::IsArithmetic T>
constexpr T Max3(T a, T b, T c) noexcept
{
    return Max(Max(a, b), c);
}

/// \brief Clamp a value between a minimum and maximum value.
/// \tparam T The type of the value.
/// \param value The value to clamp.
/// \param min The minimum value.
/// \param max The maximum value.
/// \return The clamped value.
template <Concepts::IsArithmetic T>
constexpr T Clamp(T value, T min, T max) noexcept
{
    return (value < min) ? min : (value > max) ? max : value;
}

/// \brief Clamp a value between 0 and 1.
/// \tparam T The type of the value.
/// \param value The value to clamp.
/// \return The clamped value between 0 and 1.
template <Concepts::IsArithmetic T>
constexpr T Saturate(T value) noexcept
{
    return Clamp(value, static_cast<T>(0), static_cast<T>(1));
}

/// \brief Linearly interpolate between two values.
/// \tparam T The type of the values.
/// \param a The start value.
/// \param b The end value.
/// \param t The interpolation factor (0.0 to 1.0).
/// \return The interpolated value.
template <Concepts::IsArithmetic T>
constexpr T Lerp(T a, T b, T t) noexcept
{
    return a + (b - a) * t;
}

/// \brief Inverse lerp - find the interpolation factor for a value between two bounds.
/// \tparam T The type of the values.
/// \param a The start value.
/// \param b The end value.
/// \param value The value to find the factor for.
/// \return The interpolation factor (0 when value==a, 1 when value==b).
template <Concepts::IsArithmetic T>
constexpr T InverseLerp(T a, T b, T value) noexcept
{
    return (b - a != static_cast<T>(0)) ? ((value - a) / (b - a)) : static_cast<T>(0);
}

/// \brief Smooth interpolation using Hermite interpolation (smoothstep).
/// \tparam T The type of the values.
/// \param a The start value.
/// \param b The end value.
/// \param t The interpolation factor (0.0 to 1.0).
/// \return The smoothly interpolated value.
template <Concepts::IsFloatingPoint T>
constexpr T SmoothStep(T a, T b, T t) noexcept
{
    t = Saturate(t);
    t = t * t * (static_cast<T>(3) - static_cast<T>(2) * t);
    return Lerp(a, b, t);
}

/// \brief Smoother interpolation using improved smoothstep (Ken Perlin's version).
/// \tparam T The type of the values.
/// \param a The start value.
/// \param b The end value.
/// \param t The interpolation factor (0.0 to 1.0).
/// \return The smoothly interpolated value with better derivatives.
template <Concepts::IsFloatingPoint T>
constexpr T SmootherStep(T a, T b, T t) noexcept
{
    t = Saturate(t);
    t = t * t * t * (t * (t * static_cast<T>(6) - static_cast<T>(15)) + static_cast<T>(10));
    return Lerp(a, b, t);
}

/// \brief Remap a value from one range to another.
/// \tparam T The type of the values.
/// \param value The value to remap.
/// \param inMin The input range minimum.
/// \param inMax The input range maximum.
/// \param outMin The output range minimum.
/// \param outMax The output range maximum.
/// \return The remapped value.
template <Concepts::IsArithmetic T>
constexpr T Remap(T value, T inMin, T inMax, T outMin, T outMax) noexcept
{
    T t = InverseLerp(inMin, inMax, value);
    return Lerp(outMin, outMax, t);
}

/// \brief Wrap a value to a range [0, max).
/// \tparam T The type of the value.
/// \param value The value to wrap.
/// \param max The maximum value (exclusive).
/// \return The wrapped value.
template <Concepts::IsFloatingPoint T>
constexpr T Wrap(T value, T max) noexcept
{
    T mod = std::fmod(value, max);
    return (mod < static_cast<T>(0)) ? mod + max : mod;
}

/// \brief Wrap a value to a range [min, max).
/// \tparam T The type of the value.
/// \param value The value to wrap.
/// \param min The minimum value (inclusive).
/// \param max The maximum value (exclusive).
/// \return The wrapped value.
template <Concepts::IsFloatingPoint T>
constexpr T WrapRange(T value, T min, T max) noexcept
{
    return min + Wrap(value - min, max - min);
}

/// \brief Check if two values are nearly equal within a tolerance.
/// \tparam T The type of the values.
/// \param a The first value.
/// \param b The second value.
/// \param tolerance The tolerance for comparison.
/// \return True if the values are nearly equal.
template <Concepts::IsArithmetic T>
constexpr bool IsNearlyEqual(T a, T b, T tolerance = Constants<T>::KindaSmallNumber) noexcept
{
    return Abs(a - b) <= tolerance;
}

/// \brief Check if a value is nearly zero within a tolerance.
/// \tparam T The type of the value.
/// \param value The value to check.
/// \param tolerance The tolerance for comparison.
/// \return True if the value is nearly zero.
template <Concepts::IsArithmetic T>
constexpr bool IsNearlyZero(T value, T tolerance = Constants<T>::KindaSmallNumber) noexcept
{
    return Abs(value) <= tolerance;
}

/// \brief Check if a value is within a range [min, max].
/// \tparam T The type of the value.
/// \param value The value to check.
/// \param min The minimum value (inclusive).
/// \param max The maximum value (inclusive).
/// \return True if the value is within the range.
template <Concepts::IsArithmetic T>
constexpr bool IsInRange(T value, T min, T max) noexcept
{
    return value >= min && value <= max;
}

/// \brief Check if a value is a power of two.
/// \tparam T The type of the value (must be integral).
/// \param value The value to check.
/// \return True if the value is a power of two.
template <typename T>
requires std::is_integral_v<T> constexpr bool IsPowerOfTwo(T value) noexcept
{
    return value > 0 && (value & (value - 1)) == 0;
}

/// \brief Convert degrees to radians.
/// \tparam T The type of the angle.
/// \param degrees The angle in degrees.
/// \return The angle in radians.
template <Concepts::IsFloatingPoint T>
constexpr T DegreesToRadians(T degrees) noexcept
{
    return degrees * Constants<T>::DegToRad;
}

/// \brief Convert radians to degrees.
/// \tparam T The type of the angle.
/// \param radians The angle in radians.
/// \return The angle in degrees.
template <Concepts::IsFloatingPoint T>
constexpr T RadiansToDegrees(T radians) noexcept
{
    return radians * Constants<T>::RadToDeg;
}

/// \brief Normalize an angle to the range [0, 2π).
/// \tparam T The type of the angle.
/// \param angle The angle in radians.
/// \return The normalized angle.
template <Concepts::IsFloatingPoint T>
constexpr T NormalizeAngle(T angle) noexcept
{
    return Wrap(angle, Constants<T>::TwoPi);
}

/// \brief Normalize an angle to the range [-π, π).
/// \tparam T The type of the angle.
/// \param angle The angle in radians.
/// \return The normalized angle.
template <Concepts::IsFloatingPoint T>
constexpr T NormalizeAngleSigned(T angle) noexcept
{
    T normalized = Wrap(angle, Constants<T>::TwoPi);
    return (normalized > Constants<T>::Pi) ? normalized - Constants<T>::TwoPi : normalized;
}

/// \brief Check if a floating-point value is NaN (Not a Number).
/// \tparam T The type of the value.
/// \param value The value to check.
/// \return True if the value is NaN.
template <Concepts::IsFloatingPoint T>
constexpr bool IsNaN(T value) noexcept
{
    return std::isnan(value);
}

/// \brief Check if a floating-point value is infinite.
/// \tparam T The type of the value.
/// \param value The value to check.
/// \return True if the value is infinite.
template <Concepts::IsFloatingPoint T>
constexpr bool IsInfinite(T value) noexcept
{
    return std::isinf(value);
}

/// \brief Check if a floating-point value is finite (not NaN or infinite).
/// \tparam T The type of the value.
/// \param value The value to check.
/// \return True if the value is finite.
template <Concepts::IsFloatingPoint T>
constexpr bool IsFinite(T value) noexcept
{
    return std::isfinite(value);
}

}   // namespace GP::Math
