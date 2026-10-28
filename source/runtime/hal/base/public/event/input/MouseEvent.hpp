// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "input/InputEnums.hpp"

namespace gp::hal::event
{

/// @brief Defines the category of pointer interaction.
enum class MouseAction : gp::UInt8
{
    None = 0,
    Moved,
    ButtonChanged,   // Inspect `state` (KeyState::Pressed / Released) and `button`
    WheelScrolled
};

/// @brief Fixed-size POD payload for EventCategory::Mouse (32 bytes, zero implicit padding).
struct MouseEvent
{
    gp::UInt32 windowId = 0u;
    gp::UInt32 deviceId = 0u;    // Distinguishes multiple mice / touch-emulation
    gp::Float32 x = 0.0f;        // Sub-pixel cursor X relative to window
    gp::Float32 y = 0.0f;        // Sub-pixel cursor Y relative to window
    gp::Float32 deltaX = 0.0f;   // Raw relative motion X (or horizontal scroll)
    gp::Float32 deltaY = 0.0f;   // Raw relative motion Y (or vertical scroll)
    MouseAction action = MouseAction::None;
    gp::hal::input::MouseButton button = gp::hal::input::MouseButton::Unknown;
    gp::hal::input::MouseButtonMask buttonMask = gp::hal::input::MouseButtonMask::None;   // Simultaneously held buttons
    gp::hal::input::KeyState state = gp::hal::input::KeyState::Released;   // Button state when action == ButtonChanged
    gp::UInt8 clicks = 0u;                                                 // 1 = single-click, 2 = double-click
    gp::UInt8 isWheelFlipped = 0u;                                         // Non-zero if OS uses inverted scrolling
    gp::UInt8 reserved[2] = { 0u, 0u };                                    // Explicit alignment to 32 bytes
};

static_assert(sizeof(MouseEvent) == 32, "MouseEvent size mismatch!");

}   // namespace gp::hal::event
