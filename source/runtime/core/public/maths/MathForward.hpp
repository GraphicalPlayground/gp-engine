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

template <concepts::IsFloatingPoint T> struct Quaternion;

// clang-format on

}   // namespace gp::math
