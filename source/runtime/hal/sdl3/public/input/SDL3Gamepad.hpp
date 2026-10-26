// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "input/GenericGamepad.hpp"

// Forward declaration of opaque SDL3 handle to avoid leaking <SDL3/SDL.h>
struct SDL_Gamepad;

namespace gp::hal::sdl3
{

/// @brief Concrete SDL3 implementation of `gp::hal::input::IGamepad`.
/// @details Bridges `IHapticDevice` (motor rumble, impulse trigger rumble, RGB LEDs, player slot lights)
///          and `ISpatialDevice` (6-axis IMU Gyroscope/Accelerometer activation) to native SDL3 hardware calls.
class Gamepad final : public gp::hal::input::generic::Gamepad
{
private:
    SDL_Gamepad* m_sdlGamepad{ nullptr };
    bool m_supportsRumble{ false };
    bool m_supportsTriggerRumble{ false };
    bool m_supportsLED{ false };

public:
    [[nodiscard]] constexpr Gamepad() noexcept = default;
    ~Gamepad() noexcept override;

    /// @brief Opens the native `SDL_Gamepad` handle for `deviceId` and queries hardware haptic/sensor capabilities.
    /// @param[in] deviceId Native `SDL_JoystickID` instance identifier.
    /// @param[in] slot Assigned logical player slot (`Player1`..`Player8`).
    /// @return True if opened and bound successfully, false otherwise.
    bool openSdlGamepad(gp::hal::input::InputDeviceId deviceId, gp::hal::input::GamepadSlot slot) noexcept;

    /// @brief Stops active haptics and closes the underlying `SDL_Gamepad` handle.
    void closeSdlGamepad() noexcept;

    // IHapticDevice overrides
    [[nodiscard]] constexpr bool supportsRumble() const noexcept override
    {
        return m_supportsRumble;
    }

    [[nodiscard]] constexpr bool supportsTriggerRumble() const noexcept override
    {
        return m_supportsTriggerRumble;
    }

    [[nodiscard]] constexpr bool supportsLEDColor() const noexcept override
    {
        return m_supportsLED;
    }

    bool setRumble(const gp::hal::input::HapticRumbleDesc& desc) noexcept override;
    bool setTriggerRumble(const gp::hal::input::HapticTriggerRumbleDesc& desc) noexcept override;
    void stopHaptics() noexcept override;
    bool setLEDColor(const gp::hal::input::DeviceLEDColor& color) noexcept override;
    bool setPlayerLED(gp::hal::input::GamepadSlot slot) noexcept override;

    // ISpatialDevice overrides
    [[nodiscard]] bool hasSensor(gp::hal::input::GamepadSensorType type) const noexcept override;
    bool setSensorEnabled(gp::hal::input::GamepadSensorType type, bool enabled) noexcept override;
};

}   // namespace gp::hal::sdl3
