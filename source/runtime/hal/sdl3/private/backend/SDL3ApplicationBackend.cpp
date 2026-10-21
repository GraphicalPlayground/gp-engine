// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "backend/SDL3ApplicationBackend.hpp"
#include "window/SDL3Window.hpp"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>

namespace gp::hal::sdl3
{

bool ApplicationBackend::initialize() noexcept
{
    // Prevent SDL from disabling the screensaver / sleep mode automatically.
    // The engine should decide this based on game state (e.g. playing vs paused).
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

    SDL_InitFlags flags = 0;

    // Essential subsystems for window management and event handling.
    flags |= SDL_INIT_VIDEO;   // Window creation, Vulkan/D3D surface mapping.
                               // (Note: This automatically initializes SDL_INIT_EVENTS)

    // Input and controller subsystems for handling user input.
    flags |= SDL_INIT_GAMEPAD;   // Modern gamepad API (Xbox, PS5, Switch controllers).
                                 // (Note: This automatically initializes SDL_INIT_JOYSTICK).
    flags |= SDL_INIT_HAPTIC;    // Force feedback and rumble support for controllers.
    flags |= SDL_INIT_SENSOR;    // Gyroscopes and accelerometers (crucial for Switch/Mobile/VR).

    // Audio is not initialized by default, otherwise SDL will take over the audio device
    // and prevent other libraries (e.g., OpenAL, FMOD, Wwise) from using it.
    // flags |= SDL_INIT_AUDIO;

    if (!SDL_Init(flags))
    {
        // GP_LOG_FATAL("SDL3 Initialization failed! Error: %s", SDL_GetError());
        return false;
    }

    // GP_LOG_INFO("SDL3 Backend successfully initialized.");
    return true;

    return true;
}

void ApplicationBackend::shutdown() noexcept
{
    SDL_Quit();
    // GP_LOG_INFO("SDL3 Backend shut down.");
}

UniquePtr<IWindow> ApplicationBackend::createWindow(const WindowDesc& desc) noexcept
{
    return gp::makeUnique<gp::hal::sdl3::Window>(desc);
}

}   // namespace gp::hal::sdl3
