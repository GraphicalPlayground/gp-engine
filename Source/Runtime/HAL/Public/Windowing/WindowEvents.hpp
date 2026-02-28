// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Events/EventsForward.hpp"
#include "HALBuild.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"
#include "Windowing/WindowEnums.hpp"

namespace GP::HAL
{

/// \brief Forward declaration of the IWindow interface.
class IWindow;

/// \brief Forward declaration of the IMonitor interface.
class IMonitor;

}   // namespace GP::HAL

namespace GP
{

/// \brief Event definitions for HAL::IWindow. Each nested struct represents a distinct event type that can be fired by
/// window instances. Event structs may contain relevant data for that event (e.g., new size for resize events).
template <>
struct FEvents<HAL::IWindow>
{
public:
    /// \brief Fired immediately after the window has been fully closed and its resources released. After this event,
    /// any pointer to the IWindow is invalid.
    struct Closed
    {};

    /// \brief Fired when the user or OS requests that the window be closed (e.g., clicking the X button, pressing
    /// Alt+F4). The window is NOT yet destroyed at the point this event fires; the application may cancel the close by
    /// ignoring this event.
    /// \note To forcibly destroy the window, call IWindow::Close() from the handler.
    struct CloseRequested
    {};

    /// \brief Fired when the window's top-left corner moves to a new position on the virtual desktop.
    /// \note Coordinates are in screen pixels relative to the primary monitor's top-left corner. On multi-monitor
    /// setups, x and y may be negative.
    struct Moved
    {
        Int32 x{};   //<! New X coordinate of the window's top-left corner.
        Int32 y{};   //<! New Y coordinate of the window's top-left corner.
    };

    /// \brief Fired when the window's content area (the drawable region, excluding title bars and borders) is resized.
    /// \note This is the canonical event to use for updating swap chain or render target sizes. On high-DPI displays,
    /// the framebuffer may differ from these values; listen to FWindowFramebufferResizedEvent for the physical pixel
    /// dimensions.
    struct Resized
    {
    public:
        UInt32 width{};    //<! New width of the content area, in logical pixels.
        UInt32 height{};   //<! New height of the content area, in logical pixels.

    public:
        /// \brief Convenience: returns the new size as an FIntExtent2D.
        /// \return The new size of the window's content area, in pixels.
        GP_NODISCARD FIntExtent2D ToExtent() const noexcept { return FIntExtent2D{ width, height }; }

        /// \brief Convenience: returns the new aspect ratio (width / height). Returns 0 if height is 0.
        /// \return The new aspect ratio of the window's content area.
        GP_NODISCARD Float32 GetAspectRatio() const noexcept
        {
            return (height > 0) ? (static_cast<Float32>(width) / static_cast<Float32>(height)) : 0.0f;
        }
    };

    /// \brief Fired when the framebuffer (physical pixel surface) is resized. On high-DPI (Retina/4K) displays, the
    /// framebuffer resolution differs from the logical content area size. Always use this event to resize GPU resources
    /// (render targets, depth buffers, swap chains) to avoid blurriness or incorrect clipping. On standard-DPI
    /// displays, this event fires concurrently with FWindowResizedEvent and reports the same dimensions.
    struct FramebufferResized
    {
    public:
        UInt32 width{};    //<! New width of the framebuffer, in physical pixels.
        UInt32 height{};   //<! New height of the framebuffer, in physical pixels.

    public:
        /// \brief Convenience: returns the new framebuffer size as an FIntExtent2D.
        /// \return The new size of the framebuffer, in physical pixels.
        GP_NODISCARD FIntExtent2D ToExtent() const noexcept { return FIntExtent2D{ width, height }; }
    };

    /// \brief Fired when the window gains OS input focus (becomes the active foreground window).
    struct FocusGained
    {};

    /// \brief Fired when the window loses OS input focus (another window becomes foreground).
    struct FocusLost
    {};

    /// \brief Fired when the window becomes visible to the user for the first time, or after a call to IWindow::Show().
    struct Shown
    {};

    /// \brief Fired when the window is hidden from view via IWindow::Hide().
    struct Hidden
    {};

    /// \brief Fired when the window is minimized (iconified) to the taskbar or dock.
    struct Minimized
    {};

    /// \brief Fired when the window is maximized to fill the available screen area.
    struct Maximized
    {};

    /// \brief Fired when the window is restored from a minimized or maximized state back to its previous size and
    /// position.
    struct Restored
    {};

    /// \brief Fired when the content scale factor of the window changes, typically because the window has been moved to
    /// a monitor with a different DPI, or the system DPI settings have changed while the window is open.
    ///
    /// The content scale defines the ratio between logical coordinates (used by the OS and input system) and physical
    /// pixels (used by the GPU). A scale of 2.0 means each logical pixel corresponds to 2x2 physical pixels (e.g.,
    /// macOS Retina / 4K HiDPI displays).
    struct ContentScaleChanged
    {
    public:
        Float32 scaleX{ 1.0f };   //<! New horizontal content scale factor.
        Float32 scaleY{ 1.0f };   //<! New vertical content scale factor.

    public:
        /// \brief Returns true if the display is a high-DPI surface (any scale axis > 1.0).
        /// \return True if the window is on a high-DPI display, false otherwise.
        GP_NODISCARD bool IsHighDPI() const noexcept { return (scaleX > 1.0f) || (scaleY > 1.0f); }
    };

    /// \brief Fired when the window is moved between monitors with different refresh rates, which may require the
    /// render loop to update its VSync interval.
    struct RefreshRateChanged
    {
        UInt32 refreshRateHz{ 60 };   //<! New refresh rate of the monitor the window is on, in Hz.
    };

    /// \brief Fired when the window transitions between window modes (e.g., from Windowed to Fullscreen, or Fullscreen
    /// to Borderless).
    struct ModeChanged
    {
        HAL::EWindowMode previousMode = HAL::EWindowMode::Windowed;   //<! The mode before the transition.
        HAL::EWindowMode newMode = HAL::EWindowMode::Windowed;        //<! The mode after the transition.
    };

    /// \brief Fired when the window's style flags change (e.g., decorations are added or removed, resizability is
    /// toggled).
    struct StyleChanged
    {
        HAL::EWindowStyle previousStyle = HAL::EWindowStyle::Default;   //<! The style before the change.
        HAL::EWindowStyle newStyle = HAL::EWindowStyle::Default;        //<! The style after the change.
    };

    /// \brief Fired when the window's contents are damaged and need to be redrawn. This is primarily relevant on
    /// platforms (e.g., macOS, Wayland) where the OS requests explicit window refreshes rather than providing a
    /// continuous render loop.
    struct RefreshRequested
    {};

    /// \brief Fired when the window is completely occluded by other windows and its contents are not visible to the
    /// user.
    /// \note This can be used to pause rendering to save power and GPU bandwidth.
    struct Occluded
    {};

    /// \brief Fired when the window transitions from being fully occluded back to being at least partially visible.
    struct Revealed
    {};

    /// \brief Fired when the window is moved to a different monitor, which may have different DPI, refresh rate, or
    /// other characteristics.
    struct MonitorChanged
    {
        HAL::IMonitor* previousMonitor{ nullptr };   //<! The monitor the window was previously on.
        HAL::IMonitor* newMonitor{ nullptr };        //<! The monitor the window is now on.
    };

    /// \brief Fired when the mouse cursor enters the window's content area.
    struct MouseEntered
    {};

    /// \brief Fired when the mouse cursor leaves the window's content area.
    struct MouseLeft
    {};
};

namespace HAL
{

/// \brief Alias for the window events struct, for easier usage in code that handles window events.
using FWindowEvents = FEvents<HAL::IWindow>;

}   // namespace HAL

}   // namespace GP
