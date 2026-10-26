// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"

namespace gp::math
{

/// @brief A structure representing a rotation in 2D space.
/// @tparam T The floating-point type for the rotation component.
template <concepts::IsFloatingPoint T>
struct Rotator2D
{
public:
    T angle;  // Rotation angle in radians

public:
    /// @brief Default constructor initializes to 0.
    [[nodiscard]] constexpr Rotator2D() noexcept
        : angle(T{ 0 })
    {}

    /// @brief Constructor with a specific angle.
    /// @param[in] inAngle The angle of the rotation.
    [[nodiscard]] constexpr Rotator2D(const T inAngle) noexcept
        : angle(inAngle)
    {}
};

}   // namespace gp::math
