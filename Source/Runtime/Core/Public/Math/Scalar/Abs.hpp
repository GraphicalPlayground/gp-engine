// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Math/Vector/Vector2.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Math/Vector/Vector4.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief Computes the absolute value of a number, returning the non-negative magnitude.
/// @tparam T A signed arithmetic type (integral or floating-point).
/// @param value The value to compute the absolute value of.
/// @return The absolute value of the input.
template <CArithmetic T>
GP_FORCEINLINE constexpr T Abs(T value) noexcept
{
    return value < static_cast<T>(0) ? -value : value;
}

/// @brief Computes the component-wise absolute value of a vector, returning a vector with non-negative components.
/// @tparam T A floating-point type.
/// @param vec The vector to compute the absolute value of.
/// @return A vector where each component is the absolute value of the corresponding component in the input vector.
template <CFloatingPoint T>
GP_FORCEINLINE constexpr TVector2<T> Abs(const TVector2<T>& vec) noexcept
{
    return { Abs(vec.x), Abs(vec.y) };
}

/// @brief Computes the component-wise absolute value of a vector, returning a vector with non-negative components.
/// @tparam T A floating-point type.
/// @param vec The vector to compute the absolute value of.
/// @return A vector where each component is the absolute value of the corresponding component in the input vector.
template <CFloatingPoint T>
GP_FORCEINLINE constexpr TVector3<T> Abs(const TVector3<T>& vec) noexcept
{
    return { Abs(vec.x), Abs(vec.y), Abs(vec.z) };
}

/// @brief Computes the component-wise absolute value of a vector, returning a vector with non-negative components.
/// @tparam T A floating-point type.
/// @param vec The vector to compute the absolute value of.
/// @return A vector where each component is the absolute value of the corresponding component in the input vector.
template <CFloatingPoint T>
GP_FORCEINLINE constexpr TVector4<T> Abs(const TVector4<T>& vec) noexcept
{
    return { Abs(vec.x), Abs(vec.y), Abs(vec.z), Abs(vec.w) };
}

}   // namespace GP
