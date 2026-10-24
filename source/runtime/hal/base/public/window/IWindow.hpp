// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "event/system/WindowEvent.hpp"
#include "window/CursorEnums.hpp"
#include "window/NativeWindow.hpp"
#include "window/WindowDesc.hpp"
#include "window/WindowEnums.hpp"
#include "window/WindowTypes.hpp"

namespace gp::hal
{

// Forward declarations.
class Display;

/// @brief Unique 32-bit identifier for an OS window instance, matching event payload windowId fields.
using WindowId = gp::UInt32;

/// @brief Sentinel constant representing an uninitialized or invalid window identifier.
inline constexpr WindowId kInvalidWindowId = 0u;

/// @brief Interface for platform-agnostic window management.
/// @details State query methods read from a local shadow cache updated deterministically via `onWindowEvent()`,
///          making them safe and zero-cost to invoke from the Game Thread without OS-thread syscalls.
class IWindow
{
public:
    /// @brief Virtual destructor to ensure proper cleanup in derived classes.
    virtual ~IWindow() noexcept = default;

public:
    /// @brief Retrieves the unique backend window identifier used to route HAL events.
    /// @return The window's unique ID, or `kInvalidWindowId` (0) if the window is not open.
    [[nodiscard]] virtual WindowId getId() const noexcept = 0;

    /// @brief Checks if the window is currently open and has not been closed by the user or the application.
    /// @return True if the window is open, false if it has been closed.
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;

    /// @brief Checks if the window is currently valid and can be interacted with.
    /// @return True if the window is valid, false if it is in an invalid state (e.g., failed to initialize).
    [[nodiscard]] virtual bool isValid() const noexcept = 0;

    /// @brief Checks whether the user or OS has requested that this window close (e.g., clicked the 'X' button).
    /// @return True if a close request is pending, false otherwise.
    [[nodiscard]] virtual bool isCloseRequested() const noexcept = 0;

    /// @brief Checks if the window is currently active (visible, not minimized, and holding input focus).
    /// @return True if the window is active, false otherwise.
    [[nodiscard]] virtual bool isActive() const noexcept = 0;

    /// @brief Checks if the window is currently focused (has keyboard input focus).
    /// @return True if the window is focused, false otherwise.
    [[nodiscard]] virtual bool isFocused() const noexcept = 0;

    /// @brief Checks if the window is currently minimized (iconified).
    /// @return True if the window is minimized, false otherwise.
    [[nodiscard]] virtual bool isMinimized() const noexcept = 0;

    /// @brief Checks if the window is currently maximized.
    /// @return True if the window is maximized, false otherwise.
    [[nodiscard]] virtual bool isMaximized() const noexcept = 0;

    /// @brief Checks if the window is resizable by the user.
    /// @return True if the window is resizable, false otherwise.
    [[nodiscard]] virtual bool isResizable() const noexcept = 0;

    /// @brief Checks if the window is currently in fullscreen mode (Fullscreen or FullscreenDesktop).
    /// @return True if the window is in fullscreen mode, false otherwise.
    [[nodiscard]] virtual bool isFullscreen() const noexcept = 0;

    /// @brief Checks if the window is currently in borderless mode.
    /// @return True if the window is in borderless mode, false otherwise.
    [[nodiscard]] virtual bool isBorderless() const noexcept = 0;

    /// @brief Checks if the window is transparent (allows underlying windows to show through).
    /// @return True if the window is transparent, false otherwise.
    [[nodiscard]] virtual bool isTransparent() const noexcept = 0;

    /// @brief Checks if the window is set to always be on top of other windows.
    /// @return True if the window is always on top, false otherwise.
    [[nodiscard]] virtual bool isAlwaysOnTop() const noexcept = 0;

    /// @brief Checks if the window is currently hovered by the mouse cursor.
    /// @return True if the window is hovered, false otherwise.
    [[nodiscard]] virtual bool isHovered() const noexcept = 0;

    /// @brief Checks if the window is currently visible to the user (not minimized or hidden).
    /// @return True if the window is visible, false otherwise.
    [[nodiscard]] virtual bool isVisible() const noexcept = 0;

    /// @brief Checks if the window is currently hidden from the user (minimized or explicitly hidden).
    /// @return True if the window is hidden, false otherwise.
    [[nodiscard]] virtual bool isHidden() const noexcept = 0;

    /// @brief Checks if the window has vertical synchronization (VSync) enabled for buffer swapping.
    /// @return True if VSync is enabled, false otherwise.
    [[nodiscard]] virtual bool isVSyncEnabled() const noexcept = 0;

    /// @brief Checks if the window is using a high-DPI backbuffer for rendering on supported displays.
    /// @return True if high-DPI mode is enabled, false otherwise.
    [[nodiscard]] virtual bool isHighDPI() const noexcept = 0;

    /// @brief Checks if the window has HDR output enabled on its current display.
    /// @return True if HDR output is active, false otherwise.
    [[nodiscard]] virtual bool isHDREnabled() const noexcept = 0;

    /// @brief Checks if the window has input focus grabbed (mouse or keyboard).
    /// @return True if the window has input focus grabbed, false otherwise.
    [[nodiscard]] virtual bool isInputGrabbed() const noexcept = 0;

    /// @brief Checks if the window has mouse input grabbed.
    /// @return True if the window has mouse input grabbed, false otherwise.
    [[nodiscard]] virtual bool isMouseGrabbed() const noexcept = 0;

    /// @brief Checks if the window has keyboard input grabbed.
    /// @return True if the window has keyboard input grabbed, false otherwise.
    [[nodiscard]] virtual bool isKeyboardGrabbed() const noexcept = 0;

    /// @brief Checks if the system cursor is currently visible when over the window.
    /// @return True if the cursor is visible, false otherwise.
    [[nodiscard]] virtual bool isCursorVisible() const noexcept = 0;

    /// @brief Checks if the system cursor is currently hidden when over the window.
    /// @return True if the cursor is hidden, false otherwise.
    [[nodiscard]] virtual bool isCursorHidden() const noexcept = 0;

    /// @brief Checks if the system cursor is currently confined to the window's client area.
    /// @return True if the cursor is confined, false otherwise.
    [[nodiscard]] virtual bool isCursorConfined() const noexcept = 0;

    /// @brief Checks if the system cursor is locked in relative 3D camera mode.
    /// @return True if the cursor is locked, false otherwise.
    [[nodiscard]] virtual bool isCursorLocked() const noexcept = 0;

    /// @brief Checks if the window is currently occluded (completely covered by other windows or off-screen).
    /// @return True if the window is occluded, false otherwise.
    [[nodiscard]] virtual bool isOccluded() const noexcept = 0;

    /// @brief Gets the current runtime visibility/lifecycle state of the window.
    /// @return The current `WindowState` enumerator.
    [[nodiscard]] virtual WindowState getWindowState() const noexcept = 0;

    /// @brief Gets the current display mode of the window.
    /// @return The current `WindowMode` enumerator.
    [[nodiscard]] virtual WindowMode getWindowMode() const noexcept = 0;

    /// @brief Gets the active cursor behavior mode for this window.
    /// @return The current `CursorMode` enumerator.
    [[nodiscard]] virtual CursorMode getCursorMode() const noexcept = 0;

    /// @brief Gets the active system cursor shape for this window.
    /// @return The current `CursorShape` enumerator.
    [[nodiscard]] virtual CursorShape getCursorShape() const noexcept = 0;

    /// @brief Gets the current position of the window's top-left client corner in screen coordinates.
    /// @return The current position of the window's top-left corner in screen coordinates.
    /// @see getFullPosition() for the position of the entire window including non-client areas.
    [[nodiscard]] virtual Point2D getPosition() const noexcept = 0;

    /// @brief Gets the current position of the entire window including non-client decorations in screen coordinates.
    /// @return The current position of the entire window including non-client areas in screen coordinates.
    /// @see getPosition() for the position of the client area (drawable region).
    [[nodiscard]] virtual Point2D getFullPosition() const noexcept = 0;

    /// @brief Gets the current size of the window's client area (drawable region) in logical screen coordinates.
    /// @return The current size of the window's client area in logical units.
    /// @see getFullSize() for the total window size including non-client areas.
    [[nodiscard]] virtual Extent2D getSize() const noexcept = 0;

    /// @brief Gets the current size of the entire window including non-client borders and title bar.
    /// @return The current size of the entire window including non-client areas.
    /// @see getSize() for the size of the client area (drawable region).
    [[nodiscard]] virtual Extent2D getFullSize() const noexcept = 0;

    /// @brief Gets the current size of the framebuffer or backbuffer in physical pixels used for RHI swapchain
    /// creation.
    /// @return The current physical pixel dimensions of the drawable backbuffer.
    /// @see getSize() for the logical size of the client area.
    [[nodiscard]] virtual Extent2D getFramebufferSize() const noexcept = 0;

    /// @brief Gets the minimum size constraints of the window's client area in pixels.
    /// @return The minimum size constraints of the window's client area in pixels.
    /// @see getMaximumSize() for the maximum size constraints of the window.
    [[nodiscard]] virtual Extent2D getMinimumSize() const noexcept = 0;

    /// @brief Gets the maximum size constraints of the window's client area in pixels.
    /// @return The maximum size constraints of the window's client area in pixels.
    /// @see getMinimumSize() for the minimum size constraints of the window.
    [[nodiscard]] virtual Extent2D getMaximumSize() const noexcept = 0;

    /// @brief Gets the native window handles required to bootstrap rendering APIs (Vulkan/D3D12/Metal).
    /// @return A tagged union containing the explicit backend type and associated handles.
    [[nodiscard]] virtual NativeWindowHandle getNativeHandle() const noexcept = 0;

    /// @brief Gets a pointer to the Display that the window is currently associated with.
    /// @return A pointer to the Display, or nullptr if not associated with any display.
    [[nodiscard]] virtual const Display* getCurrentDisplay() const noexcept = 0;

    /// @brief Gets the current OS display identifier index this window resides on.
    /// @return The 32-bit display identifier.
    [[nodiscard]] virtual gp::UInt32 getCurrentDisplayId() const noexcept = 0;

    /// @brief Gets the current content scale factor of the window (ratio between physical pixels and logical units).
    /// @return The current DPI content scale factor (1.0f = 96 DPI 1:1 scaling).
    [[nodiscard]] virtual Float32 getContentScale() const noexcept = 0;

    /// @brief Gets the global opacity of the window in the range [0.0f, 1.0f].
    /// @return Current window opacity value.
    [[nodiscard]] virtual Float32 getOpacity() const noexcept = 0;

    /// @brief Gets the current title of the window as a StringView.
    /// @return Current window title string view.
    [[nodiscard]] virtual StringView getTitle() const noexcept = 0;

    /// @brief Sets the position of the window's top-left corner in screen coordinates.
    /// @param[in] position The new position of the window's top-left corner in screen coordinates.
    virtual void setPosition(const Point2D& position) noexcept = 0;

    /// @brief Sets the position of the window's top-left corner in screen coordinates.
    /// @param[in] x The new X coordinate of the window's top-left corner in screen coordinates.
    /// @param[in] y The new Y coordinate of the window's top-left corner in screen coordinates.
    virtual void setPosition(Int32 x, Int32 y) noexcept = 0;

    /// @brief Sets the size of the window's client area (drawable region) in pixels.
    /// @param[in] size The new size of the window's client area in pixels.
    virtual void setSize(const Extent2D& size) noexcept = 0;

    /// @brief Sets the size of the window's client area (drawable region) in pixels.
    /// @param[in] width The new width of the window's client area in pixels.
    /// @param[in] height The new height of the window's client area in pixels.
    virtual void setSize(UInt32 width, UInt32 height) noexcept = 0;

    /// @brief Sets the size of the entire window including non-client areas in pixels.
    /// @param[in] size The new size of the entire window including non-client areas in pixels.
    virtual void setFullSize(const Extent2D& size) noexcept = 0;

    /// @brief Sets the size of the entire window including non-client areas in pixels.
    /// @param[in] width The new width of the entire window including non-client areas in pixels.
    /// @param[in] height The new height of the entire window including non-client areas in pixels.
    virtual void setFullSize(UInt32 width, UInt32 height) noexcept = 0;

    /// @brief Sets the minimum and maximum size constraints of the window's client area in pixels.
    /// @param[in] minimum The minimum size constraints of the window's client area in pixels.
    /// @param[in] maximum The maximum size constraints of the window's client area in pixels.
    virtual void setSizeLimits(const Extent2D& minimum, const Extent2D& maximum) noexcept = 0;

    /// @brief Sets the minimum size constraints of the window's client area in pixels.
    /// @param[in] size The minimum size constraint of the window's client area in pixels.
    virtual void setMinimumSize(const Extent2D& size) noexcept = 0;

    /// @brief Sets the minimum size constraints of the window's client area in pixels.
    /// @param[in] width The minimum width constraint of the window's client area in pixels.
    /// @param[in] height The minimum height constraint of the window's client area in pixels.
    virtual void setMinimumSize(UInt32 width, UInt32 height) noexcept = 0;

    /// @brief Sets the maximum size constraints of the window's client area in pixels.
    /// @param[in] size The maximum size constraint of the window's client area in pixels.
    virtual void setMaximumSize(const Extent2D& size) noexcept = 0;

    /// @brief Sets the maximum size constraints of the window's client area in pixels.
    /// @param[in] width The maximum width constraint of the window's client area in pixels.
    /// @param[in] height The maximum height constraint of the window's client area in pixels.
    virtual void setMaximumSize(UInt32 width, UInt32 height) noexcept = 0;

    /// @brief Sets the title of the window displayed in the title bar and taskbar/dock.
    /// @param[in] title The new UTF-8 title of the window as a string view.
    virtual void setTitle(StringView title) noexcept = 0;

    /// @brief Sets the global opacity of the window.
    /// @param[in] opacity Opacity factor clamped to [0.0f (transparent), 1.0f (opaque)].
    virtual void setOpacity(Float32 opacity) noexcept = 0;

    /// @brief Enables or disables user border-drag resizing on the window.
    /// @param[in] resizable True to allow user resizing, false to lock dimensions.
    virtual void setResizable(bool resizable) noexcept = 0;

    /// @brief Sets whether the window should float above all normal non-topmost windows.
    /// @param[in] alwaysOnTop True to keep window on top, false for standard Z-order.
    virtual void setAlwaysOnTop(bool alwaysOnTop) noexcept = 0;

    /// @brief Sets the VSync preference flag for swapchain presentation on this window.
    /// @param[in] enabled True to enable vertical synchronization, false to disable.
    virtual void setVSyncEnabled(bool enabled) noexcept = 0;

    /// @brief Sets the comprehensive cursor interaction mode (Normal, Hidden, Locked, Confined).
    /// @param[in] mode The desired `CursorMode` behavior.
    virtual void setCursorMode(CursorMode mode) noexcept = 0;

    /// @brief Sets the visibility of the system cursor when over the window.
    /// @param[in] visible True to make the cursor visible, false to hide it.
    virtual void setCursorVisible(bool visible) noexcept = 0;

    /// @brief Sets whether the system cursor is confined to the window's client area.
    /// @param[in] confined True to confine the cursor to the window's client area.
    virtual void setCursorConfined(bool confined) noexcept = 0;

    /// @brief Sets whether mouse input is explicitly grabbed by this window.
    /// @param[in] grabbed True to grab mouse input, false to release.
    virtual void setMouseGrabbed(bool grabbed) noexcept = 0;

    /// @brief Sets whether system keyboard shortcuts (e.g., Alt+Tab, Win key) are grabbed by this window.
    /// @param[in] grabbed True to grab keyboard input, false to release.
    virtual void setKeyboardGrabbed(bool grabbed) noexcept = 0;

    /// @brief Sets the cursor shape to be used when the cursor is over the window.
    /// @param[in] shape The cursor shape to use when the cursor is over the window.
    virtual void setCursorShape(CursorShape shape) noexcept = 0;

    /// @brief Warps the cursor to the specified position relative to the window's client area.
    /// @param[in] position The new position of the cursor relative to the window's client area.
    virtual void setCursorPosition(const Point2D& position) noexcept = 0;

    /// @brief Sets the window mode (Windowed, Borderless, Fullscreen, FullscreenDesktop).
    /// @param[in] mode The desired window mode to set.
    /// @param[in] targetDisplay Optional pointer to the Display to use for fullscreen modes.
    virtual void setWindowMode(WindowMode mode, const Display* targetDisplay = nullptr) noexcept = 0;

    /// @brief Opens the window and creates the underlying native OS window resources.
    virtual void open() noexcept = 0;

    /// @brief Closes the window immediately and releases all associated native OS resources.
    virtual void close() noexcept = 0;

    /// @brief Flags a soft close request on the window without immediately destroying native handles.
    /// @details Allows the Application loop to prompt for unsaved work or clean up RHI swapchains before `close()`.
    virtual void requestClose() noexcept = 0;

    /// @brief Clears a pending close request if the application vetoed closing the window.
    virtual void clearCloseRequest() noexcept = 0;

    /// @brief Shows the window if it is currently hidden.
    virtual void show() noexcept = 0;

    /// @brief Hides the window if it is currently visible.
    virtual void hide() noexcept = 0;

    /// @brief Minimizes the window to the system taskbar or dock.
    virtual void minimize() noexcept = 0;

    /// @brief Maximizes the window to fill the desktop work area.
    virtual void maximize() noexcept = 0;

    /// @brief Restores the window to its normal state if it is currently minimized or maximized.
    virtual void restore() noexcept = 0;

    /// @brief Raises the window to the foreground and requests keyboard input focus.
    virtual void focus() noexcept = 0;

    /// @brief Flashes the window in the taskbar or dock to request user attention.
    /// @param[in] mode The flash mode (`None`, `UntilFocused`, `Briefly`).
    virtual void requestAttention(WindowFlashMode mode = WindowFlashMode::UntilFocused) noexcept = 0;

    /// @brief Centers the window on the specified display (or its current display if `nullptr`).
    /// @param[in] display Optional target display on which to center the window.
    virtual void centerOnDisplay(const Display* display = nullptr) noexcept = 0;

    /// @brief Updates the window's internal shadow state from a decoded HAL `WindowEvent`.
    /// @note Invoked by the Application / WindowManager when draining the lock-free `SPSCByteRingBuffer`.
    /// @param[in] ev The decoded window event packet.
    virtual void onWindowEvent(const event::WindowEvent& ev) noexcept = 0;
};

}   // namespace gp::hal
