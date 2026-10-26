// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"


namespace gp::math
{

/// @brief A structure representing a rotation in 3D space.
/// @tparam T The floating-point type for the rotation components.
template <concepts::IsFloatingPoint T>
struct Rotator
{
public:
    T pitch;  // Rotation around the x-axis
    T yaw;    // Rotation around the y-axis
    T roll;   // Rotation around the z-axis

public:
    /// @brief Default constructor initializes to (0, 0, 0).
    [[nodiscard]] constexpr Rotator() noexcept
        : pitch(T{ 0 })
        , yaw(T{ 0 })
        , roll(T{ 0 })
    {}

    /// @brief Constructor with individual components.
    /// @param[in] inPitch The pitch component of the rotation.
    /// @param[in] inYaw The yaw component of the rotation.
    /// @param[in] inRoll The roll component of the rotation.
    [[nodiscard]] constexpr Rotator(const T inPitch, const T inYaw, const T inRoll) noexcept
        : pitch(inPitch)
        , yaw(inYaw)
        , roll(inRoll)
    {}
};

}   // namespace gp::math
