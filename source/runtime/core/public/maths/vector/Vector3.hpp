// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "maths/MathForward.hpp"

namespace gp::math
{

/// @brief A 3D vector template
/// @tparam T The floating-point type for the vector components
template <concepts::IsFloatingPoint T>
struct Vector3
{
public:
    T x;
    T y;
    T z;

public:
    /// @brief Default constructor initializes to (0, 0, 0).
    GP_NODISCARD constexpr Vector3() noexcept
        : x(T{ 0 })
        , y(T{ 0 })
        , z(T{ 0 })
    {}

    /// @brief Constructor with individual components.
    /// @param[in] inX The x component of the vector.
    /// @param[in] inY The y component of the vector.
    /// @param[in] inZ The z component of the vector.
    GP_NODISCARD constexpr Vector3(const T inX, const T inY, const T inZ) noexcept
        : x(inX)
        , y(inY)
        , z(inZ)
    {}

    /// @brief Constructor that initializes all components to the same scalar value.
    /// @param[in] scalar The scalar value to initialize all components of the vector.
    GP_NODISCARD explicit constexpr Vector3(const T scalar) noexcept
        : x(scalar)
        , y(scalar)
        , z(scalar)
    {}

    /// @brief Constructor that initializes the vector from an array of 3 components.
    /// @param[in] ptr A pointer to an array of 3 components (x, y, z).
    /// @note The input pointer must not be null and should point to an array of at least 3 elements.
    GP_NODISCARD explicit constexpr Vector3(const T* const ptr) noexcept
        : x(ptr[0])
        , y(ptr[1])
        , z(ptr[2])
    {
        // GP_ASSERT(ptr != nullptr, "Input pointer cannot be null");
    }

    /// @brief Constructor that converts from another Vector3 type with a different floating-point type.
    /// @tparam U The floating-point type of the input vector.
    /// @param[in] vec The input vector to convert from.
    template <concepts::IsFloatingPoint U>
    GP_NODISCARD explicit constexpr Vector3(const Vector3<U>& vec) noexcept
        : x(static_cast<T>(vec.x))
        , y(static_cast<T>(vec.y))
        , z(static_cast<T>(vec.z))
    {}

    /// @brief Constructor that initializes the vector from a Vector2 and two additional components.
    /// @tparam U The floating-point type of the input vector and additional components.
    /// @param[in] vec The input Vector2 to initialize the x and y components of the Vector3.
    /// @param[in] inZ The z component of the Vector3, defaulting to 0 if not provided.
    template <concepts::IsFloatingPoint U = T>
    GP_NODISCARD explicit constexpr Vector3(const Vector2<T>& vec, const U inZ = U{ 0 }) noexcept;

    /// @brief Constructor that initializes the vector from a Vector4 by discarding the w component.
    /// @param[in] vec The input Vector4 to initialize the x, y, and z components of the Vector3.
    GP_NODISCARD explicit constexpr Vector3(const Vector4<T>& vec) noexcept;
};

}   // namespace gp::math

// Include the implementation of the Vector3 template
#include "maths/vector/Vector3.inl"
