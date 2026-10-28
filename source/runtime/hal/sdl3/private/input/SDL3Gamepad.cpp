// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "input/SDL3Gamepad.hpp"
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

namespace
{

[[nodiscard]] constexpr gp::UInt16 toSdlRumbleU16(gp::Float32 normalizedIntensity) noexcept
{
    const gp::Float32 clamped =
        (normalizedIntensity < 0.0f) ? 0.0f : ((normalizedIntensity > 1.0f) ? 1.0f : normalizedIntensity);
    return static_cast<gp::UInt16>(clamped * 65535.0f);
}

[[nodiscard]] constexpr gp::USize safeStrLen(const char* str, gp::USize maxLen) noexcept
{
    if (str == nullptr)
    {
        return 0u;
    }
    gp::USize len = 0u;
    while (len < maxLen && str[len] != '\0')
    {
        ++len;
    }
    return len;
}

}   // namespace

Gamepad::~Gamepad() noexcept
{
    closeSdlGamepad();
}

bool Gamepad::openSdlGamepad(gp::hal::input::InputDeviceId deviceId, gp::hal::input::GamepadSlot slot) noexcept
{
    closeSdlGamepad();

    m_sdlGamepad = SDL_GetGamepadFromID(static_cast<SDL_JoystickID>(deviceId));
    if (m_sdlGamepad == nullptr)
    {
        m_sdlGamepad = SDL_OpenGamepad(static_cast<SDL_JoystickID>(deviceId));
    }

    if (m_sdlGamepad == nullptr) [[unlikely]]
    {
        return false;
    }

    const char* rawName = SDL_GetGamepadName(m_sdlGamepad);
    const StringView nameView = (rawName != nullptr)
                                    ? StringView(rawName, safeStrLen(rawName, kMaxGamepadNameBytes - 1u))
                                    : StringView("SDL3 Gamepad");

    attach(deviceId, slot, nameView);

    const SDL_PropertiesID props = SDL_GetGamepadProperties(m_sdlGamepad);
    if (props != 0u)
    {
        m_supportsRumble = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
        m_supportsTriggerRumble = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false);
        m_supportsLED = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
    }

    (void)setPlayerLED(slot);
    return true;
}

void Gamepad::closeSdlGamepad() noexcept
{
    if (m_sdlGamepad != nullptr)
    {
        stopHaptics();
        SDL_CloseGamepad(m_sdlGamepad);
        m_sdlGamepad = nullptr;
    }

    m_supportsRumble = false;
    m_supportsTriggerRumble = false;
    m_supportsLED = false;
    detach();
}

bool Gamepad::setRumble(const gp::hal::input::HapticRumbleDesc& desc) noexcept
{
    if (m_sdlGamepad == nullptr || !m_isConnected) [[unlikely]]
    {
        return false;
    }

    return SDL_RumbleGamepad(
        m_sdlGamepad,
        toSdlRumbleU16(desc.lowFrequencyIntensity),
        toSdlRumbleU16(desc.highFrequencyIntensity),
        desc.durationMs
    );
}

bool Gamepad::setTriggerRumble(const gp::hal::input::HapticTriggerRumbleDesc& desc) noexcept
{
    if (m_sdlGamepad == nullptr || !m_isConnected) [[unlikely]]
    {
        return false;
    }

    return SDL_RumbleGamepadTriggers(
        m_sdlGamepad,
        toSdlRumbleU16(desc.leftTriggerIntensity),
        toSdlRumbleU16(desc.rightTriggerIntensity),
        desc.durationMs
    );
}

void Gamepad::stopHaptics() noexcept
{
    if (m_sdlGamepad != nullptr)
    {
        (void)SDL_RumbleGamepad(m_sdlGamepad, 0u, 0u, 0u);
        (void)SDL_RumbleGamepadTriggers(m_sdlGamepad, 0u, 0u, 0u);
    }
}

bool Gamepad::setLEDColor(const gp::hal::input::DeviceLEDColor& color) noexcept
{
    if (m_sdlGamepad == nullptr || !m_isConnected || !m_supportsLED) [[unlikely]]
    {
        return false;
    }

    return SDL_SetGamepadLED(m_sdlGamepad, color.r, color.g, color.b);
}

bool Gamepad::setPlayerLED(gp::hal::input::GamepadSlot slot) noexcept
{
    m_slot = slot;
    if (m_sdlGamepad == nullptr || !m_isConnected) [[unlikely]]
    {
        return false;
    }

    return SDL_SetGamepadPlayerIndex(m_sdlGamepad, static_cast<int>(slot));
}

bool Gamepad::hasSensor(gp::hal::input::GamepadSensorType type) const noexcept
{
    if (m_sdlGamepad == nullptr || !m_isConnected) [[unlikely]]
    {
        return false;
    }

    const SDL_SensorType sdlType =
        (type == gp::hal::input::GamepadSensorType::Accelerometer)
            ? SDL_SENSOR_ACCEL
            : ((type == gp::hal::input::GamepadSensorType::Gyroscope) ? SDL_SENSOR_GYRO : SDL_SENSOR_INVALID);

    return (sdlType != SDL_SENSOR_INVALID) && SDL_GamepadHasSensor(m_sdlGamepad, sdlType);
}

bool Gamepad::setSensorEnabled(gp::hal::input::GamepadSensorType type, bool enabled) noexcept
{
    if (m_sdlGamepad == nullptr || !m_isConnected) [[unlikely]]
    {
        return false;
    }

    const SDL_SensorType sdlType =
        (type == gp::hal::input::GamepadSensorType::Accelerometer)
            ? SDL_SENSOR_ACCEL
            : ((type == gp::hal::input::GamepadSensorType::Gyroscope) ? SDL_SENSOR_GYRO : SDL_SENSOR_INVALID);

    if (sdlType == SDL_SENSOR_INVALID) [[unlikely]]
    {
        return false;
    }

    if (SDL_SetGamepadSensorEnabled(m_sdlGamepad, sdlType, enabled))
    {
        if (type == gp::hal::input::GamepadSensorType::Accelerometer)
        {
            m_accelEnabled = enabled;
        }
        else if (type == gp::hal::input::GamepadSensorType::Gyroscope)
        {
            m_gyroEnabled = enabled;
        }
        return true;
    }
    return false;
}

}   // namespace gp::hal::sdl3
