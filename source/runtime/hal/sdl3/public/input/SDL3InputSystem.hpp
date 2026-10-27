// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "input/GenericInputSystem.hpp"
#include "input/SDL3Gamepad.hpp"
#include "SDL3HAL.hpp"

namespace gp::hal::sdl3
{

/// @brief Concrete SDL3 implementation of `IInputSystem`.
/// @details Binds 8 inline `sdl3::Gamepad` instances into `generic::InputSystem`'s slot table and
///          automatically opens/closes native `SDL_Gamepad` handles on hotplug events.
class GP_HAL_SDL3_API InputSystem final : public gp::hal::input::generic::InputSystem
{
private:
    sdl3::Gamepad m_sdlGamepads[gp::hal::input::kMaxGamepads]{};

public:
    /// @brief Constructs the SDL3 input system and binds `m_sdlGamepads` into the base slot table.
    [[nodiscard]] constexpr InputSystem() noexcept
    {
        for (gp::UInt32 i = 0u; i < gp::hal::input::kMaxGamepads; ++i)
        {
            m_gamepadTable[i] = &m_sdlGamepads[i];
        }
    }

    /// @brief Destroys the SDL3 input system and closes any open `SDL_Gamepad` handles.
    ~InputSystem() noexcept override;

protected:
    /// @copydoc gp::hal::input::generic::InputSystem::onBackendGamepadConnected()
    void onBackendGamepadConnected(gp::UInt32 slotIndex, gp::hal::input::InputDeviceId deviceId) noexcept override;

    /// @copydoc gp::hal::input::generic::InputSystem::onBackendGamepadDisconnected()
    void onBackendGamepadDisconnected(gp::UInt32 slotIndex, gp::hal::input::InputDeviceId deviceId) noexcept override;
};

}   // namespace gp::hal::sdl3
