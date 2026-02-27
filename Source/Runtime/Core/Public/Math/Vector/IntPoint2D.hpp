// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief Represents a 2D point with integral components.
/// \tparam T The type of the components, which must be an integral type.
template <Concepts::IsIntegral T>
struct TIntPoint2D
{
public:
    union
    {
        struct
        {
            T x;   //<! X component of the point.
            T y;   //<! Y component of the point.
        };

        T data[2];   //<! Array access to components.
    };
};

}   // namespace GP::Math
