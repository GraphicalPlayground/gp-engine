// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "window/GenericWindow.hpp"
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

/// @brief Represents a window in the SDL3 backend, providing functionality to create, manage, and interact with.
class Window : public gp::hal::generic::Window
{
private:
    SDL_Window* m_window{ nullptr };

public:
    /// @brief Initializes the SDL3 window wrapper using the provided description.
    /// @param[in] desc The window description to initialize the window with.
    explicit constexpr Window(const WindowDesc& desc) noexcept
        : gp::hal::generic::Window(desc)
    {}

public:
    /// @brief Opens the window and makes it ready for use. This may involve creating the underlying native window
    ///        resources and making the window visible on the screen. If the window is already open, this function may
    ///        have no effect or may reset the window state depending on the implementation.
    virtual void open() override;

    /// @brief Closes the window and releases any associated resources. After calling this function, the window should
    /// no longer be
    ///        considered valid or open. If the window is already closed, this function may have no effect.
    /// @see requestClose() for requesting the window to close without immediately destroying it, allowing for cleanup
    ///      or user confirmation.
    virtual void close() override;

private:
    /// @brief Caches the native window handles for later use.
    void cacheNativeHandle() noexcept;
};

}   // namespace gp::hal::sdl3
