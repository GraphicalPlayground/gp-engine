// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"
#include "Templates/Events/Event.hpp"
#include "Window/WindowDesc.hpp"
#include "Window/WindowEnums.hpp"
#include "Window/WindowEvents.hpp"

namespace GP
{

// Forward declarations

class IDisplay;

/// @brief Interface for platform-agnostic window management.
class IWindow
{
protected:
    ///
    /// @section Events
    ///

    // clang-format off

    // Lifecycle
    TEvent<void(const FWindowOpenedEvent&)> m_onOpened;
    TEvent<void(const FWindowCloseRequestedEvent&)> m_onCloseRequested;
    TEvent<void(const FWindowClosedEvent&)> m_onClosed;

    // Position
    TEvent<void(const FWindowMovedEvent&)> m_onMoved;

    // Size
    TEvent<void(const FWindowResizedEvent&)> m_onResized;
    TEvent<void(const FWindowFramebufferResizedEvent&)> m_onFramebufferResized;
    TEvent<void(const FWindowSizeLimitsChangedEvent&)> m_onSizeLimitsChanged;

    // Focus & Activity
    TEvent<void(const FWindowFocusGainedEvent&)> m_onFocusGained;
    TEvent<void(const FWindowFocusLostEvent&)> m_onFocusLost;
    TEvent<void(const FWindowActivatedEvent&)> m_onActivated;
    TEvent<void(const FWindowDeactivatedEvent&)> m_onDeactivated;

    // Visibility & State
    TEvent<void(const FWindowShownEvent&)> m_onShown;
    TEvent<void(const FWindowHiddenEvent&)> m_onHidden;
    TEvent<void(const FWindowMinimizedEvent&)> m_onMinimized;
    TEvent<void(const FWindowMaximizedEvent&)> m_onMaximized;
    TEvent<void(const FWindowRestoredEvent&)> m_onRestored;
    TEvent<void(const FWindowOcclusionChangedEvent&)> m_onOcclusionChanged;

    // Fullscreen & Borderless
    TEvent<void(const FWindowFullscreenChangedEvent&)> m_onFullscreenChanged;
    TEvent<void(const FWindowBorderlessChangedEvent&)> m_onBorderlessChanged;

    // Display
    TEvent<void(const FWindowDisplayChangedEvent&)> m_onDisplayChanged;
    TEvent<void(const FWindowDPIScaleChangedEvent&)> m_onDPIScaleChanged;

    // VSync
    TEvent<void(const FWindowVSyncChangedEvent&)> m_onVSyncChanged;

    // Mouse & Cursor
    TEvent<void(const FWindowMouseEnteredEvent&)> m_onMouseEntered;
    TEvent<void(const FWindowMouseLeftEvent&)> m_onMouseLeft;
    TEvent<void(const FWindowMouseGrabChangedEvent&)> m_onMouseGrabChanged;
    TEvent<void(const FWindowCursorVisibilityChangedEvent&)> m_onCursorVisibilityChanged;
    TEvent<void(const FWindowCursorConfinedChangedEvent&)> m_onCursorConfinedChanged;

    // Keyboard & Aggregate Input
    TEvent<void(const FWindowKeyboardGrabChangedEvent&)> m_onKeyboardGrabChanged;
    TEvent<void(const FWindowInputGrabChangedEvent&)> m_onInputGrabChanged;

public:
    // Lifecycle
    TEventView<void(const FWindowOpenedEvent&)> OnOpened{ m_onOpened };
    TEventView<void(const FWindowCloseRequestedEvent&)> OnCloseRequested{ m_onCloseRequested };
    TEventView<void(const FWindowClosedEvent&)> OnClosed{ m_onClosed };

    // Position
    TEventView<void(const FWindowMovedEvent&)> OnMoved{ m_onMoved };

    // Size
    TEventView<void(const FWindowResizedEvent&)> OnResized{ m_onResized };
    TEventView<void(const FWindowFramebufferResizedEvent&)> OnFramebufferResized{ m_onFramebufferResized };
    TEventView<void(const FWindowSizeLimitsChangedEvent&)> OnSizeLimitsChanged{ m_onSizeLimitsChanged };

    // Focus & Activity
    TEventView<void(const FWindowFocusGainedEvent&)> OnFocusGained{ m_onFocusGained };
    TEventView<void(const FWindowFocusLostEvent&)> OnFocusLost{ m_onFocusLost };
    TEventView<void(const FWindowActivatedEvent&)> OnActivated{ m_onActivated };
    TEventView<void(const FWindowDeactivatedEvent&)> OnDeactivated{ m_onDeactivated };

    // Visibility & State
    TEventView<void(const FWindowShownEvent&)> OnShown{ m_onShown };
    TEventView<void(const FWindowHiddenEvent&)> OnHidden{ m_onHidden };
    TEventView<void(const FWindowMinimizedEvent&)> OnMinimized{ m_onMinimized };
    TEventView<void(const FWindowMaximizedEvent&)> OnMaximized{ m_onMaximized };
    TEventView<void(const FWindowRestoredEvent&)> OnRestored{ m_onRestored };
    TEventView<void(const FWindowOcclusionChangedEvent&)> OnOcclusionChanged{ m_onOcclusionChanged };

    // Fullscreen & Borderless
    TEventView<void(const FWindowFullscreenChangedEvent&)> OnFullscreenChanged{ m_onFullscreenChanged };
    TEventView<void(const FWindowBorderlessChangedEvent&)> OnBorderlessChanged{ m_onBorderlessChanged };

    // Display
    TEventView<void(const FWindowDisplayChangedEvent&)> OnDisplayChanged{ m_onDisplayChanged };
    TEventView<void(const FWindowDPIScaleChangedEvent&)> OnDPIScaleChanged{ m_onDPIScaleChanged };

    // VSync
    TEventView<void(const FWindowVSyncChangedEvent&)> OnVSyncChanged{ m_onVSyncChanged };

    // Mouse & Cursor
    TEventView<void(const FWindowMouseEnteredEvent&)> OnMouseEntered{ m_onMouseEntered };
    TEventView<void(const FWindowMouseLeftEvent&)> OnMouseLeft{ m_onMouseLeft };
    TEventView<void(const FWindowMouseGrabChangedEvent&)> OnMouseGrabChanged{ m_onMouseGrabChanged };
    TEventView<void(const FWindowCursorVisibilityChangedEvent&)> OnCursorVisibilityChanged{ m_onCursorVisibilityChanged };
    TEventView<void(const FWindowCursorConfinedChangedEvent&)> OnCursorConfinedChanged{ m_onCursorConfinedChanged };

    // Keyboard & Aggregate Input
    TEventView<void(const FWindowKeyboardGrabChangedEvent&)> OnKeyboardGrabChanged{ m_onKeyboardGrabChanged };
    TEventView<void(const FWindowInputGrabChangedEvent&)> OnInputGrabChanged{ m_onInputGrabChanged };

    // clang-format on

public:
    virtual ~IWindow() = default;

public:
    ///
    /// @section State queries
    ///

    GP_NODISCARD virtual bool IsOpen() const noexcept = 0;
    GP_NODISCARD virtual bool IsValid() const noexcept = 0;
    GP_NODISCARD virtual bool IsActive() const noexcept = 0;
    GP_NODISCARD virtual bool IsFocused() const noexcept = 0;
    GP_NODISCARD virtual bool IsMinimized() const noexcept = 0;
    GP_NODISCARD virtual bool IsMaximized() const noexcept = 0;
    GP_NODISCARD virtual bool IsResizable() const noexcept = 0;
    GP_NODISCARD virtual bool IsFullscreen() const noexcept = 0;
    GP_NODISCARD virtual bool IsBorderless() const noexcept = 0;
    GP_NODISCARD virtual bool IsTransparent() const noexcept = 0;
    GP_NODISCARD virtual bool IsAlwaysOnTop() const noexcept = 0;
    GP_NODISCARD virtual bool IsHovered() const noexcept = 0;
    GP_NODISCARD virtual bool IsVisible() const noexcept = 0;
    GP_NODISCARD virtual bool IsHidden() const noexcept = 0;
    GP_NODISCARD virtual bool IsVSyncEnabled() const noexcept = 0;
    GP_NODISCARD virtual bool IsHighDPI() const noexcept = 0;
    GP_NODISCARD virtual bool IsInputGrabbed() const noexcept = 0;
    GP_NODISCARD virtual bool IsMouseGrabbed() const noexcept = 0;
    GP_NODISCARD virtual bool IsKeyboardGrabbed() const noexcept = 0;
    GP_NODISCARD virtual bool IsCursorVisible() const noexcept = 0;
    GP_NODISCARD virtual bool IsCursorHidden() const noexcept = 0;
    GP_NODISCARD virtual bool IsCursorConfined() const noexcept = 0;
    GP_NODISCARD virtual bool IsOccluded() const noexcept = 0;

    ///
    /// @section Getters
    ///

    /// @note GetPosition() returns the position of the top-left corner of the window in screen coordinates. This may be
    ///       different from the position of the client area if the window has borders or a title
    /// @see GetFullPosition() for the position of the entire window including non-client areas.
    GP_NODISCARD virtual FIntPoint2D GetPosition() const noexcept = 0;

    /// @note GetFullPosition() returns the position of the top-left corner of the entire window including non-client
    ///       areas like borders and title bar. This may differ from GetPosition() if the window has non-client areas.
    /// @see GetPosition() for the position of the client area (drawable region).
    GP_NODISCARD virtual FIntPoint2D GetFullPosition() const noexcept = 0;

    /// @note GetSize() returns the size of the window's client area (drawable region).
    /// @see GetFullSize() for the total window size including non-client areas.
    GP_NODISCARD virtual FIntExtent2D GetSize() const noexcept = 0;
    /// @note GetFullSize() returns the total size of the window including non-client areas like title bar and borders.
    /// @see GetSize() for the size of the client area (drawable region).
    GP_NODISCARD virtual FIntExtent2D GetFullSize() const noexcept = 0;

    /// @note GetFramebufferSize() returns the size of the framebuffer (render target) associated with the window. This
    ///       may differ from the client area size returned by GetSize() due to high-DPI scaling or other factors.
    /// @see GetSize() for the size of the client area (drawable region).
    GP_NODISCARD virtual FIntExtent2D GetFramebufferSize() const noexcept = 0;

    /// @note GetMinimumSize() returns the minimum size constraints of the window, if any. This represents the smallest
    ///       allowed dimensions for the window. If the window has no minimum size constraints, this return (0, 0).
    /// @see GetMaximumSize() for the maximum size constraints of the window.
    GP_NODISCARD virtual FIntExtent2D GetMinimumSize() const noexcept = 0;

    /// @note GetMaximumSize() returns the maximum size constraints of the window, if any. This represents the largest
    ///       allowed dimensions for the window. If the window has no maximum size constraints, this return (0, 0).
    /// @see GetMinimumSize() for the minimum size constraints of the window.
    GP_NODISCARD virtual FIntExtent2D GetMaximumSize() const noexcept = 0;

    /// @note GetNativeHandle() returns a pointer to the underlying native window handle or object. The actual type and
    ///       meaning of this handle is platform-specific. It may be used for advanced operations that are not covered
    ///       by this interface, but the caller must know the expected type and semantics of the handle for the specific
    ///       platform. This may return nullptr if no native handle is available.
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept = 0;

    /// @note GetCurrentDisplay() returns a pointer to the IDisplay that the window is currently on.
    GP_NODISCARD virtual const IDisplay* GetCurrentDisplay() const noexcept = 0;

    ///
    /// @section Setters
    ///

    /// @note SetPosition() sets the position of the window's top-left corner in screen coordinates.
    virtual void SetPosition(const FIntPoint2D& position) noexcept = 0;
    virtual void SetPosition(const Int32 x, const Int32 y) noexcept = 0;

    /// @note SetSize() sets the size of the window's client area (drawable region). This may not change the actual size
    ///       of the window if there are minimum/maximum size constraints or if the window is not resizable.
    /// @see SetFullSize() to set the total window size including non-client areas.
    virtual void SetSize(const FIntExtent2D& size) noexcept = 0;
    virtual void SetSize(const UInt32 width, const UInt32 height) noexcept = 0;

    /// @note SetFullSize() sets the total size of the window including non-client areas like title bar and borders.
    ///       This may not change the actual size of the window if there are minimum/maximum size constraints or if the
    ///       window is not resizable.
    /// @see SetSize() to set the size of the client area (drawable region).
    virtual void SetFullSize(const FIntExtent2D& size) noexcept = 0;
    virtual void SetFullSize(const UInt32 width, const UInt32 height) noexcept = 0;

    /// @note SetSizeLimits() sets both the minimum and maximum size constraints of the window. The window may not be
    ///       resized to dimensions outside of these limits. If the window is currently outside the new constraints, it
    ///       may be resized to fit within them depending on the implementation.
    virtual void SetSizeLimits(const FIntExtent2D& minimum, const FIntExtent2D& maximum) noexcept = 0;

    virtual void SetMinimumSize(const FIntExtent2D& size) noexcept = 0;
    virtual void SetMinimumSize(const UInt32 width, const UInt32 height) noexcept = 0;
    virtual void SetMaximumSize(const FIntExtent2D& size) noexcept = 0;
    virtual void SetMaximumSize(const UInt32 width, const UInt32 height) noexcept = 0;

    ///
    /// @section Actions
    ///

    virtual void Open() = 0;

    /// @note Close() closes the window immediately. After calling Close(), the window should no longer be considered
    ///       valid or open.
    virtual void Close() = 0;

    /// @note RequestClose() requests that the window be closed, but does not force it to close immediately.
    virtual void RequestClose() = 0;

    virtual void Show() = 0;
    virtual void Hide() = 0;
    virtual void Minimize() = 0;
    virtual void Maximize() = 0;
    virtual void Restore() = 0;
    virtual void Focus() = 0;
    virtual void RequestAttention(EWindowFlashMode mode = EWindowFlashMode::UntilFocused) = 0;

    /// @note CenterOnDisplay() moves the window to the center of the specified display. If the display parameter is
    ///       nullptr, the window should be centered on the actual display.
    virtual void CenterOnDisplay(const IDisplay* display = nullptr) = 0;
};

}   // namespace GP
