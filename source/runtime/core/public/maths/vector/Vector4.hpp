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
    T x;   //<! The x component of the vector
    T y;   //<! The y component of the vector
    T z;   //<! The z component of the vector
    T w;   //<! The w component of the vector

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
        return { T{}, T{ 0 }, T{ 0 }, T{ -1 } };
    }

    /// @brief Returns a vector pointing to the positive W-axis.
    /// @return A vector pointing to the positive W-axis.
    [[nodiscard]] static inline constexpr Vector4<T> positiveW()
    {
        return { T{}, T{ 0 }, T{ 0 }, T{ 1 } };
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

public:
    /// @brief Component-wise cross product of this vector with another vector.
    /// @param[in] other The other vector to compute the cross product with.
    /// @return The cross product of this vector and the other vector.
    [[nodiscard]] constexpr Vector4<T> operator^(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x, w * other.w);
    }

    /// @brief Component-wise dot product of this vector with another vector.
    /// @param[in] other The other vector to compute the dot product with.
    /// @return The dot product of this vector and the other vector.
    [[nodiscard]] constexpr T operator|(const Vector4<T>& other) const noexcept
    {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    /// @brief Negation operator, returns a vector with all components negated.
    /// @return A vector with all components negated.
    [[nodiscard]] constexpr Vector4<T> operator-() const noexcept
    {
        return Vector4<T>(-x, -y, -z, -w);
    }

    /// @brief Unary plus operator, returns the vector itself.
    /// @return The vector itself.
    [[nodiscard]] constexpr Vector4<T> operator+() const noexcept
    {
        return *this;
    }

    /// @brief Component-wise addition of this vector with another vector.
    /// @param[in] other The other vector to add to this vector.
    /// @return The component-wise sum of this vector and the other vector.
    [[nodiscard]] constexpr Vector4<T> operator+(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    /// @brief Component-wise addition of a scalar bias to this vector.
    /// @param[in] bias The scalar bias to add to each component of the vector.
    /// @return The component-wise sum of this vector and the scalar bias.
    [[nodiscard]] constexpr Vector4<T> operator+(const T bias) const noexcept
    {
        return Vector4<T>(x + bias, y + bias, z + bias, w + bias);
    }

    /// @brief Component-wise subtraction of another vector from this vector.
    /// @param[in] other The other vector to subtract from this vector.
    /// @return The component-wise difference of this vector and the other vector.
    [[nodiscard]] constexpr Vector4<T> operator-(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(x - other.x, y - other.y, z - other.z, w - other.w);
    }

    /// @brief Component-wise subtraction of a scalar bias from this vector.
    /// @param[in] bias The scalar bias to subtract from each component of the vector.
    /// @return The component-wise difference of this vector and the scalar bias.
    [[nodiscard]] constexpr Vector4<T> operator-(const T bias) const noexcept
    {
        return Vector4<T>(x - bias, y - bias, z - bias, w - bias);
    }

    /// @brief Component-wise multiplication of this vector with another vector.
    /// @param[in] other The other vector to multiply with this vector.
    /// @return The component-wise product of this vector and the other vector.
    [[nodiscard]] constexpr Vector4<T> operator*(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(x * other.x, y * other.y, z * other.z, w * other.w);
    }

    /// @brief Component-wise division of this vector by another vector.
    /// @param[in] other The other vector to divide this vector by.
    /// @return The component-wise quotient of this vector and the other vector.
    [[nodiscard]] constexpr Vector4<T> operator/(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(x / other.x, y / other.y, z / other.z, w / other.w);
    }

    /// @brief Component-wise division of this vector by a scalar scale factor.
    /// @param[in] scale The scalar scale factor to divide each component of the vector by.
    /// @return The component-wise quotient of this vector and the scalar scale factor.
    /// @details
    /// This operator computes the inverse of the scalar scale factor and multiplies it with each component
    /// of the vector for improved performance, especially when the scalar is a constant or can be optimized by the
    /// compiler.
    [[nodiscard]] constexpr Vector4<T> operator/(const T scale) const noexcept
    {
        GP_ASSERT(scale != T{ 0 }, "Division by zero");
        T invScale = static_cast<T>(1) / static_cast<T>(scale);
        return Vector4<T>(x * invScale, y * invScale, z * invScale, w * invScale);
    }

    /// @brief Component-wise equality comparison of this vector with another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return True if all components of this vector are equal to the corresponding components of the other vector,
    /// false otherwise.
    [[nodiscard]] constexpr bool operator==(const Vector4<T>& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }

    /// @brief Component-wise inequality comparison of this vector with another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return True if any component of this vector is not equal to the corresponding component of the other vector,
    /// false otherwise.
    [[nodiscard]] constexpr bool operator!=(const Vector4<T>& other) const noexcept
    {
        return !(*this == other);
    }

    /// @brief Subscript operator for non-const access to vector components by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y, 2 for z, 3 for w).
    /// @return A reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0, 1, 2, or 3).
    [[nodiscard]] constexpr T& operator[](const Int32 index) noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "Index out of range");
        return *(&x + index);
    }

    /// @brief Subscript operator for const access to vector components by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y, 2 for z, 3 for w).
    /// @return A const reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0, 1, 2, or 3).
    [[nodiscard]] constexpr const T& operator[](const Int32 index) const noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "Index out of range");
        return *(&x + index);
    }

    /// @brief In-place component-wise addition of another vector to this vector.
    /// @param[in] other The other vector to add to this vector.
    /// @return A reference to this vector after the addition.
    constexpr Vector4<T>& operator+=(const Vector4<T>& other) noexcept
    {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    /// @brief In-place component-wise addition of a scalar bias to this vector.
    /// @tparam U The arithmetic type of the scalar bias.
    /// @param[in] bias The scalar bias to add to each component of the vector.
    /// @return A reference to this vector after the addition.
    template <concepts::IsArithmetic U>
    constexpr Vector4<T>& operator+=(const U bias) noexcept
    {
        x += static_cast<T>(bias);
        y += static_cast<T>(bias);
        z += static_cast<T>(bias);
        w += static_cast<T>(bias);
        return *this;
    }

    /// @brief In-place component-wise subtraction of another vector from this vector.
    /// @param[in] other The other vector to subtract from this vector.
    /// @return A reference to this vector after the subtraction.
    constexpr Vector4<T>& operator-=(const Vector4<T>& other) noexcept
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    /// @brief In-place component-wise subtraction of a scalar bias from this vector.
    /// @tparam U The arithmetic type of the scalar bias.
    /// @param[in] bias The scalar bias to subtract from each component of the vector.
    /// @return A reference to this vector after the subtraction.
    template <concepts::IsArithmetic U>
    constexpr Vector4<T>& operator-=(const U bias) noexcept
    {
        x -= static_cast<T>(bias);
        y -= static_cast<T>(bias);
        z -= static_cast<T>(bias);
        w -= static_cast<T>(bias);
        return *this;
    }

    /// @brief In-place component-wise multiplication of this vector with another vector.
    /// @param[in] other The other vector to multiply with this vector.
    /// @return A reference to this vector after the multiplication.
    constexpr Vector4<T>& operator*=(const Vector4<T>& other) noexcept
    {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        w *= other.w;
        return *this;
    }

    /// @brief In-place component-wise multiplication of this vector by a scalar scale factor.
    /// @tparam U The arithmetic type of the scalar scale factor.
    /// @param[in] scale The scalar scale factor to multiply each component of the vector by.
    /// @return A reference to this vector after the multiplication.
    template <concepts::IsArithmetic U>
    constexpr Vector4<T>& operator*=(const U scale) noexcept
    {
        x *= static_cast<T>(scale);
        y *= static_cast<T>(scale);
        z *= static_cast<T>(scale);
        w *= static_cast<T>(scale);
        return *this;
    }

    /// @brief In-place component-wise division of this vector by another vector.
    /// @param[in] other The other vector to divide this vector by.
    /// @return A reference to this vector after the division.
    constexpr Vector4<T>& operator/=(const Vector4<T>& other) noexcept
    {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        w /= other.w;
        return *this;
    }

    /// @brief In-place component-wise division of this vector by a scalar scale factor.
    /// @tparam U The arithmetic type of the scalar scale factor.
    /// @param[in] scale The scalar scale factor to divide each component of the vector by.
    /// @return A reference to this vector after the division.
    template <concepts::IsArithmetic U>
    constexpr Vector4<T>& operator/=(const U scale) noexcept
    {
        GP_ASSERT(scale != T{ 0 }, "Division by zero");
        T invScale = static_cast<T>(1) / static_cast<T>(scale);
        x *= invScale;
        y *= invScale;
        z *= invScale;
        w *= invScale;
        return *this;
    }
};

}   // namespace gp::math

#include "maths/vector/Vector4.inl"
