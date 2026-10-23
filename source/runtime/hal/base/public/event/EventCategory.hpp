// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal::event
{

/// @brief Defines the high-level routing domain for an OS event.
/// @note Stored as a 16-bit integer to keep the EventHeader perfectly aligned to 4 bytes.
enum class EventCategory : gp::UInt16
{
    None = 0,
    Application,      // Quit requests, OS Sleep/Wake, Low Memory warnings
    Window,           // Resize, Move, Focus Lost/Gained, Minimized, Restored
    Display,          // Monitor connected/disconnected, HDR toggle, DPI changes

    Keyboard,         // Physical key state changes (Pressed/Released/Modifiers)
    Mouse,            // Cursor motion, button clicks, scroll wheel deltas
    Gamepad,          // Controller buttons, analog sticks, analog triggers
    Touch,            // Multi-touch gestures (Mobile, SteamDeck screen)
    Pen,              // Stylus inputs (Pressure, Tilt, Eraser)
    Sensor,           // Gyroscopes, Accelerometers (Switch controllers, VR, Mobile)

    TextInput,        // UTF-8 string inputs, IME composition (Korean/Japanese text)
    Drop,             // Drag-and-drop actions (File paths, dragged text)
    Clipboard,        // OS Clipboard content updated

    DeviceChange,     // Gamepad added/removed, Audio device switched, MIDI connected

    Custom = 0xFFFF   // Reserved for runtime plugins passing their own TLV payloads
};

}   // namespace gp::hal::event
