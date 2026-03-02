// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/Vector/Vector2.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Math/Vector/Vector4.hpp"

namespace GP
{

template <CFloatingPoint T>
constexpr TVector4<T>::TVector4() noexcept
    : x(static_cast<T>(0))
    , y(static_cast<T>(0))
    , z(static_cast<T>(0))
    , w(static_cast<T>(0))
{}

template <CFloatingPoint T>
constexpr TVector4<T>::TVector4(const T inX, const T inY, const T inZ, const T inW) noexcept
    : x(inX)
    , y(inY)
    , z(inZ)
    , w(inW)
{}

template <CFloatingPoint T>
constexpr TVector4<T>::TVector4(const T* arr) noexcept
    : x(arr[0])
    , y(arr[1])
    , z(arr[2])
    , w(arr[3])
{}

template <CFloatingPoint T>
constexpr TVector4<T>::TVector4(const T (&arr)[4]) noexcept
    : x(arr[0])
    , y(arr[1])
    , z(arr[2])
    , w(arr[3])
{}

template <CFloatingPoint T>
constexpr TVector4<T>::TVector4(const TVector2<T>& vec2, const T inZ, const T inW) noexcept
    : x(vec2.x)
    , y(vec2.y)
    , z(inZ)
    , w(inW)
{}

template <CFloatingPoint T>
constexpr TVector4<T>::TVector4(const TVector3<T>& vec3, const T inW) noexcept
    : x(vec3.x)
    , y(vec3.y)
    , z(vec3.z)
    , w(inW)
{}

}   // namespace GP
