// Copyright (c) - Graphical Playground. All rights reserved.

#include "SDL/SDLWindow.hpp"
#include "SDL/SDLWindowingSystem.hpp"

namespace GP::HAL
{

SDLWindow::SDLWindow()
    : FBaseWindow(false)
    , m_window(nullptr)
{
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) { return; }
    }
}

SDLWindow::SDLWindow(const FWindowDesc& desc, SDLWindowingSystem* owner)
{
    GP_UNUSED(owner);
    GP_UNUSED(desc);
}

SDLWindow::~SDLWindow() { /*Close();*/ }

void SDLWindow::DispatchSDLEvent(const SDL_Event& event) { GP_UNUSED(event); }

GP_NODISCARD SDL_Window* SDLWindow::GetSDLWindow() const noexcept { return m_window; }

GP_NODISCARD SDL_WindowID SDLWindow::GetSDLWindowID() const noexcept
{
    return (m_window != nullptr) ? SDL_GetWindowID(m_window) : 0;
}

}   // namespace GP::HAL
