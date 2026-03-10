// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"

namespace GP
{
// Forward declarations
class IDisplay;

/// @brief Fired after the window's underlying platform resource has been
///        successfully created and the window is ready for use.
struct FWindowOpenedEvent
{};

/// @brief Fired when a close request is received (e.g. Alt+F4, OS chrome X button,
///        or an explicit IWindow::RequestClose() call).
struct FWindowCloseRequestedEvent
{};

/// @brief Fired immediately before the window's platform resource is destroyed.
/// After this event returns no further events will be fired by this window.
struct FWindowClosedEvent
{};

/// @brief Fired whenever the window's top-left corner moves in screen space.
///
/// Both positions are in screen coordinates (pixels). The delta is provided
/// for convenience; it equals \c newPosition - \c oldPosition.
struct FWindowMovedEvent
{
    FIntPoint2D oldPosition;   //<! Client-area position before the move.
    FIntPoint2D newPosition;   //<! Client-area position after the move.
    FIntPoint2D delta;         //<! Signed pixel offset of the move.
};

/// @brief Fired whenever the client area (drawable region) changes size.
///
/// \note  This event fires for programmatic resizes (SetSize) and user-driven
///        drags alike. It does NOT fire for framebuffer-only changes caused by
///        DPI transitions; see FWindowFramebufferResizedEvent for that.
struct FWindowResizedEvent
{
    FIntExtent2D oldSize;   //<! Client-area size before the resize.
    FIntExtent2D newSize;   //<! Client-area size after the resize.
};

/// @brief Fired whenever the GPU render target (framebuffer) changes size.
///
/// On high-DPI displays the framebuffer dimensions may exceed the client-area
/// dimensions reported by FWindowResizedEvent. Always use this event to resize
/// swap-chain / render targets, not FWindowResizedEvent.
///
/// \note  This event may fire without FWindowResizedEvent (DPI scale change),
///        and FWindowResizedEvent may fire without this event (logical resize
///        that lands on the same physical pixel count).
struct FWindowFramebufferResizedEvent
{
    FIntExtent2D oldSize;   //<! Framebuffer size before the change.
    FIntExtent2D newSize;   //<! Framebuffer size after the change.
};

/// @brief Fired when the window's minimum and/or maximum size limits change.
///
/// A zero extent in either field means that dimension has no constraint.
struct FWindowSizeLimitsChangedEvent
{
    FIntExtent2D minimumSize;   //<! New minimum allowed size.
    FIntExtent2D maximumSize;   //<! New maximum allowed size.
};

/// @brief Fired when the window gains OS-level keyboard focus.
struct FWindowFocusGainedEvent
{};

/// @brief Fired when the window loses OS-level keyboard focus.
struct FWindowFocusLostEvent
{};

/// @brief Fired when the window becomes the active (foreground) window.
///
/// \note  On some platforms this coincides with FWindowFocusGainedEvent;
///        on others (e.g. Windows MDI) a window can be active without focus.
struct FWindowActivatedEvent
{};

/// @brief Fired when the window is no longer the active (foreground) window.
struct FWindowDeactivatedEvent
{};

/// @brief Fired when the window transitions from hidden to visible.
struct FWindowShownEvent
{};

/// @brief Fired when the window transitions from visible to hidden.
struct FWindowHiddenEvent
{};

/// @brief Fired when the window is minimised to the taskbar / dock.
struct FWindowMinimizedEvent
{};

/// @brief Fired when the window is maximised to fill the available work area.
struct FWindowMaximizedEvent
{};

/// @brief Fired when the window returns from a minimised or maximised state.
struct FWindowRestoredEvent
{};

/// @brief Fired when the window's occlusion (covered-by-another-window) state changes.
struct FWindowOcclusionChangedEvent
{
    bool isOccluded;   //<! True if the window is now fully occluded.
};

/// @brief Fired when the window enters or exits fullscreen mode.
struct FWindowFullscreenChangedEvent
{
    bool isFullscreen;   //<! True if the window is now in fullscreen mode.
};

/// @brief Fired when the window's border/chrome visibility changes.
struct FWindowBorderlessChangedEvent
{
    bool isBorderless;   //<! True if borders are now hidden.
};

/// @brief Fired when the window moves to a different physical display.
///
/// \note  Either pointer may be nullptr if the display is unknown or not
///        applicable on the current platform.
struct FWindowDisplayChangedEvent
{
    const IDisplay* oldDisplay;   //<! Display the window was on before.
    const IDisplay* newDisplay;   //<! Display the window is now on.
};

/// @brief Fired when the content scale (DPI ratio) of the display changes.
///
/// This may occur when the user drags the window to a different monitor with a
/// different pixel density, or changes the OS display scaling setting.
struct FWindowDPIScaleChangedEvent
{
    Float32 oldScale;   //<! Previous display content-scale factor.
    Float32 newScale;   //<! New display content-scale factor.
};

/// @brief Fired when vertical synchronisation is toggled on or off.
struct FWindowVSyncChangedEvent
{
    bool isEnabled;   //<! True if VSync is now enabled.
};

/// @brief Fired when the mouse cursor enters the window's client area.
struct FWindowMouseEnteredEvent
{};

/// @brief Fired when the mouse cursor leaves the window's client area.
struct FWindowMouseLeftEvent
{};

/// @brief Fired when mouse grab (pointer lock / relative mode) is toggled.
struct FWindowMouseGrabChangedEvent
{
    bool isGrabbed;   //<! True if the mouse is now grabbed.
};

/// @brief Fired when cursor visibility is toggled inside the window.
struct FWindowCursorVisibilityChangedEvent
{
    bool isVisible;   //<! True if the cursor is now visible.
};

/// @brief Fired when cursor confinement (restricted to window bounds) changes.
struct FWindowCursorConfinedChangedEvent
{
    bool isConfined;   //<! True if the cursor is now confined to the window.
};

/// @brief Fired when keyboard grab (exclusive keyboard capture) is toggled.
struct FWindowKeyboardGrabChangedEvent
{
    bool isGrabbed;   //<! True if keyboard input is now exclusively grabbed.
};

/// @brief Fired when the aggregate input grab state changes.
///
/// Input grab is the logical OR of mouse grab and keyboard grab. This event is
/// fired after either FWindowMouseGrabChangedEvent or
/// FWindowKeyboardGrabChangedEvent changes the combined state.
struct FWindowInputGrabChangedEvent
{
    bool isGrabbed;           //<! True if any input device is now grabbed.
    bool isMouseGrabbed;      //<! Current mouse grab sub-state.
    bool isKeyboardGrabbed;   //<! Current keyboard grab sub-state.
};

}   // namespace GP
