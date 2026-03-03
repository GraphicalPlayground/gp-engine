// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/Vector/Vector2.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Math/Vector/Vector4.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief Returns the maximum of two values.
/// @tparam T Arithmetic type.
/// @param a First value.
/// @param b Second value.
/// @return The greater of a and b.
template <CArithmetic T>
GP_NODISCARD constexpr T Max(T a, T b) noexcept
{
    return a > b ? a : b;
}

/// @brief Returns the maximum of three values.
/// @tparam T Arithmetic type.
/// @param a First value.
/// @param b Second value.
/// @param c Third value.
/// @return The greatest of a, b, and c.
template <CArithmetic T>
GP_NODISCARD constexpr T Max(T a, T b, T c) noexcept
{
    return Max(Max(a, b), c);
}

/// @brief Returns the component-wise maximum of two TVector2.
/// @tparam T Floating-point type.
/// @param a First vector.
/// @param b Second vector.
/// @return A TVector2 where each component is the maximum of the corresponding components of a and b.
template <CFloatingPoint T>
GP_NODISCARD constexpr TVector2<T> Max(const TVector2<T>& a, const TVector2<T>& b) noexcept
{
    return TVector2<T>(Max(a.x, b.x), Max(a.y, b.y));
}

/// @brief Returns the component-wise maximum of two TVector3.
/// @tparam T Floating-point type.
/// @param a First vector.
/// @param b Second vector.
/// @return A TVector3 where each component is the maximum of the corresponding components of a and b.
template <CFloatingPoint T>
GP_NODISCARD constexpr TVector3<T> Max(const TVector3<T>& a, const TVector3<T>& b) noexcept
{
    return TVector3<T>(Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z));
}

/// @brief Returns the component-wise maximum of two TVector4.
/// @tparam T Floating-point type.
/// @param a First vector.
/// @param b Second vector.
/// @return A TVector4 where each component is the maximum of the corresponding components of a and b.
template <CFloatingPoint T>
GP_NODISCARD constexpr TVector4<T> Max(const TVector4<T>& a, const TVector4<T>& b) noexcept
{
    return TVector4<T>(Max(a.x, b.x), Max(a.y, b.y), Max(a.z, b.z), Max(a.w, b.w));
}

}   // namespace GP
