// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::math
{

/// @brief A 3D vector template
/// @tparam T The floating-point type for the vector components
template <concepts::IsFloatingPoint T>
struct Vector3
{
public:
    T x;
    T y;
    T z;
};

}   // namespace gp::math

// Include the implementation of the Vector3 template
#include "maths/vector/Vector3.inl"
