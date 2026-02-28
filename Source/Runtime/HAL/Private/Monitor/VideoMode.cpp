// Copyright (c) - Graphical Playground. All rights reserved.

#include "Monitor/VideoMode.hpp"

std::ostream& operator<<(std::ostream& os, const GP::HAL::FVideoMode& videoMode)
{
    os << videoMode.width << "x" << videoMode.height << " @ " << videoMode.refreshRateHz << "Hz ("
       << static_cast<int>(videoMode.redBits) + static_cast<int>(videoMode.greenBits) +
              static_cast<int>(videoMode.blueBits)
       << " bpp)";
    return os;
}
