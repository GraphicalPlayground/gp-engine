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
struct TAABB
{
public:
    TVector3<T> min;   //<! The minimum corner of the AABB (the corner with the smallest x, y, and z values).
    TVector3<T> max;   //<! The maximum corner of the AABB (the corner with the largest x, y, and z values).
};

}   // namespace GP::Math
