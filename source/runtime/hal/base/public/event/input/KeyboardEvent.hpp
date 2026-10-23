// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "input/InputEnums.hpp"

namespace gp::hal::event
{

/// @brief Fixed-size POD payload for EventCategory::Keyboard (16 bytes, zero implicit padding).
/// @details Enforces ADR-0016 by transporting the physical hardware matrix location (scancode)
///          and the localized OS layout mapping (keycode) alongside active modifier flags.
struct KeyboardEvent
{
    gp::UInt32 windowId = 0u;                                                // Window holding focus
    gp::hal::input::Keycode keycode = gp::hal::input::Keycode::Unknown;      // Layout-dependent virtual key (4 bytes)
    gp::hal::input::Scancode scancode = gp::hal::input::Scancode::Unknown;   // Physical USB HID scan code (2 bytes)
    gp::hal::input::KeyModifier modifiers = gp::hal::input::KeyModifier::None;   // Active modifier bitmask (2 bytes)
    gp::hal::input::KeyState state = gp::hal::input::KeyState::Released;         // Pressed or Released (1 byte)
    gp::UInt8 isRepeat = 0u;                                                     // Non-zero if OS auto-repeat (1 byte)
    gp::UInt8 reserved[2] = { 0u, 0u };                                          // Explicit alignment to 16 bytes
};

static_assert(sizeof(KeyboardEvent) == 16, "KeyboardEvent size mismatch!");

}   // namespace gp::hal::event
