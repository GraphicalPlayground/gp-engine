// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreMinimal.hpp"
#include <concepts>

namespace gp::math
{

template <std::floating_point T>
struct Vec2;
template <std::floating_point T>
struct Vec3;
template <std::floating_point T>
struct Vec4;

template <std::floating_point T>
struct Mat2;
template <std::floating_point T>
struct Mat3;
template <std::floating_point T>
struct Mat34;
template <std::floating_point T>
struct Mat4;

template <std::floating_point T>
struct Quat;
template <std::floating_point T>
struct DualQuat;
template <std::floating_point T>
struct Rotator;
template <std::floating_point T>
struct Rotator2;

template <std::floating_point T>
struct Transform;

} // namespace gp::math
