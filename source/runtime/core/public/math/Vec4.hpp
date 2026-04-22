// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "math/Forward.hpp"
#include <concepts>

namespace gp::math
{

/// @brief A 4D vector class template that represents a vector in 4D space with components of type T.
/// @tparam T The type of the vector components, which must be a floating-point type (e.g., float, double).
template <std::floating_point T>
struct Vec4
{
public:
    union
    {
        struct
        {
            T x;
            T y;
            T z;
            T w;
        };

        T data[4];
    };

public:
    /// @brief Default constructor. Initializes the vector to (0, 0, 0, 0).
    constexpr Vec4() noexcept
        : x(0)
        , y(0)
        , z(0)
        , w(0)
    {}

    /// @brief Constructs a vector with the given x, y, z, and w components.
    /// @param x The x component of the vector.
    /// @param y The y component of the vector.
    /// @param z The z component of the vector.
    /// @param w The w component of the vector.
    constexpr Vec4(T x, T y, T z, T w) noexcept
        : x(x)
        , y(y)
        , z(z)
        , w(w)
    {}

    /// @brief Constructs a vector with the same value for both x and y components.
    /// @param value The value to set for both x and y components.
    explicit constexpr Vec4(T value) noexcept
        : x(value)
        , y(value)
        , z(value)
        , w(value)
    {}
};

}   // namespace gp::math
