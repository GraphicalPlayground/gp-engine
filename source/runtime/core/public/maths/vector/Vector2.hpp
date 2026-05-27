// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::math
{

/// @brief A 2D vector template
/// @tparam T The floating-point type for the vector components
template <concepts::IsFloatingPoint T>
struct Vector2
{
public:
    T x;
    T y;
};

}   // namespace gp::math

// Include the implementation of the Vector2 template
#include "maths/vector/Vector2.inl"
