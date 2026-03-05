// Copyright (c) - Graphical Playground. All rights reserved.

#include "Window/SDL3/SDL3WindowSystem.hpp"
#include "Window/SDL3/SDL3Window.hpp"
#include <SDL3/SDL.h>

namespace GP
{

TSharedPtr<IWindow> FSDL3WindowSystem::CreateWindow(const FWindowDesc& desc) noexcept
{
    return GP::MakeShared<FSDL3Window>(desc);
}

void FSDL3WindowSystem::PollEvents() noexcept
{
    // TODO: Implement event polling using SDL_PollEvent and translate SDL events into the engine's event system.
}

void FSDL3WindowSystem::DispatchEvents() noexcept
{
    // TODO: Implement event dispatching to the appropriate window instances based on the events polled in PollEvents().
}

}   // namespace GP
