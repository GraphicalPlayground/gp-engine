// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"

namespace GP
{

/// @brief Defines how the system cursor behaves and interacts with the window.
enum class ECursorMode : UInt8
{
    /// @brief Standard behavior: Cursor is visible and moves freely.
    Normal,

    /// @brief The cursor is hidden when over the window client area but functions normally.
    Hidden,

    /// @brief Cursor is hidden, locked to the window center, and provides unlimited relative motion.
    /// @note Essential for FPS games and 3D camera control.
    Locked,

    /// @brief The cursor is visible but cannot leave the window boundaries.
    /// @note Common in RTS games to allow for edge-scrolling without clicking outside the game.
    Confined
};

/// @brief Defines the visual icon used for the system cursor.
enum class ECursorShape : UInt8
{
    /// @brief The standard OS arrow.
    Arrow,

    /// @brief A text input "I-Beam" cursor.
    IBeam,

    /// @brief A crosshair icon, often used for precision tools.
    Crosshair,

    /// @brief A hand icon, typically used for clicking links or grabbing objects.
    Hand,

    /// @brief Horizontal resize arrows (West-East).
    ResizeEW,

    /// @brief Vertical resize arrows (North-South).
    ResizeNS,

    /// @brief Diagonal resize arrows (NorthWest-SouthEast).
    ResizeNWSE,

    /// @brief Diagonal resize arrows (NorthEast-SouthWest).
    ResizeNESW,

    /// @brief Four-way movement arrows.
    ResizeAll,

    /// @brief A circle with a slash or "blocked" icon.
    NotAllowed,

    /// @brief The "Waiting" or "Busy" spinning icon.
    Wait
};

}   // namespace GP
