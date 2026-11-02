// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "maths/rotation/Quaternion.hpp"

namespace gp::math
{
/// @brief A structure representing a dual quaternion for 3D transformations.
/// @tparam T The floating-point type for the dual quaternion components.
template <concepts::IsFloatingPoint T>
struct DualQuaternion
{
public:
    Quaternion<T> real;   // The real part of the dual quaternion
    Quaternion<T> dual;   // The dual part of the dual quaternion

public:
    /// @brief Default constructor initializes to the identity dual quaternion.
    /// @return A DualQuaternion with the real part set to the identity quaternion and the dual part set to zero.
    [[nodiscard]] static inline constexpr DualQuaternion<T> identity() noexcept
    {
        return { Quaternion<T>::identity(), Quaternion<T>(T{ 0 }, T{ 0 }, T{ 0 }, T{ 0 }) };
    }

public:
    /// @brief Constructor with individual components for the real and dual parts.
    /// @param[in] realX The x component of the real part.
    /// @param[in] realY The y component of the real part.
    /// @param[in] realZ The z component of the real part.
    /// @param[in] realW The w component of the real part.
    /// @param[in] dualX The x component of the dual part.
    /// @param[in] dualY The y component of the dual part.
    /// @param[in] dualZ The z component of the dual part.
    /// @param[in] dualW The w component of the dual part.
    /// @return A DualQuaternion with the specified components for the real and dual parts.
    [[nodiscard]] constexpr DualQuaternion(const T realX, const T realY, const T realZ, const T realW,
                                           const T dualX, const T dualY, const T dualZ, const T dualW) noexcept
        : real(realX, realY, realZ, realW)
        , dual(dualX, dualY, dualZ, dualW)
    {}
};

}   // namespace gp::math
