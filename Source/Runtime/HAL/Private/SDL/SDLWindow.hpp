// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Windowing/BaseWindow.hpp"
#include "Windowing/WindowDesc.hpp"
#include <SDL3/SDL.h>

namespace GP::HAL
{

class SDLWindowingSystem;

/// \brief SDL3-backed concrete implementation of IWindow.
///
/// SDLWindow translates every platform-agnostic IWindow call into the corresponding SDL3 API call and
/// feeds incoming SDL_Events back into the engine event system via DispatchSDLEvent(). All cached state
/// is kept consistent with SDL by re-querying the actual SDL window state whenever SDL fires an event
/// rather than relying solely on the values the caller provided.
///
/// Ownership is managed by SDLWindowingSystem. Do not create or destroy SDLWindow instances directly.
class SDLWindow final : public FBaseWindow
{
private:
    SDL_Window* m_window{ nullptr };          //!< Underlying SDL window handle; null until successfully created.
    SDL_Cursor* m_cursor{ nullptr };          //!< Currently active SDL system cursor; null = platform default.
    SDLWindowingSystem* m_owner{ nullptr };   //!< Non-owning back-pointer to the windowing system.

public:
    /// \brief Default-constructs a headless SDLWindow with no underlying OS window.
    /// \note Useful for unit-test or server-side contexts where no display is available.
    SDLWindow();

    /// \brief Constructs and opens an OS window from a fully populated descriptor.
    /// \param desc  Creation parameters (size, mode, style flags, etc.).
    /// \param owner Non-null back-pointer to the SDLWindowingSystem that owns this instance.
    explicit SDLWindow(const FWindowDesc& desc, SDLWindowingSystem* owner);

    /// \brief Destroys the SDL cursor and the SDL window handle.
    ~SDLWindow() override;

    /// Deleted copy/move constructors and assignment operators to prevent slicing and ensure unique ownership
    /// semantics.
    SDLWindow(const SDLWindow&) = delete;
    SDLWindow& operator=(const SDLWindow&) = delete;
    SDLWindow(SDLWindow&&) = delete;
    SDLWindow& operator=(SDLWindow&&) = delete;

public:
    /// \brief Routes an SDL_Event received from the OS to the appropriate IWindow delegate.
    /// \param event The raw SDL_Event to process. Only window events whose windowID matches this instance are handled.
    void DispatchSDLEvent(const SDL_Event& event);

    /// \brief Returns the raw SDL_Window handle.
    /// \return Pointer to the underlying SDL_Window, or nullptr if the window was never successfully created.
    GP_NODISCARD SDL_Window* GetSDLWindow() const noexcept;

    /// \brief Returns the SDL-assigned numeric identifier for this window.
    /// \return The SDL_WindowID used for event routing, or 0 if the handle is null.
    GP_NODISCARD SDL_WindowID GetSDLWindowID() const noexcept;

public:
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

    /// \brief Sets the cursor mode, which determines how the mouse cursor behaves when it is over the window.
    /// \param mode The cursor mode to set.
    virtual void SetCursorMode(ECursorMode mode) override;

    /// \brief Sets the cursor shape, which determines the appearance of the mouse cursor when it is over the window.
    /// \param shape The cursor shape to set.
    virtual void SetCursorShape(ECursorShape shape) override;

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

    /// \brief Sets the size of the window's content area, which is the area available for rendering and displaying
    /// content, excluding title bars and borders.
    /// \param width The width of the window's content area, in pixels.
    /// \param height The height of the window's content area, in pixels.
    virtual void SetSize(UInt32 width, UInt32 height) override;

    /// \brief Sets the full size of the window, including title bars and borders.
    /// \param width The full width of the window, in pixels.
    /// \param height The full height of the window, in pixels.
    virtual void SetFullWindowSize(UInt32 width, UInt32 height) override;

    /// \brief Sets the position of the window on the screen, relative to the top-left corner of the primary monitor.
    /// \param x The X coordinate of the window's position, in pixels.
    /// \param y The Y coordinate of the window's position, in pixels.
    virtual void SetPosition(UInt32 x, UInt32 y) override;

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

    /// \brief Sets the aspect ratio of the window's content area, which is the ratio of the width to the height.
    /// \param aspectRatio The aspect ratio to set for the window's content area, defined as the width divided by the
    /// height.
    virtual void SetAspectRatio(Float32 aspectRatio) override;

    /// \brief Sets the aspect ratio of the window's content area, which is the ratio of the width to the height.
    /// \param numerator The numerator of the aspect ratio to set for the window's content area.
    /// \param denominator The denominator of the aspect ratio to set for the window's content area.
    virtual void SetAspectRatio(UInt32 numerator, UInt32 denominator) override;

    /// \brief Centers the window on the specified monitor, or on the primary monitor if no monitor is specified.
    /// \param monitor A pointer to the monitor to center the window on, or nullptr to center on the primary monitor. If
    /// the specified monitor is not currently available or does not exist, the behavior is implementation-defined
    /// (e.g., the window may be centered on the primary monitor instead).
    virtual void Center(const IMonitor* monitor = nullptr) override;

    /// \brief Sets the opacity of the window, which determines how transparent or opaque the window appears on the
    /// screen.
    /// \note The opacity value should be in the range [0.0, 1.0], where 0.0 is fully transparent and 1.0 is fully
    /// opaque.
    /// \param opacity The opacity value to set for the window.
    virtual void SetOpacity(Float32 opacity) override;

    /// \brief Sets the title of the window.
    /// \note The title is typically displayed in the window's title bar, and may also be used by the operating system
    /// to identify the window in task switchers, alt-tab lists, and other UI elements.
    /// \param title The title to set for the window, as a string view.
    virtual void SetTitle(FStringView title) override;

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

    /// \brief Gets the monitor that the window is currently on.
    /// \return A pointer to the monitor that the window is currently on, or nullptr if the window is not currently on
    /// any monitor.
    GP_NODISCARD virtual const IMonitor* GetCurrentMonitor() const noexcept override;

    /// \brief Gets the native handle of the window, which is a platform-specific identifier that can be used to perform
    /// low-level operations on the window.
    /// \return The native handle of the window, as a void pointer. The actual type and meaning of the handle depends on
    /// the platform and windowing system being used (e.g., HWND on Windows, NSWindow* on macOS, etc.).
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept override;

private:
    /// \brief Translates a FWindowDesc into the SDL_WindowFlags bitmask used at creation time.
    /// \param desc The window creation descriptor.
    /// \return The combined SDL_WindowFlags value.
    GP_NODISCARD static SDL_WindowFlags BuildSDLFlags(const FWindowDesc& desc) noexcept;

    /// \brief Re-queries SDL for the current window state and updates all protected FBaseWindow members.
    /// Call this after creation or after any bulk-state change to guarantee consistency.
    void SyncStateFromSDL();

    /// \brief Destroys the current SDL cursor (if any) and installs a new system cursor for the given shape.
    /// \param shape The logical cursor shape to apply.
    void ApplySDLCursorShape(ECursorShape shape);

    /// \brief Applies the given cursor mode to the SDL window (show/hide/relative/grab).
    /// \param mode The logical cursor mode to apply.
    void ApplySDLCursorMode(ECursorMode mode);
};

}   // namespace GP::HAL
