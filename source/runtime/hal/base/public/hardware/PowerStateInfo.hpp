// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief The type of power source.
enum class PowerSource : UInt8
{
    Unknown = 0,
    AC,       // Connected to mains power
    Battery   // Running on battery
};

/// @brief Information about the power state.
struct PowerStateInfo
{
    /// @brief The current power source.
    PowerSource source{ PowerSource::AC };

    /// @brief The current battery level as a percentage (0-100).
    /// @note Only valid if the power source is Battery.
    float batteryLifePercent{ 100.0f };

    /// @brief The estimated remaining battery life in seconds.
    bool isInLowPowerMode{ false };
};

}   // namespace gp::hal
