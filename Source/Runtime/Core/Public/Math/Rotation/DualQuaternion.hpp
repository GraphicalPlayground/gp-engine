// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Math/Rotation/Quaternion.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief A dual quaternion representing a combined rotation and translation in 3D space. Dual quaternions are used for
/// efficient representation and interpolation of rigid body transformations.
/// \tparam T The floating-point type used for the dual quaternion components (e.g., float, double).
template <Concepts::IsFloatingPoint T>
struct TDualQuaternion
{
public:
    TQuaternion<T> real;   //<! The real part of the dual quaternion, representing the rotation component.
    TQuaternion<T> dual;   //<! The dual part of the dual quaternion, representing the translation component.
};

}   // namespace GP::Math
