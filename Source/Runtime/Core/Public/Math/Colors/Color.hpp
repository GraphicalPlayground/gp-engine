// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

namespace GP::Math
{

/// \brief A simple RGBA color struct with 32-bit float components in the range [0.0, 1.0].
struct FColor
{
public:
    union
    {
        struct
        {
            Float32 r, g, b, a;   //<! Red, Green, Blue, Alpha components in the range [0.0, 1.0].
        };

        Float32 rgba[4];   //<! Array access to color components (rgba[0] = r, rgba[1] = g, etc.).
    };
};

}   // namespace GP::Math
