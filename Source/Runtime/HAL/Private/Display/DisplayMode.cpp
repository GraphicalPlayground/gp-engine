// Copyright (c) - Graphical Playground. All rights reserved.

#include "Display/DisplayMode.hpp"

namespace GP
{

std::ostream& operator<<(std::ostream& os, const FDisplayMode& mode)
{
    os << mode.width << "x" << mode.height << " @ " << mode.refreshRate << " Hz, " << mode.bitsPerPixel << " bpp";
    return os;
}

}   // namespace GP
