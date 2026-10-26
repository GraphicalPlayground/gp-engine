// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "event/device/DeviceChangeEvent.hpp"
#include "event/input/GamepadEvent.hpp"
#include "event/input/KeyboardEvent.hpp"
#include "event/input/MouseEvent.hpp"
#include "event/input/PenEvent.hpp"
#include "event/input/SensorEvent.hpp"
#include "event/input/TouchEvent.hpp"
#include "input/IGamepad.hpp"
#include "input/IKeyboard.hpp"
#include "input/IMouse.hpp"
#include "input/ISpatialDevice.hpp"

namespace gp::hal::input
{

/// @brief Master coordinator interface owning and routing HAL events to `IKeyboard`, `IMouse`, `IGamepad`, and
/// `ISpatialDevice`.
class IInputSystem
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IInputSystem() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IInputSystem() noexcept = default;

    IInputSystem(const IInputSystem&) = delete;
    IInputSystem& operator=(const IInputSystem&) = delete;
    IInputSystem(IInputSystem&&) = delete;
    IInputSystem& operator=(IInputSystem&&) = delete;

public:
    /// @brief Clears transient edge flags (`PressedThisFrame`, `ReleasedThisFrame`) and motion deltas across all
    /// devices.
    virtual void beginFrame() noexcept = 0;

    /// @brief Synthesizes release transitions for all held inputs across all devices on focus loss (ADR-0036).
    virtual void releaseAllInputs() noexcept = 0;

    /// @brief Resets all managed input devices to their initial state.
    virtual void reset() noexcept = 0;

    /// @brief Returns the most recently used physical input device type (hysteresis-filtered, ADR-0035).
    /// @return Active `InputDeviceType` (`Keyboard`, `Mouse`, `Gamepad`, `Touch`, or `Pen`).
    [[nodiscard]] virtual InputDeviceType getActiveDeviceType() const noexcept = 0;

    /// @brief Returns a mutable reference to the primary system keyboard device.
    /// @return Reference to `IKeyboard`.
    [[nodiscard]] virtual IKeyboard& getKeyboard() noexcept = 0;

    /// @brief Returns a const reference to the primary system keyboard device.
    /// @return Const reference to `IKeyboard`.
    [[nodiscard]] virtual const IKeyboard& getKeyboard() const noexcept = 0;

    /// @brief Returns a mutable reference to the primary system mouse device.
    /// @return Reference to `IMouse`.
    [[nodiscard]] virtual IMouse& getMouse() noexcept = 0;

    /// @brief Returns a const reference to the primary system mouse device.
    /// @return Const reference to `IMouse`.
    [[nodiscard]] virtual const IMouse& getMouse() const noexcept = 0;

    /// @brief Retrieves a mutable pointer to the gamepad assigned to `slot` (`Player1`..`Player8` or `Any`).
    /// @param[in] slot Logical player slot index.
    /// @return Pointer to `IGamepad` (for haptics/LEDs/queries), or `nullptr` if invalid slot.
    [[nodiscard]] virtual IGamepad* getGamepad(GamepadSlot slot = GamepadSlot::Player1) noexcept = 0;

    /// @brief Retrieves a const pointer to the gamepad assigned to `slot` (`Player1`..`Player8` or `Any`).
    /// @param[in] slot Logical player slot index.
    /// @return Const pointer to `IGamepad`, or `nullptr` if invalid slot.
    [[nodiscard]] virtual const IGamepad* getGamepad(GamepadSlot slot = GamepadSlot::Player1) const noexcept = 0;

    /// @brief Returns the number of currently connected gamepads across all player slots.
    /// @return Connected gamepad count in `[0, kMaxGamepads]`.
    [[nodiscard]] virtual gp::UInt32 getConnectedGamepadCount() const noexcept = 0;

    /// @brief Returns a mutable reference to the standalone spatial device (multi-touch screen & graphics tablet
    /// stylus).
    /// @return Reference to `ISpatialDevice`.
    [[nodiscard]] virtual ISpatialDevice& getSpatialDevice() noexcept = 0;

    /// @brief Returns a const reference to the standalone spatial device (multi-touch screen & graphics tablet stylus).
    /// @return Const reference to `ISpatialDevice`.
    [[nodiscard]] virtual const ISpatialDevice& getSpatialDevice() const noexcept = 0;

    /// @brief Convenience check for whether a physical keyboard Scancode is currently held down.
    /// @param[in] scancode Physical USB HID key location.
    /// @return True if held down, false otherwise.
    [[nodiscard]] virtual bool isScancodeDown(Scancode scancode) const noexcept = 0;

    /// @brief Convenience check for whether a physical keyboard Scancode was pressed this frame.
    /// @param[in] scancode Physical USB HID key location.
    /// @return True if pressed this frame, false otherwise.
    [[nodiscard]] virtual bool wasScancodeJustPressed(Scancode scancode) const noexcept = 0;

    /// @brief Convenience check for whether a virtual Keycode is currently held down.
    /// @param[in] keycode Localized virtual keycode.
    /// @return True if held down, false otherwise.
    [[nodiscard]] virtual bool isKeyDown(Keycode keycode) const noexcept = 0;

    /// @brief Convenience check for whether a virtual Keycode was pressed this frame.
    /// @param[in] keycode Localized virtual keycode.
    /// @return True if pressed this frame, false otherwise.
    [[nodiscard]] virtual bool wasKeyJustPressed(Keycode keycode) const noexcept = 0;

    /// @brief Convenience check for whether a mouse button is currently held down.
    /// @param[in] button Target `MouseButton`.
    /// @return True if held down, false otherwise.
    [[nodiscard]] virtual bool isMouseButtonDown(MouseButton button) const noexcept = 0;

    /// @brief Convenience check for whether a gamepad button is currently held down on `slot`.
    /// @param[in] button Target `GamepadButton`.
    /// @param[in] slot Logical player slot.
    /// @return True if held down, false otherwise.
    [[nodiscard]] virtual bool
        isGamepadButtonDown(GamepadButton button, GamepadSlot slot = GamepadSlot::Player1) const noexcept = 0;

    /// @brief Ingests a decoded `KeyboardEvent` packet.
    /// @param[in] ev Decoded keyboard event.
    virtual void onKeyboardEvent(const event::KeyboardEvent& ev) noexcept = 0;

    /// @brief Ingests a decoded `MouseEvent` packet.
    /// @param[in] ev Decoded mouse event.
    virtual void onMouseEvent(const event::MouseEvent& ev) noexcept = 0;

    /// @brief Ingests a decoded `GamepadEvent` packet.
    /// @param[in] ev Decoded gamepad event.
    virtual void onGamepadEvent(const event::GamepadEvent& ev) noexcept = 0;

    /// @brief Ingests a decoded `TouchEvent` packet.
    /// @param[in] ev Decoded touch event.
    virtual void onTouchEvent(const event::TouchEvent& ev) noexcept = 0;

    /// @brief Ingests a decoded `PenEvent` packet.
    /// @param[in] ev Decoded pen event.
    virtual void onPenEvent(const event::PenEvent& ev) noexcept = 0;

    /// @brief Ingests a decoded `SensorEvent` packet.
    /// @param[in] ev Decoded sensor event.
    virtual void onSensorEvent(const event::SensorEvent& ev) noexcept = 0;

    /// @brief Ingests a decoded `DeviceChangeEvent` to connect, disconnect, or rebind hardware IDs.
    /// @param[in] ev Decoded device change event.
    virtual void onDeviceChangeEvent(const event::DeviceChangeEvent& ev) noexcept = 0;
};

}   // namespace gp::hal::input
