// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/Scalar.hpp"
#include "maths/base/Tresholds.hpp"
#include "maths/MathForward.hpp"

namespace gp::math
{

/// @brief A 2D vector template
/// @tparam T The floating-point type for the vector components
template <concepts::IsFloatingPoint T>
struct Vector2
{
public:
    T x;   //<! The x component of the vector
    T y;   //<! The y component of the vector

public:
    /// @brief Returns a zero vector.
    /// @return A vector with all components set to zero.
    GP_NODISCARD static inline constexpr Vector2<T> zero()
    {
        return { T{ 0 }, T{ 0 } };
    }

    /// @brief Returns a vector with all components set to one.
    /// @return A vector with all components set to one.
    GP_NODISCARD static inline constexpr Vector2<T> one()
    {
        return { T{ 1 }, T{ 1 } };
    }

    /// @brief Returns a vector pointing upwards.
    /// @return A vector pointing upwards.
    GP_NODISCARD static inline constexpr Vector2<T> up()
    {
        return { T{ 0 }, T{ 1 } };
    }

    /// @brief Returns a vector pointing downwards.
    /// @return A vector pointing downwards.
    GP_NODISCARD static inline constexpr Vector2<T> down()
    {
        return { T{ 0 }, T{ -1 } };
    }

    /// @brief Returns a vector pointing to the left.
    /// @return A vector pointing to the left.
    GP_NODISCARD static inline constexpr Vector2<T> left()
    {
        return { T{ -1 }, T{ 0 } };
    }

    /// @brief Returns a vector pointing to the right.
    /// @return A vector pointing to the right.
    GP_NODISCARD static inline constexpr Vector2<T> right()
    {
        return { T{ 1 }, T{ 0 } };
    }

    /// @brief Returns a unit vector along the x-axis.
    /// @return A unit vector along the x-axis.
    GP_NODISCARD static inline constexpr Vector2<T> unitX()
    {
        return { T{ 1 }, T{ 0 } };
    }

    /// @brief Returns a unit vector along the y-axis.
    /// @return A unit vector along the y-axis.
    GP_NODISCARD static inline constexpr Vector2<T> unitY()
    {
        return { T{ 0 }, T{ 1 } };
    }

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

public:
    /// @brief Component-wise cross product of this vector with another vector.
    /// @param[in] other The other vector to compute the cross product with.
    /// @return The cross product of this vector and the other vector.
    GP_NODISCARD constexpr T operator^(const Vector2<T>& other) const noexcept
    {
        return x * other.y - y * other.x;
    }

    /// @brief Component-wise dot product of this vector with another vector.
    /// @param[in] other The other vector to compute the dot product with.
    /// @return The dot product of this vector and the other vector.
    GP_NODISCARD constexpr T operator|(const Vector2<T>& other) const noexcept
    {
        return x * other.x + y * other.y;
    }

    /// @brief Negation operator, returns a vector with all components negated.
    /// @return A vector with all components negated.
    GP_NODISCARD constexpr Vector2<T> operator-() const noexcept
    {
        return Vector2<T>(-x, -y);
    }

    /// @brief Unary plus operator, returns the vector itself.
    /// @return The vector itself.
    GP_NODISCARD constexpr Vector2<T> operator+() const noexcept
    {
        return *this;
    }

    /// @brief Component-wise addition of this vector with another vector.
    /// @param[in] other The other vector to add to this vector.
    /// @return The component-wise sum of this vector and the other vector.
    GP_NODISCARD constexpr Vector2<T> operator+(const Vector2<T>& other) const noexcept
    {
        return Vector2<T>(x + other.x, y + other.y);
    }

    /// @brief Component-wise addition of a scalar bias to this vector.
    /// @param[in] bias The scalar bias to add to each component of the vector.
    /// @return The component-wise sum of this vector and the scalar bias.
    template <concepts::IsArithmetic U>
    GP_NODISCARD constexpr Vector2<T> operator+(const U bias) const noexcept
    {
        return Vector2<T>(x + static_cast<T>(bias), y + static_cast<T>(bias));
    }

    /// @brief Component-wise subtraction of another vector from this vector.
    /// @param[in] other The other vector to subtract from this vector.
    /// @return The component-wise difference of this vector and the other vector.
    GP_NODISCARD constexpr Vector2<T> operator-(const Vector2<T>& other) const noexcept
    {
        return Vector2<T>(x - other.x, y - other.y);
    }

    /// @brief Component-wise subtraction of a scalar bias from this vector.
    /// @param[in] bias The scalar bias to subtract from each component of the vector.
    /// @return The component-wise difference of this vector and the scalar bias.
    template <concepts::IsArithmetic U>
    GP_NODISCARD constexpr Vector2<T> operator-(const U bias) const noexcept
    {
        return Vector2<T>(x - static_cast<T>(bias), y - static_cast<T>(bias));
    }

    /// @brief Component-wise multiplication of this vector with another vector.
    /// @param[in] other The other vector to multiply with this vector.
    /// @return The component-wise product of this vector and the other vector.
    GP_NODISCARD constexpr Vector2<T> operator*(const Vector2<T>& other) const noexcept
    {
        return Vector2<T>(x * other.x, y * other.y);
    }

    /// @brief Component-wise multiplication of this vector by a scalar scale factor.
    /// @param[in] scale The scalar scale factor to multiply each component of the vector by.
    /// @return The component-wise product of this vector and the scalar scale factor.
    template <concepts::IsArithmetic U>
    GP_NODISCARD constexpr Vector2<T> operator*(const U scale) const noexcept
    {
        return Vector2<T>(x * static_cast<T>(scale), y * static_cast<T>(scale));
    }

    /// @brief Component-wise division of this vector by another vector.
    /// @param[in] other The other vector to divide this vector by.
    /// @return The component-wise quotient of this vector and the other vector.
    GP_NODISCARD constexpr Vector2<T> operator/(const Vector2<T>& other) const noexcept
    {
        return Vector2<T>(x / other.x, y / other.y);
    }

    /// @brief Component-wise division of this vector by a scalar scale factor.
    /// @param[in] scale The scalar scale factor to divide each component of the vector by.
    /// @return The component-wise quotient of this vector and the scalar scale factor.
    /// @details
    /// This operator computes the inverse of the scalar scale factor and multiplies it with each component
    /// of the vector for improved performance, especially when the scalar is a constant or can be optimized by the
    /// compiler.
    template <concepts::IsArithmetic U>
    GP_NODISCARD constexpr Vector2<T> operator/(const U scale) const noexcept
    {
        const T invScale = static_cast<T>(1) / static_cast<T>(scale);
        return Vector2<T>(x * invScale, y * invScale);
    }

    /// @brief Component-wise equality comparison of this vector with another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return True if all components of this vector are equal to the corresponding components of the other vector,
    /// false otherwise.
    GP_NODISCARD constexpr bool operator==(const Vector2<T>& other) const noexcept
    {
        return x == other.x && y == other.y;
    }

    /// @brief Component-wise inequality comparison of this vector with another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return True if any component of this vector is not equal to the corresponding component of the other vector,
    /// false otherwise.
    GP_NODISCARD constexpr bool operator!=(const Vector2<T>& other) const noexcept
    {
        return x != other.x || y != other.y;
    }

    /// @brief Subscript operator for non-const access to vector components by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y).
    /// @return A reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0 or 1).
    GP_NODISCARD constexpr T& operator[](const Int32 index) noexcept
    {
        return this->component(index);
    }

    /// @brief Subscript operator for const access to vector components by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y).
    /// @return A const reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0 or 1).
    GP_NODISCARD constexpr const T& operator[](const Int32 index) const noexcept
    {
        return this->component(index);
    }

    /// @brief In-place component-wise addition of another vector to this vector.
    /// @param[in] other The other vector to add to this vector.
    /// @return A reference to this vector after the addition.
    constexpr Vector2<T>& operator+=(const Vector2<T>& other) noexcept
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    /// @brief In-place component-wise addition of a scalar bias to this vector.
    /// @tparam U The arithmetic type of the scalar bias.
    /// @param[in] bias The scalar bias to add to each component of the vector.
    /// @return A reference to this vector after the addition.
    template <concepts::IsArithmetic U>
    constexpr Vector2<T>& operator+=(const U bias) noexcept
    {
        x += static_cast<T>(bias);
        y += static_cast<T>(bias);
        return *this;
    }

    /// @brief In-place component-wise subtraction of another vector from this vector.
    /// @param[in] other The other vector to subtract from this vector.
    /// @return A reference to this vector after the subtraction.
    constexpr Vector2<T>& operator-=(const Vector2<T>& other) noexcept
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    /// @brief In-place component-wise subtraction of a scalar bias from this vector.
    /// @tparam U The arithmetic type of the scalar bias.
    /// @param[in] bias The scalar bias to subtract from each component of the vector.
    /// @return A reference to this vector after the subtraction.
    template <concepts::IsArithmetic U>
    constexpr Vector2<T>& operator-=(const U bias) noexcept
    {
        x -= static_cast<T>(bias);
        y -= static_cast<T>(bias);
        return *this;
    }

    /// @brief In-place component-wise multiplication of this vector with another vector.
    /// @param[in] other The other vector to multiply with this vector.
    /// @return A reference to this vector after the multiplication.
    constexpr Vector2<T>& operator*=(const Vector2<T>& other) noexcept
    {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    /// @brief In-place component-wise multiplication of this vector by a scalar scale factor.
    /// @tparam U The arithmetic type of the scalar scale factor.
    /// @param[in] scale The scalar scale factor to multiply each component of the vector by.
    /// @return A reference to this vector after the multiplication.
    template <concepts::IsArithmetic U>
    constexpr Vector2<T>& operator*=(const U scale) noexcept
    {
        x *= static_cast<T>(scale);
        y *= static_cast<T>(scale);
        return *this;
    }

    /// @brief In-place component-wise division of this vector by another vector.
    /// @param[in] other The other vector to divide this vector by.
    /// @return A reference to this vector after the division.
    constexpr Vector2<T>& operator/=(const Vector2<T>& other) noexcept
    {
        x /= other.x;
        y /= other.y;
        return *this;
    }

    /// @brief In-place component-wise division of this vector by a scalar scale factor.
    /// @tparam U The arithmetic type of the scalar scale factor.
    /// @param[in] scale The scalar scale factor to divide each component of the vector by.
    /// @return A reference to this vector after the division.
    template <concepts::IsArithmetic U>
    constexpr Vector2<T>& operator/=(const U scale) noexcept
    {
        x /= static_cast<T>(scale);
        y /= static_cast<T>(scale);
        return *this;
    }

    /// @brief In-place pre-increment operator, increments each component of the vector by 1.
    /// @return A reference to this vector after the increment.
    constexpr Vector2<T>& operator++() noexcept
    {
        ++x;
        ++y;
        return *this;
    }

    /// @brief In-place post-increment operator, increments each component of the vector by 1.
    /// @return A copy of this vector before the increment.
    constexpr Vector2<T> operator++(int) noexcept
    {
        Vector2<T> temp = *this;
        ++(*this);
        return temp;
    }

    /// @brief In-place pre-decrement operator, decrements each component of the vector by 1.
    /// @return A reference to this vector after the decrement.
    constexpr Vector2<T>& operator--() noexcept
    {
        --x;
        --y;
        return *this;
    }

    /// @brief In-place post-decrement operator, decrements each component of the vector by 1.
    /// @return A copy of this vector before the decrement.
    constexpr Vector2<T> operator--(int) noexcept
    {
        Vector2<T> temp = *this;
        --(*this);
        return temp;
    }

public:
    /// @brief Gets a reference to a component of the vector by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y).
    /// @return A reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0 or 1).
    GP_NODISCARD constexpr T& component(const Int32 index) noexcept
    {
        GP_ASSERT(index >= 0 && index < 2, "Index out of range");
        return *(&x + index);
    }

    /// @brief Gets a const reference to a component of the vector by index.
    /// @param[in] index The index of the component to access (0 for x, 1 for y).
    /// @return A const reference to the component at the specified index.
    /// @note The behavior is undefined if the index is out of range (not 0 or 1).
    GP_NODISCARD constexpr const T& component(const Int32 index) const noexcept
    {
        GP_ASSERT(index >= 0 && index < 2, "Index out of range");
        return *(&x + index);
    }

    /// @brief Component-wise cross product of this vector with another vector.
    /// @param[in] other The other vector to compute the cross product with.
    /// @return The cross product of this vector and the other vector.
    GP_NODISCARD constexpr Vector2<T> cross(const Vector2<T>& other) const noexcept
    {
        return *this ^ other;
    }

    /// @brief Component-wise dot product of this vector with another vector.
    /// @param[in] other The other vector to compute the dot product with.
    /// @return The dot product of this vector and the other vector.
    GP_NODISCARD constexpr T dot(const Vector2<T>& other) const noexcept
    {
        return *this | other;
    }

    /// @brief Checks if this vector is equal to another vector within a given tolerance.
    /// @param[in] other The other vector to compare with this vector.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if all components of this vector are equal to the corresponding components of the other vector
    /// within the tolerance, false otherwise.
    GP_NODISCARD constexpr bool
        equals(const Vector2<T>& other, const T tolerance = constants<T>::kindaSmallNumber) const noexcept
    {
        return math::abs(x - other.x) <= tolerance && math::abs(y - other.y) <= tolerance;
    }

    /// @brief Checks if all components of this vector are equal within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if all components of this vector are equal within the tolerance, false otherwise.
    GP_NODISCARD constexpr bool isAllComponentsEqual(const T tolerance = constants<T>::kindaSmallNumber) const noexcept
    {
        return math::abs(x - y) <= tolerance;
    }

    /// @brief Checks if the vector is nearly zero within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if all components of the vector are nearly zero within the tolerance, false otherwise.
    GP_NODISCARD constexpr bool isNearlyZero(const T tolerance = constants<T>::kindaSmallNumber) const noexcept
    {
        return math::abs(x) <= tolerance && math::abs(y) <= tolerance;
    }

    /// @brief Checks if the vector is exactly zero.
    /// @return True if all components of the vector are exactly zero, false otherwise.
    GP_NODISCARD constexpr bool isZero() const noexcept
    {
        return x == T{ 0 } && y == T{ 0 };
    }

    /// @brief Checks if the vector is a unit vector within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if the vector is a unit vector within the tolerance, false otherwise.
    GP_NODISCARD constexpr bool isUnit(const T tolerance = constants<T>::kindaSmallNumber) const noexcept
    {
        return math::abs(T{ 1 } - lengthSquared()) <= tolerance;
    }

    /// @brief Checks if the vector is normalized within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if the vector is normalized within the tolerance, false otherwise.
    GP_NODISCARD constexpr bool isNormalized(const T tolerance = tresholds<T>::vectorNormalized) const noexcept
    {
        return isUnit(tolerance);
    }

    /// @brief Checks if all components of the vector are uniform within a given tolerance.
    /// @param[in] tolerance The tolerance for the comparison.
    /// @return True if all components of the vector are uniform within the tolerance, false otherwise.
    GP_NODISCARD constexpr bool isUniform(const T tolerance = constants<T>::kindaSmallNumber) const noexcept
    {
        return isAllComponentsEqual(tolerance);
    }

    /// @brief Get the maximum component value of the vector.
    /// @return The maximum component value among x, y, and z.
    GP_NODISCARD constexpr T getMax() const noexcept
    {
        return math::max(x, y);
    }

    /// @brief Get the minimum component value of the vector.
    /// @return The minimum component value among x, y, and z.
    GP_NODISCARD constexpr T getMin() const noexcept
    {
        return math::min(x, y);
    }

    /// @brief Get the maximum absolute component value of the vector.
    /// @return The maximum absolute component value among x, y, and z.
    GP_NODISCARD constexpr T getAbsMax() const noexcept
    {
        return math::max(math::abs(x), math::abs(y));
    }

    /// @brief Get the minimum absolute component value of the vector.
    /// @return The minimum absolute component value among x, y, and z.
    GP_NODISCARD constexpr T getAbsMin() const noexcept
    {
        return math::min(math::abs(x), math::abs(y));
    }

    /// @brief Get the component-wise minimum of this vector and another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return A vector containing the minimum of each component between this vector and the other vector.
    GP_NODISCARD constexpr Vector2<T> getComponentMin(const Vector2<T>& other) const noexcept
    {
        return Vector2<T>(math::min(x, other.x), math::min(y, other.y));
    }

    /// @brief Get the component-wise maximum of this vector and another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return A vector containing the maximum of each component between this vector and the other vector.
    GP_NODISCARD constexpr Vector2<T> getComponentMax(const Vector2<T>& other) const noexcept
    {
        return Vector2<T>(math::max(x, other.x), math::max(y, other.y));
    }

    /// @brief Get the component-wise minimum of the absolute values of this vector and another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return A vector containing the minimum of the absolute values of each component between this vector and the
    /// other vector.
    GP_NODISCARD constexpr Vector2<T> getComponentAbsMin(const Vector2<T>& other) const noexcept
    {
        return Vector2<T>(
            math::min(math::abs(x), math::abs(other.x)),
            math::min(math::abs(y), math::abs(other.y))
        );
    }

    /// @brief Get the component-wise maximum of the absolute values of this vector and another vector.
    /// @param[in] other The other vector to compare with this vector.
    /// @return A vector containing the maximum of the absolute values of each component between this vector and the
    /// other vector.
    GP_NODISCARD constexpr Vector2<T> getComponentAbsMax(const Vector2<T>& other) const noexcept
    {
        return Vector2<T>(
            math::max(math::abs(x), math::abs(other.x)),
            math::max(math::abs(y), math::abs(other.y))
        );
    }

    /// @brief Get a vector containing the absolute values of each component of this vector.
    /// @return A vector with the absolute values of each component of this vector.
    GP_NODISCARD constexpr Vector2<T> getAbsolute() const noexcept
    {
        return Vector2<T>(math::abs(x), math::abs(y));
    }

    /// @brief Get the length of the vector.
    /// @return The length of the vector.
    GP_NODISCARD constexpr T length() const noexcept
    {
        return math::sqrt(x * x + y * y);
    }

    /// @brief Get the squared length of the vector.
    /// @return The squared length of the vector.
    GP_NODISCARD constexpr T lengthSquared() const noexcept
    {
        return x * x + y * y;
    }

    /// @brief Normalize the vector in place if its length is greater than a given tolerance.
    /// @param[in] tolerance The tolerance for the length check to avoid division by zero or very small numbers.
    /// @return True if the vector was successfully normalized, false if the length was too small and the vector was not
    /// modified.
    constexpr bool normalize(const T tolerance = constants<T>::smallNumber) noexcept
    {
        const T squareSum = x * x + y * y;
        if (squareSum > tolerance)
        {
            const T scale = math::inverseSqrt(squareSum);
            x *= scale;
            y *= scale;
            return true;
        }
        return false;
    }

    /// @brief Get a normalized version of this vector without modifying the original vector unsafely, assuming the
    /// length of the vector is greater than zero.
    /// @return A normalized version of this vector if its length is greater than zero, or an undefined result if the
    /// length is zero (caller must ensure the length is greater than zero).
    GP_NODISCARD constexpr Vector2<T> getUnsafeNormal() const
    {
        const T scale = math::inverseSqrt(x * x + y * y);
        return Vector2<T>(x * scale, y * scale);
    }

    /// @brief Get a normalized version of this vector, returning a zero vector if the length is less than or equal to a
    /// given tolerance to avoid division by zero or very small numbers.
    /// @param[in] tolerance The tolerance for the length check to determine if the vector is too small to normalize.
    /// @return A normalized version of this vector if its length is greater than the tolerance, or a zero vector if the
    /// length is less than or equal to the tolerance.
    GP_NODISCARD constexpr Vector2<T> getSafeNormal(const T tolerance = constants<T>::smallNumber) const noexcept
    {
        const T squareSum = x * x + y * y;

        if (squareSum == T{ 1 })
        {
            return *this;
        }
        else if (squareSum < tolerance)
        {
            return Vector2<T>::zero();
        }

        const T scale = math::inverseSqrt(squareSum);
        return Vector2<T>(x * scale, y * scale);
    }

    /// @brief Get a vector containing the sign of each component of this vector.
    /// @return A vector with the sign of each component of this vector, where each component is -1, 0, or 1 depending
    /// on whether the original component is negative, zero, or positive, respectively.
    GP_NODISCARD constexpr Vector2<T> getSignVector() const noexcept
    {
        return Vector2<T>(math::sign(x), math::sign(y));
    }

    /// @brief Get the component-wise reciprocal of this vector, where each component is replaced by its reciprocal.
    /// @return A vector containing the reciprocal of each component of this vector, where each component is 1 divided
    /// by the original component.
    GP_NODISCARD constexpr Vector2<T> getReciprocal() const noexcept
    {
        return Vector2<T>(
            x != T{ 0 } ? T{ 1 } / x : constants<T>::bigNumber,
            y != T{ 0 } ? T{ 1 } / y : constants<T>::bigNumber
        );
    }

    /// @brief Project this vector onto another vector.
    /// @param[in] other The vector to project onto.
    /// @return The projected vector.
    GP_NODISCARD constexpr Vector2<T> projectOnTo(const Vector2<T>& other) const noexcept
    {
        return other * ((*this | other) / (other | other));
    }

    /// @brief Project this vector onto a normal vector.
    /// @param[in] normal The normal vector to project onto.
    /// @return The projected vector.
    GP_NODISCARD constexpr Vector2<T> projectOnToNormal(const Vector2<T>& normal) const noexcept
    {
        return normal * (*this | normal);
    }

public:

    /// @brief Component-wise cross product of two vectors.
    /// @param[in] lhs The first vector to compute the cross product with.
    /// @param[in] rhs The second vector to compute the cross product with.
    /// @return The cross product of the two vectors.
    GP_NODISCARD constexpr static T cross(const Vector2<T>& lhs, const Vector2<T>& rhs) noexcept
    {
        return lhs ^ rhs;
    }

    /// @brief Component-wise dot product of two vectors.
    /// @param[in] lhs The first vector to compute the dot product with.
    /// @param[in] rhs The second vector to compute the dot product with.
    /// @return The dot product of the two vectors.
    GP_NODISCARD constexpr static T dot(const Vector2<T>& lhs, const Vector2<T>& rhs) noexcept
    {
        return lhs | rhs;
    }

    /// @brief Get the component-wise minimum of two vectors.
    /// @param[in] a The first vector to compare.
    /// @param[in] b The second vector to compare.
    /// @return A vector containing the minimum of each component between the two vectors.
    GP_NODISCARD constexpr static inline Vector2<T> min(const Vector2<T>& a, const Vector2<T>& b) noexcept
    {
        return Vector2<T>(math::min(a.x, b.x), math::min(a.y, b.y));
    }

    /// @brief Get the component-wise maximum of two vectors.
    /// @param[in] a The first vector to compare.
    /// @param[in] b The second vector to compare.
    /// @return A vector containing the maximum of each component between the two vectors.
    GP_NODISCARD constexpr static inline Vector2<T> max(const Vector2<T>& a, const Vector2<T>& b) noexcept
    {
        return Vector2<T>(math::max(a.x, b.x), math::max(a.y, b.y));
    }

    /// @brief Get the component-wise minimum of three vectors.
    /// @param[in] a The first vector to compare.
    /// @param[in] b The second vector to compare.
    /// @param[in] c The third vector to compare.
    /// @return A vector containing the minimum of each component between the three vectors.
    GP_NODISCARD constexpr static inline Vector2<T>
        min(const Vector2<T>& a, const Vector2<T>& b, const Vector2<T>& c) noexcept
    {
        return Vector2<T>(math::min(a.x, b.x, c.x), math::min(a.y, b.y, c.y));
    }

    /// @brief Get the component-wise maximum of three vectors.
    /// @param[in] a The first vector to compare.
    /// @param[in] b The second vector to compare.
    /// @param[in] c The third vector to compare.
    /// @return A vector containing the maximum of each component between the three vectors.
    GP_NODISCARD constexpr static inline Vector2<T>
        max(const Vector2<T>& a, const Vector2<T>& b, const Vector2<T>& c) noexcept
    {
        return Vector2<T>(math::max(a.x, b.x, c.x), math::max(a.y, b.y, c.y));
    }

    /// @brief Clamp a vector between two other vectors.
    /// @param[in] value The vector to clamp.
    /// @param[in] minVec The minimum vector.
    /// @param[in] maxVec The maximum vector.
    /// @return The clamped vector.
    GP_NODISCARD constexpr static inline Vector2<T>
        clamp(const Vector2<T>& value, const Vector2<T>& minVec, const Vector2<T>& maxVec) noexcept
    {
        return Vector2<T>(
            math::clamp(value.x, minVec.x, maxVec.x),
            math::clamp(value.y, minVec.y, maxVec.y)
        );
    }
};

/// @brief Get the component-wise minimum of two vectors.
/// @param[in] a The first vector to compare.
/// @param[in] b The second vector to compare.
/// @return A vector containing the minimum of each component between the two vectors.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr Vector2<T> min(const Vector2<T>& a, const Vector2<T>& b) noexcept
{
    return Vector2<T>::min(a, b);
}

/// @brief Get the component-wise maximum of two vectors.
/// @param[in] a The first vector to compare.
/// @param[in] b The second vector to compare.
/// @return A vector containing the maximum of each component between the two vectors.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr Vector2<T> max(const Vector2<T>& a, const Vector2<T>& b) noexcept
{
    return Vector2<T>::max(a, b);
}

/// @brief Get the component-wise minimum of three vectors.
/// @param[in] a The first vector to compare.
/// @param[in] b The second vector to compare.
/// @param[in] c The third vector to compare.
/// @return A vector containing the minimum of each component between the three vectors.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr Vector2<T> min(const Vector2<T>& a, const Vector2<T>& b, const Vector2<T>& c) noexcept
{
    return Vector2<T>::min(a, b, c);
}

/// @brief Get the component-wise maximum of three vectors.
/// @param[in] a The first vector to compare.
/// @param[in] b The second vector to compare.
/// @param[in] c The third vector to compare.
/// @return A vector containing the maximum of each component between the three vectors.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr Vector2<T> max(const Vector2<T>& a, const Vector2<T>& b, const Vector2<T>& c) noexcept
{
    return Vector2<T>::max(a, b, c);
}

/// @brief Clamp a vector between two other vectors.
/// @param[in] value The vector to clamp.
/// @param[in] minVec The minimum vector.
/// @param[in] maxVec The maximum vector.
/// @return The clamped vector.
template <concepts::IsFloatingPoint T>
GP_NODISCARD constexpr Vector2<T>
    clamp(const Vector2<T>& value, const Vector2<T>& minVec, const Vector2<T>& maxVec) noexcept
{
    return Vector2<T>::clamp(value, minVec, maxVec);
}

}   // namespace gp::math

/// @brief Component-wise addition of a scalar bias to a vector, with the scalar on the left-hand side of the operator.
/// @param[in] bias The scalar bias to add to each component of the vector.
/// @param[in] vec The vector to add the bias to.
/// @return A vector containing the result of the component-wise addition.
template <gp::concepts::IsFloatingPoint T, gp::concepts::IsArithmetic U>
GP_NODISCARD constexpr gp::math::Vector2<T> operator+(const U bias, const gp::math::Vector2<T>& vec) noexcept
{
    return vec + bias;
}

/// @brief Component-wise multiplication of a vector by a scalar scale factor, with the scalar on the left-hand side of
/// the operator.
/// @param[in] scale The scalar scale factor to multiply each component of the vector by.
/// @param[in] vec The vector to multiply by the scale factor.
/// @return A vector containing the result of the component-wise multiplication.
template <gp::concepts::IsFloatingPoint T, gp::concepts::IsArithmetic U>
GP_NODISCARD constexpr gp::math::Vector2<T> operator*(const U scale, const gp::math::Vector2<T>& vec) noexcept
{
    return vec * scale;
}

// Include the implementation of the Vector2 template
#include "maths/vector/Vector2.inl"
