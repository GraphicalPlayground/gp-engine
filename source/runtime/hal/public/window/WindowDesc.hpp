// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicStringView.hpp" // IWYU pragma: keep
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "window/CursorEnums.hpp"
#include "window/WindowEnums.hpp"
#include "window/WindowTypes.hpp"

namespace gp::hal
{

/// @section Forward declarations

class Display;

/// @brief Configuration settings used to initialize a new Window instance.
/// @details This structure defines the initial state, look, and OS-level behavior of a window.
struct WindowDesc
{
    /// @brief The text displayed in the window's title bar and taskbar entry.
    StringView title{ "Graphical Playground" };

    /// @brief The initial top-left position of the window in screen coordinates.
    /// @note If targetDisplay is non-null, this is often treated as an offset from that display's origin.
    Point2D position{ 100, 100 };

    /// @brief The preferred display to spawn the window on. If nullptr, the OS determines the best display.
    const Display* targetDisplay{ nullptr };

    /// @brief The initial size of the window's client area (drawable region) in pixels.
    Extent2D size{ 1280u, 720u };

    /// @brief The minimum allowed dimensions for the window. Set to (0,0) for no limit.
    Extent2D minimumSize{ 0u, 0u };

    /// @brief The maximum allowed dimensions for the window. Set to (0,0) for no limit.
    Extent2D maximumSize{ 0u, 0u };

    /// @brief The display mode (Windowed, Fullscreen, etc.) to use upon creation.
    WindowMode mode{ WindowMode::Windowed };

    /// @brief The initial visibility state (Normal, Minimized, etc.).
    WindowState initialState{ WindowState::Normal };

    /// @brief Bitmask of aesthetic and functional flags (Resizable, TitleBar, etc.).
    WindowStyle style{ WindowStyle::Default };

    /// @brief Defines how the mouse cursor behaves when interacting with this window.
    CursorMode cursorMode{ CursorMode::Normal };

    /// @brief The default system icon shape for the cursor when over this window.
    CursorShape cursorShape{ CursorShape::Arrow };

    /// @brief The global transparency of the window. Range: [0.0 (Invisible) to 1.0 (Opaque)].
    /// @note Requires EWindowStyle::Transparent to be set in most platform implementations.
    Float32 opacity{ 1.0f };

    /// @brief If true, the window will request a high-resolution backbuffer on supported DPI-aware displays.
    bool useHighDPI{ true };

    /// @brief Whether to synchronize the buffer swap with the display's vertical refresh rate.
    bool useVSync{ true };

    /// @brief If true, the window will be created without a visible framebuffer and will not render any content. This
    ///        is useful for headless applications or when the window is only used for input handling without any visual
    ///        output.
    /// @note This has absolute priority over all other settings. If useHeadless is true, the window will be created in
    ///       a headless state regardless of the values of other fields in this struct. In headless mode, the window
    ///       will not be visible, will not have a framebuffer, and will not render any content, but it can still
    ///       receive input events and be manipulated through the IWindow interface.
    bool useHeadless{ false };

    /// @brief If true, the window will be created in an open/visible state. If false, the window will not be created
    ///        until Open() is called.
    bool startOpened{ true };

    /// @brief If true, the window will attempt to take input focus immediately upon creation.
    /// @note This may be ignored by the OS or window manager if it deems it inappropriate to steal focus from the
    ///       currently active application. Additionally, if startVisible is false, the window will be created hidden
    ///       regardless of the value of startFocused.
    bool startFocused{ true };

    /// @brief If true, the window will be visible immediately upon creation. If false, the window will be created
    ///        hidden and must be shown explicitly by the caller.
    /// @note This has priority over initialState. If startVisible is false, the window will be created hidden
    ///       regardless of the value of initialState.
    bool startVisible{ true };

    /// @brief If true, the window will automatically request focus whenever it transitions to a visible state.
    bool focusOnShow{ true };

    /// @brief If true, the window will be created with the mouse input grabbed, meaning it will receive all mouse
    ///        events regardless of whether it is the active/focused window.
    bool grabMouse{ false };

    /// @brief If true, the window will be created with the keyboard input grabbed, meaning it will receive all keyboard
    ///        events regardless of whether it is the active/focused window.
    bool grabKeyboard{ false };

    /// @brief Optional native handle to a parent window. If provided, this window becomes a child.
    /// @details This is used for embedding the engine into external tools or editor environments (e.g., Win32 HWND).
    void* nativeParentHandle{ nullptr };

    // TODO: Add more fields as needed, such as:
    // - Whether to enable per-monitor DPI awareness on Windows.
    // - Custom cursor images or icons.
    // - Custom icon image for the window (e.g. taskbar and title bar icon on Windows).
};

}   // namespace gp::hal
