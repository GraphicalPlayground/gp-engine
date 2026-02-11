// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "HALBuild.hpp"
#include "HALForward.hpp"
#include "Monitor/IMonitor.hpp"
#include "Windowing/WindowEnums.hpp"

namespace GP::HAL
{

/// \brief Interface for the window class.
/// The window class is responsible for managing the application's main window, including its creation, configuration,
/// and destruction. It also provides access to the window's properties, such as its size, position, and state, and
/// allows for handling of window events, such as resizing, moving, and closing.
class IWindow
{
public:
    virtual ~IWindow() = default;

public:
    GP_NODISCARD virtual bool IsOpen() const noexcept = 0;
    GP_NODISCARD virtual bool ShouldClose() const noexcept = 0;
    GP_NODISCARD virtual bool IsFocused() const noexcept = 0;
    GP_NODISCARD virtual bool IsFullscreen() const noexcept = 0;
    GP_NODISCARD virtual bool IsResizable() const noexcept = 0;
    GP_NODISCARD virtual bool IsVisible() const noexcept = 0;
    GP_NODISCARD virtual bool IsMinimized() const noexcept = 0;
    GP_NODISCARD virtual bool IsMaximized() const noexcept = 0;
    GP_NODISCARD virtual bool IsBorderless() const noexcept = 0;
    GP_NODISCARD virtual bool IsDecorated() const noexcept = 0;
    GP_NODISCARD virtual bool IsTransparent() const noexcept = 0;
    GP_NODISCARD virtual bool IsAlwaysOnTop() const noexcept = 0;
    GP_NODISCARD virtual bool IsHovered() const noexcept = 0;
    GP_NODISCARD virtual bool IsInputGrabbed() const noexcept = 0;
    GP_NODISCARD virtual bool IsMouseGrabbed() const noexcept = 0;
    GP_NODISCARD virtual bool IsKeyboardGrabbed() const noexcept = 0;
    GP_NODISCARD virtual bool IsHighDPI() const noexcept = 0;
    GP_NODISCARD virtual bool IsOccluded() const noexcept = 0;
    GP_NODISCARD virtual bool HasMouseCapture() const noexcept = 0;

    GP_NODISCARD virtual EWindowMode GetWindowMode() const noexcept = 0;
    GP_NODISCARD virtual EWindowState GetWindowState() const noexcept = 0;
    GP_NODISCARD virtual EWindowStyle GetWindowStyle() const noexcept = 0;
    GP_NODISCARD virtual ECursorMode GetCursorMode() const noexcept = 0;
    GP_NODISCARD virtual ECursorShape GetCursorShape() const noexcept = 0;
};

}   // namespace GP::HAL
