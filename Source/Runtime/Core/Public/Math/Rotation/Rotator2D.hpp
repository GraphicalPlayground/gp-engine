// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief A simple rotator representing a rotation in 2D space. It consists of a single angle component, which defines
/// the rotation around the origin in degrees.
/// \tparam T The floating-point type used for the angle component (e.g., float, double).
template <Concepts::IsFloatingPoint T>
struct TRotator2D
{
public:
    T angle;   //<! The angle of rotation in degrees.
};

}   // namespace GP::Math
