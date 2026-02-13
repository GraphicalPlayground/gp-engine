// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

namespace GP::Math
{

/// \brief A simple RGBA color struct with 8-bit unsigned integer components in the range [0, 255].
struct FLinearColor
{
public:
    union
    {
        struct
        {
            UInt8 r, g, b, a;   //<! Red, Green, Blue, Alpha components in the range [0, 255].
        };

        UInt32 rgba;   //<! Packed RGBA value (r in lowest byte, then g, b, a in higher bytes).
    };
};

}   // namespace GP::Math
