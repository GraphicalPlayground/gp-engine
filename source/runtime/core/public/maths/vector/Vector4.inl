// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "maths/vector/Vector4.hpp"

// Include the other vector for conversions and operations
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"

namespace gp::math
{

template <concepts::IsFloatingPoint T>
template <concepts::IsFloatingPoint U>
[[nodiscard]] constexpr Vector4<T>::Vector4(const Vector3<T>& vec, const U inW) noexcept
    : x(vec.x)
    , y(vec.y)
    , z(vec.z)
    , w(static_cast<T>(inW))
{}

template <concepts::IsFloatingPoint T>
template <concepts::IsFloatingPoint U>
[[nodiscard]] constexpr Vector4<T>::Vector4(const Vector2<T>& vec, const U inZ, const U inW) noexcept
    : x(vec.x)
    , y(vec.y)
    , z(static_cast<T>(inZ))
    , w(static_cast<T>(inW))
{}

template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr Vector4<T>::Vector4(const Vector2<T>& vecXY, const Vector2<T>& vecZW) noexcept
    : x(vecXY.x)
    , y(vecXY.y)
    , z(vecZW.x)
    , w(vecZW.y)
{}

}   // namespace gp::math
