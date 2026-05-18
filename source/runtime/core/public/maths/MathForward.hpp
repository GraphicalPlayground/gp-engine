// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::math
{

// clang-format off

template <concepts::IsFloatingPoint T> struct Vector2;
template <concepts::IsFloatingPoint T> struct Vector3;
template <concepts::IsFloatingPoint T> struct Vector4;

template <concepts::IsFloatingPoint T> struct Matrix2x2;
template <concepts::IsFloatingPoint T> struct Matrix3x3;
template <concepts::IsFloatingPoint T> struct Matrix3x4;
template <concepts::IsFloatingPoint T> struct Matrix4x4;

template <concepts::IsFloatingPoint T> struct Rotator;
template <concepts::IsFloatingPoint T> struct Rotator2D;
template <concepts::IsFloatingPoint T> struct Quaternion;
template <concepts::IsFloatingPoint T> struct DualQuaternion;

template <concepts::IsFloatingPoint T> struct Frustum;
template <concepts::IsFloatingPoint T> struct Plane;
template <concepts::IsFloatingPoint T> struct Ray;
template <concepts::IsFloatingPoint T> struct Sphere;
template <concepts::IsFloatingPoint T> struct Triangle;

template <concepts::IsFloatingPoint T> struct Transform;
template <concepts::IsFloatingPoint T> struct Projection;
template <concepts::IsFloatingPoint T> struct View;

// clang-format on

}   // namespace gp::math
