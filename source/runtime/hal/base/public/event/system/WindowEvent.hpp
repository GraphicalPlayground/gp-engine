// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "window/WindowEnums.hpp"
#include "window/WindowTypes.hpp"

namespace gp::hal::event
{

/// @brief Defines state transitions and geometry changes for an OS window instance.
enum class WindowAction : gp::UInt8
{
    None = 0,
    Shown,
    Hidden,
    Exposed,            // Window region invalidated and requires redraw
    Moved,              // Window position changed (populates position: gp::hal::Point2D)
    Resized,            // Logical window client area changed (populates extent: gp::hal::Extent2D)
    PixelSizeChanged,   // Backbuffer pixel resolution changed (populates extent: gp::hal::Extent2D)
    StateChanged,       // Minimized / Maximized / Restored / Occluded (populates state: gp::hal::WindowState)
    ModeChanged,        // Windowed / Borderless / Fullscreen (populates mode: gp::hal::WindowMode)
    MouseEntered,       // Cursor entered client area
    MouseLeft,          // Cursor left client area
    FocusGained,        // Window gained keyboard focus
    FocusLost,          // Window lost keyboard focus
    CloseRequested,     // User clicked the window's 'X' button
    DisplayChanged,     // Window was dragged to a different monitor (populates display.displayId)
    DPIChanged,         // Window DPI scale factor changed (populates dpi)
    HDRStateChanged     // Window HDR output capability toggled
};

/// @brief Fixed-size POD payload for EventCategory::Window (16 bytes, zero implicit padding).
struct WindowEvent
{
    gp::UInt32 windowId = 0u;   // Engine/Backend unique window identifier
    WindowAction action = WindowAction::None;
    gp::hal::WindowState state = gp::hal::WindowState::Normal;
    gp::hal::WindowMode mode = gp::hal::WindowMode::Windowed;
    gp::UInt8 isHDREnabled = 0u;

    union
    {
        gp::hal::Point2D position{};   // Signed screen coordinates (Moved)
        gp::hal::Extent2D extent;      // Unsigned client dimensions (Resized, PixelSizeChanged)

        struct
        {
            gp::Float32 scaleX;   // DPI scaling factors (DPIChanged)
            gp::Float32 scaleY;
        } dpi;

        struct
        {
            gp::UInt32 displayId;   // Target display index (DisplayChanged)
            gp::UInt32 reservedData;
        } display;
    };
};

static_assert(sizeof(WindowEvent) == 16, "WindowEvent size mismatch!");

}   // namespace gp::hal::event
