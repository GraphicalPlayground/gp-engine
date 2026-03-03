// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Utility/Enums.hpp"

namespace GP
{

/// @brief Defines the flashing behavior of a window when requesting user attention.
enum class EWindowFlashMode : UInt8
{
    /// @brief Stop flashing the window.
    None,

    /// @brief Flash the window until it receives focus.
    UntilFocused,

    /// @brief Flash the window for a short period of time.
    Briefly,
};

/// @brief Defines the primary display mode of the window.
enum class EWindowMode : UInt8
{
    /// @brief Standard window with borders and title bar.
    Windowed,

    /// @brief Windowed mode without borders, typically matching display resolution.
    Borderless,

    /// @brief Exclusive hardware control of the display (Legacy/Performance).
    Fullscreen,

    /// @brief Borderless window that spans across all available monitors.
    FullscreenDesktop
};

/// @brief Defines the current runtime state or "visibility" of the window.
enum class EWindowState : UInt8
{
    /// @brief The window is visible and rendered normally.
    Normal,

    /// @brief The window is minimized to the taskbar/dock and likely not rendering.
    Minimized,

    /// @brief The window is expanded to fill the workspace.
    Maximized,

    /// @brief The window is hidden from the user and taskbar.
    Hidden,

    /// @brief The window is visible but currently being occluded by another application.
    Occluded
};

/// @brief Bitmask defining the aesthetic and functional properties of the window.
enum class EWindowStyle : UInt32
{
    None = 0,

    /// @brief The window has a visible title bar.
    TitleBar = 1 << 0,

    /// @brief The window has a system menu / close button.
    Closable = 1 << 1,

    /// @brief The user can drag the edges to resize the window.
    Resizable = 1 << 2,

    /// @brief The window has minimize and maximize buttons.
    MinMaxButtons = 1 << 3,

    /// @brief The window is always rendered on top of other non-topmost windows.
    AlwaysOnTop = 1 << 4,

    /// @brief The window's background can be partially or fully transparent.
    Transparent = 1 << 5,

    /// @brief Useful for splash screens or tooltips; window doesn't appear in taskbar.
    NoTaskbar = 1 << 6,

    /// @brief A standard decorative window.
    Default = TitleBar | Closable | Resizable | MinMaxButtons
};

GP_ENABLE_ENUM_BITWISE_OPERATIONS(EWindowStyle);

}   // namespace GP
