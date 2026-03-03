// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/MathFwd.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief
/// @tparam T
template <CIntegral T>
struct TIntExtent2D
{
public:
    union
    {
        struct
        {
            T width;    //<! Width of the extent.
            T height;   //<! Height of the extent.
        };

        T data[2];   //<! Array access to the extent's dimensions.
    };

public:
    /// @brief Default constructor initializes extent to (0, 0).
    constexpr TIntExtent2D() noexcept;

    /// @brief Constructs an extent with the given dimensions.
    /// @param inWidth Width of the extent.
    /// @param inHeight Height of the extent.
    constexpr TIntExtent2D(const T inWidth, const T inHeight) noexcept;

    /// @brief Constructs an extent from an array of 2 elements, where data[0] is width and data[1] is height.
    /// @param arr Pointer to an array of 2 elements representing the extent's dimensions.
    /// @warning The caller must ensure that `arr` points to at least 2 valid elements. No bounds checking is performed.
    constexpr TIntExtent2D(const T* arr) noexcept;

    /// @brief Constructs an extent from a reference to a C-style array of 2 elements, where arr[0] is width and arr[1]
    ///        is height.
    /// @param arr Reference to a C-style array of 2 elements representing the extent's dimensions.
    /// @warning The caller must ensure that `arr` has at least 2 elements. No bounds checking is performed.
    constexpr TIntExtent2D(const T (&arr)[2]) noexcept;

    /// @brief Converts an extent of one integral type to another, e.g. TIntExtent2D<int> to TIntExtent2D<long>.
    /// @tparam U Source extent's integral type, which must be convertible to T.
    /// @param other Extent to convert from.
    template <CIntegral U>
    requires TIsConvertible_V<U, T> constexpr explicit TIntExtent2D(const TIntExtent2D<U>& other) noexcept;

    /// @brief Copy constructor.
    /// @param extent Extent to copy from.
    constexpr TIntExtent2D(const TIntExtent2D& extent) noexcept = default;

    /// @brief Move constructor.
    /// @param extent Extent to move from.
    constexpr TIntExtent2D(TIntExtent2D&& extent) noexcept = default;

public:
    /// @brief Copy assignment operator.
    /// @param other Source extent to copy from.
    /// @return Reference to this.
    constexpr TIntExtent2D& operator=(const TIntExtent2D& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param other Source extent to move from.
    /// @return Reference to this.
    constexpr TIntExtent2D& operator=(TIntExtent2D&& other) noexcept = default;

    /// @brief Equality operator.
    /// @param other Source extent to compare with.
    /// @return True if this extent is equal to the other, false otherwise.
    GP_NODISCARD constexpr bool operator==(const TIntExtent2D& other) const noexcept;

    /// @brief Inequality operator.
    /// @param other Source extent to compare with.
    /// @return True if this extent is not equal to the other, false otherwise.
    GP_NODISCARD constexpr bool operator!=(const TIntExtent2D& other) const noexcept;
};

}   // namespace GP

#include "Math/Vector/IntExtent2D.inl"
