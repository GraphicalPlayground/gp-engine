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
struct TRay
{
public:
    TVector3<T> origin;      //<! The origin point of the ray.
    TVector3<T> direction;   //<! The direction vector of the ray (should be normalized).
};

}   // namespace GP::Math
