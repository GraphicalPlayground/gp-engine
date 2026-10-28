// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "HALBase.hpp"
#include "input/InputEnums.hpp"
#include "input/InputTypes.hpp"

namespace gp::hal::input
{

/// @brief Base capability interface for all hardware and virtual input peripherals in the HAL (ADR-0018).
/// @details Provides uniform device identification, connection status, battery telemetry, and
///          per-frame edge-latch lifecycle management.
class GP_HAL_BASE_API IInputDevice
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IInputDevice() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IInputDevice() noexcept = default;

    IInputDevice(const IInputDevice&) = delete;
    IInputDevice& operator=(const IInputDevice&) = delete;
    IInputDevice(IInputDevice&&) = delete;
    IInputDevice& operator=(IInputDevice&&) = delete;

public:
    /// @brief Retrieves the unique backend hardware instance ID for this device.
    /// @return The 32-bit `InputDeviceId`, or `kInvalidInputDeviceId` (0) if virtual/unconnected.
    [[nodiscard]] virtual InputDeviceId getDeviceId() const noexcept = 0;

    /// @brief Retrieves the high-level classification of this input device.
    /// @return `InputDeviceType` enumerator (`Keyboard`, `Mouse`, `Gamepad`, `Touch`, `Pen`, `Sensor`).
    [[nodiscard]] virtual InputDeviceType getDeviceType() const noexcept = 0;

    /// @brief Retrieves the human-readable UTF-8 product name of the device.
    /// @return `StringView` referencing the cached device name.
    [[nodiscard]] virtual StringView getDeviceName() const noexcept = 0;

    /// @brief Checks whether the physical peripheral is currently connected and active.
    /// @return True if connected, false otherwise.
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    /// @brief Retrieves the current battery charge percentage for wireless devices.
    /// @return Battery percentage in `[0, 100]`, or `100` for wired devices.
    [[nodiscard]] virtual gp::UInt8 getBatteryPercent() const noexcept = 0;

    /// @brief Clears transient per-frame edge flags (`PressedThisFrame`, `ReleasedThisFrame`) and motion deltas.
    virtual void beginFrame() noexcept = 0;

    /// @brief Synthesizes release transitions for all held buttons and zeroes all axes (ADR-0036).
    virtual void releaseAllInputs() noexcept = 0;

    /// @brief Completely resets the device state to its initial disconnected/zeroed configuration.
    virtual void reset() noexcept = 0;
};

}   // namespace gp::hal::input
