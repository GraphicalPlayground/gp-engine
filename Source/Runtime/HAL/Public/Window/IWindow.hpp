// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"

namespace GP
{

/// @class IWindow
/// @brief Interface for platform-agnostic window management.
class IWindow
{
public:
    virtual ~IWindow() = default;

public:
    ///
    /// @section State queries
    ///

    GP_NODISCARD virtual bool IsOpen() const noexcept = 0;
    GP_NODISCARD virtual bool ShouldClose() const noexcept = 0;
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
};

}   // namespace GP
