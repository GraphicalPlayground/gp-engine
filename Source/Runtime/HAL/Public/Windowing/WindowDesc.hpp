// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicStringView.hpp"
#include "Container/ContainerForward.hpp"
#include "HALBuild.hpp"
#include "HALForward.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"
#include "Windowing/WindowEnums.hpp"

namespace GP::HAL
{

/// \brief Sentinel value for window position coordinates indicating that the platform should choose the position
/// automatically (typically centered on the target monitor).
inline constexpr Int32 WindowPosCentered = INT32_MIN + 1;

/// \brief Sentinel value for window position coordinates indicating that the platform should use its default
/// placement strategy (e.g., cascading windows on Windows, compositor-assigned on Wayland).
inline constexpr Int32 WindowPosDefault = INT32_MIN;

/// \brief Sentinel value for window size dimensions indicating that the platform should use its default size.
inline constexpr UInt32 WindowSizeDefault = 0u;

/// \brief Immutable descriptor passed to IWindowingSystem::CreateWindow().
/// All fields are plain data; no heap allocation occurs inside this struct.
/// The caller retains ownership of any pointer fields (monitor, parent) for the duration of the CreateWindow() call;
/// the window implementation copies what it needs during construction.
struct FWindowDesc
{
public:
    /// \brief Title displayed in the window's title bar and OS task switcher.
    FStringView title{ "Graphical Playground" };

    /// \brief Width of the window content area in logical pixels.
    /// Use WindowSizeDefault (0) to let the platform choose a default width.
    UInt32 width{ 1280u };

    /// \brief Height of the window content area in logical pixels.
    /// Use WindowSizeDefault (0) to let the platform choose a default height.
    UInt32 height{ 720u };

    /// \brief X position of the window on the virtual desktop in logical pixels.
    /// Use WindowPosCentered to center on the target monitor, or WindowPosDefault for OS placement.
    Int32 x{ WindowPosCentered };

    /// \brief Y position of the window on the virtual desktop in logical pixels.
    /// Use WindowPosCentered to center on the target monitor, or WindowPosDefault for OS placement.
    Int32 y{ WindowPosCentered };

    /// \brief Minimum allowed content area size. A zero extent means no minimum is enforced.
    FIntExtent2D minSize{ 0u, 0u };

    /// \brief Maximum allowed content area size. A zero extent means no maximum is enforced.
    FIntExtent2D maxSize{ 0u, 0u };

    /// \brief Initial window mode (Windowed, Borderless, Fullscreen, FullscreenWindowed).
    EWindowMode windowMode = EWindowMode::Windowed;

    /// \brief Initial style flags controlling decorations, resizability, and focus behaviour.
    EWindowStyle windowStyle = EWindowStyle::Default;

    /// \brief Initial cursor mode when the cursor is inside the window's content area.
    ECursorMode cursorMode = ECursorMode::Normal;

    /// \brief Initial cursor shape.
    ECursorShape cursorShape = ECursorShape::Arrow;

    /// \brief Initial opacity of the window in [0.0, 1.0]. 1.0 is fully opaque.
    Float32 opacity{ 1.0f };

    /// \brief The monitor on which to create the window.
    /// If nullptr, the window is created on the primary monitor.
    /// Used for both the initial position calculation (when x/y are WindowPosCentered) and fullscreen assignment.
    const IMonitor* targetMonitor{ nullptr };

    /// \brief Optional parent window. When set, this window becomes a child / owned window of the parent.
    /// On most platforms this means the child is always rendered on top of the parent and is destroyed when
    /// the parent is destroyed.
    const IWindow* parentWindow{ nullptr };

    /// \brief Whether to enable high-DPI / Retina support. When true the framebuffer may be larger than the
    /// logical content area (see IWindow::GetFramebufferSize()).
    bool highDPI{ true };

    /// \brief Whether to enable VSync on the initial swap chain tied to this window.
    bool vSync{ true };
};

}   // namespace GP::HAL
