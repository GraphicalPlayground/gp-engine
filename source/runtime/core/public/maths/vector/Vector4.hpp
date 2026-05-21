// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "maths/MathForward.hpp"

namespace gp::math
{

/// @brief A 4D vector template.
/// @tparam T The floating-point type for the vector components.
template <concepts::IsFloatingPoint T>
struct GP_ALIGN(sizeof(T) * 4) Vector4
{
public:
    T x;
    T y;
    T z;
    T w;

public:
    /// @brief Default constructor initializes to (0, 0, 0, 1).
    GP_NODISCARD constexpr Vector4() noexcept
        : x(T{ 0 })
        , y(T{ 0 })
        , z(T{ 0 })
        , w(T{ 1 })
    {}

    /// @brief Constructor with individual components.
    /// @param[in] inX The x component of the vector.
    /// @param[in] inY The y component of the vector.
    /// @param[in] inZ The z component of the vector.
    /// @param[in] inW The w component of the vector.
    GP_NODISCARD constexpr Vector4(const T inX, const T inY, const T inZ, const T inW) noexcept
        : x(inX)
        , y(inY)
        , z(inZ)
        , w(inW)
    {}

    /// @brief Constructor that initializes all components to the same scalar value.
    /// @param[in] scalar The scalar value to initialize all components of the vector.
    GP_NODISCARD explicit constexpr Vector4(const T scalar) noexcept
        : x(scalar)
        , y(scalar)
        , z(scalar)
        , w(scalar)
    {}

    /// @brief Constructor that initializes the vector from an array of 4 components.
    /// @param[in] ptr A pointer to an array of 4 components (x, y, z, w).
    /// @note The input pointer must not be null and should point to an array of at least 4 elements.
    GP_NODISCARD explicit constexpr Vector4(const T* const ptr) noexcept
        : x(ptr[0])
        , y(ptr[1])
        , z(ptr[2])
        , w(ptr[3])
    {
        // GP_ASSERT(ptr != nullptr, "Input pointer cannot be null");
    }

    /// @brief Constructor that converts from another Vector4 type with a different floating-point type.
    /// @tparam U The floating-point type of the input vector.
    /// @param[in] vec The input vector to convert from.
    template <concepts::IsFloatingPoint U>
    GP_NODISCARD explicit constexpr Vector4(const Vector4<U>& vec) noexcept
        : x(static_cast<T>(vec.x))
        , y(static_cast<T>(vec.y))
        , z(static_cast<T>(vec.z))
        , w(static_cast<T>(vec.w))
    {}

    /// @brief Constructor that initializes the vector from a Vector3 and an optional w component.
    /// @tparam U The floating-point type of the input vector and w component.
    /// @param[in] vec The input Vector3 to initialize the x, y, and z components of the Vector4.
    /// @param[in] inW The w component of the Vector4, defaulting to 1 if not provided.
    template <concepts::IsFloatingPoint U>
    GP_NODISCARD explicit constexpr Vector4(const Vector3<T>& vec, const U inW = U{ 1 }) noexcept;

    /// @brief Constructor that initializes the vector from a Vector2 and two additional components.
    /// @tparam U The floating-point type of the input vector and additional components.
    /// @param[in] vec The input Vector2 to initialize the x and y components of the Vector4.
    /// @param[in] inZ The z component of the Vector4, defaulting to 0 if not provided.
    /// @param[in] inW The w component of the Vector4, defaulting to 1 if not provided.
    /// @param[in] inW The w component of the Vector4, defaulting to 1 if not provided.
    template <concepts::IsFloatingPoint U>
    GP_NODISCARD explicit constexpr Vector4(const Vector2<T>& vec, const U inZ = U{ 0 }, const U inW = U{ 1 }) noexcept;

    /// @brief Constructor that initializes the vector from two Vector2 components.
    /// @param[in] vecXY The input Vector2 to initialize the x and y components of the Vector4.
    /// @param[in] vecZW The input Vector2 to initialize the z and w components of the Vector4.
    GP_NODISCARD constexpr Vector4(const Vector2<T>& vecXY, const Vector2<T>& vecZW) noexcept;
};

}   // namespace gp::math

// Include the implementation of the Vector4 template
#include "maths/vector/Vector4.inl"
