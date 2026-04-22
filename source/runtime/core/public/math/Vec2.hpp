// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "math/Forward.hpp"
#include <concepts>

namespace gp::math
{

/// @brief A 2D vector class template that represents a vector in 2D space with components of type T.
/// @tparam T The type of the vector components, which must be a floating-point type (e.g., float, double).
template <std::floating_point T>
struct Vec2
{
public:
    union
    {
        struct
        {
            T x;
            T y;
        };

        T data[2];
    };

public:
    /// @brief Default constructor. Initializes the vector to (0, 0).
    constexpr Vec2() noexcept
        : x(0)
        , y(0)
    {}

    /// @brief Constructs a vector with the given x and y components.
    /// @param x The x component of the vector.
    /// @param y The y component of the vector.
    constexpr Vec2(T x, T y) noexcept
        : x(x)
        , y(y)
    {}

    /// @brief Constructs a vector with the same value for both x and y components.
    /// @param value The value to set for both x and y components.
    explicit constexpr Vec2(T value) noexcept
        : x(value)
        , y(value)
    {}
};

}   // namespace gp::math
