// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "window/WindowDesc.hpp"
#include "window/WindowEnums.hpp"
#include "window/WindowTypes.hpp"

namespace gp::hal
{

/// @section Forward declarations

class Display;

/// @brief Interface for platform-agnostic window management.
class Window
{
protected:

public:
    /// @brief Virtual destructor to ensure proper cleanup in derived classes.
    virtual ~Window() = default;

public:
    /// @brief Checks if the window is currently open and has not been closed by the user or the application.
    /// @return True if the window is open, false if it has been closed.
    GP_NODISCARD virtual bool isOpen() const noexcept = 0;

    /// @brief Checks if the window is currently valid and can be interacted with.
    /// @return True if the window is valid, false if it is in an invalid state (e.g., failed to initialize).
    GP_NODISCARD virtual bool isValid() const noexcept = 0;

    /// @brief Checks if the window is currently active (focused and receiving input).
    /// @return True if the window is active, false otherwise.
    GP_NODISCARD virtual bool isActive() const noexcept = 0;

    /// @brief Checks if the window is currently focused (has input focus).
    /// @return True if the window is focused, false otherwise.
    GP_NODISCARD virtual bool isFocused() const noexcept = 0;

    /// @brief Checks if the window is currently minimized (iconified).
    /// @return True if the window is minimized, false otherwise.
    GP_NODISCARD virtual bool isMinimized() const noexcept = 0;

    /// @brief Checks if the window is currently maximized.
    /// @return True if the window is maximized, false otherwise.
    GP_NODISCARD virtual bool isMaximized() const noexcept = 0;

    /// @brief Checks if the window is resizable by the user.
    /// @return True if the window is resizable, false otherwise.
    GP_NODISCARD virtual bool isResizable() const noexcept = 0;

    /// @brief Checks if the window is currently in fullscreen mode.
    /// @return True if the window is in fullscreen mode, false otherwise.
    GP_NODISCARD virtual bool isFullscreen() const noexcept = 0;

    /// @brief Checks if the window is currently in borderless mode.
    /// @return True if the window is in borderless mode, false otherwise.
    GP_NODISCARD virtual bool isBorderless() const noexcept = 0;

    /// @brief Checks if the window is transparent (allows underlying windows to show through).
    /// @return True if the window is transparent, false otherwise.
    GP_NODISCARD virtual bool isTransparent() const noexcept = 0;

    /// @brief Checks if the window is set to always be on top of other windows.
    /// @return True if the window is always on top, false otherwise.
    GP_NODISCARD virtual bool isAlwaysOnTop() const noexcept = 0;

    /// @brief Checks if the window is currently hovered by the mouse cursor.
    /// @return True if the window is hovered, false otherwise.
    GP_NODISCARD virtual bool isHovered() const noexcept = 0;

    /// @brief Checks if the window is currently visible to the user (not minimized or hidden).
    /// @return True if the window is visible, false otherwise.
    GP_NODISCARD virtual bool isVisible() const noexcept = 0;

    /// @brief Checks if the window is currently hidden from the user (minimized or explicitly hidden).
    /// @return True if the window is hidden, false otherwise.
    GP_NODISCARD virtual bool isHidden() const noexcept = 0;

    /// @brief Checks if the window has vertical synchronization (VSync) enabled for buffer swapping.
    /// @return True if VSync is enabled, false otherwise.
    GP_NODISCARD virtual bool isVSyncEnabled() const noexcept = 0;

    /// @brief Checks if the window is using a high-DPI backbuffer for rendering on supported displays.
    /// @return True if high-DPI mode is enabled, false otherwise.
    GP_NODISCARD virtual bool isHighDPI() const noexcept = 0;

    /// @brief Checks if the window has input focus grabbed.
    /// @return True if the window has input focus grabbed, false otherwise.
    GP_NODISCARD virtual bool isInputGrabbed() const noexcept = 0;

    /// @brief Checks if the window has mouse input grabbed.
    /// @return True if the window has mouse input grabbed, false otherwise.
    GP_NODISCARD virtual bool isMouseGrabbed() const noexcept = 0;

    /// @brief Checks if the window has keyboard input grabbed.
    /// @return True if the window has keyboard input grabbed, false otherwise.
    GP_NODISCARD virtual bool isKeyboardGrabbed() const noexcept = 0;

    /// @brief Checks if the system cursor is currently visible when over the window.
    /// @return True if the cursor is visible, false otherwise.
    GP_NODISCARD virtual bool isCursorVisible() const noexcept = 0;

    /// @brief Checks if the system cursor is currently hidden when over the window.
    /// @return True if the cursor is hidden, false otherwise.
    GP_NODISCARD virtual bool isCursorHidden() const noexcept = 0;

    /// @brief Checks if the system cursor is currently confined to the window's client area.
    /// @return True if the cursor is confined, false otherwise.
    GP_NODISCARD virtual bool isCursorConfined() const noexcept = 0;

    /// @brief Checks if the window is currently occluded (completely covered by other windows or off-screen).
    /// @return True if the window is occluded, false otherwise.
    GP_NODISCARD virtual bool isOccluded() const noexcept = 0;

    /// @brief Get the current position of the window's top-left corner in screen coordinates.
    /// @return The current position of the window's top-left corner in screen coordinates.
    /// @see getFullPosition() for the position of the entire window including non-client areas.
    GP_NODISCARD virtual Point2D getPosition() const noexcept = 0;

    /// @brief Get the current position of the entire window including non-client areas in screen coordinates.
    /// @return The current position of the entire window including non-client areas in screen coordinates.
    /// @see getPosition() for the position of the client area (drawable region).
    GP_NODISCARD virtual Point2D getFullPosition() const noexcept = 0;

    /// @brief Get the current size of the window's client area (drawable region) in pixels.
    /// @return The current size of the window's client area (drawable region) in pixels.
    /// @see getFullSize() for the total window size including non-client areas.
    GP_NODISCARD virtual Extent2D getSize() const noexcept = 0;

    /// @brief Get the current size of the entire window including non-client areas in pixels.
    /// @return The current size of the entire window including non-client areas in pixels.
    /// @see getSize() for the size of the client area (drawable region).
    GP_NODISCARD virtual Extent2D getFullSize() const noexcept = 0;

    /// @brief Get the current size of the framebuffer or backbuffer used for rendering, which may differ from the
    ///        window's client area size on high-DPI displays.
    /// @return The current size of the framebuffer or backbuffer used for rendering.
    /// @see getSize() for the size of the client area (drawable region).
    GP_NODISCARD virtual Extent2D getFramebufferSize() const noexcept = 0;

    /// @brief Get the minimum size constraints of the window's client area (drawable region) in pixels.
    /// @return The minimum size constraints of the window's client area (drawable region) in pixels.
    /// @see getMaximumSize() for the maximum size constraints of the window.
    GP_NODISCARD virtual Extent2D getMinimumSize() const noexcept = 0;

    /// @brief Get the maximum size constraints of the window's client area (drawable region) in pixels.
    /// @return The maximum size constraints of the window's client area (drawable region) in pixels.
    /// @see getMinimumSize() for the minimum size constraints of the window.
    GP_NODISCARD virtual Extent2D getMaximumSize() const noexcept = 0;

    /// @brief Gets a pointer to the underlying native window handle or object. The actual type and meaning of this
    ///        handle is platform-specific. It may be used for advanced operations that are not covered by this
    ///        interface, but the caller must know the expected type and semantics of the handle for the specific
    ///        platform. This may return nullptr if no native handle is available.
    /// @return A pointer to the underlying native window handle or object, or nullptr if no native handle is available.
    GP_NODISCARD virtual void* getNativeHandle() const noexcept = 0;

    /// @brief Gets a pointer to the Display that the window is currently associated with. This may return nullptr if
    ///        the window is not currently associated with any display. The returned Display pointer should not be owned
    ///        or deleted by the caller,
    /// @return A pointer to the Display that the window is currently associated with, or nullptr if the window is not
    ///         associated with any display.
    GP_NODISCARD virtual const Display* getCurrentDisplay() const noexcept = 0;

    /// @brief Sets the position of the window's top-left corner in screen coordinates.
    /// @param[in] position The new position of the window's top-left corner in screen coordinates.
    virtual void setPosition(const Point2D& position) noexcept = 0;

    /// @brief Sets the position of the window's top-left corner in screen coordinates.
    /// @param[in] x The new X coordinate of the window's top-left corner in screen coordinates.
    /// @param[in] y The new Y coordinate of the window's top-left corner in screen coordinates.
    virtual void setPosition(const Int32 x, const Int32 y) noexcept = 0;

    /// @brief Sets the size of the window's client area (drawable region) in pixels.
    /// @param[in] size The new size of the window's client area (drawable region) in pixels.
    virtual void setSize(const Extent2D& size) noexcept = 0;

    /// @brief Sets the size of the window's client area (drawable region) in pixels.
    /// @param[in] width The new width of the window's client area (drawable region) in pixels.
    /// @param[in] height The new height of the window's client area (drawable region) in pixels.
    virtual void setSize(const UInt32 width, const UInt32 height) noexcept = 0;

    /// @brief Sets the size of the entire window including non-client areas in pixels.
    /// @param[in] size The new size of the entire window including non-client areas in pixels.
    virtual void setFullSize(const Extent2D& size) noexcept = 0;

    /// @brief Sets the size of the entire window including non-client areas in pixels.
    /// @param[in] width The new width of the entire window including non-client areas in pixels.
    /// @param[in] height The new height of the entire window including non-client areas in pixels.
    virtual void setFullSize(const UInt32 width, const UInt32 height) noexcept = 0;

    /// @brief Set the minimum and maximum size constraints of the window's client area (drawable region) in pixels. The
    ///        window will not be resizable to sizes outside of these constraints.
    /// @param[in] minimum The minimum size constraints of the window's client area (drawable region) in pixels.
    /// @param[in] maximum The maximum size constraints of the window's client area (drawable region) in pixels.
    virtual void setSizeLimits(const Extent2D& minimum, const Extent2D& maximum) noexcept = 0;

    /// @brief Set the minimum size constraints of the window's client area (drawable region) in pixels. The window will
    ///        not be resizable to sizes smaller than these constraints.
    /// @param[in] size The minimum size constraint of the window's client area (drawable region) in pixels.
    virtual void setMinimumSize(const Extent2D& size) noexcept = 0;

    /// @brief Set the minimum size constraints of the window's client area (drawable region) in pixels. The window will
    ///        not be resizable to sizes smaller than these constraints.
    /// @param[in] width The minimum width constraint of the window's client area (drawable region) in pixels.
    /// @param[in] height The minimum height constraint of the window's client area (drawable region) in pixels.
    virtual void setMinimumSize(const UInt32 width, const UInt32 height) noexcept = 0;

    /// @brief Set the maximum size constraints of the window's client area (drawable region) in pixels. The window will
    ///        not be resizable to sizes larger than these constraints.
    /// @param[in] size The maximum size constraint of the window's client area (drawable region) in pixels.
    virtual void setMaximumSize(const Extent2D& size) noexcept = 0;

    /// @brief Set the maximum size constraints of the window's client area (drawable region) in pixels. The window will
    ///        not be resizable to sizes larger than these constraints.
    /// @param[in] width The maximum width constraint of the window's client area (drawable region) in pixels.
    /// @param[in] height The maximum height constraint of the window's client area (drawable region) in pixels.
    virtual void setMaximumSize(const UInt32 width, const UInt32 height) noexcept = 0;

    /// @brief Opens the window and makes it ready for use. This may involve creating the underlying native window
    ///        resources and making the window visible on the screen. If the window is already open, this function may
    ///        have no effect or may reset the window state depending on the implementation.
    virtual void open() = 0;

    /// @brief Closes the window and releases any associated resources. After calling this function, the window should
    /// no longer be
    ///        considered valid or open. If the window is already closed, this function may have no effect.
    /// @see requestClose() for requesting the window to close without immediately destroying it, allowing for cleanup
    ///      or user confirmation.
    virtual void close() = 0;

    /// @brief Requests the window to close, which may trigger a close event that can be handled by the application.
    ///        This allows the application to perform any necessary cleanup or prompt the user for confirmation before
    ///        the window is actually closed. The window may not close immediately after this call, and the application
    ///        can choose to ignore the close request if desired.
    virtual void requestClose() = 0;

    /// @brief Shows the window if it is currently hidden.
    virtual void show() = 0;

    /// @brief Hides the window if it is currently visible.
    virtual void hide() = 0;

    /// @brief Minimizes the window if it is currently not minimized.
    virtual void minimize() = 0;

    /// @brief Maximizes the window if it is currently not maximized.
    virtual void maximize() = 0;

    /// @brief Restores the window to its normal state if it is currently minimized or maximized.
    virtual void restore() = 0;

    /// @brief Focuses the window, bringing it to the foreground and giving it input focus if possible.
    virtual void focus() = 0;

    /// @brief Requests the user's attention to the window, which may cause the window to flash or bounce in the taskbar
    ///        or dock depending on the platform and the specified flash mode. This can be used to notify the user of
    ///        important events when the window is not currently focused.
    /// @param[in] mode The flash mode that determines how the window should request the user's attention.
    virtual void requestAttention(WindowFlashMode mode = WindowFlashMode::UntilFocused) = 0;

    /// @brief Centers the window on the specified display.
    /// @param[in] display The display on which to center the window.
    /// @note If the display parameter is nullptr, the window will be centered on the display that it is currently
    ///       associated with, or on the primary display if it is not currently associated with any display.
    virtual void centerOnDisplay(const Display* display = nullptr) = 0;

    /// @brief Dispatches any pending events for the window.
    virtual void dispatchEvents() noexcept = 0;
};

}   // namespace gp::hal
