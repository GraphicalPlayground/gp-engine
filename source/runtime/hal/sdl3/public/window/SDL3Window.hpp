// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "window/GenericWindow.hpp"
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

class Window : public gp::hal::generic::Window
{
private:
    SDL_Window* m_window{ nullptr };

public:
    /// @brief Gets the native window handles required to bootstrap rendering APIs (Vulkan/D3D/Metal).
    /// @return A tagged union containing the explicit backend type and associated handles.
    [[nodiscard]] virtual NativeWindowHandle getNativeHandle() const noexcept override;

private:
    /// @brief Caches the native window handles for later use.
    void cacheNativeHandle() noexcept;
};

}   // namespace gp::hal::sdl3
