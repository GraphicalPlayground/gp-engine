// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/ContainerFwd.hpp"
#include "Container/Views/StringView.hpp"
#include "CoreTypes.hpp"
#include "Math/Vector/IntExtent2D.hpp"
#include "Math/Vector/IntPoint2D.hpp"
#include "Window/CursorEnums.hpp"
#include "Window/WindowEnums.hpp"

namespace GP
{

// Forward declarations

class IDisplay;

/// @brief Configuration settings used to initialize a new IWindow instance.
/// @details This structure defines the initial state, look, and OS-level behavior of a window.
struct FWindowDesc
{
    /// @brief The text displayed in the window's title bar and taskbar entry.
    FStringView title{ "Graphical Playground" };

    /// @brief The initial top-left position of the window in screen coordinates.
    /// @note If targetDisplay is non-null, this is often treated as an offset from that display's origin.
    FIntPoint2D position{ 100, 100 };

    /// @brief The preferred display to spawn the window on. If nullptr, the OS determines the best display.
    const IDisplay* targetDisplay{ nullptr };

    /// @brief The initial size of the window's client area (drawable region) in pixels.
    FIntExtent2D size{ 1280u, 720u };

    /// @brief The minimum allowed dimensions for the window. Set to (0,0) for no limit.
    FIntExtent2D minimumSize{ 0u, 0u };

    /// @brief The maximum allowed dimensions for the window. Set to (0,0) for no limit.
    FIntExtent2D maximumSize{ 0u, 0u };

    /// @brief The display mode (Windowed, Fullscreen, etc.) to use upon creation.
    EWindowMode mode{ EWindowMode::Windowed };

    /// @brief The initial visibility state (Normal, Minimized, etc.).
    EWindowState initialState{ EWindowState::Normal };

    /// @brief Bitmask of aesthetic and functional flags (Resizable, TitleBar, etc.).
    EWindowStyle style{ EWindowStyle::Default };

    /// @brief Defines how the mouse cursor behaves when interacting with this window.
    ECursorMode cursorMode{ ECursorMode::Normal };

    /// @brief The default system icon shape for the cursor when over this window.
    ECursorShape cursorShape{ ECursorShape::Arrow };

    /// @brief The global transparency of the window. Range: [0.0 (Invisible) to 1.0 (Opaque)].
    /// @note Requires EWindowStyle::Transparent to be set in most platform implementations.
    Float32 opacity{ 1.0f };

    /// @brief If true, the window will request a high-resolution backbuffer on supported DPI-aware displays.
    bool useHighDPI{ true };

    /// @brief Whether to synchronize the buffer swap with the display's vertical refresh rate.
    bool useVSync{ true };

    /// @brief If true, the window will attempt to take input focus immediately upon creation.
    bool startFocused{ true };

    /// @brief If true, the window will be visible immediately upon creation. If false, the window will be created
    ///        hidden and must be shown explicitly by the caller.
    bool startVisible{ true };

    /// @brief If true, the window will automatically request focus whenever it transitions to a visible state.
    bool focusOnShow{ true };

    /// @brief Optional native handle to a parent window. If provided, this window becomes a child.
    /// @details This is used for embedding the engine into external tools or editor environments (e.g., Win32 HWND).
    void* nativeParentHandle{ nullptr };

    // TODO: Add more fields as needed, such as:
    // - Initial background color or clear color for the window.
    // - Whether to enable per-monitor DPI awareness on Windows.
    // - Custom cursor images or icons.
    // - Custom icon image for the window (e.g. taskbar and title bar icon on Windows).
};

}   // namespace GP
