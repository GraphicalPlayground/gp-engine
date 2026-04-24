// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "math/Forward.hpp"
#include <concepts>

namespace gp::math
{

/// @brief A 3x4 matrix template struct that can be used for various mathematical operations.
/// @tparam T The type of the matrix components, which must be a floating-point type (e.g., float, double).
template <std::floating_point T>
struct Mat34
{
public:
    union
    {
        struct
        {
            T x00, x01, x02, x03;
            T x10, x11, x12, x13;
            T x20, x21, x22, x23;
        };

        T data[12];
    };

public:
    /// @brief Default constructor. Initializes the matrix to the identity matrix.
    constexpr Mat34() noexcept
        // clang-format off
        : x00(1), x01(0), x02(0), x03(0)
        , x10(0), x11(1), x12(0), x13(0)
        , x20(0), x21(0), x22(1), x23(0)
    // clang-format on
    {}

    /// @brief Constructs a matrix with the given components.
    /// @param x00 The (0,0) component of the matrix.
    /// @param x01 The (0,1) component of the matrix.
    /// @param x02 The (0,2) component of the matrix.
    /// @param x03 The (0,3) component of the matrix.
    /// @param x10 The (1,0) component of the matrix.
    /// @param x11 The (1,1) component of the matrix.
    /// @param x12 The (1,2) component of the matrix.
    /// @param x13 The (1,3) component of the matrix.
    /// @param x20 The (2,0) component of the matrix.
    /// @param x21 The (2,1) component of the matrix.
    /// @param x22 The (2,2) component of the matrix.
    /// @param x23 The (2,3) component of the matrix.
    constexpr Mat34(T x00, T x01, T x02, T x03, T x10, T x11, T x12, T x13, T x20, T x21, T x22, T x23) noexcept
        // clang-format off
        : x00(x00), x01(x01), x02(x02), x03(x03)
        , x10(x10), x11(x11), x12(x12), x13(x13)
        , x20(x20), x21(x21), x22(x22), x23(x23)
    // clang-format on
    {}

    /// @brief Constructs a matrix with the same value for all diagonal components.
    /// @param value The value to set for all diagonal components.
    explicit constexpr Mat34(T value) noexcept
        // clang-format off
        : x00(value), x01(0)    , x02(0)    , x03(0)
        , x10(0)    , x11(value), x12(0)    , x13(0)
        , x20(0)    , x21(0)    , x22(value), x23(0)
    // clang-format on
    {}

    /// @brief Constructs a matrix from an array of all elements.
    /// @param ptr An array containing the components of the matrix.
    /// @warning No bounds checking is performed on the input array.
    explicit constexpr Mat34(const T* ptr) noexcept
        // clang-format off
        : x00(ptr[0]), x01(ptr[1]), x02(ptr[2]), x03(ptr[3])
        , x10(ptr[4]), x11(ptr[5]), x12(ptr[6]), x13(ptr[7])
        , x20(ptr[8]), x21(ptr[9]), x22(ptr[10]), x23(ptr[11])
    // clang-format on
    {}

    /// @brief Constructs a matrix from an array of all elements.
    /// @param arr An array containing the components of the matrix.
    /// @warning No bounds checking is performed on the input array.
    explicit constexpr Mat34(const T (&arr)[12]) noexcept
        // clang-format off
        : x00(arr[0]), x01(arr[1]), x02(arr[2]), x03(arr[3])
        , x10(arr[4]), x11(arr[5]), x12(arr[6]), x13(arr[7])
        , x20(arr[8]), x21(arr[9]), x22(arr[10]), x23(arr[11])
    // clang-format on
    {}
};

}   // namespace gp::math
