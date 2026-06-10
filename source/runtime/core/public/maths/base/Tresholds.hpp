// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::math
{

/// @brief A struct containing threshold values for various mathematical operations.
/// @tparam T The floating-point type for the threshold values.
template <concepts::IsFloatingPoint T>
struct tresholds final
{
    static constexpr T pointOnPlane = static_cast<T>(0.10);
    static constexpr T pointOnSide = static_cast<T>(0.20);
    static constexpr T pointsAreSame = static_cast<T>(0.00002);
    static constexpr T pointsAreNear = static_cast<T>(0.015);
    static constexpr T normalsAreSame = static_cast<T>(0.00002);
    static constexpr T uvsAreSame = static_cast<T>(0.0009765625);
    static constexpr T vectorsAreNear = static_cast<T>(0.0004);
    static constexpr T splitPolyWithPlane = static_cast<T>(0.25);
    static constexpr T splitPolyPrecisely = static_cast<T>(0.01);
    static constexpr T zeroNormSquared = static_cast<T>(0.0001);
    static constexpr T normalsAreParallel = static_cast<T>(0.999845);
    static constexpr T normalsAreOrthogonal = static_cast<T>(0.017455);
    static constexpr T vectorNormalized = static_cast<T>(0.01);
    static constexpr T quatNormalized = static_cast<T>(0.01);
};

}   // namespace gp::math
