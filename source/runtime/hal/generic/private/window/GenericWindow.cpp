// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "window/GenericWindow.hpp"

namespace gp::hal::generic
{

bool Window::isOpen() const noexcept
{
    return m_isOpen;
}

bool Window::isValid() const noexcept
{
    return m_isOpen && m_nativeHandle.backend != NativeWindowBackend::Unknown;
}

NativeWindowHandle Window::getNativeHandle() const noexcept
{
    return m_nativeHandle;
}

}   // namespace gp::hal::generic
