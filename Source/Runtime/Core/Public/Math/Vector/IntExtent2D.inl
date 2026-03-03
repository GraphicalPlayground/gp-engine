// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/Vector/IntExtent2D.hpp"

namespace GP
{

template <CIntegral T>
constexpr TIntExtent2D<T>::TIntExtent2D() noexcept
    : width(static_cast<T>(0))
    , height(static_cast<T>(0))
{}

template <CIntegral T>
constexpr TIntExtent2D<T>::TIntExtent2D(const T inWidth, const T inHeight) noexcept
    : width(inWidth)
    , height(inHeight)
{}

template <CIntegral T>
constexpr TIntExtent2D<T>::TIntExtent2D(const T* arr) noexcept
    : width(arr[0])
    , height(arr[1])
{}

template <CIntegral T>
constexpr TIntExtent2D<T>::TIntExtent2D(const T (&arr)[2]) noexcept
    : width(arr[0])
    , height(arr[1])
{}

template <CIntegral T>
template <CIntegral U>
requires TIsConvertible_V<U, T> constexpr TIntExtent2D<T>::TIntExtent2D(const TIntExtent2D<U>& other) noexcept
    : width(static_cast<T>(other.width))
    , height(static_cast<T>(other.height))
{}

template <CIntegral T>
GP_NODISCARD constexpr bool TIntExtent2D<T>::operator==(const TIntExtent2D& other) const noexcept
{
    return width == other.width && height == other.height;
}

template <CIntegral T>
GP_NODISCARD constexpr bool TIntExtent2D<T>::operator!=(const TIntExtent2D& other) const noexcept
{
    return !(*this == other);
}

}   // namespace GP
