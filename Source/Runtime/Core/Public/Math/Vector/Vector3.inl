// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Math/Utils/Linear.hpp"
#include "Math/Vector/Vector2.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Math/Vector/Vector4.hpp"
#include <limits>

namespace GP::Math
{

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Zero() noexcept
{
    return { static_cast<T>(0), static_cast<T>(0), static_cast<T>(0) };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::One() noexcept
{
    return { static_cast<T>(1), static_cast<T>(1), static_cast<T>(1) };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::UnitX() noexcept
{
    return { static_cast<T>(1), static_cast<T>(0), static_cast<T>(0) };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::UnitY() noexcept
{
    return { static_cast<T>(0), static_cast<T>(1), static_cast<T>(0) };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::UnitZ() noexcept
{
    return { static_cast<T>(0), static_cast<T>(0), static_cast<T>(1) };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::NegUnitX() noexcept
{
    return { static_cast<T>(-1), static_cast<T>(0), static_cast<T>(0) };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::NegUnitY() noexcept
{
    return { static_cast<T>(0), static_cast<T>(-1), static_cast<T>(0) };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::NegUnitZ() noexcept
{
    return { static_cast<T>(0), static_cast<T>(0), static_cast<T>(-1) };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Max() noexcept
{
    return { std::numeric_limits<T>::max(), std::numeric_limits<T>::max(), std::numeric_limits<T>::max() };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Min() noexcept
{
    return { std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest() };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::NaN() noexcept
{
    return { std::numeric_limits<T>::quiet_NaN(),
             std::numeric_limits<T>::quiet_NaN(),
             std::numeric_limits<T>::quiet_NaN() };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Infinity() noexcept
{
    return { std::numeric_limits<T>::infinity(),
             std::numeric_limits<T>::infinity(),
             std::numeric_limits<T>::infinity() };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::NegInfinity() noexcept
{
    return { -std::numeric_limits<T>::infinity(),
             -std::numeric_limits<T>::infinity(),
             -std::numeric_limits<T>::infinity() };
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Left() noexcept
{
    return NegUnitX();
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Right() noexcept
{
    return UnitX();
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Up() noexcept
{
    return UnitY();
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Down() noexcept
{
    return NegUnitY();
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Forward() noexcept
{
    return UnitZ();
}

template <Concepts::IsFloatingPoint T>
static constexpr TVector3<T> TVector3<T>::Backward() noexcept
{
    return NegUnitZ();
}

template <Concepts::IsFloatingPoint T>
constexpr TVector3<T>::TVector3() noexcept
    : x{}
    , y{}
    , z{}
{}

template <Concepts::IsFloatingPoint T>
constexpr TVector3<T>::TVector3(T value) noexcept
    : x{ value }
    , y{ value }
    , z{ value }
{}

template <Concepts::IsFloatingPoint T>
constexpr TVector3<T>::TVector3(T inX, T inY, T inZ) noexcept
    : x{ inX }
    , y{ inY }
    , z{ inZ }
{}

template <Concepts::IsFloatingPoint T>
constexpr TVector3<T>::TVector3(const TVector2<T>& inXY, T inZ) noexcept
    : x{ inXY.x }
    , y{ inXY.y }
    , z{ inZ }
{}

template <Concepts::IsFloatingPoint T>
constexpr TVector3<T>::TVector3(const TVector4<T>& inXYZW) noexcept
    : x{ inXYZW.x }
    , y{ inXYZW.y }
    , z{ inXYZW.z }
{}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T& TVector3<T>::operator[](SizeT index) noexcept
{
    GP_ASSUME(index < 3);
    return data[index];
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr const T& TVector3<T>::operator[](SizeT index) const noexcept
{
    GP_ASSUME(index < 3);
    return data[index];
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T TVector3<T>::X() const noexcept
{
    return { x };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T TVector3<T>::Y() const noexcept
{
    return { y };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T TVector3<T>::Z() const noexcept
{
    return { z };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector2<T> TVector3<T>::XY() const noexcept
{
    return { x, y };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector2<T> TVector3<T>::XZ() const noexcept
{
    return { x, z };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector2<T> TVector3<T>::YZ() const noexcept
{
    return { y, z };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector2<T> TVector3<T>::YX() const noexcept
{
    return { y, x };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector2<T> TVector3<T>::ZX() const noexcept
{
    return { z, x };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector2<T> TVector3<T>::ZY() const noexcept
{
    return { z, y };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector3<T> TVector3<T>::XYZ() const noexcept
{
    return { x, y, z };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector3<T> TVector3<T>::XZY() const noexcept
{
    return { x, z, y };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector3<T> TVector3<T>::YXZ() const noexcept
{
    return { y, x, z };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector3<T> TVector3<T>::YZX() const noexcept
{
    return { y, z, x };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector3<T> TVector3<T>::ZXY() const noexcept
{
    return { z, x, y };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector3<T> TVector3<T>::ZYX() const noexcept
{
    return { z, y, x };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T TVector3<T>::Dot(const TVector3<T>& other) const noexcept
{
    return x * other.x + y * other.y + z * other.z;
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr TVector3<T> TVector3<T>::Cross(const TVector3<T>& other) const noexcept
{
    return { y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x };
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T TVector3<T>::LengthSquared() const noexcept
{
    return x * x + y * y + z * z;
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr T TVector3<T>::Length() const noexcept
{
    return Math::Sqrt(LengthSquared());
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr bool TVector3<T>::IsZero(T tolerance) const noexcept
{
    return Math::Abs(x) <= tolerance && Math::Abs(y) <= tolerance && Math::Abs(z) <= tolerance;
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr bool TVector3<T>::IsNormalized(T tolerance) const noexcept
{
    return Math::Abs(LengthSquared() - static_cast<T>(1)) <= tolerance;
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr bool TVector3<T>::IsFinite() const noexcept
{
    return Math::IsFinite(x) && Math::IsFinite(y) && Math::IsFinite(z);
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr bool TVector3<T>::IsInfinite() const noexcept
{
    return Math::IsInfinite(x) || Math::IsInfinite(y) || Math::IsInfinite(z);
}

template <Concepts::IsFloatingPoint T>
GP_NODISCARD constexpr bool TVector3<T>::IsNaN() const noexcept
{
    return Math::IsNaN(x) || Math::IsNaN(y) || Math::IsNaN(z);
}

}   // namespace GP::Math

namespace GP::Math
{

template <Concepts::IsFloatingPoint T>
constexpr bool IsFinite(const TVector3<T>& value) noexcept
{
    return value.IsFinite();
}

template <Concepts::IsFloatingPoint T>
constexpr bool IsInfinite(const TVector3<T>& value) noexcept
{
    return value.IsInfinite();
}

template <Concepts::IsFloatingPoint T>
constexpr bool IsNaN(const TVector3<T>& value) noexcept
{
    return value.IsNaN();
}

}   // namespace GP::Math
