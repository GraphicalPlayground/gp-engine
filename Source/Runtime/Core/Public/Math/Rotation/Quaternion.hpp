// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief A quaternion representing a rotation in 3D space. Quaternions are used to avoid gimbal lock and provide
/// smooth interpolation between rotations.
/// \tparam T The floating-point type used for the quaternion components (e.g., float, double).
template <Concepts::IsFloatingPoint T>
struct TQuaternion
{
public:
    union
    {
        struct
        {
            T x, y, z, w;   //<! Components of the quaternion.
        };

        T data[4];   //<! Array access to components.
    };
};

}   // namespace GP::Math
