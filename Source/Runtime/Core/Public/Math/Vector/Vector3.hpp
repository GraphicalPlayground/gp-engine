// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Math/Utils/Constants.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief 3D vector template class.
/// \tparam T The type of the vector components, must be a floating-point type.
/// \warning This class not designed for SIMD optimizations, use specialized SIMD vector classes for that purpose.
template <Concepts::IsFloatingPoint T>
struct TVector3
{
public:
    union
    {
        struct
        {
            T x, y, z;   //<! Components of the vector.
        };

        T data[3];   //<! Array access to components.
    };

public:
    /// \brief Returns a vector with all components set to zero.
    /// \return A vector with all components set to zero.
    static constexpr TVector3<T> Zero() noexcept;

    /// \brief Returns a vector with all components set to one.
    /// \return A vector with all components set to one.
    static constexpr TVector3<T> One() noexcept;

    /// \brief Returns the unit vector along the X axis.
    /// \return The unit vector along the X axis.
    static constexpr TVector3<T> UnitX() noexcept;

    /// \brief Returns the unit vector along the Y axis.
    /// \return The unit vector along the Y axis.
    static constexpr TVector3<T> UnitY() noexcept;

    /// \brief Returns the unit vector along the Z axis.
    /// \return The unit vector along the Z axis.
    static constexpr TVector3<T> UnitZ() noexcept;

    /// \brief Returns the negative unit vector along the X axis.
    /// \return The negative unit vector along the X axis.
    static constexpr TVector3<T> NegUnitX() noexcept;

    /// \brief Returns the negative unit vector along the Y axis.
    /// \return The negative unit vector along the Y axis.
    static constexpr TVector3<T> NegUnitY() noexcept;

    /// \brief Returns the negative unit vector along the Z axis.
    /// \return The negative unit vector along the Z axis.
    static constexpr TVector3<T> NegUnitZ() noexcept;

    /// \brief Returns a vector with all components set to the maximum representable value.
    /// \return A vector with all components set to the maximum representable value.
    static constexpr TVector3<T> Max() noexcept;

    /// \brief Returns a vector with all components set to the minimum representable value.
    /// \return A vector with all components set to the minimum representable value.
    static constexpr TVector3<T> Min() noexcept;

    /// \brief Returns a vector with all components set to NaN (Not a Number).
    /// \return A vector with all components set to NaN.
    static constexpr TVector3<T> NaN() noexcept;

    /// \brief Returns a vector with all components set to positive infinity.
    /// \return A vector with all components set to positive infinity.
    static constexpr TVector3<T> Infinity() noexcept;

    /// \brief Returns a vector with all components set to negative infinity.
    /// \return A vector with all components set to negative infinity.
    static constexpr TVector3<T> NegInfinity() noexcept;

    /// \brief Returns the left direction vector (-X axis).
    /// \return The left direction vector.
    static constexpr TVector3<T> Left() noexcept;

    /// \brief Returns the right direction vector (+X axis).
    /// \return The right direction vector.
    static constexpr TVector3<T> Right() noexcept;

    /// \brief Returns the up direction vector (+Y axis).
    /// \return The up direction vector.
    static constexpr TVector3<T> Up() noexcept;

    /// \brief Returns the down direction vector (-Y axis).
    /// \return The down direction vector.
    static constexpr TVector3<T> Down() noexcept;

    /// \brief Returns the forward direction vector (+Z axis).
    /// \return The forward direction vector.
    static constexpr TVector3<T> Forward() noexcept;

    /// \brief Returns the backward direction vector (-Z axis).
    /// \return The backward direction vector.
    static constexpr TVector3<T> Backward() noexcept;

public:
    /// \brief Default constructor (zero-initializes the vector).
    constexpr TVector3() noexcept;

    /// \brief Explicit constructor with a single value for all components.
    /// \param value The value to set for all components.
    constexpr explicit TVector3(T value) noexcept;

    /// \brief Constructor with individual components.
    /// \param inX The x component.
    /// \param inY The y component.
    /// \param inZ The z component.
    constexpr TVector3(T inX, T inY, T inZ) noexcept;

    /// \brief Constructor with a 2D vector and a z component.
    /// \param inXY The 2D vector for x and y components.
    /// \param inZ The z component (default: 0).
    constexpr explicit TVector3(const TVector2<T>& inXY, T inZ = static_cast<T>(0)) noexcept;

    /// \brief Constructor with a 4D vector (drops the w component).
    /// \param inXYZW The 4D vector to convert from.
    constexpr explicit TVector3(const TVector4<T>& inXYZW) noexcept;

public:
    /// \brief Subscript operator for accessing components by index.
    /// \param index The index of the component (0 for x, 1 for y, 2 for z).
    /// \return Reference to the component at the specified index.
    /// \warning No bounds checking is performed; ensure index is valid.
    GP_NODISCARD constexpr T& operator[](Size index) noexcept;

    /// \brief Subscript operator for accessing components by index (const version).
    /// \param index The index of the component (0 for x, 1 for y, 2 for z).
    /// \return Const reference to the component at the specified index.
    /// \warning No bounds checking is performed; ensure index is valid.
    GP_NODISCARD constexpr const T& operator[](Size index) const noexcept;

public:
    /// \brief Getter for the X component.
    /// \return The X component of the vector.
    GP_NODISCARD constexpr T X() const noexcept;

    /// \brief Getter for the Y component.
    /// \return The Y component of the vector.
    GP_NODISCARD constexpr T Y() const noexcept;

    /// \brief Getter for the Z component.
    /// \return The Z component of the vector.
    GP_NODISCARD constexpr T Z() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector2<T> XY() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector2<T> XZ() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector2<T> YZ() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector2<T> YX() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector2<T> ZX() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector2<T> ZY() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector3<T> XYZ() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector3<T> XZY() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector3<T> YXZ() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector3<T> YZX() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector3<T> ZXY() const noexcept;

    /// \brief Swizzle methods to access vector components in different orders.
    /// \return A new vector with components arranged in the specified order.
    GP_NODISCARD constexpr TVector3<T> ZYX() const noexcept;

    /// \brief Calculates the dot product of this vector with another vector.
    /// \param other The other vector.
    /// \return The dot product of the two vectors.
    GP_NODISCARD constexpr T Dot(const TVector3<T>& other) const noexcept;

    /// \brief Calculates the cross product of this vector with another vector.
    /// \param other The other vector.
    /// \return The cross product of the two vectors.
    GP_NODISCARD constexpr TVector3<T> Cross(const TVector3<T>& other) const noexcept;

    /// \brief Calculates the squared length of the vector.
    /// \return The squared length of the vector.
    GP_NODISCARD constexpr T LengthSquared() const noexcept;

    /// \brief Calculates the length of the vector.
    /// \return The length of the vector.
    GP_NODISCARD constexpr T Length() const noexcept;

    /// \brief Checks if the vector is approximately zero within a given tolerance.
    /// \param tolerance The tolerance value.
    /// \return True if the vector is approximately zero, false otherwise.
    GP_NODISCARD constexpr bool IsZero(T tolerance = Constants<T>::KindaSmallNumber) const noexcept;

    /// \brief Checks if the vector is normalized (length approximately equal to 1) within a given tolerance.
    /// \param tolerance The tolerance value.
    /// \return True if the vector is normalized, false otherwise.
    GP_NODISCARD constexpr bool IsNormalized(T tolerance = Constants<T>::KindaSmallNumber) const noexcept;

    /// \brief Checks if all components of the vector are finite numbers.
    /// \return True if all components are finite, false otherwise.
    GP_NODISCARD constexpr bool IsFinite() const noexcept;

    /// \brief Checks if any component of the vector is infinite.
    /// \return True if any component is infinite, false otherwise.
    GP_NODISCARD constexpr bool IsInfinite() const noexcept;

    /// \brief Checks if any component of the vector is NaN (Not a Number).
    /// \return True if any component is NaN, false otherwise.
    GP_NODISCARD constexpr bool IsNaN() const noexcept;
};

/// \brief Check if all components of the vector are finite numbers.
/// \tparam T The type of the vector components.
/// \param value The vector to check.
/// \return True if all components are finite.
template <Concepts::IsFloatingPoint T>
constexpr bool IsFinite(const TVector3<T>& value) noexcept;

/// \brief Check if any component of the vector is infinite.
/// \tparam T The type of the vector components.
/// \param value The vector to check.
/// \return True if any component is infinite.
template <Concepts::IsFloatingPoint T>
constexpr bool IsInfinite(const TVector3<T>& value) noexcept;

/// \brief Check if any component of the vector is NaN (Not a Number).
/// \tparam T The type of the vector components.
/// \param value The vector to check.
/// \return True if any component is NaN.
template <Concepts::IsFloatingPoint T>
constexpr bool IsNaN(const TVector3<T>& value) noexcept;

}   // namespace GP::Math

// Include inline implementations.
#include "Math/Vector/Vector3.inl"
