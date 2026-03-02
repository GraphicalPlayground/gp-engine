// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/MathFwd.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief
/// @tparam T
template <CFloatingPoint T>
struct TVector3
{
public:
    union
    {
        struct
        {
            T x;   //<! The x component of the vector.
            T y;   //<! The y component of the vector.
            T z;   //<! The z component of the vector.
        };

        T data[3];   //<! Array access to the vector components.
    };

public:
    /// @brief Default constructor initializes components to zero.
    constexpr TVector3() noexcept;

    /// @brief Constructs a vector with the given x, y and z components.
    /// @param inX The x component of the vector.
    /// @param inY The y component of the vector.
    /// @param inZ The z component of the vector.
    constexpr TVector3(const T inX, const T inY, const T inZ) noexcept;

    /// @brief Constructs a vector from an array of three elements.
    /// @param arr An array containing the x, y and z components of the vector.
    /// @note The array must have at least 3 elements; excess elements are ignored.
    /// @warning No bounds checking is performed on the input array.
    constexpr TVector3(const T* arr) noexcept;

    /// @brief Constructs a vector from an array of three elements.
    /// @param arr An array containing the x, y and z components of the vector.
    /// @note The array must have exactly 3 elements; excess elements will cause a compile-time error.
    /// @warning No bounds checking is performed on the input array.
    constexpr TVector3(const T (&arr)[3]) noexcept;

    /// @brief Constructs a vector by copying the x and y components from a TVector2, and setting z to inZ.
    /// @param vec2 The TVector2 to copy from. The x and y components are copied.
    /// @param inZ The z component of the vector. Defaults to zero if not specified.
    constexpr explicit TVector3(const TVector2<T>& vec2, const T inZ = static_cast<T>(0)) noexcept;

    /// @brief Constructs a vector by copying the x, y and z components from a TVector4, discarding the w component.
    /// @param vec4 The TVector4 to copy from. The x, y and z components are copied.
    /// @note The w component of vec4 are ignored.
    constexpr explicit TVector3(const TVector4<T>& vec4) noexcept;

    /// @brief Copy constructor.
    /// @param other The TVector3 to copy from.
    constexpr TVector3(const TVector3& other) noexcept = default;

    /// @brief Move constructor.
    /// @param other The TVector3 to move from.
    constexpr TVector3(TVector3&& other) noexcept = default;

public:
    /// @brief Copy assignment operator.
    /// @param other The TVector3 to copy from.
    /// @return A reference to this TVector3 after assignment.
    constexpr TVector3& operator=(const TVector3& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param other The TVector3 to move from.
    /// @return A reference to this TVector3 after assignment.
    constexpr TVector3& operator=(TVector3&& other) noexcept = default;
};

}   // namespace GP

#include "Math/Vector/Vector3.inl"
