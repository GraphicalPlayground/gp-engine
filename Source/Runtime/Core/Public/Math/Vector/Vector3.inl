// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/Vector/Vector2.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Math/Vector/Vector4.hpp"

namespace GP
{

template <CFloatingPoint T>
constexpr TVector3<T>::TVector3() noexcept
    : x(static_cast<T>(0))
    , y(static_cast<T>(0))
    , z(static_cast<T>(0))
{}

template <CFloatingPoint T>
constexpr TVector3<T>::TVector3(const T inX, const T inY, const T inZ) noexcept
    : x(inX)
    , y(inY)
    , z(inZ)
{}

template <CFloatingPoint T>
constexpr TVector3<T>::TVector3(const T* arr) noexcept
    : x(arr[0])
    , y(arr[1])
    , z(arr[2])
{}

template <CFloatingPoint T>
constexpr TVector3<T>::TVector3(const T (&arr)[3]) noexcept
    : x(arr[0])
    , y(arr[1])
    , z(arr[2])
{}

template <CFloatingPoint T>
constexpr TVector3<T>::TVector3(const TVector2<T>& vec2, const T inZ) noexcept
    : x(vec2.x)
    , y(vec2.y)
    , z(inZ)
{}

template <CFloatingPoint T>
constexpr TVector3<T>::TVector3(const TVector4<T>& vec4) noexcept
    : x(vec4.x)
    , y(vec4.y)
    , z(vec4.z)
{}

}   // namespace GP
