// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Defines how the system cursor behaves and interacts with the window.
enum class CursorMode : gp::UInt8
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
enum class CursorShape : gp::UInt8
{
    Default = 0,
    Arrow = 1,
    IBeam = 2,       // Text input
    Wait = 3,        // Hourglass / spinner
    Crosshair = 4,
    WaitArrow = 5,   // Arrow + hourglass
    ResizeNWSE = 6,
    ResizeNESW = 7,
    ResizeWE = 8,
    ResizeNS = 9,
    ResizeAll = 10,   // Move (four-way arrow)
    No = 11,          // Not allowed / forbidden
    Hand = 12,        // Pointing hand (link hover)
    Window = 13,      // Window dragging
    Count = 14
};

}   // namespace gp::hal
