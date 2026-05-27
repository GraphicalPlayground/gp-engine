// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "maths/vector/Vector2.hpp"

// Include the other vector for conversions and operations
#include "maths/vector/Vector3.hpp"
#include "maths/vector/Vector4.hpp"

namespace gp::math
{

template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr Vector2<T>::Vector2(const Vector3<T>& vec) noexcept
    : x(vec.x)
    , y(vec.y)
{}

template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr Vector2<T>::Vector2(const Vector4<T>& vec) noexcept
    : x(vec.x)
    , y(vec.y)
{}

}   // namespace gp::math
