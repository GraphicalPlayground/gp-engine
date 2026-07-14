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
    /// @brief Gets a pointer to the underlying native window handle or object. The actual type and meaning of this
    ///        handle is platform-specific. It may be used for advanced operations that are not covered by this
    ///        interface, but the caller must know the expected type and semantics of the handle for the specific
    ///        platform. This may return nullptr if no native handle is available.
    /// @return A pointer to the underlying native window handle or object, or nullptr if no native handle is available.
    [[nodiscard]] virtual void* getNativeHandle() const noexcept override;
};

}   // namespace gp::hal::sdl3
