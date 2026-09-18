// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "concepts/Fundamental.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/Scalar.hpp"
#include "maths/base/Tresholds.hpp"
#include "maths/MathForward.hpp"

namespace gp::math
{

/// @brief A 4D vector template.
/// @tparam T The floating-point type for the vector components.
template <concepts::IsFloatingPoint T>
struct Vector4
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
    [[nodiscard]] explicit constexpr Vector4(
        const Vector2<T>& vec, const U inZ = U{ 0 }, const U inW = U{ 0 }
    ) noexcept;

    /// @brief Constructor that initializes the vector from a Vector3 and one additional component.
    /// @tparam U The floating-point type of the input vector and additional component.
    /// @param[in] vec The input Vector3 to initialize the x, y, and z components of the Vector4.
    /// @param[in] inW The w component of the Vector4, defaulting to 0 if not provided.
    template <concepts::IsFloatingPoint U = T>
    [[nodiscard]] explicit constexpr Vector4(const Vector3<T>& vec, const U inW = U{ 0 }) noexcept;

    /// @brief Constructor that initializes the vector from two Vector2.
    /// @tparam U The floating-point type of the input vectors.
    /// @param[in] vecXY The Vector2 to initialize the x and y components of the Vector4.
    /// @param[in] vecZW The Vector2 to initialize the z and w components of the Vector4.
    [[nodiscard]] explicit constexpr Vector4(const Vector2<T>& vecXY, const Vector2<T>& vecZW) noexcept;

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
        return this->component(index);
    }

    /// @brief Subscript operator for const access to vector components by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y, 2 for z, 3 for w).
    /// @return A const reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0, 1, 2, or 3).
    [[nodiscard]] constexpr const T& operator[](const Int32 index) const noexcept
    {
        return this->component(index);
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

public:
    /// @brief Gets a reference to a component of the vector by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y, 2 for z, 3 for w).
    /// @return A reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0, 1, 2, or 3).
    [[nodiscard]] constexpr T& component(const Int32 index) noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "Index out of range");
        return *(&x + index);
    }

    /// @brief Gets a const reference to a component of the vector by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y, 2 for z, 3 for w).
    /// @return A const reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0, 1, 2, or 3).
    [[nodiscard]] constexpr const T& component(const Int32 index) const noexcept
    {
        GP_ASSERT(index >= 0 && index < 4, "Index out of range");
        return *(&x + index);
    }

    /// @brief Component-wise cross product of this vector with another vector.
    /// @param[in] other The other vector to compute the cross product with.
    /// @return The cross product of this vector and the other vector.
    [[nodiscard]] constexpr Vector4<T> cross(const Vector4<T>& other) const noexcept
    {
        return *this ^ other;
    }

    /// @brief Component-wise dot product of this vector with another vector.
    /// @param[in] other The other vector to compute the dot product with.
    /// @return The dot product of this vector and the other vector.
    [[nodiscard]] constexpr T dot(const Vector4<T>& other) const noexcept
    {
        return *this | other;
    }

    /// @brief Checks if this vector is equal to another vector within a specified tolerance.
    /// @param[in] other The other vector to compare with this vector.
    /// @param[in] tolerance The tolerance value for the comparison.
    /// @return True if the absolute difference between each corresponding component of the two vectors is less than or
    /// equal to the specified tolerance, false otherwise.
    [[nodiscard]] constexpr bool
        equals(const Vector4<T>& other, const T tolerance = Constants<T>::kindaSmallNumber) const noexcept
    {
        return math::abs(x - other.x) <= tolerance && math::abs(y - other.y) <= tolerance &&
               math::abs(z - other.z) <= tolerance && math::abs(w - other.w) <= tolerance;
    }

    /// @brief Checks if all components of this vector are equal within a specified tolerance.
    /// @param[in] tolerance The tolerance value for the comparison.
    /// @return True if the absolute difference between each pair of components of the vector is less than or equal to
    /// the specified tolerance, false otherwise.
    [[nodiscard]] constexpr bool isAllComponentsEqual(const T tolerance = Constants<T>::kindaSmallNumber) const noexcept
    {
        return math::abs(x - y) <= tolerance && math::abs(x - z) <= tolerance && math::abs(x - w) <= tolerance &&
               math::abs(y - z) <= tolerance && math::abs(y - w) <= tolerance && math::abs(z - w) <= tolerance;
    }

    /// @brief Checks if the vector is nearly zero within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if all components of the vector are nearly zero within the tolerance, false otherwise.
    [[nodiscard]] constexpr bool isNearlyZero(const T tolerance = Constants<T>::kindaSmallNumber) const noexcept
    {
        return math::abs(x) <= tolerance && math::abs(y) <= tolerance && math::abs(z) <= tolerance &&
               math::abs(w) <= tolerance;
    }

    /// @brief Checks if the vector is exactly zero.
    /// @return True if all components of the vector are exactly zero, false otherwise.
    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        return x == T{ 0 } && y == T{ 0 } && z == T{ 0 } && w == T{ 0 };
    }

    /// @brief Checks if the vector is a unit vector within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if the vector is a unit vector within the tolerance, false otherwise.
    [[nodiscard]] constexpr bool isUnit(const T tolerance = Constants<T>::kindaSmallNumber) const noexcept
    {
        return math::abs(T{ 1 } - lengthSquared()) <= tolerance;
    }

    /// @brief Checks if the vector is normalized within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if the vector is normalized within the tolerance, false otherwise.
    [[nodiscard]] constexpr bool isNormalized(const T tolerance = Tresholds<T>::vectorNormalized) const noexcept
    {
        return isUnit(tolerance);
    }

    /// @brief Checks if all components of the vector are uniform within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if all components of the vector are uniform within the tolerance, false otherwise.
    [[nodiscard]] constexpr bool isUniform(const T tolerance = Constants<T>::kindaSmallNumber) const noexcept
    {
        return isAllComponentsEqual(tolerance);
    }

    /// @brief Get the maximum component value of the vector.
    /// @return The maximum component value among x, y, z, and w.
    [[nodiscard]] constexpr T getMax() const noexcept
    {
        return math::max(x, math::max(y, math::max(z, w)));
    }

    /// @brief Get the minimum component value of the vector.
    /// @return The minimum component value among x, y, z, and w.
    [[nodiscard]] constexpr T getMin() const noexcept
    {
        return math::min(x, math::min(y, math::min(z, w)));
    }

    /// @brief Get the maximum absolute component value of the vector.
    /// @return The maximum absolute component value among x, y, z, and w.
    [[nodiscard]] constexpr T getAbsMax() const noexcept
    {
        return math::max(math::abs(x), math::max(math::abs(y), math::max(math::abs(z), math::abs(w))));
    }

    /// @brief Get the minimum absolute component value of the vector.
    /// @return The minimum absolute component value among x, y, z, and w.
    [[nodiscard]] constexpr T getAbsMin() const noexcept
    {
        return math::min(math::abs(x), math::min(math::abs(y), math::min(math::abs(z), math::abs(w))));
    }

    /// @brief Get the component-wise minimum of this vector and another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return A vector containing the minimum of each component between this vector and the other vector.
    [[nodiscard]] constexpr Vector4<T> getComponentWiseMin(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(math::min(x, other.x), math::min(y, other.y), math::min(z, other.z), math::min(w, other.w));
    }

    /// @brief Get the component-wise maximum of this vector and another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return A vector containing the maximum of each component between this vector and the other vector.
    [[nodiscard]] constexpr Vector4<T> getComponentWiseMax(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(math::max(x, other.x), math::max(y, other.y), math::max(z, other.z), math::max(w, other.w));
    }

    /// @brief Get the component-wise minimum of the absolute values of this vector and another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return A vector containing the minimum of the absolute values of each component between this vector and the
    /// other vector.
    [[nodiscard]] constexpr Vector4<T> getComponentWiseAbsMin(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(
            math::min(math::abs(x), math::abs(other.x)),
            math::min(math::abs(y), math::abs(other.y)),
            math::min(math::abs(z), math::abs(other.z)),
            math::min(math::abs(w), math::abs(other.w))
        );
    }

    /// @brief Get the component-wise maximum of the absolute values of this vector and another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return A vector containing the maximum of the absolute values of each component between this vector and the
    /// other vector.
    [[nodiscard]] constexpr Vector4<T> getComponentWiseAbsMax(const Vector4<T>& other) const noexcept
    {
        return Vector4<T>(
            math::max(math::abs(x), math::abs(other.x)),
            math::max(math::abs(y), math::abs(other.y)),
            math::max(math::abs(z), math::abs(other.z)),
            math::max(math::abs(w), math::abs(other.w))
        );
    }

    /// @brief Get a vector containing the absolute values of each component of this vector.
    /// @return A vector with the absolute values of each component of this vector.
    [[nodiscard]] constexpr Vector4<T> getAbs() const noexcept
    {
        return Vector4<T>(math::abs(x), math::abs(y), math::abs(z), math::abs(w));
    }

    /// @brief Get the length of the vector.
    /// @return The length of the vector.
    [[nodiscard]] constexpr T length() const noexcept
    {
        return math::sqrt(lengthSquared());
    }

    /// @brief Get the squared length of the vector.
    /// @return The squared length of the vector.
    [[nodiscard]] constexpr T lengthSquared() const noexcept
    {
        return x * x + y * y + z * z + w * w;
    }

    /// @brief Normalize the vector in place if its length is greater than a given tolerance.
    /// @param[in] tolerance The tolerance for the length check to avoid division by zero or very small numbers.
    /// @return True if the vector was successfully normalized, false if the length was too small and the vector was not
    /// modified.
    constexpr bool normalize(const T tolerance = Constants<T>::smallNumber) noexcept
    {
        const T lenSq = lengthSquared();
        if (lenSq > tolerance * tolerance)
        {
            const T invLen = static_cast<T>(1) / math::sqrt(lenSq);
            x *= invLen;
            y *= invLen;
            z *= invLen;
            w *= invLen;
            return true;
        }
        return false;
    }

    /// @brief Get a normalized version of this vector without modifying the original vector unsafely, assuming the
    /// length of the vector is greater than zero.
    /// @return A normalized version of this vector if its length is greater than zero, or an undefined result if the
    /// length is zero (caller must ensure the length is greater than zero).
    [[nodiscard]] constexpr Vector4<T> getNormalizedUnsafe() const noexcept
    {
        const T lenSq = lengthSquared();
        GP_ASSERT(lenSq > T{ 0 }, "Cannot normalize a vector with zero length");
        const T invLen = static_cast<T>(1) / math::sqrt(lenSq);
        return Vector4<T>(x * invLen, y * invLen, z * invLen, w * invLen);
    }

    /// @brief Get a normalized version of this vector, returning a zero vector if the length is less than or equal to a
    /// given tolerance to avoid division by zero or very small numbers.
    /// @param[in] tolerance The tolerance for the length check to determine if the vector is too small to normalize.
    /// @return A normalized version of this vector if its length is greater than the tolerance, or a zero vector if the
    /// length is less than or equal to the tolerance.
    [[nodiscard]] constexpr Vector4<T> getSafeNormal(const T tolerance = Constants<T>::smallNumber) const noexcept
    {
        const T lenSq = lengthSquared();
        if (lenSq > tolerance * tolerance)
        {
            const T invLen = static_cast<T>(1) / math::sqrt(lenSq);
            return Vector4<T>(x * invLen, y * invLen, z * invLen, w * invLen);
        }
        return Vector4<T>::zero();
    }

    /// @brief Get a vector containing the sign of each component of this vector.
    /// @return A vector with the sign of each component of this vector, where each component is -1, 0, or 1 depending
    /// on whether the original component is negative, zero, or positive, respectively.
    [[nodiscard]] constexpr Vector4<T> getSign() const noexcept
    {
        return Vector4<T>(math::sign(x), math::sign(y), math::sign(z), math::sign(w));
    }

    /// @brief Get the projection of this vector onto the xy-plane by dividing the x and y components by the z component
    /// @return A vector containing the projected x and y components, with the z component set to 1.
    [[nodiscard]] constexpr Vector4<T> getProjectedToXY() const noexcept
    {
        GP_ASSERT(z != T{ 0 }, "Cannot project to XY plane when z component is zero");
        return Vector4<T>(x / z, y / z, T{ 1 }, w);
    }

    /// @brief Get the component-wise reciprocal of this vector, where each component is replaced by its reciprocal.
    /// @return A vector containing the reciprocal of each component of this vector, where each component is 1 divided
    /// by the original component.
    [[nodiscard]] constexpr Vector4<T> getReciprocal() const noexcept
    {
        GP_ASSERT(
            x != T{ 0 } && y != T{ 0 } && z != T{ 0 } && w != T{ 0 }, "Cannot compute reciprocal of zero component"
        );
        return Vector4<T>(T{ 1 } / x, T{ 1 } / y, T{ 1 } / z, T{ 1 } / w);
    }

    /// @brief Project this vector onto a normal vector.
    /// @param[in] normal The normal vector to project onto.
    /// @return The projected vector.
    [[nodiscard]] constexpr Vector4<T> projectOnToNormal(const Vector4<T>& normal) const noexcept
    {
        GP_ASSERT(normal.isNormalized(), "Normal vector must be normalized for projection");
        return normal * (this->dot(normal));
    }

    /// @brief Mirror this vector by another vector.
    /// @param[in] normal The vector to mirror by.
    /// @return The mirrored vector.
    [[nodiscard]] constexpr Vector4<T> mirrorBy(const Vector4<T>& normal) const noexcept
    {
        GP_ASSERT(normal.isNormalized(), "Normal vector must be normalized for mirroring");
        return *this - normal * (T{ 2 } * this->dot(normal));
    }

public:
    /// @brief Component-wise cross product of two vectors.
    /// @param[in] lhs The first vector to compute the cross product with.
    /// @param[in] rhs The second vector to compute the cross product with.
    /// @return The cross product of the two vectors.
    [[nodiscard]] static constexpr Vector4<T> cross(const Vector4<T>& lhs, const Vector4<T>& rhs) noexcept
    {
        return lhs ^ rhs;
    }

    /// @brief Component-wise dot product of two vectors.
    /// @param[in] lhs The first vector to compute the dot product with.
    /// @param[in] rhs The second vector to compute the dot product with.
    /// @return The dot product of the two vectors.
    [[nodiscard]] static constexpr T dot(const Vector4<T>& lhs, const Vector4<T>& rhs) noexcept
    {
        return lhs | rhs;
    }

    /// @brief Get the component-wise minimum of two vectors.
    /// @param[in] a The first vector to compare.
    /// @param[in] b The second vector to compare.
    /// @return A vector containing the minimum of each component between the two vectors.
    [[nodiscard]] static constexpr Vector4<T> getComponentWiseMin(const Vector4<T>& a, const Vector4<T>& b) noexcept
    {
        return a.getComponentWiseMin(b);
    }

    /// @brief Get the component-wise maximum of two vectors.
    /// @param[in] a The first vector to compare.
    /// @param[in] b The second vector to compare.
    /// @return A vector containing the maximum of each component between the two vectors.
    [[nodiscard]] static constexpr Vector4<T> getComponentWiseMax(const Vector4<T>& a, const Vector4<T>& b) noexcept
    {
        return a.getComponentWiseMax(b);
    }

    /// @brief Clamp a vector between two other vectors.
    /// @param[in] value The vector to clamp.
    /// @param[in] minVec The minimum vector.
    /// @param[in] maxVec The maximum vector.
    /// @return The clamped vector.
    [[nodiscard]] static constexpr Vector4<T>
        clamp(const Vector4<T>& value, const Vector4<T>& minVec, const Vector4<T>& maxVec) noexcept
    {
        return Vector4<T>(
            math::clamp(value.x, minVec.x, maxVec.x),
            math::clamp(value.y, minVec.y, maxVec.y),
            math::clamp(value.z, minVec.z, maxVec.z),
            math::clamp(value.w, minVec.w, maxVec.w)
        );
    }
};

/// @brief Get the component-wise minimum of two vectors.
/// @param[in] a The first vector to compare.
/// @param[in] b The second vector to compare.
/// @return A vector containing the minimum of each component between the two vectors.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr Vector4<T> min(const Vector4<T>& a, const Vector4<T>& b) noexcept
{
    return Vector4<T>::getComponentWiseMin(a, b);
}

/// @brief Get the component-wise maximum of two vectors.
/// @param[in] a The first vector to compare.
/// @param[in] b The second vector to compare.
/// @return A vector containing the maximum of each component between the two vectors.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr Vector4<T> max(const Vector4<T>& a, const Vector4<T>& b) noexcept
{
    return Vector4<T>::getComponentWiseMax(a, b);
}

/// @brief Get the component-wise minimum of three vectors.
/// @param[in] a The first vector to compare.
/// @param[in] b The second vector to compare.
/// @param[in] c The third vector to compare.
/// @return A vector containing the minimum of each component between the three vectors.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr Vector4<T> min(const Vector4<T>& a, const Vector4<T>& b, const Vector4<T>& c) noexcept
{
    return min(min(a, b), c);
}

/// @brief Get the component-wise maximum of three vectors.
/// @param[in] a The first vector to compare.
/// @param[in] b The second vector to compare.
/// @param[in] c The third vector to compare.
/// @return A vector containing the maximum of each component between the three vectors.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr Vector4<T> max(const Vector4<T>& a, const Vector4<T>& b, const Vector4<T>& c) noexcept
{
    return max(max(a, b), c);
}

/// @brief Clamp a vector between two other vectors.
/// @param[in] value The vector to clamp.
/// @param[in] minVec The minimum vector.
/// @param[in] maxVec The maximum vector.
/// @return The clamped vector.
template <concepts::IsFloatingPoint T>
[[nodiscard]] constexpr Vector4<T>
    clamp(const Vector4<T>& value, const Vector4<T>& minVec, const Vector4<T>& maxVec) noexcept
{
    return Vector4<T>::clamp(value, minVec, maxVec);
}

}   // namespace gp::math

/// @brief Component-wise addition of a scalar bias to a vector, with the scalar on the left-hand side of the operator.
/// @param[in] bias The scalar bias to add to each component of the vector.
/// @param[in] vec The vector to add the bias to.
/// @return A vector containing the result of the component-wise addition.
template <gp::concepts::IsFloatingPoint T, gp::concepts::IsArithmetic U>
[[nodiscard]] constexpr gp::math::Vector4<T> operator+(const U bias, const gp::math::Vector4<T>& vec) noexcept
{
    return vec + bias;
}

/// @brief Component-wise multiplication of a vector by a scalar scale factor, with the scalar on the left-hand side of
/// the operator.
/// @param[in] scale The scalar scale factor to multiply each component of the vector by.
/// @param[in] vec The vector to multiply by the scale factor.
/// @return A vector containing the result of the component-wise multiplication.
template <gp::concepts::IsFloatingPoint T, gp::concepts::IsArithmetic U>
[[nodiscard]] constexpr gp::math::Vector4<T> operator*(const U scale, const gp::math::Vector4<T>& vec) noexcept
{
    return vec * scale;
}

// Include the implementation of the Vector4 template
#include "maths/vector/Vector4.inl"
