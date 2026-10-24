// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "input/InputEnums.hpp"

namespace gp::hal::event
{

/// @brief Broad hardware subsystem classification for hotplugged devices.
enum class DeviceDomain : gp::UInt8
{
    Unknown = 0,
    Input,          // Uses `inputType` (gp::hal::input::InputDeviceType)
    AudioOutput,    // Headphones, speakers, HDMI audio endpoints
    AudioCapture,   // Microphones, audio interfaces
    Camera          // Webcams, video capture devices
};

/// @brief Hardware peripheral hotplug lifecycle transition.
enum class DeviceAction : gp::UInt8
{
    None = 0,
    Connected,
    Disconnected,
    Remapped,            // Controller button mapping updated
    DefaultChanged,      // OS default audio endpoint switched
    PermissionGranted,   // User granted OS permission for Camera / Microphone
    PermissionDenied     // User denied OS permission for Camera / Microphone
};

/// @brief Fixed-size POD payload for EventCategory::DeviceChange (8 bytes, zero implicit padding).
struct DeviceChangeEvent
{
    gp::UInt32 deviceId = 0u;   // Unique peripheral instance ID
    DeviceDomain domain = DeviceDomain::Unknown;
    gp::hal::input::InputDeviceType inputType =
        gp::hal::input::InputDeviceType::Unknown;   // Populated when domain == Input
    DeviceAction action = DeviceAction::None;
    gp::UInt8 reserved = 0u;
};

static_assert(sizeof(DeviceChangeEvent) == 8, "DeviceChangeEvent size mismatch!");

}   // namespace gp::hal::event
