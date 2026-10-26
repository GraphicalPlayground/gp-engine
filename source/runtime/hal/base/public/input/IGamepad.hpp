// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "event/input/GamepadEvent.hpp"
#include "input/IHapticDevice.hpp"
#include "input/IInputDevice.hpp"
#include "input/ISpatialDevice.hpp"

namespace gp::hal::input
{

/// @brief Unified interface for a modern gamepad controller composing Input, Haptic, and Spatial capabilities
/// (ADR-0018).
class IGamepad
    : public IInputDevice
    , public IHapticDevice
    , public ISpatialDevice
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IGamepad() noexcept = default;

    /// @brief Virtual destructor.
    ~IGamepad() noexcept override = default;

public:
    /// @brief Retrieves the logical player slot (`Player1`..`Player8`) assigned to this controller.
    /// @return Assigned `GamepadSlot`.
    [[nodiscard]] virtual GamepadSlot getSlot() const noexcept = 0;

    /// @brief Checks if the specified gamepad button is currently held down.
    /// @param[in] button Target `GamepadButton`.
    /// @return True if held down, false otherwise.
    [[nodiscard]] virtual bool isButtonDown(GamepadButton button) const noexcept = 0;

    /// @brief Checks if the specified gamepad button transitioned to Pressed during the current frame.
    /// @param[in] button Target `GamepadButton`.
    /// @return True if pressed this frame (sub-frame latch safe).
    [[nodiscard]] virtual bool wasButtonJustPressed(GamepadButton button) const noexcept = 0;

    /// @brief Checks if the specified gamepad button transitioned to Released during the current frame.
    /// @param[in] button Target `GamepadButton`.
    /// @return True if released this frame, false otherwise.
    [[nodiscard]] virtual bool wasButtonJustReleased(GamepadButton button) const noexcept = 0;

    /// @brief Retrieves the raw `ButtonStateFlags` bitmask for a gamepad button.
    /// @param[in] button Target `GamepadButton`.
    /// @return `ButtonStateFlags` bitmask.
    [[nodiscard]] virtual ButtonStateFlags getButtonState(GamepadButton button) const noexcept = 0;

    /// @brief Retrieves the current 8-way directional hat switch position.
    /// @return `HatPosition` bitmask.
    [[nodiscard]] virtual HatPosition getHatPosition() const noexcept = 0;

    /// @brief Retrieves a deadzone-filtered analog axis value in `[-1.0f, 1.0f]` (sticks) or `[0.0f, 1.0f]` (triggers).
    /// @param[in] axis Target `GamepadAxis`.
    /// @return Deadzone-filtered floating-point value.
    [[nodiscard]] virtual gp::Float32 getAxis(GamepadAxis axis) const noexcept = 0;

    /// @brief Retrieves the raw, unfiltered hardware analog axis value.
    /// @param[in] axis Target `GamepadAxis`.
    /// @return Unfiltered hardware axis value.
    [[nodiscard]] virtual gp::Float32 getRawAxis(GamepadAxis axis) const noexcept = 0;

    /// @brief Retrieves the 2D radial-deadzone-filtered Left Thumbstick vector (ADR-0035).
    /// @return `Stick2D` containing filtered `(LeftX, LeftY)`.
    [[nodiscard]] virtual Stick2D getLeftStick() const noexcept = 0;

    /// @brief Retrieves the 2D radial-deadzone-filtered Right Thumbstick vector (ADR-0035).
    /// @return `Stick2D` containing filtered `(RightX, RightY)`.
    [[nodiscard]] virtual Stick2D getRightStick() const noexcept = 0;

    /// @brief Configures the deadzones for the left stick, right stick, and analog triggers on this controller.
    /// @param[in] leftStick Deadzone settings for the left thumbstick.
    /// @param[in] rightStick Deadzone settings for the right thumbstick.
    /// @param[in] triggers Deadzone settings for the left and right triggers.
    virtual void setDeadzones(
        const AnalogDeadzoneConfig& leftStick,
        const AnalogDeadzoneConfig& rightStick,
        const AnalogDeadzoneConfig& triggers
    ) noexcept = 0;

    /// @brief Retrieves the current contact state of the controller's built-in touchpad (e.g., DualSense).
    /// @return `TouchFingerState` (`Up`, `Down`, or `Motion`).
    [[nodiscard]] virtual TouchFingerState getTouchpadState() const noexcept = 0;

    /// @brief Retrieves the normalized `[0.0f..1.0f]` finger coordinates on the controller's built-in touchpad.
    /// @return `Stick2D` containing normalized touchpad `(x, y)` coordinates.
    [[nodiscard]] virtual Stick2D getTouchpadPosition() const noexcept = 0;

    /// @brief Ingests a decoded HAL `GamepadEvent` packet into this controller's state.
    /// @param[in] ev The decoded gamepad event packet.
    virtual void onGamepadEvent(const event::GamepadEvent& ev) noexcept = 0;
};

}   // namespace gp::hal::input
