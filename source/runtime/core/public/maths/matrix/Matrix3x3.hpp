// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "maths/MathForward.hpp"

namespace gp::math
{

/// @brief
/// @tparam T
template <concepts::IsFloatingPoint T>
struct Matrix3x3
{
public:
    T m00, m01, m02;
    T m10, m11, m12;
    T m20, m21, m22;
};

}   // namespace gp::math
