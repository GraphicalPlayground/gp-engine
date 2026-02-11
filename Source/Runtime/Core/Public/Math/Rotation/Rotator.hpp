// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief A structure representing a rotation in 3D space, defined by three angles: pitch, yaw, and roll.
/// \tparam T The type of the angles, typically a floating-point type such as float or double.
template <Concepts::IsFloatingPoint T>
struct TRotator
{
public:
    union
    {
        struct
        {
            T pitch, yaw, roll;   //<! The three components of the rotator.
        };

        T data[3];   //<! Array access to components.
    };
};

}   // namespace GP::Math
