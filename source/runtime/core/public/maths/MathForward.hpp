// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"                  // IWYU pragma: keep
#include "maths/random/RandomForward.hpp"   // IWYU pragma: export

namespace gp::math
{

// clang-format off

template <concepts::IsFloatingPoint T> struct Vector2;
template <concepts::IsFloatingPoint T> struct Vector3;
template <concepts::IsFloatingPoint T> struct Vector4;

template <concepts::IsFloatingPoint T, int R, int C> class Matrix;

template <concepts::IsFloatingPoint T> using Matrix2x2 = Matrix<T, 2, 2>;
template <concepts::IsFloatingPoint T> using Matrix3x3 = Matrix<T, 3, 3>;
template <concepts::IsFloatingPoint T> using Matrix3x4 = Matrix<T, 3, 4>;
template <concepts::IsFloatingPoint T> using Matrix4x4 = Matrix<T, 4, 4>;

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
