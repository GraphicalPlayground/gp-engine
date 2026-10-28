// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "window/WindowTypes.hpp"

namespace gp::hal::event
{

/// @brief Defines hardware monitor topology and configuration changes.
enum class DisplayAction : gp::UInt8
{
    None = 0,
    Connected,
    Disconnected,
    Moved,                 // Monitor arrangement changed in OS display settings (populates position)
    Resized,               // Monitor resolution changed (populates extent)
    OrientationChanged,    // Landscape / Portrait rotation
    ContentScaleChanged,   // Desktop DPI scaling modified
    HDRStateChanged        // OS HDR toggle switched for this monitor
};

/// @brief Physical rotation state of a display panel.
enum class DisplayOrientation : gp::UInt8
{
    Unknown = 0,
    Landscape,
    LandscapeFlipped,
    Portrait,
    PortraitFlipped
};

/// @brief Fixed-size POD payload for EventCategory::Display (20 bytes, zero implicit padding).
struct DisplayEvent
{
    gp::UInt32 displayId = 0u;
    gp::Float32 contentScale = 1.0f;
    DisplayAction action = DisplayAction::None;
    DisplayOrientation orientation = DisplayOrientation::Unknown;
    gp::UInt8 isHDREnabled = 0u;
    gp::UInt8 reserved = 0u;

    union
    {
        gp::hal::Point2D position{};   // Desktop virtual origin (Moved)
        gp::hal::Extent2D extent;      // Display pixel resolution (Connected, Resized)
    };
};

static_assert(sizeof(DisplayEvent) == 20, "DisplayEvent size mismatch!");

}   // namespace gp::hal::event
