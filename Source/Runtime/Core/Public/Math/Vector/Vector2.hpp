// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/MathFwd.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief
/// @tparam T
template <CFloatingPoint T>
struct TVector2
{
public:
    union
    {
        struct
        {
            T x;   //<! The x component of the vector.
            T y;   //<! The y component of the vector.
        };

        T data[2];   //<! Array access to the vector components.
    };

public:
    /// @brief Default constructor initializes components to zero.
    constexpr TVector2() noexcept;

    /// @brief Constructs a vector with the given x and y components.
    /// @param inX The x component of the vector.
    /// @param inY The y component of the vector.
    constexpr TVector2(const T inX, const T inY) noexcept;

    /// @brief Constructs a vector from an array of two elements.
    /// @param arr An array containing the x and y components of the vector.
    /// @note The array must have at least 2 elements; excess elements are ignored.
    /// @warning No bounds checking is performed on the input array.
    constexpr TVector2(const T* arr) noexcept;

    /// @brief Constructs a vector from an array of two elements.
    /// @param arr An array containing the x and y components of the vector.
    /// @note The array must have exactly 2 elements; excess elements will cause a compile-time error.
    /// @warning No bounds checking is performed on the input array.
    constexpr TVector2(const T (&arr)[2]) noexcept;

    /// @brief Constructs a vector by copying the x and y components from a TVector3, discarding the z component.
    /// @param vec3 The TVector3 to copy from. The x and y components are copied.
    /// @note The z component of vec3 is ignored.
    constexpr explicit TVector2(const TVector3<T>& vec3) noexcept;

    /// @brief Constructs a vector by copying the x and y components from a TVector4, discarding the z and w components.
    /// @param vec4 The TVector4 to copy from. The x and y components are copied.
    /// @note The z and w components of vec4 are ignored.
    constexpr explicit TVector2(const TVector4<T>& vec4) noexcept;

    /// @brief Copy constructor.
    /// @param other The TVector2 to copy from.
    constexpr TVector2(const TVector2& other) noexcept = default;

    /// @brief Move constructor.
    /// @param other The TVector2 to move from.
    constexpr TVector2(TVector2&& other) noexcept = default;

public:
    /// @brief Copy assignment operator.
    /// @param other The TVector2 to copy from.
    /// @return A reference to this TVector2 after assignment.
    constexpr TVector2& operator=(const TVector2& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param other The TVector2 to move from.
    /// @return A reference to this TVector2 after assignment.
    constexpr TVector2& operator=(TVector2&& other) noexcept = default;

    /// @brief Equality operator compares two TVector2 for component-wise equality.
    /// @param other The TVector2 to compare with.
    /// @return true if all components of this TVector2 are equal to the corresponding components of other.
    GP_NODISCARD constexpr bool operator==(const TVector2& other) const noexcept;

    /// @brief Inequality operator compares two TVector2 for component-wise inequality.
    /// @param other The TVector2 to compare with.
    /// @return true if any component of this TVector2 is not equal to the corresponding component of other.
    GP_NODISCARD constexpr bool operator!=(const TVector2& other) const noexcept;
};

}   // namespace GP

#include "Math/Vector/Vector2.inl"
