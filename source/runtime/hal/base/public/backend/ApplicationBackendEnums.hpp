// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::hal
{

/// @brief Explicit status codes for HAL application backend operations.
enum class [[nodiscard]] BackendResult : gp::UInt8
{
    Success = 0,
    AlreadyInitialized,
    NotInitialized,
    InvalidArguments,
    InitializationFailed,
    SubsystemFailure,
    BufferOverflow,
    UnsupportedPlatform
};

/// @brief Bitmask flags specifying which hardware subsystems the backend should initialize.
enum class BackendSubsystemFlags : gp::UInt32
{
    None = 0,
    Events = 1 << 0,    // Core OS event queue and device notifications
    Video = 1 << 1,     // Display management and windowing surfaces
    Audio = 1 << 2,     // Audio endpoint enumeration and hotplug notifications
    Gamepad = 1 << 3,   // Standardized controller mappings and XInput/GameInput/HID backends
    Haptic = 1 << 4,    // Force feedback and DualSense/Xbox rumble actuators
    Sensor = 1 << 5,    // Gyroscope and accelerometer IMU polling
    Camera = 1 << 6,    // Video capture device enumeration

    /// @brief Default configuration for a standard interactive runtime session.
    Default = Events | Video | Gamepad | Haptic | Sensor
};

}   // namespace gp::hal

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::hal::BackendSubsystemFlags);
