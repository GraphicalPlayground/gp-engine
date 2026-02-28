// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Windowing/BaseWindow.hpp"
#include <SDL3/SDL.h>

namespace GP::HAL
{

/// \brief Window implementation using SDL.
class SDLWindow final : public FBaseWindow
{
private:
    SDL_Window* m_window;   //!< The SDL window handle.

public:
    /// \brief Empty constructor for SDLWindow.
    SDLWindow();

    /// \brief Destructor that cleans up the SDL window resources.
    ~SDLWindow() override;

public:
    /// \brief Checks if the window is currently open.
    /// \return True if the window is open, false otherwise.
    GP_NODISCARD virtual bool IsOpen() const noexcept override;

    /// \brief Checks if the window should close.
    /// \return True if the window should close, false otherwise.
    GP_NODISCARD virtual bool ShouldClose() const noexcept override;

    /// \brief Checks if the window is currently focused.
    /// \return True if the window is focused, false otherwise.
    GP_NODISCARD virtual bool IsFocused() const noexcept override;

    /// \brief Checks if the window is currently active.
    /// \note An active window is one that is currently receiving input and is the main focus of the user's attention.
    /// It may be focused, but not necessarily (e.g., if another window is in front of it).
    /// \return True if the window is active, false otherwise.
    GP_NODISCARD virtual bool IsActive() const noexcept override;

    /// \brief Checks if the window is currently in fullscreen mode.
    /// \return True if the window is in fullscreen mode, false otherwise.
    GP_NODISCARD virtual bool IsFullscreen() const noexcept override;

    /// \brief Checks if the window is currently in borderless mode.
    /// \return True if the window is in borderless mode, false otherwise.
    GP_NODISCARD virtual bool IsResizable() const noexcept override;

    /// \brief Checks if the window is currently visible.
    /// \return True if the window is visible, false otherwise.
    GP_NODISCARD virtual bool IsVisible() const noexcept override;

    /// \brief Checks if the window is currently minimized.
    /// \return True if the window is minimized, false otherwise.
    GP_NODISCARD virtual bool IsMinimized() const noexcept override;

    /// \brief Checks if the window is currently maximized.
    /// \return True if the window is maximized, false otherwise.
    GP_NODISCARD virtual bool IsMaximized() const noexcept override;

    /// \brief Checks if the window is borderless.
    /// \note This typically means the window has no title bar or borders, and may be used for fullscreen or kiosk
    /// applications.
    /// \return True if the window is borderless, false otherwise.
    GP_NODISCARD virtual bool IsBorderless() const noexcept override;

    /// \brief Checks if the window is decorated.
    /// \note This typically means the window has a title bar and borders provided by the operating system.
    /// \return True if the window is decorated, false otherwise.
    GP_NODISCARD virtual bool IsDecorated() const noexcept override;

    /// \brief Checks if the window is transparent.
    /// \return True if the window is transparent, false otherwise.
    GP_NODISCARD virtual bool IsTransparent() const noexcept override;

    /// \brief Checks if the window is always on top.
    /// \return True if the window is always on top, false otherwise.
    GP_NODISCARD virtual bool IsAlwaysOnTop() const noexcept override;

    /// \brief Checks if the window is currently hovered by the mouse cursor.
    /// \return True if the window is hovered, false otherwise.
    GP_NODISCARD virtual bool IsHovered() const noexcept override;

    /// \brief Checks if the window is currently focused for input.
    /// \return True if the window is focused for input, false otherwise.
    GP_NODISCARD virtual bool IsInputGrabbed() const noexcept override;

    /// \brief Checks if the mouse cursor is currently grabbed by the window.
    /// \return True if the mouse cursor is grabbed, false otherwise.
    GP_NODISCARD virtual bool IsMouseGrabbed() const noexcept override;

    /// \brief Checks if the keyboard is currently grabbed by the window.
    /// \return True if the keyboard is grabbed, false otherwise.
    GP_NODISCARD virtual bool IsKeyboardGrabbed() const noexcept override;

    /// \brief Checks if the window is currently in high DPI mode.
    /// \return True if the window is in high DPI mode, false otherwise.
    GP_NODISCARD virtual bool IsHighDPI() const noexcept override;

    /// \brief Checks if the window is currently occluded (i.e., completely covered by other windows).
    /// \return True if the window is occluded, false otherwise.
    GP_NODISCARD virtual bool IsOccluded() const noexcept override;

    /// \brief Checks if VSync is currently enabled for the window.
    /// \return True if VSync is enabled, false otherwise.
    GP_NODISCARD virtual bool IsVSyncEnabled() const noexcept override;

    /// \brief Checks if the window has mouse capture.
    /// \return True if the window has mouse capture, false otherwise.
    GP_NODISCARD virtual bool HasMouseCapture() const noexcept override;

    /// \brief Gets the current window mode.
    /// \return The current window mode.
    GP_NODISCARD virtual EWindowMode GetWindowMode() const noexcept override;

    /// \brief Gets the current window state.
    /// \return The current window state.
    GP_NODISCARD virtual EWindowState GetWindowState() const noexcept override;

    /// \brief Gets the current window style.
    /// \return The current window style.
    GP_NODISCARD virtual EWindowStyle GetWindowStyle() const noexcept override;

    /// \brief Gets the current cursor mode.
    /// \return The current cursor mode.
    GP_NODISCARD virtual ECursorMode GetCursorMode() const noexcept override;

    /// \brief Gets the current cursor shape.
    /// \return The current cursor shape.
    GP_NODISCARD virtual ECursorShape GetCursorShape() const noexcept override;

    /// \brief Gets the current window opacity.
    /// \return The current window opacity, in the range [0.0, 1.0], where 0.0 is fully transparent and 1.0 is fully
    /// opaque.
    GP_NODISCARD virtual Float32 GetOpacity() const noexcept override;

    /// \brief Gets the full size of the window, including title bars and borders.
    /// \note This may be larger than the content area size, depending on the window style and platform.
    /// \return The full size of the window, in pixels.
    GP_NODISCARD virtual FIntExtent2D GetFullWindowSize() const noexcept override;

    /// \brief Gets the full width of the window, including title bars and borders.
    /// \return The full width of the window, in pixels.
    GP_NODISCARD virtual UInt32 GetFullWindowWidth() const noexcept override;

    /// \brief Gets the full height of the window, including title bars and borders.
    /// \return The full height of the window, in pixels.
    GP_NODISCARD virtual UInt32 GetFullWindowHeight() const noexcept override;

    /// \brief Gets the current size of the window's content area (i.e., the area available for rendering, excluding
    /// title bars and borders).
    /// \note The content area size may be different from the full window size, depending on the window style and
    /// platform.
    /// \return The current size of the window's content area, in pixels.
    GP_NODISCARD virtual FIntExtent2D GetSize() const noexcept override;

    /// \brief Gets the width of the window's content area.
    /// \return The width of the window's content area, in pixels.
    GP_NODISCARD virtual UInt32 GetWidth() const noexcept override;

    /// \brief Gets the height of the window's content area.
    /// \return The height of the window's content area, in pixels.
    GP_NODISCARD virtual UInt32 GetHeight() const noexcept override;

    /// \brief Gets the size of the framebuffer associated with the window.
    /// \note The framebuffer size may be different from the content area size, especially on high DPI displays, where
    /// the framebuffer may have a higher resolution than the content area to provide sharper visuals.
    /// \return The size of the framebuffer, in pixels.
    GP_NODISCARD virtual FIntExtent2D GetFramebufferSize() const noexcept override;

    /// \brief Gets the width of the framebuffer associated with the window.
    /// \return The width of the framebuffer, in pixels.
    GP_NODISCARD virtual UInt32 GetFramebufferWidth() const noexcept override;

    /// \brief Gets the height of the framebuffer associated with the window.
    /// \return The height of the framebuffer, in pixels.
    GP_NODISCARD virtual UInt32 GetFramebufferHeight() const noexcept override;

    /// \brief Gets the current position of the window on the screen.
    /// \return The current position of the window, in pixels, relative to the top-left corner of the primary monitor.
    GP_NODISCARD virtual FIntPoint2D GetPosition() const noexcept override;

    /// \brief Gets the X coordinate of the window's position on the screen.
    /// \return The X coordinate of the window's position, in pixels, relative to the top-left corner of the primary
    /// monitor.
    GP_NODISCARD virtual UInt32 GetX() const noexcept override;

    /// \brief Gets the Y coordinate of the window's position on the screen.
    /// \return The Y coordinate of the window's position, in pixels, relative to the top-left corner of the primary
    /// monitor.
    GP_NODISCARD virtual UInt32 GetY() const noexcept override;

    /// \brief Gets the center point of the window on the screen.
    /// \return The center point of the window, in pixels, relative to the top-left corner of the primary monitor.
    GP_NODISCARD virtual FIntPoint2D GetCenter() const noexcept override;

    /// \brief Gets the coordinates of the corners of the window on the screen.
    /// \return The coordinates of the corners of the window, in pixels, relative to the top-left corner of the primary
    /// monitor.
    GP_NODISCARD virtual FIntPoint2D GetTopLeft() const noexcept override;

    /// \brief Gets the coordinates of the corners of the window on the screen.
    /// \return The coordinates of the corners of the window, in pixels, relative to the top-left corner of the primary
    /// monitor.
    GP_NODISCARD virtual FIntPoint2D GetTopRight() const noexcept override;

    /// \brief Gets the coordinates of the corners of the window on the screen.
    /// \return The coordinates of the corners of the window, in pixels, relative to the top-left corner of the primary
    /// monitor.
    GP_NODISCARD virtual FIntPoint2D GetBottomLeft() const noexcept override;

    /// \brief Gets the coordinates of the corners of the window on the screen.
    /// \return The coordinates of the corners of the window, in pixels, relative to the top-left corner of the primary
    /// monitor.
    GP_NODISCARD virtual FIntPoint2D GetBottomRight() const noexcept override;

    /// \brief Gets the aspect ratio of the window's content area.
    /// \return The aspect ratio of the window's content area, defined as the width divided by the height.
    GP_NODISCARD virtual Float32 GetAspectRatio() const noexcept override;

    /// \brief Gets the content scale factor of the window, which is used to convert between logical pixels (used for
    /// layout and input) and physical pixels (used for rendering).
    /// \note The content scale factor is typically 1.0 on standard DPI displays, and greater than 1.0 on high DPI
    /// displays (e.g., 2.0 on Retina displays). It is used to ensure that the window's content is rendered at the
    /// appropriate resolution and size, regardless of the display's DPI.
    /// \return The content scale factor of the window.
    GP_NODISCARD virtual Float32 GetContentScale() const noexcept override;

    /// \brief Gets the title of the window.
    /// \return The title of the window as a string view.
    GP_NODISCARD virtual FStringView GetTitle() const noexcept override;

    /// \brief Gets the minimum size of the window's content area.
    /// \return The minimum size of the window's content area, in pixels. The window cannot be resized to be smaller
    /// than this size.
    GP_NODISCARD virtual FIntExtent2D GetMinSize() const noexcept override;

    /// \brief Gets the maximum size of the window's content area.
    /// \return The maximum size of the window's content area, in pixels. The window cannot be resized to be larger than
    /// this size.
    GP_NODISCARD virtual FIntExtent2D GetMaxSize() const noexcept override;

    /// \brief Gets the monitor that the window is currently on.
    /// \return A pointer to the monitor that the window is currently on, or nullptr if the window is not currently on
    /// any monitor.
    GP_NODISCARD virtual const IMonitor* GetCurrentMonitor() const noexcept override;

    /// \brief Gets the native handle of the window, which is a platform-specific identifier that can be used to perform
    /// low-level operations on the window.
    /// \return The native handle of the window, as a void pointer. The actual type and meaning of the handle depends on
    /// the platform and windowing system being used (e.g., HWND on Windows, NSWindow* on macOS, etc.).
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept override;

    /// \brief Closes the window and releases any associated resources.
    virtual void Close() override;

    /// \brief Shows the window if it is hidden, or brings it to the front if it is already visible.
    virtual void Show() override;

    /// \brief Hides the window if it is visible, or minimizes it if it is already hidden.
    virtual void Hide() override;

    /// \brief Focuses the window, bringing it to the front and giving it input focus.
    virtual void Focus() override;

    /// \brief Toggles the window between maximized and restored states.
    virtual void Maximize() override;

    /// \brief Minimizes the window, hiding it from view and reducing it to an icon or taskbar entry, depending on the
    /// platform.
    virtual void Minimize() override;

    /// \brief Restores the window from a minimized or maximized state back to its normal size and position.
    virtual void Restore() override;

    /// \brief Brings the window to the user's attention, typically by flashing the window or its taskbar entry,
    /// depending on the platform.
    virtual void RequestAttention() override;

    /// \brief Sets the window mode, which determines how the window is displayed and interacts with the user.
    /// \param mode The window mode to set.
    virtual void SetWindowMode(EWindowMode mode) override;

    /// \brief Sets the window style, which determines the appearance and behavior of the window's borders, title bar,
    /// and other decorations.
    /// \param style The window style to set, as a bitmask of EWindowStyle flags.
    virtual void SetWindowStyle(EWindowStyle style) override;

    /// \brief Sets the cursor mode, which determines how the mouse cursor behaves when it is over the window.
    /// \param mode The cursor mode to set.
    virtual void SetCursorMode(ECursorMode mode) override;

    /// \brief Sets the cursor shape, which determines the appearance of the mouse cursor when it is over the window.
    /// \param shape The cursor shape to set.
    virtual void SetCursorShape(ECursorShape shape) override;

    /// \brief Sets the opacity of the window, which determines how transparent or opaque the window appears on the
    /// screen.
    /// \note The opacity value should be in the range [0.0, 1.0], where 0.0 is fully transparent and 1.0 is fully
    /// opaque.
    /// \param opacity The opacity value to set for the window.
    virtual void SetOpacity(Float32 opacity) override;

    /// \brief Sets the size of the window's content area, which is the area available for rendering and displaying
    /// content, excluding title bars and borders.
    /// \param width The width of the window's content area, in pixels.
    /// \param height The height of the window's content area, in pixels.
    virtual void SetSize(UInt32 width, UInt32 height) override;

    /// \brief Sets the size of the window's content area, which is the area available for rendering and displaying
    /// content, excluding title bars and borders.
    /// \param size The size of the window's content area, in pixels.
    virtual void SetSize(const FIntExtent2D& size) override;

    /// \brief Sets the width of the window's content area, which is the area available for rendering and displaying
    /// content, excluding title bars and borders.
    /// \param width The width of the window's content area, in pixels.
    virtual void SetWidth(UInt32 width) override;

    /// \brief Sets the height of the window's content area, which is the area available for rendering and displaying
    /// content, excluding title bars and borders.
    /// \param height The height of the window's content area, in pixels.
    virtual void SetHeight(UInt32 height) override;

    /// \brief Sets the full size of the window, including title bars and borders.
    /// \param width The full width of the window, in pixels.
    /// \param height The full height of the window, in pixels.
    virtual void SetFullWindowSize(UInt32 width, UInt32 height) override;

    /// \brief Sets the full size of the window, including title bars and borders.
    /// \param size The full size of the window, in pixels.
    virtual void SetFullWindowSize(const FIntExtent2D& size) override;

    /// \brief Sets the full width of the window, including title bars and borders.
    /// \param width The full width of the window, in pixels.
    virtual void SetFullWindowWidth(UInt32 width) override;

    /// \brief Sets the full height of the window, including title bars and borders.
    /// \param height The full height of the window, in pixels.
    virtual void SetFullWindowHeight(UInt32 height) override;

    /// \brief Sets the position of the window on the screen, relative to the top-left corner of the primary monitor.
    /// \param x The X coordinate of the window's position, in pixels.
    /// \param y The Y coordinate of the window's position, in pixels.
    virtual void SetPosition(UInt32 x, UInt32 y) override;

    /// \brief Sets the position of the window on the screen, relative to the top-left corner of the primary monitor.
    /// \param position The position of the window, in pixels.
    virtual void SetPosition(const FIntPoint2D& position) override;

    /// \brief Sets the X coordinate of the window's position on the screen, relative to the top-left corner of the
    /// primary monitor.
    /// \param x The X coordinate of the window's position, in pixels.
    virtual void SetX(UInt32 x) override;

    /// \brief Sets the Y coordinate of the window's position on the screen, relative to the top-left corner of the
    /// primary monitor.
    /// \param y The Y coordinate of the window's position, in pixels.
    virtual void SetY(UInt32 y) override;

    /// \brief Sets the aspect ratio of the window's content area, which is the ratio of the width to the height.
    /// \param aspectRatio The aspect ratio to set for the window's content area, defined as the width divided by the
    /// height.
    virtual void SetAspectRatio(Float32 aspectRatio) override;

    /// \brief Sets the aspect ratio of the window's content area, which is the ratio of the width to the height.
    /// \param numerator The numerator of the aspect ratio to set for the window's content area.
    /// \param denominator The denominator of the aspect ratio to set for the window's content area.
    virtual void SetAspectRatio(UInt32 numerator, UInt32 denominator) override;

    /// \brief Sets the size limits for the window's content area, which define the minimum and maximum size that the
    /// window can be resized to.
    /// \param minWidth The minimum width of the window's content area, in pixels. The window cannot be resized to be
    /// narrower than this width.
    /// \param minHeight The minimum height of the window's content area, in pixels. The window cannot be resized to be
    /// shorter than this height.
    /// \param maxWidth The maximum width of the window's content area, in pixels. The window cannot be resized to be
    /// wider than this width.
    /// \param maxHeight The maximum height of the window's content area, in pixels. The window cannot be resized to be
    /// taller than this height.
    virtual void SetSizeLimits(UInt32 minWidth, UInt32 minHeight, UInt32 maxWidth, UInt32 maxHeight) override;

    /// \brief Sets the size limits for the window's content area, which define the minimum and maximum size that the
    /// window can be resized to.
    /// \param minSize The minimum size of the window's content area, in pixels. The window cannot be resized to be
    /// smaller than this size.
    /// \param maxSize The maximum size of the window's content area, in pixels. The window cannot be resized to be
    /// larger than this size.
    virtual void SetSizeLimits(const FIntExtent2D& minSize, const FIntExtent2D& maxSize) override;

    /// \brief Sets the minimum size of the window's content area, which is the smallest size that the window can be
    /// resized to.
    /// \param minSize The minimum size of the window's content area, in pixels. The window cannot be resized to be
    /// smaller than this size.
    virtual void SetMinSize(const FIntExtent2D& minSize) override;

    /// \brief Sets the minimum size of the window's content area, which is the smallest size that the window can be
    /// resized to.
    /// \param minWidth The minimum width of the window's content area, in pixels. The window cannot be resized to be
    /// narrower than this width.
    /// \param minHeight The minimum height of the window's content area, in pixels. The window cannot be resized to be
    /// shorter than this height.
    virtual void SetMinSize(UInt32 minWidth, UInt32 minHeight) override;

    /// \brief Sets the minimum width of the window's content area, which is the smallest width that the window can be
    /// resized to.
    /// \param minWidth The minimum width of the window's content area, in pixels. The window cannot be resized to be
    /// narrower than this width.
    virtual void SetMinWidth(UInt32 minWidth) override;

    /// \brief Sets the minimum height of the window's content area, which is the smallest height that the window can be
    /// resized to.
    /// \param minHeight The minimum height of the window's content area, in pixels. The window cannot be resized to be
    /// shorter than this height.
    virtual void SetMinHeight(UInt32 minHeight) override;

    /// \brief Sets the maximum size of the window's content area, which is the largest size that the window can be
    /// resized to.
    /// \param maxSize The maximum size of the window's content area, in pixels. The window cannot be resized to be
    /// larger than this size.
    virtual void SetMaxSize(const FIntExtent2D& maxSize) override;

    /// \brief Sets the maximum size of the window's content area, which is the largest size that the window can be
    /// resized to.
    /// \param maxWidth The maximum width of the window's content area, in pixels. The window cannot be resized to be
    /// wider than this width.
    /// \param maxHeight The maximum height of the window's content area, in pixels. The window cannot be resized to be
    /// taller than this height.
    virtual void SetMaxSize(UInt32 maxWidth, UInt32 maxHeight) override;

    /// \brief Sets the maximum width of the window's content area, which is the largest width that the window can be
    /// resized to.
    /// \param maxWidth The maximum width of the window's content area, in pixels. The window cannot be resized to be
    /// wider than this width.
    virtual void SetMaxWidth(UInt32 maxWidth) override;

    /// \brief Sets the maximum height of the window's content area, which is the largest height that the window can be
    /// resized to.
    /// \param maxHeight The maximum height of the window's content area, in pixels. The window cannot be resized to be
    /// taller than this height.
    virtual void SetMaxHeight(UInt32 maxHeight) override;

    /// \brief Centers the window on the specified monitor, or on the primary monitor if no monitor is specified.
    /// \param monitor A pointer to the monitor to center the window on, or nullptr to center on the primary monitor. If
    /// the specified monitor is not currently available or does not exist, the behavior is implementation-defined
    /// (e.g., the window may be centered on the primary monitor instead).
    virtual void Center(const IMonitor* monitor = nullptr) override;

    /// \brief Sets the title of the window.
    /// \note The title is typically displayed in the window's title bar, and may also be used by the operating system
    /// to identify the window in task switchers, alt-tab lists, and other UI elements.
    /// \param title The title to set for the window, as a string view.
    virtual void SetTitle(FStringView title) override;

    /// \brief Sets whether the window should be displayed in fullscreen mode.
    /// \param enabled True to enable fullscreen mode, false to disable it.
    virtual void SetFullscreen(bool enabled) override;

    /// \brief Sets whether the window should be displayed in borderless mode.
    /// \param enabled True to enable borderless mode, false to disable it.
    virtual void SetBorderless(bool enabled) override;

    /// \brief Sets whether the window should be resizable by the user.
    /// \param enabled True to enable resizable mode, false to disable it.
    virtual void SetResizable(bool enabled) override;

    /// \brief Sets whether the window should be visible on the screen.
    /// \param enabled True to make the window visible, false to hide it.
    virtual void SetDecorated(bool enabled) override;

    /// \brief Sets whether the window should have a transparent background, allowing the desktop or other windows
    /// behind it to show through.
    /// \param enabled True to enable transparency, false to disable it.
    virtual void SetTransparent(bool enabled) override;

    /// \brief Sets whether the window should always be on top of other windows, even when it is not focused.
    /// \param enabled True to enable always-on-top mode, false to disable it.
    virtual void SetAlwaysOnTop(bool enabled) override;

    /// \brief Sets whether the mouse cursor should be visible when it is over the window.
    /// \param visible True to make the cursor visible, false to hide it.
    virtual void SetCursorVisibility(bool visible) override;

    /// \brief Sets whether the mouse cursor should be grabbed by the window, confining it to the window's content area
    /// and potentially hiding it.
    /// \param grabbed True to grab the cursor, false to release it.
    virtual void SetCursorGrab(bool grabbed) override;

    /// \brief Sets whether the keyboard input should be grabbed by the window, confining it to the window and
    /// potentially preventing it from being sent to other applications.
    /// \param grabbed True to grab the keyboard input, false to release it.
    virtual void SetKeyboardGrab(bool grabbed) override;

    /// \brief Sets whether the window should use high DPI mode, which allows it to take advantage of high-resolution
    /// displays and render content at a higher resolution for sharper visuals.
    /// \param enabled True to enable high DPI mode, false to disable it.
    virtual void SetHighDPI(bool enabled) override;

    /// \brief Sets whether the window should use VSync, which synchronizes the window's frame updates with the
    /// display's refresh rate to reduce screen tearing and provide smoother visuals.
    /// \param enabled True to enable VSync, false to disable it.
    virtual void SetVSync(bool enabled) override;

    /// \brief Sets the window's icon.
    /// \note The icon is typically displayed in the window's title bar and may also be used by the operating system to
    /// represent the window in task switchers, alt-tab lists, and other UI elements.
    /// \param pixels A pointer to the pixel data for the icon, in RGBA format (4 bytes per pixel).
    /// \param width The width of the icon, in pixels.
    /// \param height The height of the icon, in pixels.
    virtual void SetIcon(const UInt8* pixels, UInt32 width, UInt32 height) override;
};

}   // namespace GP::HAL
