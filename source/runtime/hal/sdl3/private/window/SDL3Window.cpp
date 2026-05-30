// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "window/SDL3Window.hpp"

namespace gp::hal::sdl3
{

GP_NODISCARD void* Window::getNativeHandle() const noexcept
{
    return static_cast<void*>(m_window);
}

}   // namespace gp::hal::sdl3
