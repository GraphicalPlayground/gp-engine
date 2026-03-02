// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/MathFwd.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief
/// @tparam T
template <CFloatingPoint T>
struct TVector4
{
public:
    union
    {
        struct
        {
            T x;   //<! The x component of the vector.
            T y;   //<! The y component of the vector.
            T z;   //<! The z component of the vector.
            T w;   //<! The w component of the vector.
        };

        T data[4];   //<! Array access to the vector components.
    };

public:
    /// @brief Default constructor initializes components to zero.
    constexpr TVector4() noexcept;

    /// @brief Constructs a vector with the given x, y, z and w components.
    /// @param inX The x component of the vector.
    /// @param inY The y component of the vector.
    /// @param inZ The z component of the vector.
    /// @param inW The w component of the vector.
    constexpr TVector4(const T inX, const T inY, const T inZ, const T inW) noexcept;

    /// @brief Constructs a vector from an array of four elements.
    /// @param arr An array containing the x, y, z and w components of the vector.
    /// @note The array must have at least 4 elements; excess elements are ignored.
    /// @warning No bounds checking is performed on the input array.
    constexpr TVector4(const T* arr) noexcept;

    /// @brief Constructs a vector from an array of four elements.
    /// @param arr An array containing the x, y, z and w components of the vector.
    /// @note The array must have exactly 4 elements; excess elements will cause a compile-time error.
    /// @warning No bounds checking is performed on the input array.
    constexpr TVector4(const T (&arr)[4]) noexcept;

    /// @brief Constructs a vector by copying the x and y components from a TVector2, and setting z and w to inZ and
    /// inW.
    /// @param vec2 The TVector2 to copy from. The x and y components are copied.
    /// @param inZ The z component of the vector. Defaults to zero if not specified.
    /// @param inW The w component of the vector. Defaults to zero if not specified.
    constexpr explicit TVector4(
        const TVector2<T>& vec2, const T inZ = static_cast<T>(0), const T inW = static_cast<T>(0)
    ) noexcept;

    /// @brief Constructs a vector by copying the x, y and z components from a TVector3, and setting w to inW.
    /// @param vec3 The TVector3 to copy from. The x, y and z components are copied.
    /// @param inW The w component of the vector. Defaults to zero if not specified.
    constexpr explicit TVector4(const TVector3<T>& vec3, const T inW = static_cast<T>(0)) noexcept;

    /// @brief Copy constructor.
    /// @param other The TVector4 to copy from.
    constexpr TVector4(const TVector4& other) noexcept = default;

    /// @brief Move constructor.
    /// @param other The TVector4 to move from.
    constexpr TVector4(TVector4&& other) noexcept = default;

public:
    /// @brief Copy assignment operator.
    /// @param other The TVector4 to copy from.
    /// @return A reference to this TVector4 after assignment.
    constexpr TVector4<T>& operator=(const TVector4& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param other The TVector4 to move from.
    /// @return A reference to this TVector4 after assignment.
    constexpr TVector4<T>& operator=(TVector4&& other) noexcept = default;
};

}   // namespace GP

#include "Math/Vector/Vector4.inl"
