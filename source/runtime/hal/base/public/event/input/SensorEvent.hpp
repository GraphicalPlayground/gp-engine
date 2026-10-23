// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "input/InputEnums.hpp"

namespace gp::hal::event
{

/// @brief Fixed-size POD payload for EventCategory::Sensor (32 bytes, zero implicit padding).
struct SensorEvent
{
    gp::UInt64 timestampNs = 0ULL;                  // Hardware sensor timestamp (ns)
    gp::UInt32 deviceId = 0u;                       // Parent controller or mobile sensor ID
    gp::Float32 values[3] = { 0.0f, 0.0f, 0.0f };   // 3D axial reading (X, Y, Z)
    gp::hal::input::GamepadSensorType type = gp::hal::input::GamepadSensorType::Unknown;   // Accelerometer or Gyroscope
    gp::hal::input::ActuatorSide side = gp::hal::input::ActuatorSide::Both;   // Left, Right, or Both (unified IMU)
    gp::UInt8 reserved[6] = {};                                               // Explicit alignment to 32 bytes
};

static_assert(sizeof(SensorEvent) == 32, "SensorEvent size mismatch!");

}   // namespace gp::hal::event
