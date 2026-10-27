// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "HALBase.hpp"
#include "input/InputTypes.hpp"

namespace gp::hal::input
{

/// @brief Capability interface for force-feedback actuators, trigger rumble motors, and device LEDs (ADR-0018).
class GP_HAL_BASE_API IHapticDevice
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IHapticDevice() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IHapticDevice() noexcept = default;

    IHapticDevice(const IHapticDevice&) = delete;
    IHapticDevice& operator=(const IHapticDevice&) = delete;
    IHapticDevice(IHapticDevice&&) = delete;
    IHapticDevice& operator=(IHapticDevice&&) = delete;

public:
    /// @brief Checks whether the hardware device supports dual-motor (low/high frequency) vibration.
    /// @return True if motor rumble is supported, false otherwise.
    [[nodiscard]] virtual bool supportsRumble() const noexcept = 0;

    /// @brief Checks whether the hardware device supports independent left/right trigger vibration.
    /// @return True if trigger rumble is supported, false otherwise.
    [[nodiscard]] virtual bool supportsTriggerRumble() const noexcept = 0;

    /// @brief Checks whether the hardware device features a programmable RGB lightbar LED.
    /// @return True if RGB LED control is supported, false otherwise.
    [[nodiscard]] virtual bool supportsLEDColor() const noexcept = 0;

    /// @brief Triggers dual-motor low/high-frequency vibration on the device.
    /// @param[in] desc Vibration intensities `[0.0f..1.0f]` and duration in milliseconds.
    /// @return True if the haptic command was accepted by the hardware backend, false otherwise.
    virtual bool setRumble(const HapticRumbleDesc& desc) noexcept = 0;

    /// @brief Triggers independent left/right analog trigger vibration on supported controllers.
    /// @param[in] desc Trigger vibration intensities `[0.0f..1.0f]` and duration in milliseconds.
    /// @return True if the trigger haptic command was accepted by the hardware backend, false otherwise.
    virtual bool setTriggerRumble(const HapticTriggerRumbleDesc& desc) noexcept = 0;

    /// @brief Immediately halts all active motor and trigger vibration effects on the device.
    virtual void stopHaptics() noexcept = 0;

    /// @brief Sets the RGB color of the device's lightbar or home button LED.
    /// @param[in] color Target RGB color value.
    /// @return True if the LED update succeeded, false otherwise.
    virtual bool setLEDColor(const DeviceLEDColor& color) noexcept = 0;

    /// @brief Sets the player indicator LED pattern on the controller (e.g., Player 1..8 lights).
    /// @param[in] slot Logical player slot index.
    /// @return True if supported and updated, false otherwise.
    virtual bool setPlayerLED(GamepadSlot slot) noexcept = 0;
};

}   // namespace gp::hal::input
