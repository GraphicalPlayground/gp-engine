// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "window/GenericWindow.hpp"

// Forward declarations of opaque SDL3 handles to prevent <SDL3/SDL.h> header pollution.
struct SDL_Window;
struct SDL_Cursor;

namespace gp::hal::sdl3
{

/// @brief Concrete SDL3 implementation of `gp::hal::IWindow`.
/// @details Manages an underlying `SDL_Window*` and `SDL_Cursor*` while keeping `GenericWindow`'s shadow state
///          synchronized for lock-free Game Thread reads.
class Window final : public gp::hal::generic::Window
{
private:
    SDL_Window* m_window{ nullptr };
    SDL_Cursor* m_cursor{ nullptr };

public:
    /// @brief Default-constructs an uninitialized SDL3 window wrapper.
    [[nodiscard]] explicit constexpr Window() noexcept = default;

    /// @brief Initializes the SDL3 window wrapper using the provided description.
    /// @note If `desc.startOpened` is true, automatically invokes `open()`.
    /// @param[in] desc The window configuration descriptor.
    explicit Window(const WindowDesc& desc) noexcept;

    /// @brief Destroys the SDL3 window and releases any allocated OS cursor and window handles.
    ~Window() noexcept override;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

public:
    /// @copydoc IWindow::open()
    void open() noexcept override;

    /// @copydoc IWindow::close()
    void close() noexcept override;

    /// @copydoc IWindow::show()
    void show() noexcept override;

    /// @copydoc IWindow::hide()
    void hide() noexcept override;

    /// @copydoc IWindow::minimize()
    void minimize() noexcept override;

    /// @copydoc IWindow::maximize()
    void maximize() noexcept override;

    /// @copydoc IWindow::restore()
    void restore() noexcept override;

    /// @copydoc IWindow::focus()
    void focus() noexcept override;

    /// @copydoc IWindow::requestAttention()
    void requestAttention(WindowFlashMode mode = WindowFlashMode::UntilFocused) noexcept override;

    /// @copydoc IWindow::centerOnDisplay()
    void centerOnDisplay(const IDisplay* display = nullptr) noexcept override;

    /// @copydoc IWindow::setPosition(const Point2D&)
    void setPosition(const Point2D& position) noexcept override;

    /// @copydoc IWindow::setPosition(Int32, Int32)
    void setPosition(Int32 x, Int32 y) noexcept override;

    /// @copydoc IWindow::setSize(const Extent2D&)
    void setSize(const Extent2D& size) noexcept override;

    /// @copydoc IWindow::setSize(UInt32, UInt32)
    void setSize(UInt32 width, UInt32 height) noexcept override;

    /// @copydoc IWindow::setFullSize(const Extent2D&)
    void setFullSize(const Extent2D& size) noexcept override;

    /// @copydoc IWindow::setFullSize(UInt32, UInt32)
    void setFullSize(UInt32 width, UInt32 height) noexcept override;

    /// @copydoc IWindow::setSizeLimits()
    void setSizeLimits(const Extent2D& minimum, const Extent2D& maximum) noexcept override;

    /// @copydoc IWindow::setMinimumSize(const Extent2D&)
    void setMinimumSize(const Extent2D& size) noexcept override;

    /// @copydoc IWindow::setMinimumSize(UInt32, UInt32)
    void setMinimumSize(UInt32 width, UInt32 height) noexcept override;

    /// @copydoc IWindow::setMaximumSize(const Extent2D&)
    void setMaximumSize(const Extent2D& size) noexcept override;

    /// @copydoc IWindow::setMaximumSize(UInt32, UInt32)
    void setMaximumSize(UInt32 width, UInt32 height) noexcept override;

    /// @copydoc IWindow::setTitle()
    void setTitle(StringView title) noexcept override;

    /// @copydoc IWindow::setOpacity()
    void setOpacity(Float32 opacity) noexcept override;

    /// @copydoc IWindow::setResizable()
    void setResizable(bool resizable) noexcept override;

    /// @copydoc IWindow::setAlwaysOnTop()
    void setAlwaysOnTop(bool alwaysOnTop) noexcept override;

    /// @copydoc IWindow::setWindowMode()
    void setWindowMode(WindowMode mode, const IDisplay* targetDisplay = nullptr) noexcept override;

    /// @copydoc IWindow::setCursorMode()
    void setCursorMode(CursorMode mode) noexcept override;

    /// @copydoc IWindow::setMouseGrabbed()
    void setMouseGrabbed(bool grabbed) noexcept override;

    /// @copydoc IWindow::setKeyboardGrabbed()
    void setKeyboardGrabbed(bool grabbed) noexcept override;

    /// @copydoc IWindow::setCursorShape()
    void setCursorShape(CursorShape shape) noexcept override;

    /// @copydoc IWindow::setCursorPosition()
    void setCursorPosition(const Point2D& position) noexcept override;

    /// @brief Returns the underlying `SDL_Window*` pointer for internal SDL3 backend operations.
    /// @return Raw `SDL_Window*` handle, or `nullptr` if closed.
    [[nodiscard]] constexpr SDL_Window* getSdlWindow() const noexcept
    {
        return m_window;
    }

private:
    /// @brief Queries SDL3 window properties to populate `m_nativeHandle` for RHI surface creation.
    void cacheNativeHandle() noexcept;

    /// @brief Queries SDL3 for initial pixel dimensions, non-client border sizes, DPI scale, and display ID.
    void refreshShadowMetrics() noexcept;
};

}   // namespace gp::hal::sdl3
