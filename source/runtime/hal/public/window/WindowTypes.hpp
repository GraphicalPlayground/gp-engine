// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Simple structure representing a 2D point with integer coordinates, commonly used for window positions and
///        sizes.
struct Point2D
{
public:
    Int32 x;
    Int32 y;

public:
    /// @brief Default constructor initializes point to (0, 0).
    constexpr Point2D() noexcept
        : x(0)
        , y(0)
    {}

    /// @brief Constructs a point with the given coordinates.
    /// @param[in] inX X coordinate of the point.
    /// @param[in] inY Y coordinate of the point.
    constexpr Point2D(const Int32 inX, const Int32 inY) noexcept
        : x(inX)
        , y(inY)
    {}

    /// @brief Constructs a point from an array of 2 elements, where data[0] is x and data[1] is y.
    /// @param[in] arr Pointer to an array of 2 elements representing the point's coordinates.
    /// @warning The caller must ensure that `arr` points to at least 2 valid elements. No bounds checking is performed.
    constexpr Point2D(const Int32* arr) noexcept
        : x(arr[0])
        , y(arr[1])
    {}

    /// @brief Constructs a point from a reference to a C-style array of 2 elements, where arr[0] is x and arr[1] is y.
    /// @param[in] arr Reference to a C-style array of 2 elements representing the point's coordinates.
    /// @warning The caller must ensure that `arr` has at least 2 elements. No bounds checking is performed.
    constexpr Point2D(const Int32 (&arr)[2]) noexcept
        : x(arr[0])
        , y(arr[1])
    {}

    /// @brief Copy constructor.
    /// @param[in] extent Point to copy from.
    constexpr Point2D(const Point2D& extent) noexcept = default;

    /// @brief Move constructor.
    /// @param[in] extent Point to move from.
    constexpr Point2D(Point2D&& extent) noexcept = default;

public:
    /// @brief Copy assignment operator.
    /// @param[in] other Point to copy from.
    /// @return Reference to the assigned point.
    constexpr Point2D& operator=(const Point2D& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param[in] other Point to move from.
    /// @return Reference to the assigned point.
    constexpr Point2D& operator=(Point2D&& other) noexcept = default;

    /// @brief Equality comparison operator.
    /// @param[in] other Point to compare with.
    /// @return true if both points have the same coordinates, false otherwise.
    GP_NODISCARD constexpr bool operator==(const Point2D& other) const noexcept { return x == other.x && y == other.y; }

    /// @brief Inequality comparison operator.
    /// @param[in] other Point to compare with.
    /// @return true if the points have different coordinates, false if they are equal.
    GP_NODISCARD constexpr bool operator!=(const Point2D& other) const noexcept { return !(*this == other); }

    /// @brief Unary negation operator, which negates both coordinates of the point.
    /// @return A new point with both coordinates negated.
    GP_NODISCARD constexpr Point2D operator-() const noexcept { return Point2D(-x, -y); }

    /// @brief Addition operator, which adds the corresponding coordinates of two points.
    /// @param[in] other Point to add.
    /// @return A new point resulting from the coordinate-wise addition of the two points.
    GP_NODISCARD constexpr Point2D operator+(const Point2D& other) const noexcept
    {
        return Point2D(x + other.x, y + other.y);
    }

    /// @brief Subtraction operator, which subtracts the corresponding coordinates of two points.
    /// @param[in] other Point to subtract.
    /// @return A new point resulting from the coordinate-wise subtraction of the two points.
    GP_NODISCARD constexpr Point2D operator-(const Point2D& other) const noexcept
    {
        return Point2D(x - other.x, y - other.y);
    }
};

/// @brief Simple structure representing a 2D extent (width and height) with unsigned integer values, commonly used for
///        window sizes and dimension constraints.
struct Extent2D
{
public:
    UInt32 width;
    UInt32 height;

public:
    /// @brief Default constructor initializes extent to (0, 0).
    constexpr Extent2D() noexcept
        : width(0)
        , height(0)
    {}

    /// @brief Constructs an extent with the given dimensions.
    /// @param[in] inWidth Width of the extent.
    /// @param[in] inHeight Height of the extent.
    constexpr Extent2D(const UInt32 inWidth, const UInt32 inHeight) noexcept
        : width(inWidth)
        , height(inHeight)
    {}

    /// @brief Constructs an extent from an array of 2 elements, where data[0] is width and data[1] is height.
    /// @param[in] arr Pointer to an array of 2 elements representing the extent's dimensions.
    /// @warning The caller must ensure that `arr` points to at least 2 valid elements. No bounds checking is performed.
    constexpr Extent2D(const UInt32* arr) noexcept
        : width(arr[0])
        , height(arr[1])
    {}

    /// @brief Constructs an extent from a reference to a C-style array of 2 elements, where arr[0] is width and arr[1]
    ///        is height.
    /// @param[in] arr Reference to a C-style array of 2 elements representing the extent's dimensions.
    /// @warning The caller must ensure that `arr` has at least 2 elements. No bounds checking is performed.
    constexpr Extent2D(const UInt32 (&arr)[2]) noexcept
        : width(arr[0])
        , height(arr[1])
    {}

    /// @brief Copy constructor.
    /// @param[in] extent Extent to copy from.
    constexpr Extent2D(const Extent2D& extent) noexcept = default;

    /// @brief Move constructor.
    /// @param[in] extent Extent to move from.
    constexpr Extent2D(Extent2D&& extent) noexcept = default;

public:
    /// @brief Copy assignment operator.
    /// @param[in] other Source extent to copy from.
    /// @return Reference to this.
    constexpr Extent2D& operator=(const Extent2D& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param[in] other Source extent to move from.
    /// @return Reference to this.
    constexpr Extent2D& operator=(Extent2D&& other) noexcept = default;

    /// @brief Equality operator.
    /// @param[in] other Source extent to compare with.
    /// @return True if this extent is equal to the other, false otherwise.
    GP_NODISCARD constexpr bool operator==(const Extent2D& other) const noexcept
    {
        return width == other.width && height == other.height;
    }

    /// @brief Inequality operator.
    /// @param[in] other Source extent to compare with.
    /// @return True if this extent is not equal to the other, false otherwise.
    GP_NODISCARD constexpr bool operator!=(const Extent2D& other) const noexcept { return !(*this == other); }
};

}   // namespace gp::hal
