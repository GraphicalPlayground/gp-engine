// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"

namespace gp::math
{
/// @brief A structure representing a quaternion for 3D rotations.
/// @tparam T The floating-point type for the quaternion components.
template <concepts::IsFloatingPoint T>
struct Quaternion
{
public:
    T x;  // The x component of the quaternion
    T y;  // The y component of the quaternion
    T z;  // The z component of the quaternion
    T w;  // The w component of the quaternion

public:
    /// @brief Default constructor initializes to (0, 0, 0, 1).
    /// @return A Quaternion with all components set to 0 except w, which is set to 1.
    [[nodiscard]] static inline constexpr Quaternion<T> identity() noexcept
    {
        return { T{ 0 }, T{ 0 }, T{ 0 }, T{ 1 } };
    }

public:
    /// @brief Constructor with individual components.
    /// @param[in] inX The x component of the quaternion.
    /// @param[in] inY The y component of the quaternion.
    /// @param[in] inZ The z component of the quaternion.
    /// @param[in] inW The w component of the quaternion.
    /// @return A Quaternion with the specified components.
    [[nodiscard]] constexpr Quaternion(const T inX, const T inY, const T inZ, const T inW) noexcept
        : x(inX)
        , y(inY)
        , z(inZ)
        , w(inW)
    {}
};

}   // namespace gp::math
