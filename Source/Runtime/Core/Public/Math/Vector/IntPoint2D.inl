// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/Vector/IntPoint2D.hpp"

namespace GP
{

template <CIntegral T>
constexpr TIntPoint2D<T>::TIntPoint2D() noexcept
    : x(static_cast<T>(0))
    , y(static_cast<T>(0))
{}

template <CIntegral T>
constexpr TIntPoint2D<T>::TIntPoint2D(const T inX, const T inY) noexcept
    : x(inX)
    , y(inY)
{}

template <CIntegral T>
constexpr TIntPoint2D<T>::TIntPoint2D(const T* arr) noexcept
    : x(arr[0])
    , y(arr[1])
{}

template <CIntegral T>
constexpr TIntPoint2D<T>::TIntPoint2D(const T (&arr)[2]) noexcept
    : x(arr[0])
    , y(arr[1])
{}

template <CIntegral T>
template <CIntegral U>
requires TIsConvertible_V<U, T> constexpr TIntPoint2D<T>::TIntPoint2D(const TIntPoint2D<U>& other) noexcept
    : x(static_cast<T>(other.x))
    , y(static_cast<T>(other.y))
{}

template <CIntegral T>
GP_NODISCARD constexpr bool TIntPoint2D<T>::operator==(const TIntPoint2D& other) const noexcept
{
    return x == other.x && y == other.y;
}

template <CIntegral T>
GP_NODISCARD constexpr bool TIntPoint2D<T>::operator!=(const TIntPoint2D& other) const noexcept
{
    return !(*this == other);
}

}   // namespace GP
