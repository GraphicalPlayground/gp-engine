// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "HALBuild.hpp"
#include "Templates/Enums.hpp"

namespace GP::HAL
{

/// \brief Enumeration for different window modes.
enum class EWindowMode : UInt8
{
    Windowed = 0,         //<! Normal windowed mode
    Borderless,           //<! Borderless window
    Fullscreen,           //<! Exclusive fullscreen
    FullscreenWindowed,   //<! Borderless fullscreen (fake fullscreen)
};

/// \brief Enumeration for different window states.
enum class EWindowState : UInt8
{
    Normal = 0,   //<! Normal state
    Minimized,    //<! Minimized/iconified
    Maximized,    //<! Maximized
    Hidden,       //<! Hidden
};

/// \brief Enumeration for different window styles using bit flags.
enum class EWindowStyle : UInt32
{
    None = 0,
    Resizable = 1 << 0,                //<! Window can be resized
    Decorated = 1 << 1,                //<! Window has title bar and borders
    Floating = 1 << 2,                 //<! Window stays on top
    Focused = 1 << 3,                  //<! Window is focused on creation
    Maximized = 1 << 4,                //<! Window is maximized on creation
    Minimized = 1 << 5,                //<! Window is minimized on creation
    Visible = 1 << 6,                  //<! Window is visible on creation
    TransparentFramebuffer = 1 << 7,   //<! Framebuffer is transparent

    // Combined presets
    Default = Resizable | Decorated | Focused | Visible,
    Borderless = Visible,
};

/// \brief Enumeration for different cursor modes.
enum class ECursorMode : UInt8
{
    Normal = 0,   //<! Cursor is visible and behaves normally
    Hidden,       //<! Cursor is hidden when over window
    Disabled,     //<! Cursor is hidden and locked to window
    Captured,     //<! Cursor is captured but visible
};

/// \brief Enumeration for different cursor shapes.
enum class ECursorShape : UInt8
{
    Arrow = 0,    //<! Standard arrow cursor
    IBeam,        //<! Text input I-beam
    Crosshair,    //<! Crosshair
    Hand,         //<! Pointing hand
    ResizeEW,     //<! Horizontal resize arrows
    ResizeNS,     //<! Vertical resize arrows
    ResizeNWSE,   //<! Diagonal resize (NW-SE)
    ResizeNESW,   //<! Diagonal resize (NE-SW)
    ResizeAll,    //<! Move/resize all directions
    NotAllowed,   //<! Operation not allowed
};

}   // namespace GP::HAL

GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::HAL::EWindowStyle);
