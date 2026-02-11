// Copyright (c) - Graphical Playground. All rights reserved.

#include "SDL/SDLWindow.hpp"

namespace GP::HAL
{

SDLWindow::SDLWindow()
    : m_window(nullptr)
{
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) { return; }
    }
}

SDLWindow::~SDLWindow() { /*Close();*/ }

}   // namespace GP::HAL
