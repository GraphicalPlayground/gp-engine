// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

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

using Vec2f = Vec2<gp::Float32>;
using Vec3f = Vec3<gp::Float32>;
using Vec4f = Vec4<gp::Float32>;

using Vec2d = Vec2<gp::Float64>;
using Vec3d = Vec3<gp::Float64>;
using Vec4d = Vec4<gp::Float64>;

using Mat2f = Mat2<gp::Float32>;
using Mat3f = Mat3<gp::Float32>;
using Mat34f = Mat34<gp::Float32>;
using Mat4f = Mat4<gp::Float32>;

using Mat2d = Mat2<gp::Float64>;
using Mat3d = Mat3<gp::Float64>;
using Mat34d = Mat34<gp::Float64>;
using Mat4d = Mat4<gp::Float64>;

using Quatf = Quat<gp::Float32>;
using Quatd = Quat<gp::Float64>;
using DualQuatf = DualQuat<gp::Float32>;
using DualQuatd = DualQuat<gp::Float64>;
using Rotatorf = Rotator<gp::Float32>;
using Rotatord = Rotator<gp::Float64>;
using Rotator2f = Rotator2<gp::Float32>;
using Rotator2d = Rotator2<gp::Float64>;

using Transformf = Transform<gp::Float32>;
using Transformd = Transform<gp::Float64>;

}   // namespace gp::math
