// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "HALBuild.hpp"
#include <span>
#include <string_view>


namespace GP::HAL
{

/// \brief Interface for the monitor class.
/// The monitor class is responsible for managing the display and its properties, such as resolution, refresh rate, and
/// color depth. It also provides access to the monitor's capabilities and allows for configuration of display settings.
class IMonitor
{
public:
    virtual ~IMonitor() = default;
};

}   // namespace GP::HAL
