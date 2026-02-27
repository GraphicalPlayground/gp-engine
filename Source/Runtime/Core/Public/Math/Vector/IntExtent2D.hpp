// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/MathForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Math
{

/// \brief Represents a 2D extent with integral components.
/// \tparam T The type of the components, which must be an integral type.
template <Concepts::IsIntegral T>
struct TIntExtent2D
{
public:
    union
    {
        struct
        {
            T width;    //<! Width component of the extent.
            T height;   //<! Height component of the extent.
        };

        T data[2];   //<! Array access to components.
    };
};

}   // namespace GP::Math
