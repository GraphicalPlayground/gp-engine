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

    public:
    /// @brief Default constructor initializes to (0, 0, 0, 0).
    [[nodiscard]] constexpr Vector4() noexcept
        : x(T{ 0 })
        , y(T{ 0 })
        , z(T{ 0 })
        , w(T{ 0 })
    {}

    /// @brief Constructor with individual components.
    /// @param[in] inX The x component of the vector.
    /// @param[in] inY The y component of the vector.
    /// @param[in] inZ The z component of the vector.
    /// @param[in] inW The w component of the vector.
    [[nodiscard]] constexpr Vector4(const T inX, const T inY, const T inZ, const T inW) noexcept
        : x(inX)
        , y(inY)
        , z(inZ)
        , w(inW)
    {}

    /// @brief Constructor that initializes all components to the same scalar value.
    /// @param[in] scalar The scalar value to initialize all components of the vector.
    [[nodiscard]] explicit constexpr Vector4(const T scalar) noexcept
        : x(scalar)
        , y(scalar)
        , z(scalar)
        , w(scalar)
    {}

    /// @brief Constructor that initializes the vector from an array of 3 components.
    /// @param[in] ptr A pointer to an array of 3 components (x, y, z).
    /// @note The input pointer must not be null and should point to an array of at least 3 elements.
    [[nodiscard]] explicit constexpr Vector4(const T* const ptr) noexcept
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
    [[nodiscard]] explicit constexpr Vector4(const Vector4<U>& vec) noexcept
        : x(static_cast<T>(vec.x))
        , y(static_cast<T>(vec.y))
        , z(static_cast<T>(vec.z))
        , w(static_cast<T>(vec.w))
    {}

    /// @brief Constructor that initializes the vector from a Vector2 and two additional components.
    /// @tparam U The floating-point type of the input vector and additional components.
    /// @param[in] vec The input Vector2 to initialize the x and y components of the Vector4.
    /// @param[in] inZ The z component of the Vector4, defaulting to 0 if not provided.
    /// @param[in] inW The w component of the Vector4, defaulting to 0 if not provided.
    template <concepts::IsFloatingPoint U = T>
    [[nodiscard]] explicit constexpr Vector4(const Vector2<T>& vec, const U inZ = U{ 0 }, const U inW = U{ 0 }) noexcept
        : x(vec.x)
        , y(vec.y)
        , z(inZ)
        , w(inW)
    {}

    /// @brief Constructor that initializes the vector from a Vector3 and one additional component.
    /// @tparam U The floating-point type of the input vector and additional component.
    /// @param[in] vec The input Vector3 to initialize the x, y, and z components of the Vector4.
    /// @param[in] inW The w component of the Vector4, defaulting to 0 if not provided.
    template <concepts::IsFloatingPoint U = T>
    [[nodiscard]] explicit constexpr Vector4(const Vector3<T>& vec, const U inW = U{ 0 }) noexcept
        : x(vec.x)
        , y(vec.y)
        , z(vec.z)
        , w(inW)
    {}
};

}   // namespace gp::math

// Include the implementation of the Vector4 template
#include "maths/vector/Vector4.inl"
