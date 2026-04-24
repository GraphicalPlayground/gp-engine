// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "math/Forward.hpp"
#include <concepts>

namespace gp::math
{

/// @brief A 2x2 matrix template struct that can be used for various mathematical operations.
/// @tparam T The type of the matrix components, which must be a floating-point type (e.g., float, double).
template <std::floating_point T>
struct Mat2
{
public:
    union
    {
        struct
        {
            T x00, x01;
            T x10, x11;
        };

        T data[4];
    };

public:
    /// @brief Default constructor. Initializes the matrix to the identity matrix.
    constexpr Mat2() noexcept
        // clang-format off
        : x00(1), x01(0)
        , x10(0), x11(1)
    // clang-format on
    {}

    /// @brief Constructs a matrix with the given components.
    /// @param x00 The (0,0) component of the matrix.
    /// @param x01 The (0,1) component of the matrix.
    /// @param x10 The (1,0) component of the matrix.
    /// @param x11 The (1,1) component of the matrix.
    constexpr Mat2(T x00, T x01, T x10, T x11) noexcept
        // clang-format off
        : x00(x00), x01(x01)
        , x10(x10), x11(x11)
    // clang-format on
    {}

    /// @brief Constructs a matrix with the same value for all diagonal components.
    /// @param value The value to set for all diagonal components.
    explicit constexpr Mat2(T value) noexcept
        // clang-format off
        : x00(value), x01(0)
        , x10(0)    , x11(value)
    // clang-format on
    {}

    /// @brief Constructs a matrix from an array of four elements.
    /// @param ptr An array containing the components of the matrix.
    /// @warning No bounds checking is performed on the input array.
    explicit constexpr Mat2(const T* ptr) noexcept
        // clang-format off
        : x00(ptr[0]), x01(ptr[1])
        , x10(ptr[2]), x11(ptr[3])
    // clang-format on
    {}

    /// @brief Constructs a matrix from an array of four elements.
    /// @param arr An array containing the components of the matrix.
    /// @warning No bounds checking is performed on the input array.
    explicit constexpr Mat2(const T (&arr)[4]) noexcept
        // clang-format off
        : x00(arr[0]), x01(arr[1])
        , x10(arr[2]), x11(arr[3])
    // clang-format on
    {}
};

}   // namespace gp::math
