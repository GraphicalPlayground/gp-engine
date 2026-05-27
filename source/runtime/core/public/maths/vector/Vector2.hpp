// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "maths/MathForward.hpp"

namespace gp::math
{

/// @brief A 2D vector template
/// @tparam T The floating-point type for the vector components
template <concepts::IsFloatingPoint T>
struct Vector2
{
public:
    T x;
    T y;

public:
    /// @brief Default constructor initializes to (0, 0).
    GP_NODISCARD constexpr Vector2() noexcept
        : x(T{ 0 })
        , y(T{ 0 })
    {}

    /// @brief Constructor with individual components.
    /// @param[in] inX The x component of the vector.
    /// @param[in] inY The y component of the vector.
    GP_NODISCARD constexpr Vector2(const T inX, const T inY) noexcept
        : x(inX)
        , y(inY)
    {}

    /// @brief Constructor that initializes all components to the same scalar value.
    /// @param[in] scalar The scalar value to initialize all components of the vector.
    GP_NODISCARD explicit constexpr Vector2(const T scalar) noexcept
        : x(scalar)
        , y(scalar)
    {}

    /// @brief Constructor that initializes the vector from an array of 2 components.
    /// @param[in] ptr A pointer to an array of 2 components (x, y).
    /// @note The input pointer must not be null and should point to an array of at least 2 elements.
    GP_NODISCARD explicit constexpr Vector2(const T* const ptr) noexcept
        : x(ptr[0])
        , y(ptr[1])
    {
        // GP_ASSERT(ptr != nullptr, "Input pointer cannot be null");
    }

    /// @brief Constructor that converts from another Vector3 type with a different floating-point type.
    /// @tparam U The floating-point type of the input vector.
    /// @param[in] vec The input vector to convert from.
    template <concepts::IsFloatingPoint U>
    GP_NODISCARD explicit constexpr Vector2(const Vector2<U>& vec) noexcept
        : x(static_cast<T>(vec.x))
        , y(static_cast<T>(vec.y))
    {}

    /// @brief Constructor that initializes the vector from a Vector3 by discarding the z component.
    /// @param[in] vec The input Vector3 to initialize the x and y components of the Vector2.
    GP_NODISCARD explicit constexpr Vector2(const Vector3<T>& vec) noexcept;

    /// @brief Constructor that initializes the vector from a Vector4 by discarding the z and w components.
    /// @param[in] vec The input Vector4 to initialize the x and y components of the Vector2.
    GP_NODISCARD explicit constexpr Vector2(const Vector4<T>& vec) noexcept;
};

}   // namespace gp::math

// Include the implementation of the Vector2 template
#include "maths/vector/Vector2.inl"
