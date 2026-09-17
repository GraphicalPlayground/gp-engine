// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "maths/MathForward.hpp"

namespace gp::math
{

/// @brief A 4D vector template.
/// @tparam T The floating-point type for the vector components.
template <concepts::IsFloatingPoint T>
struct alignas(sizeof(T) * 4) Vector4
{
public:
    T x;    //<! The x component of the vector
    T y;    //<! The y component of the vector
    T z;    //<! The z component of the vector
    T w;    //<! The w component of the vector

public:
    /// @brief Returns a zero vector.
    /// @return A vector with all the coponents set to zero.
    [[nodiscard]] static inline constexpr Vector4<T> zero()
    {
        return { T{ 0 }, T{ 0 }, T{ 0 }, T{ 0 } };
    }

    /// @brief Returns a vector with all components set to one.
    /// @return A vector with all components set to one.
    [[nodiscard]] static inline constexpr Vector4<T> one()
    {
        return { T{ 1 }, T{ 1 }, T{ 1 }, T{ 1 } };
    }

    /// @brief Returns a vector pointing upwards.
    /// @return A vector pointing upwards.
    [[nodiscard]] static inline constexpr Vector4<T> up()
    {
        return { T{ 0 }, T{ 1 }, T{ 0 }, T{ 0 } };
    }

    /// @brief Returns a vector pointing downwards.
    /// @return A vector pointing dowwards.
    [[nodiscard]] static inline constexpr Vector4<T> down()
    {
        return { T{ 0 }, T{ -1 }, T{ 0 }, T{ 0 } };
    }

    /// @brief Returns a vector pointing to the left.
    /// @return A vector pointing to the left.
    [[nodiscard]] static inline constexpr Vector4<T> left()
    {
        return { T{ -1 }, T{ 0 }, T{ 0 }, T{ 0 } };
    }

    /// @brief Returns a vector pointing to the right.
    /// @return A vector pointing to the right.
    [[nodiscard]] static inline constexpr Vector4<T> right()
    {
        return { T{ 1 }, T{ 0 }, T{ 0 }, T{ 0 } };
    }

    /// @brief Returns a vector pointing forward.
    /// @return A vector pointing forward.
    [[nodiscard]] static inline constexpr Vector4<T> forward()
    {
        return { T{ 0 }, T{ 0 }, T{ 1 }, T{ 0 } };
    }

    /// @brief Returns a vector pointing backward.
    /// @return A vector pointing backward.
    [[nodiscard]] static inline constexpr Vector4<T> backward()
    {
        return { T{ 0 }, T{ 0 }, T{ -1 }, T{ 0 } };
    }

    /// @brief Returns a vector pointing to the negative W-axis.
    /// @return A vector pointing to the negative W-axis.
    [[nodiscard]] static inline constexpr Vector4<T> negativeW()
    {
        return { T{  }, T{ 0 }, T{ 0 }, T{ -1 } };
    }

    /// @brief Returns a vector pointing to the positive W-axis.
    /// @return A vector pointing to the positive W-axis.
    [[nodiscard]] static inline constexpr Vector4<T> positiveW()
    {
        return { T{  }, T{ 0 }, T{ 0 }, T{ 1 } };
    }

    /// @brief Returns a unit vector along the x-axis.
    /// @return A unit vector along the x-axis.
    [[nodiscard]] static inline constexpr Vector4<T> unitX()
    {
        return { T{ 1 }, T{ 0 }, T{ 0 }, T{ 0 } };
    }

    /// @brief Returns a unit vector along the y-axis.
    /// @return A unit vector along the y-axis.
    [[nodiscard]] static inline constexpr Vector4<T> unitY()
    {
        return { T{ 0 }, T{ 1 }, T{ 0 }, T{ 0 } };
    }

    /// @brief Returns a unit vector along the z-axis.
    /// @return A unit vector along the z-axis.
    [[nodiscard]] static inline constexpr Vector4<T> unitZ()
    {
        return { T{ 0 }, T{ 0 }, T{ 1 }, T{ 0 } };
    }

    /// @brief Returns a unit vector along the w-axis.
    /// @return A unit vector along the w-axis.
    [[nodiscard]] static inline constexpr Vector4<T> unitW()
    {
        return { T{ 0 }, T{ 0 }, T{ 0 }, T{ 1 } };
    }
};

}   // namespace gp::math

// Include the implementation of the Vector4 template
#include "maths/vector/Vector4.inl"
