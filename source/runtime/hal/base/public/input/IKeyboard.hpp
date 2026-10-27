// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "event/input/KeyboardEvent.hpp"
#include "HALBase.hpp"
#include "input/IInputDevice.hpp"

namespace gp::hal::input
{

/// @brief Interface for keyboard input devices supporting both physical Scancodes and localized Keycodes.
class GP_HAL_BASE_API IKeyboard : public IInputDevice
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IKeyboard() noexcept = default;

    /// @brief Virtual destructor.
    ~IKeyboard() noexcept override = default;

public:
    /// @brief Checks if a physical key (by USB HID Scancode) is currently held down.
    /// @param[in] scancode Physical hardware key location.
    /// @return True if held down, false otherwise.
    [[nodiscard]] virtual bool isScancodeDown(Scancode scancode) const noexcept = 0;

    /// @brief Checks if a physical key transitioned to Pressed during the current frame (ADR-0034).
    /// @param[in] scancode Physical hardware key location.
    /// @return True if pressed this frame, false otherwise.
    [[nodiscard]] virtual bool wasScancodeJustPressed(Scancode scancode) const noexcept = 0;

    /// @brief Checks if a physical key transitioned to Released during the current frame.
    /// @param[in] scancode Physical hardware key location.
    /// @return True if released this frame, false otherwise.
    [[nodiscard]] virtual bool wasScancodeJustReleased(Scancode scancode) const noexcept = 0;

    /// @brief Checks if a physical key triggered an OS auto-repeat event during the current frame.
    /// @param[in] scancode Physical hardware key location.
    /// @return True if repeated this frame, false otherwise.
    [[nodiscard]] virtual bool wasScancodeRepeated(Scancode scancode) const noexcept = 0;

    /// @brief Retrieves the raw `ButtonStateFlags` bitmask for a physical Scancode.
    /// @param[in] scancode Physical hardware key location.
    /// @return `ButtonStateFlags` bitmask.
    [[nodiscard]] virtual ButtonStateFlags getScancodeState(Scancode scancode) const noexcept = 0;

    /// @brief Checks if a layout-mapped virtual key is currently held down.
    /// @param[in] keycode Localized OS virtual keycode.
    /// @return True if held down, false otherwise.
    [[nodiscard]] virtual bool isKeyDown(Keycode keycode) const noexcept = 0;

    /// @brief Checks if a layout-mapped virtual key transitioned to Pressed during the current frame.
    /// @param[in] keycode Localized OS virtual keycode.
    /// @return True if pressed this frame, false otherwise.
    [[nodiscard]] virtual bool wasKeyJustPressed(Keycode keycode) const noexcept = 0;

    /// @brief Checks if a layout-mapped virtual key transitioned to Released during the current frame.
    /// @param[in] keycode Localized OS virtual keycode.
    /// @return True if released this frame, false otherwise.
    [[nodiscard]] virtual bool wasKeyJustReleased(Keycode keycode) const noexcept = 0;

    /// @brief Retrieves the raw `ButtonStateFlags` bitmask for a virtual Keycode.
    /// @param[in] keycode Localized OS virtual keycode.
    /// @return `ButtonStateFlags` bitmask.
    [[nodiscard]] virtual ButtonStateFlags getKeyState(Keycode keycode) const noexcept = 0;

    /// @brief Retrieves the currently active keyboard modifier bitmask (`Shift`, `Ctrl`, `Alt`, `Gui`, `CapsLock`).
    /// @return Active `KeyModifier` bitmask.
    [[nodiscard]] virtual KeyModifier getModifiers() const noexcept = 0;

    /// @brief Checks whether all specified modifier flags are currently active.
    /// @param[in] modifiers Bitmask of `KeyModifier` flags to test.
    /// @return True if all requested modifiers are active.
    [[nodiscard]] virtual bool hasModifiers(KeyModifier modifiers) const noexcept = 0;

    /// @brief Ingests a decoded HAL `KeyboardEvent` packet into the keyboard state tables.
    /// @param[in] ev The decoded keyboard event packet.
    virtual void onKeyboardEvent(const event::KeyboardEvent& ev) noexcept = 0;
};

}   // namespace gp::hal::input
