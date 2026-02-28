// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Windowing/BaseWindow.hpp"
#include "Windowing/WindowDesc.hpp"
#include <SDL3/SDL.h>

namespace GP::HAL
{

class SDLWindowingSystem;

/// \brief Window implementation using SDL.
class SDLWindow final : public FBaseWindow
{
private:
    SDL_Window* m_window;   //!< The SDL window handle.

public:
    /// \brief Empty constructor for SDLWindow.
    SDLWindow();

    /// \brief Constructs a window from a descriptor and a back-pointer to the owning system.
    /// \param desc Fully populated creation descriptor.
    /// \param owner Non-null pointer to the FSDL3WindowingSystem that owns this window.
    explicit SDLWindow(const FWindowDesc& desc, SDLWindowingSystem* owner);

    /// \brief Destructor that cleans up the SDL window resources.
    ~SDLWindow() override;

public:
    /// \brief Dispatches an SDL event to the appropriate window event handlers.
    /// \param event The SDL_Event to process.
    void DispatchSDLEvent(const SDL_Event& event);

    /// \brief Get the underlying SDL_Window pointer for direct SDL interactions if necessary.
    /// \return A pointer to the SDL_Window, or nullptr if the window is not initialized.
    GP_NODISCARD SDL_Window* GetSDLWindow() const noexcept;

    /// \brief Get the SDL window ID associated with this window, which can be used for event filtering and other SDL
    /// operations.
    /// \return The SDL_WindowID for this window, or 0 if the window is not initialized.
    GP_NODISCARD SDL_WindowID GetSDLWindowID() const noexcept;
};

}   // namespace GP::HAL
