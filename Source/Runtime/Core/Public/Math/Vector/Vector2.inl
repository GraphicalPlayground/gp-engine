// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/Vector/Vector2.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Math/Vector/Vector4.hpp"

namespace GP
{

template <CFloatingPoint T>
constexpr TVector2<T>::TVector2() noexcept
    : x(static_cast<T>(0))
    , y(static_cast<T>(0))
{}

template <CFloatingPoint T>
constexpr TVector2<T>::TVector2(const T inX, const T inY) noexcept
    : x(inX)
    , y(inY)
{}

template <CFloatingPoint T>
constexpr TVector2<T>::TVector2(const T* arr) noexcept
    : x(arr[0])
    , y(arr[1])
{}

template <CFloatingPoint T>
constexpr TVector2<T>::TVector2(const T (&arr)[2]) noexcept
    : x(arr[0])
    , y(arr[1])
{}

template <CFloatingPoint T>
constexpr TVector2<T>::TVector2(const TVector3<T>& vec3) noexcept
    : x(vec3.x)
    , y(vec3.y)
{}

template <CFloatingPoint T>
constexpr TVector2<T>::TVector2(const TVector4<T>& vec4) noexcept
    : x(vec4.x)
    , y(vec4.y)
{}

template <CFloatingPoint T>
GP_NODISCARD constexpr bool TVector2<T>::operator==(const TVector2& other) const noexcept
{
    return x == other.x && y == other.y;
}

template <CFloatingPoint T>
GP_NODISCARD constexpr bool TVector2<T>::operator!=(const TVector2& other) const noexcept
{
    return !(*this == other);
}

}   // namespace GP
