// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "input/InputEnums.hpp"

namespace gp::hal::event
{

/// @brief Fixed-size POD payload for EventCategory::Touch (40 bytes, zero implicit padding).
struct TouchEvent
{
    gp::UInt64 touchDeviceId = 0ULL;   // Hardware touchscreen/trackpad ID
    gp::UInt64 fingerId = 0ULL;        // Unique contact tracking ID
    gp::Float32 x = 0.0f;              // Normalized [0.0..1.0] X coordinate
    gp::Float32 y = 0.0f;              // Normalized [0.0..1.0] Y coordinate
    gp::Float32 deltaX = 0.0f;         // Normalized relative motion X
    gp::Float32 deltaY = 0.0f;         // Normalized relative motion Y
    gp::Float32 pressure = 0.0f;       // Normalized [0.0..1.0] contact pressure
    gp::UInt32 windowId = 0u;          // Target window receiving the touch
    gp::hal::input::TouchFingerState state = gp::hal::input::TouchFingerState::Up;           // Up, Down, or Motion
    gp::hal::input::TouchDeviceType deviceType = gp::hal::input::TouchDeviceType::Invalid;   // Direct or Indirect
    gp::UInt8 isCancelled = 0u;   // Non-zero if OS palm-rejected/cancelled
    gp::UInt8 reserved = 0u;      // Explicit alignment to 40 bytes
};

static_assert(sizeof(TouchEvent) == 48, "TouchEvent size mismatch!");

}   // namespace gp::hal::event
