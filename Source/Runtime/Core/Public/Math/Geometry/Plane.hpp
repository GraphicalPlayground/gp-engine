// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief
/// \tparam T
template <Concepts::IsFloatingPoint T>
struct TPlane
{
public:
    TVector3<T> normal;   //<! The normal vector of the plane.
    T distance;           //<! The distance from the origin to the plane along the normal vector.
};

}   // namespace GP::Math
