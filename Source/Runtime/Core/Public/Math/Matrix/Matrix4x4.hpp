// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

template <Concepts::IsFloatingPoint T>
struct GP_ALIGN(16) TMatrix4x4
{
public:
    union
    {
        struct
        {
            T m00, m01, m02, m03;   //<! Row 0
            T m10, m11, m12, m13;   //<! Row 1
            T m20, m21, m22, m23;   //<! Row 2
            T m30, m31, m32, m33;   //<! Row 3
        };

        T data[16];   //<! Array access to components.
    };
};

}   // namespace GP::Math
