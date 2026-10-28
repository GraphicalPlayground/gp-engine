// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "input/InputEnums.hpp"

namespace gp::hal::event
{

/// @brief Stylus / graphics tablet interaction type.
enum class PenAction : gp::UInt8
{
    None = 0,
    ProximityIn,    // Stylus entered tablet electromagnetic detection range
    ProximityOut,   // Stylus left detection range
    TipChanged,     // Stylus tip touched or lifted from surface (inspect `tipState`)
    Moved,          // Stylus moved or axis changed
    ButtonChanged   // Barrel button pressed or released (inspect `buttons` and `buttonState`)
};

/// @brief Fixed-size POD payload for EventCategory::Pen (48 bytes, zero implicit padding).
struct PenEvent
{
    gp::UInt32 windowId = 0u;
    gp::UInt32 penId = 0u;   // Unique hardware stylus ID
    gp::Float32 x = 0.0f;    // Sub-pixel X in window client coords
    gp::Float32 y = 0.0f;    // Sub-pixel Y in window client coords
    gp::Float32 axes[static_cast<gp::UInt8>(
        gp::hal::input::PenAxis::Count
    )] = {};   // Indexed by gp::hal::input::PenAxis (28 bytes)
    PenAction action = PenAction::None;
    gp::hal::input::PenButton buttons = gp::hal::input::PenButton::None;   // Active barrel/tip/eraser bitmask
    gp::hal::input::KeyState state = gp::hal::input::KeyState::Released;   // Pressed/Released state for Tip/Button
    gp::UInt8 isEraser = 0u;                                               // Non-zero if inverted eraser tip is active
};

static_assert(sizeof(PenEvent) == 48, "PenEvent size mismatch!");

}   // namespace gp::hal::event
