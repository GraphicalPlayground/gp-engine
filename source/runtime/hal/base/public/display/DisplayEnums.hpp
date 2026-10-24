// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "event/system/DisplayEvent.hpp"
#include "templates/Enums.hpp"

namespace gp::hal
{

/// @brief Physical rotation state of a display panel (shared with DisplayEvent).
using DisplayOrientation = gp::hal::event::DisplayOrientation;

/// @brief Hardware scanout pixel format of a display mode.
enum class DisplayPixelFormat : gp::UInt8
{
    Unknown = 0,
    RGB888,
    RGBX8888,
    BGRX8888,
    RGBA8888,
    BGRA8888,
    RGB565,
    RGB101010,
    BGR101010,
    RGBA16Float
};

/// @brief Bitmask describing hardware capabilities and topology attributes of a display.
enum class DisplayFlags : gp::UInt8
{
    None = 0,
    Primary = 1 << 0,              // Primary desktop monitor designated by the OS
    Connected = 1 << 1,            // Monitor is currently plugged in and active
    HDRCapable = 1 << 2,           // Panel hardware supports High Dynamic Range scanout
    HDREnabled = 1 << 3,           // OS HDR toggle is currently enabled on this display
    InternalPanel = 1 << 4,        // Built-in laptop, handheld (SteamDeck), or mobile screen
    VariableRefreshRate = 1 << 5   // Supports G-Sync / FreeSync / VESA Adaptive-Sync
};

}   // namespace gp::hal

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::hal::DisplayFlags);
