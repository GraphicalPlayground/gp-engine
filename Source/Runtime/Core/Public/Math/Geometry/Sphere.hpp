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
struct TSphere
{
public:
    TVector3<T> center;   //<! The center of the sphere.
    T radius;             //<! The radius of the sphere.
};

}   // namespace GP::Math
