// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "maths/vector/Vector3.hpp"

// Include the other vector for conversions and operations
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector4.hpp"

namespace gp::math
{

template <concepts::IsFloatingPoint T>
template <concepts::IsFloatingPoint U>
GP_NODISCARD constexpr Vector3<T>::Vector3(const Vector2<T>& vec, const U inZ) noexcept
    : x(vec.x)
    , y(vec.y)
    , z(static_cast<T>(inZ))
{}

template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr Vector3<T>::Vector3(const Vector4<T>& vec) noexcept
    : x(vec.x)
    , y(vec.y)
    , z(vec.z)
{}

}   // namespace gp::math
