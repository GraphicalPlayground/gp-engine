// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "Windowing/IWindow.hpp"
#include <SDL3/SDL.h>

namespace GP::HAL
{

/// \brief Window implementation using SDL.
class SDLWindow final : public IWindow
{
private:
    SDL_Window* m_window;   //!< The SDL window handle.

public:
    /// \brief Empty constructor for SDLWindow.
    SDLWindow();

    /// \brief Destructor that cleans up the SDL window resources.
    ~SDLWindow() override;
};

}   // namespace GP::HAL
