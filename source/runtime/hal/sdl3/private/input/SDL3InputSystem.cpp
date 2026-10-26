// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "input/SDL3InputSystem.hpp"

namespace gp::hal::sdl3
{

InputSystem::~InputSystem() noexcept
{
    for (gp::UInt32 i = 0u; i < gp::hal::input::kMaxGamepads; ++i)
    {
        m_sdlGamepads[i].closeSdlGamepad();
    }
}

void InputSystem::onBackendGamepadConnected(gp::UInt32 slotIndex, gp::hal::input::InputDeviceId deviceId) noexcept
{
    if (slotIndex < gp::hal::input::kMaxGamepads) [[likely]]
    {
        (void)m_sdlGamepads[slotIndex].openSdlGamepad(deviceId, static_cast<gp::hal::input::GamepadSlot>(slotIndex));
    }
}

void InputSystem::onBackendGamepadDisconnected(
    gp::UInt32 slotIndex, [[maybe_unused]] gp::hal::input::InputDeviceId deviceId
) noexcept
{
    if (slotIndex < gp::hal::input::kMaxGamepads) [[likely]]
    {
        m_sdlGamepads[slotIndex].closeSdlGamepad();
    }
}

}   // namespace gp::hal::sdl3
