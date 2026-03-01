// Copyright (c) - Graphical Playground. All rights reserved.

#include "SDL/SDLWindow.hpp"
#include "SDL/SDLMonitor.hpp"
#include "SDL/SDLWindowingSystem.hpp"

namespace GP::HAL
{

/// \brief Maps ECursorShape to the corresponding SDL_SystemCursor enum.
static SDL_SystemCursor ToSDLSystemCursor(ECursorShape shape) noexcept
{
    switch (shape)
    {
    case ECursorShape::IBeam     : return SDL_SYSTEM_CURSOR_TEXT;
    case ECursorShape::Crosshair : return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case ECursorShape::Hand      : return SDL_SYSTEM_CURSOR_POINTER;
    case ECursorShape::ResizeEW  : return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case ECursorShape::ResizeNS  : return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case ECursorShape::ResizeNWSE: return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case ECursorShape::ResizeNESW: return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case ECursorShape::ResizeAll : return SDL_SYSTEM_CURSOR_MOVE;
    case ECursorShape::NotAllowed: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    case ECursorShape::Arrow     :
    default                      : return SDL_SYSTEM_CURSOR_DEFAULT;
    }
}

SDLWindow::SDLWindow()
    : FBaseWindow(true)   // headless — no OS window is created
    , m_window(nullptr)
    , m_cursor(nullptr)
    , m_owner(nullptr)
{}

SDLWindow::SDLWindow(const FWindowDesc& desc, SDLWindowingSystem* owner)
    : FBaseWindow(false)
    , m_window(nullptr)
    , m_cursor(nullptr)
    , m_owner(owner)
{
    FBaseWindow::SetTitle(desc.title);
    FBaseWindow::SetSize(desc.width, desc.height);
    FBaseWindow::SetSizeLimits(desc.minSize.width, desc.minSize.height, desc.maxSize.width, desc.maxSize.height);
    FBaseWindow::SetOpacity(desc.opacity);
    FBaseWindow::SetWindowMode(desc.windowMode);
    FBaseWindow::SetWindowStyle(desc.windowStyle);
    FBaseWindow::SetCursorMode(desc.cursorMode);
    FBaseWindow::SetCursorShape(desc.cursorShape);
    FBaseWindow::SetHighDPI(desc.highDPI);
    FBaseWindow::SetVSync(desc.vSync);

    const SDL_WindowFlags flags = BuildSDLFlags(desc);

    m_window = SDL_CreateWindow(m_title.Data(), static_cast<Int32>(desc.width), static_cast<Int32>(desc.height), flags);

    if (!m_window)
    {
        GP_ASSERT(false, "SDLWindow: SDL_CreateWindow failed — {}", SDL_GetError());
        return;
    }

    // SDL3 CreateWindow does not accept position; set it separately.
    const Int32 sdlX = (desc.x == WindowPosCentered)  ? SDL_WINDOWPOS_CENTERED
                       : (desc.x == WindowPosDefault) ? SDL_WINDOWPOS_UNDEFINED
                                                      : desc.x;
    const Int32 sdlY = (desc.y == WindowPosCentered)  ? SDL_WINDOWPOS_CENTERED
                       : (desc.y == WindowPosDefault) ? SDL_WINDOWPOS_UNDEFINED
                                                      : desc.y;
    SDL_SetWindowPosition(m_window, sdlX, sdlY);

    if (desc.minSize.width > 0u || desc.minSize.height > 0u)
    {
        SDL_SetWindowMinimumSize(
            m_window, static_cast<Int32>(desc.minSize.width), static_cast<Int32>(desc.minSize.height)
        );
    }
    if (desc.maxSize.width > 0u || desc.maxSize.height > 0u)
    {
        SDL_SetWindowMaximumSize(
            m_window, static_cast<Int32>(desc.maxSize.width), static_cast<Int32>(desc.maxSize.height)
        );
    }

    SDL_SetWindowOpacity(m_window, desc.opacity);

    ApplySDLCursorShape(desc.cursorShape);
    ApplySDLCursorMode(desc.cursorMode);

    SyncStateFromSDL();

    m_isOpen = true;
}

SDLWindow::~SDLWindow()
{
    if (m_cursor)
    {
        SDL_DestroyCursor(m_cursor);
        m_cursor = nullptr;
    }
    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

GP_NODISCARD SDL_Window* SDLWindow::GetSDLWindow() const noexcept { return m_window; }

GP_NODISCARD SDL_WindowID SDLWindow::GetSDLWindowID() const noexcept
{
    return (m_window != nullptr) ? SDL_GetWindowID(m_window) : 0u;
}

void SDLWindow::DispatchSDLEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        m_shouldClose = true;
        OnCloseRequested.Broadcast(FWindowEvents::CloseRequested{});
        break;

    case SDL_EVENT_WINDOW_DESTROYED:
        m_isOpen = false;
        m_shouldClose = true;
        m_isFocused = false;
        OnClosed.Broadcast(FWindowEvents::Closed{});
        break;

    case SDL_EVENT_WINDOW_MOVED:
    {
        m_position.x = event.window.data1;
        m_position.y = event.window.data2;
        OnMoved.Broadcast(FWindowEvents::Moved{ event.window.data1, event.window.data2 });
        break;
    }

    case SDL_EVENT_WINDOW_RESIZED:
    {
        const UInt32 w = static_cast<UInt32>(event.window.data1);
        const UInt32 h = static_cast<UInt32>(event.window.data2);
        m_contentSize = { w, h };
        if (!m_isHighDPI) { m_framebufferSize = m_contentSize; }
        if (h > 0u) { m_aspectRatio = static_cast<Float32>(w) / static_cast<Float32>(h); }
        OnResized.Broadcast(FWindowEvents::Resized{ w, h });
        break;
    }

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    {
        const UInt32 pw = static_cast<UInt32>(event.window.data1);
        const UInt32 ph = static_cast<UInt32>(event.window.data2);
        m_framebufferSize = { pw, ph };
        m_isHighDPI = (pw != m_contentSize.width || ph != m_contentSize.height);
        OnFramebufferResized.Broadcast(FWindowEvents::FramebufferResized{ pw, ph });
        break;
    }

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        m_isFocused = true;
        OnFocusGained.Broadcast(FWindowEvents::FocusGained{});
        break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        m_isFocused = false;
        OnFocusLost.Broadcast(FWindowEvents::FocusLost{});
        break;

    case SDL_EVENT_WINDOW_SHOWN:
        m_windowStyle = Enums::SetFlags(m_windowStyle, EWindowStyle::Visible);
        if (m_windowState == EWindowState::Hidden) { m_windowState = EWindowState::Normal; }
        m_isOpen = true;
        OnShown.Broadcast(FWindowEvents::Shown{});
        break;

    case SDL_EVENT_WINDOW_HIDDEN:
        m_windowStyle = Enums::ClearFlags(m_windowStyle, EWindowStyle::Visible);
        m_windowState = EWindowState::Hidden;
        OnHidden.Broadcast(FWindowEvents::Hidden{});
        break;

    case SDL_EVENT_WINDOW_MINIMIZED:
        m_windowState = EWindowState::Minimized;
        m_windowStyle =
            Enums::SetFlags(Enums::ClearFlags(m_windowStyle, EWindowStyle::Maximized), EWindowStyle::Minimized);
        m_isFocused = false;
        OnMinimized.Broadcast(FWindowEvents::Minimized{});
        break;

    case SDL_EVENT_WINDOW_MAXIMIZED:
        m_windowState = EWindowState::Maximized;
        m_windowStyle =
            Enums::SetFlags(Enums::ClearFlags(m_windowStyle, EWindowStyle::Minimized), EWindowStyle::Maximized);
        OnMaximized.Broadcast(FWindowEvents::Maximized{});
        break;

    case SDL_EVENT_WINDOW_RESTORED:
        m_windowState = EWindowState::Normal;
        m_windowStyle = Enums::ClearFlags(m_windowStyle, EWindowStyle::Minimized | EWindowStyle::Maximized);
        OnRestored.Broadcast(FWindowEvents::Restored{});
        break;

    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    {
        if (m_window)
        {
            const Float32 scale = SDL_GetWindowDisplayScale(m_window);
            m_contentScale = scale;
            m_isHighDPI = (scale > 1.0f);

            Int32 pw{}, ph{};
            SDL_GetWindowSizeInPixels(m_window, &pw, &ph);
            m_framebufferSize = { static_cast<UInt32>(pw), static_cast<UInt32>(ph) };

            OnContentScaleChanged.Broadcast(FWindowEvents::ContentScaleChanged{ scale, scale });
        }
        break;
    }

    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    {
        const SDL_DisplayID newDisplayID = static_cast<SDL_DisplayID>(event.window.data1);

        // Refresh rate on the new display
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(newDisplayID);
        if (mode)
        {
            OnRefreshRateChanged.Broadcast(FWindowEvents::RefreshRateChanged{ static_cast<UInt32>(mode->refresh_rate) }
            );
        }

        // Resolve monitor pointer through the owning system (may be null on standalone windows)
        const IMonitor* newMonitor = m_owner ? m_owner->FindMonitorBySDLDisplayID(newDisplayID) : nullptr;
        OnMonitorChanged.Broadcast(FWindowEvents::MonitorChanged{ nullptr, const_cast<IMonitor*>(newMonitor) });
        break;
    }

    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    {
        const EWindowMode oldMode = m_windowMode;
        // Distinguish desktop-fullscreen (FullscreenWindowed) from exclusive fullscreen:
        // If no specific mode was pinned we treat it as FullscreenWindowed, otherwise Fullscreen.
        const SDL_DisplayMode* pinned = m_window ? SDL_GetWindowFullscreenMode(m_window) : nullptr;
        m_windowMode = pinned ? EWindowMode::Fullscreen : EWindowMode::FullscreenWindowed;
        if (oldMode != m_windowMode) { OnModeChanged.Broadcast(FWindowEvents::ModeChanged{ oldMode, m_windowMode }); }
        break;
    }

    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    {
        const EWindowMode oldMode = m_windowMode;
        m_windowMode =
            Enums::HasAny(m_windowStyle, EWindowStyle::Decorated) ? EWindowMode::Windowed : EWindowMode::Borderless;
        if (oldMode != m_windowMode) { OnModeChanged.Broadcast(FWindowEvents::ModeChanged{ oldMode, m_windowMode }); }
        break;
    }

    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        m_isHovered = true;
        OnMouseEntered.Broadcast(FWindowEvents::MouseEntered{});
        break;

    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        m_isHovered = false;
        OnMouseLeft.Broadcast(FWindowEvents::MouseLeft{});
        break;

    case SDL_EVENT_WINDOW_OCCLUDED:
        m_isOccluded = true;
        OnOccluded.Broadcast(FWindowEvents::Occluded{});
        break;

    case SDL_EVENT_WINDOW_EXPOSED:
        m_isOccluded = false;
        OnRevealed.Broadcast(FWindowEvents::Revealed{});
        OnRefreshRequested.Broadcast(FWindowEvents::RefreshRequested{});
        break;

    default: break;
    }
}

void SDLWindow::Close()
{
    if (m_window && m_isOpen) { SDL_HideWindow(m_window); }
    FBaseWindow::Close();
    // SDL_EVENT_WINDOW_DESTROYED fires the OnClosed delegate; no double-fire needed here.
}

void SDLWindow::Show()
{
    if (m_window) { SDL_ShowWindow(m_window); }
    FBaseWindow::Show();
    // OnShown fires via SDL_EVENT_WINDOW_SHOWN once the event is dispatched.
}

void SDLWindow::Hide()
{
    if (m_window) { SDL_HideWindow(m_window); }
    FBaseWindow::Hide();
    // OnHidden fires via SDL_EVENT_WINDOW_HIDDEN.
}

void SDLWindow::Focus()
{
    if (m_window) { SDL_RaiseWindow(m_window); }
    FBaseWindow::Focus();
}

void SDLWindow::Maximize()
{
    if (m_window) { SDL_MaximizeWindow(m_window); }
    FBaseWindow::Maximize();
    // OnMaximized fires via SDL_EVENT_WINDOW_MAXIMIZED.
}

void SDLWindow::Minimize()
{
    if (m_window) { SDL_MinimizeWindow(m_window); }
    FBaseWindow::Minimize();
    // OnMinimized fires via SDL_EVENT_WINDOW_MINIMIZED.
}

void SDLWindow::Restore()
{
    if (m_window) { SDL_RestoreWindow(m_window); }
    FBaseWindow::Restore();
    // OnRestored fires via SDL_EVENT_WINDOW_RESTORED.
}

void SDLWindow::RequestAttention()
{
    if (m_window) { SDL_FlashWindow(m_window, SDL_FLASH_UNTIL_FOCUSED); }
}

void SDLWindow::SetWindowMode(EWindowMode mode)
{
    const EWindowMode oldMode = m_windowMode;
    if (!m_window || oldMode == mode)
    {
        FBaseWindow::SetWindowMode(mode);
        return;
    }

    switch (mode)
    {
    case EWindowMode::Windowed:
        SDL_SetWindowFullscreen(m_window, false);
        SDL_SetWindowBordered(m_window, true);
        break;

    case EWindowMode::Borderless:
        SDL_SetWindowFullscreen(m_window, false);
        SDL_SetWindowBordered(m_window, false);
        break;

    case EWindowMode::Fullscreen:
    {
        // Pin the current display mode so SDL uses exclusive fullscreen at the current resolution.
        const SDL_DisplayID displayID = SDL_GetDisplayForWindow(m_window);
        const SDL_DisplayMode* currentMode = SDL_GetCurrentDisplayMode(displayID);
        SDL_SetWindowFullscreenMode(m_window, currentMode);
        SDL_SetWindowFullscreen(m_window, true);
        break;
    }

    case EWindowMode::FullscreenWindowed:
        // Passing nullptr instructs SDL to use the desktop display mode (borderless fullscreen).
        SDL_SetWindowFullscreenMode(m_window, nullptr);
        SDL_SetWindowFullscreen(m_window, true);
        break;
    }

    FBaseWindow::SetWindowMode(mode);
    // Mode-changed delegate fires from SDL_EVENT_WINDOW_ENTER/LEAVE_FULLSCREEN via DispatchSDLEvent.
}

void SDLWindow::SetWindowStyle(EWindowStyle style)
{
    const EWindowStyle oldStyle = m_windowStyle;
    FBaseWindow::SetWindowStyle(style);

    if (m_window)
    {
        SDL_SetWindowResizable(m_window, Enums::HasAny(style, EWindowStyle::Resizable));
        SDL_SetWindowBordered(m_window, Enums::HasAny(style, EWindowStyle::Decorated));
        SDL_SetWindowAlwaysOnTop(m_window, Enums::HasAny(style, EWindowStyle::Floating));

        const bool wantsVisible = Enums::HasAny(style, EWindowStyle::Visible);
        if (wantsVisible != Enums::HasAny(oldStyle, EWindowStyle::Visible))
        {
            if (wantsVisible) { SDL_ShowWindow(m_window); }
            else { SDL_HideWindow(m_window); }
        }

        if (Enums::HasAny(style, EWindowStyle::Minimized)) { SDL_MinimizeWindow(m_window); }
        else if (Enums::HasAny(style, EWindowStyle::Maximized)) { SDL_MaximizeWindow(m_window); }
    }

    if (oldStyle != style) { OnStyleChanged.Broadcast(FWindowEvents::StyleChanged{ oldStyle, style }); }
}

void SDLWindow::SetFullscreen(bool enabled)
{
    const EWindowMode oldMode = m_windowMode;
    FBaseWindow::SetFullscreen(enabled);
    if (m_window) { SDL_SetWindowFullscreen(m_window, enabled); }
    if (oldMode != m_windowMode) { OnModeChanged.Broadcast(FWindowEvents::ModeChanged{ oldMode, m_windowMode }); }
}

void SDLWindow::SetBorderless(bool enabled)
{
    const EWindowMode oldMode = m_windowMode;
    FBaseWindow::SetBorderless(enabled);
    if (m_window)
    {
        if (enabled)
        {
            SDL_SetWindowFullscreen(m_window, false);
            SDL_SetWindowBordered(m_window, false);
        }
        else { SDL_SetWindowBordered(m_window, true); }
    }
    if (oldMode != m_windowMode) { OnModeChanged.Broadcast(FWindowEvents::ModeChanged{ oldMode, m_windowMode }); }
}

void SDLWindow::SetResizable(bool enabled)
{
    FBaseWindow::SetResizable(enabled);
    if (m_window) { SDL_SetWindowResizable(m_window, enabled); }
}

void SDLWindow::SetDecorated(bool enabled)
{
    FBaseWindow::SetDecorated(enabled);
    if (m_window) { SDL_SetWindowBordered(m_window, enabled); }
}

void SDLWindow::SetAlwaysOnTop(bool enabled)
{
    FBaseWindow::SetAlwaysOnTop(enabled);
    if (m_window) { SDL_SetWindowAlwaysOnTop(m_window, enabled); }
}

void SDLWindow::SetTransparent(bool enabled)
{
    FBaseWindow::SetTransparent(enabled);
    // SDL_WINDOW_TRANSPARENT is a creation-time flag; post-creation transparency is achieved through opacity.
    if (m_window && enabled && m_opacity < 1.0f) { SDL_SetWindowOpacity(m_window, m_opacity); }
}

void SDLWindow::SetCursorMode(ECursorMode mode)
{
    FBaseWindow::SetCursorMode(mode);
    ApplySDLCursorMode(mode);
}

void SDLWindow::SetCursorShape(ECursorShape shape)
{
    FBaseWindow::SetCursorShape(shape);
    ApplySDLCursorShape(shape);
}

void SDLWindow::SetCursorVisibility(bool visible)
{
    FBaseWindow::SetCursorVisibility(visible);
    if (visible) { SDL_ShowCursor(); }
    else { SDL_HideCursor(); }
}

void SDLWindow::SetCursorGrab(bool grabbed)
{
    FBaseWindow::SetCursorGrab(grabbed);
    if (m_window) { SDL_SetWindowMouseGrab(m_window, grabbed); }
}

void SDLWindow::SetKeyboardGrab(bool grabbed)
{
    FBaseWindow::SetKeyboardGrab(grabbed);
    if (m_window) { SDL_SetWindowKeyboardGrab(m_window, grabbed); }
}

void SDLWindow::SetSize(UInt32 width, UInt32 height)
{
    FBaseWindow::SetSize(width, height);
    if (m_window)
    {
        SDL_SetWindowSize(m_window, static_cast<Int32>(m_contentSize.width), static_cast<Int32>(m_contentSize.height));
    }
}

void SDLWindow::SetFullWindowSize(UInt32 width, UInt32 height)
{
    FBaseWindow::SetFullWindowSize(width, height);
    // SDL has no API to set the outer (decorated) window size; this is informational on the SDL backend.
}

void SDLWindow::SetPosition(UInt32 x, UInt32 y)
{
    FBaseWindow::SetPosition(x, y);
    if (m_window) { SDL_SetWindowPosition(m_window, static_cast<Int32>(x), static_cast<Int32>(y)); }
}

void SDLWindow::SetSizeLimits(UInt32 minWidth, UInt32 minHeight, UInt32 maxWidth, UInt32 maxHeight)
{
    FBaseWindow::SetSizeLimits(minWidth, minHeight, maxWidth, maxHeight);
    if (m_window)
    {
        SDL_SetWindowMinimumSize(m_window, static_cast<Int32>(minWidth), static_cast<Int32>(minHeight));
        SDL_SetWindowMaximumSize(m_window, static_cast<Int32>(maxWidth), static_cast<Int32>(maxHeight));
    }
}

void SDLWindow::SetAspectRatio(Float32 aspectRatio)
{
    FBaseWindow::SetAspectRatio(aspectRatio);
    if (m_window && aspectRatio > 0.0f) { SDL_SetWindowAspectRatio(m_window, aspectRatio, aspectRatio); }
}

void SDLWindow::SetAspectRatio(UInt32 numerator, UInt32 denominator)
{
    FBaseWindow::SetAspectRatio(numerator, denominator);
    if (m_window && m_aspectRatio > 0.0f) { SDL_SetWindowAspectRatio(m_window, m_aspectRatio, m_aspectRatio); }
}

void SDLWindow::Center(const IMonitor* monitor)
{
    if (!m_window)
    {
        FBaseWindow::Center(monitor);
        return;
    }

    if (monitor)
    {
        const auto* sdlMonitor = static_cast<const SDLMonitor*>(monitor);
        SDL_Rect displayBounds{};
        if (SDL_GetDisplayBounds(sdlMonitor->GetDisplayID(), &displayBounds))
        {
            const Int32 cx = displayBounds.x + (displayBounds.w - static_cast<Int32>(m_contentSize.width)) / 2;
            const Int32 cy = displayBounds.y + (displayBounds.h - static_cast<Int32>(m_contentSize.height)) / 2;
            SDL_SetWindowPosition(m_window, cx, cy);
        }
        else { SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED); }
    }
    else { SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED); }

    // Sync the actual position back into base state after SDL resolves the centering.
    Int32 x{}, y{};
    SDL_GetWindowPosition(m_window, &x, &y);
    FBaseWindow::SetPosition(static_cast<UInt32>(x >= 0 ? x : 0), static_cast<UInt32>(y >= 0 ? y : 0));
}

void SDLWindow::SetOpacity(Float32 opacity)
{
    FBaseWindow::SetOpacity(opacity);
    if (m_window) { SDL_SetWindowOpacity(m_window, m_opacity); }
}

void SDLWindow::SetTitle(FStringView title)
{
    FBaseWindow::SetTitle(title);   // updates m_title (null-terminated FString)
    if (m_window) { SDL_SetWindowTitle(m_window, m_title.Data()); }
}

void SDLWindow::SetHighDPI(bool enabled)
{
    FBaseWindow::SetHighDPI(enabled);
    // SDL_WINDOW_HIGH_PIXEL_DENSITY is a creation-time flag; re-sync framebuffer size from SDL.
    if (m_window)
    {
        Int32 pw{}, ph{};
        SDL_GetWindowSizeInPixels(m_window, &pw, &ph);
        m_framebufferSize = { static_cast<UInt32>(pw), static_cast<UInt32>(ph) };

        const Float32 scale = SDL_GetWindowDisplayScale(m_window);
        m_contentScale = scale;
        OnContentScaleChanged.Broadcast(FWindowEvents::ContentScaleChanged{ scale, scale });
    }
}

void SDLWindow::SetVSync(bool enabled)
{
    FBaseWindow::SetVSync(enabled);
    // VSync is governed by the RHI swap-chain, not the window. The stored flag is
    // queried by RHI backends via IsVSyncEnabled() at swap-chain creation time.
}

void SDLWindow::SetIcon(const UInt8* pixels, UInt32 width, UInt32 height)
{
    FBaseWindow::SetIcon(pixels, width, height);
    if (!m_window || !pixels || width == 0u || height == 0u) { return; }

    // SDL3 expects RGBA8 pixel data with a row pitch of width * 4 bytes.
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        static_cast<Int32>(width),
        static_cast<Int32>(height),
        SDL_PIXELFORMAT_RGBA32,
        const_cast<void*>(static_cast<const void*>(pixels)),
        static_cast<Int32>(width * 4u)
    );

    if (surface)
    {
        SDL_SetWindowIcon(m_window, surface);
        SDL_DestroySurface(surface);
    }
}

GP_NODISCARD void* SDLWindow::GetNativeHandle() const noexcept
{
    if (!m_window) { return nullptr; }

    const SDL_PropertiesID props = SDL_GetWindowProperties(m_window);

#if GP_PLATFORM_WINDOWS
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif GP_PLATFORM_MACOS
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif GP_PLATFORM_IOS
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
#elif GP_PLATFORM_ANDROID
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
#elif GP_PLATFORM_LINUX
    // Prefer Wayland surface; fall back to X11 window number if running under XWayland or X11.
    void* wayland = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (wayland) { return wayland; }
    const Sint64 x11 = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    return reinterpret_cast<void*>(static_cast<UIntPtr>(x11 >= 0 ? static_cast<UInt64>(x11) : 0u));
#else
    return static_cast<void*>(m_window);
#endif
}

GP_NODISCARD const IMonitor* SDLWindow::GetCurrentMonitor() const noexcept
{
    if (!m_window || !m_owner) { return nullptr; }
    return m_owner->FindMonitorForWindow(this);
}

SDL_WindowFlags SDLWindow::BuildSDLFlags(const FWindowDesc& desc) noexcept
{
    SDL_WindowFlags flags = 0;

    if (Enums::HasAny(desc.windowStyle, EWindowStyle::Resizable)) { flags |= SDL_WINDOW_RESIZABLE; }
    if (!Enums::HasAny(desc.windowStyle, EWindowStyle::Decorated)) { flags |= SDL_WINDOW_BORDERLESS; }
    if (Enums::HasAny(desc.windowStyle, EWindowStyle::Floating)) { flags |= SDL_WINDOW_ALWAYS_ON_TOP; }
    if (Enums::HasAny(desc.windowStyle, EWindowStyle::TransparentFramebuffer)) { flags |= SDL_WINDOW_TRANSPARENT; }
    if (!Enums::HasAny(desc.windowStyle, EWindowStyle::Visible)) { flags |= SDL_WINDOW_HIDDEN; }
    if (Enums::HasAny(desc.windowStyle, EWindowStyle::Maximized)) { flags |= SDL_WINDOW_MAXIMIZED; }
    if (Enums::HasAny(desc.windowStyle, EWindowStyle::Minimized)) { flags |= SDL_WINDOW_MINIMIZED; }
    if (desc.highDPI) { flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY; }

    switch (desc.windowMode)
    {
    case EWindowMode::Fullscreen:
    case EWindowMode::FullscreenWindowed: flags |= SDL_WINDOW_FULLSCREEN; break;
    case EWindowMode::Borderless        : flags |= SDL_WINDOW_BORDERLESS; break;
    case EWindowMode::Windowed          :
    default                             : break;
    }

    return flags;
}

void SDLWindow::SyncStateFromSDL()
{
    if (!m_window) { return; }

    Int32 x{}, y{}, w{}, h{}, pw{}, ph{};
    SDL_GetWindowPosition(m_window, &x, &y);
    SDL_GetWindowSize(m_window, &w, &h);
    SDL_GetWindowSizeInPixels(m_window, &pw, &ph);

    m_position = { x, y };
    m_contentSize = { static_cast<UInt32>(w), static_cast<UInt32>(h) };
    m_framebufferSize = { static_cast<UInt32>(pw), static_cast<UInt32>(ph) };

    // Approximate full-window size using SDL border metrics (may fail on some platforms).
    Int32 top{}, left{}, bottom{}, right{};
    if (SDL_GetWindowBordersSize(m_window, &top, &left, &bottom, &right))
    {
        m_fullWindowSize = { m_contentSize.width + static_cast<UInt32>(left + right),
                             m_contentSize.height + static_cast<UInt32>(top + bottom) };
    }
    else { m_fullWindowSize = m_contentSize; }

    if (m_contentSize.height > 0u)
    {
        m_aspectRatio = static_cast<Float32>(m_contentSize.width) / static_cast<Float32>(m_contentSize.height);
    }

    m_isHighDPI = (pw != w || ph != h);
    m_contentScale = SDL_GetWindowDisplayScale(m_window);

    const SDL_WindowFlags sdlFlags = SDL_GetWindowFlags(m_window);
    m_isFocused = (sdlFlags & SDL_WINDOW_INPUT_FOCUS) != 0;
    m_isHovered = (sdlFlags & SDL_WINDOW_MOUSE_FOCUS) != 0;
    m_isMouseGrabbed = (sdlFlags & SDL_WINDOW_MOUSE_GRABBED) != 0;
    m_isKeyboardGrabbed = (sdlFlags & SDL_WINDOW_KEYBOARD_GRABBED) != 0;
    m_isOccluded = (sdlFlags & SDL_WINDOW_OCCLUDED) != 0;
}

void SDLWindow::ApplySDLCursorShape(ECursorShape shape)
{
    if (m_cursor)
    {
        SDL_DestroyCursor(m_cursor);
        m_cursor = nullptr;
    }

    m_cursor = SDL_CreateSystemCursor(ToSDLSystemCursor(shape));
    if (m_cursor) { SDL_SetCursor(m_cursor); }
}

void SDLWindow::ApplySDLCursorMode(ECursorMode mode)
{
    if (!m_window) { return; }

    // Reset all cursor constraints before applying the new mode.
    SDL_ShowCursor();
    SDL_SetWindowMouseGrab(m_window, false);
    SDL_SetWindowRelativeMouseMode(m_window, false);

    switch (mode)
    {
    case ECursorMode::Normal:
        // All constraints already cleared above; cursor is visible and unconfined.
        break;

    case ECursorMode::Hidden: SDL_HideCursor(); break;

    case ECursorMode::Disabled:
        // Relative mouse mode: cursor hidden, movement is unbounded (FPS-style).
        SDL_HideCursor();
        SDL_SetWindowRelativeMouseMode(m_window, true);
        break;

    case ECursorMode::Captured:
        // Cursor remains visible but is confined to the window's client area.
        SDL_SetWindowMouseGrab(m_window, true);
        break;
    }
}

}   // namespace GP::HAL

