// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/MathFwd.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief
/// @tparam T
template <CIntegral T>
struct TIntPoint2D
{
public:
    union
    {
        struct
        {
            T x;   //<! X coordinate of the point.
            T y;   //<! Y coordinate of the point.
        };

        T data[2];   //<! Array access to the point's coordinates.
    };

public:
    /// @brief Default constructor initializes point to (0, 0).
    constexpr TIntPoint2D() noexcept;

    /// @brief Constructs a point with the given coordinates.
    /// @param inX X coordinate of the point.
    /// @param inY Y coordinate of the point.
    constexpr TIntPoint2D(const T inX, const T inY) noexcept;

    /// @brief Constructs a point from an array of 2 elements, where data[0] is x and data[1] is y.
    /// @param arr Pointer to an array of 2 elements representing the point's coordinates.
    /// @warning The caller must ensure that `arr` points to at least 2 valid elements. No bounds checking is performed.
    constexpr TIntPoint2D(const T* arr) noexcept;

    /// @brief Constructs a point from a reference to a C-style array of 2 elements, where arr[0] is x and arr[1] is y.
    /// @param arr Reference to a C-style array of 2 elements representing the point's coordinates.
    /// @warning The caller must ensure that `arr` has at least 2 elements. No bounds checking is performed.
    constexpr TIntPoint2D(const T (&arr)[2]) noexcept;

    /// @brief Converts a point of one integral type to another, e.g. TIntPoint2D<int> to TIntPoint2D<long>.
    /// @tparam U Source point's integral type, which must be convertible to T.
    /// @param other Point to convert from.
    template <CIntegral U>
    requires TIsConvertible_V<U, T> constexpr explicit TIntPoint2D(const TIntPoint2D<U>& other) noexcept;

    /// @brief Copy constructor.
    /// @param extent Point to copy from.
    constexpr TIntPoint2D(const TIntPoint2D& extent) noexcept = default;

    /// @brief Move constructor.
    /// @param extent Point to move from.
    constexpr TIntPoint2D(TIntPoint2D&& extent) noexcept = default;

public:
    /// @brief Copy assignment operator.
    /// @param other Point to copy from.
    /// @return Reference to the assigned point.
    constexpr TIntPoint2D& operator=(const TIntPoint2D& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param other Point to move from.
    /// @return Reference to the assigned point.
    constexpr TIntPoint2D& operator=(TIntPoint2D&& other) noexcept = default;

    /// @brief Equality comparison operator.
    /// @param other Point to compare with.
    /// @return true if both points have the same coordinates, false otherwise.
    GP_NODISCARD constexpr bool operator==(const TIntPoint2D& other) const noexcept;

    /// @brief Inequality comparison operator.
    /// @param other Point to compare with.
    /// @return true if the points have different coordinates, false if they are equal.
    GP_NODISCARD constexpr bool operator!=(const TIntPoint2D& other) const noexcept;

    /// @brief Unary negation operator, which negates both coordinates of the point.
    /// @return A new point with both coordinates negated.
    GP_NODISCARD constexpr TIntPoint2D operator-() const noexcept;

    /// @brief Addition operator, which adds the corresponding coordinates of two points.
    /// @param other Point to add.
    /// @return A new point resulting from the coordinate-wise addition of the two points.
    GP_NODISCARD constexpr TIntPoint2D operator+(const TIntPoint2D& other) const noexcept;

    /// @brief Subtraction operator, which subtracts the corresponding coordinates of two points.
    /// @param other Point to subtract.
    /// @return A new point resulting from the coordinate-wise subtraction of the two points.
    GP_NODISCARD constexpr TIntPoint2D operator-(const TIntPoint2D& other) const noexcept;
};

}   // namespace GP

#include "Math/Vector/IntPoint2D.inl"
