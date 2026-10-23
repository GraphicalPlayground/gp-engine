// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "input/InputEnums.hpp"

namespace gp::hal::event
{

/// @brief Defines the nature of the controller interaction.
enum class GamepadAction : gp::UInt8
{
    None = 0,
    ButtonChanged,     // Uses control.button and state (KeyState::Pressed / Released)
    AxisMoved,         // Uses control.axis and value [-1.0..1.0] or [0.0..1.0]
    HatChanged,        // Uses control.hatPosition (for raw HID joysticks)
    TouchpadChanged,   // Uses control.touchState, touchpadId, value (X), secondaryValue (Y)
    BatteryUpdated     // Uses batteryPercent [0..100]
};

/// @brief Fixed-size POD payload for EventCategory::Gamepad (16 bytes, zero implicit padding).
struct GamepadEvent
{
    gp::UInt32 deviceId = 0u;            // Unique hardware instance ID
    gp::Float32 value = 0.0f;            // Axis value, trigger depth, or touchpad X
    gp::Float32 secondaryValue = 0.0f;   // Touchpad Y coordinate [0.0..1.0]
    GamepadAction action = GamepadAction::None;

    union
    {
        gp::hal::input::GamepadButton button = gp::hal::input::GamepadButton::Invalid;
        gp::hal::input::GamepadAxis axis;
        gp::hal::input::HatPosition hatPosition;
        gp::hal::input::TouchFingerState touchState;
    } control;

    gp::hal::input::KeyState state =
        gp::hal::input::KeyState::Released;   // Button press state or touchpad finger index
    gp::UInt8 batteryPercent = 0u;            // [0..100] or touchpadId
};

static_assert(sizeof(GamepadEvent) == 16, "GamepadEvent size mismatch!");

}   // namespace gp::hal::event
