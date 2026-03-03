// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Window/SDL3/SDL3Window.hpp"
#include "CoreBuild.hpp"
#include "Templates/Utility/Enums.hpp"

namespace GP
{

///
/// @section Construction and Destruction
///

FSDL3Window::FSDL3Window(const FWindowDesc& desc) noexcept
    : m_desc(desc)
{
    ResetToDefaultState();
    if (desc.startOpened) { Open(); }
}

FSDL3Window::~FSDL3Window() noexcept
{
    if (IsOpen()) { Close(); }
}

///
/// @section State queries
///

GP_NODISCARD bool FSDL3Window::IsOpen() const noexcept { return m_window; }

GP_NODISCARD bool FSDL3Window::IsValid() const noexcept { return m_window; }

GP_NODISCARD bool FSDL3Window::IsActive() const noexcept { return m_window && m_isFocused; }

GP_NODISCARD bool FSDL3Window::IsFocused() const noexcept { return m_isFocused; }

GP_NODISCARD bool FSDL3Window::IsMinimized() const noexcept { return m_windowState == EWindowState::Minimized; }

GP_NODISCARD bool FSDL3Window::IsMaximized() const noexcept { return m_windowState == EWindowState::Maximized; }

GP_NODISCARD bool FSDL3Window::IsResizable() const noexcept
{
    return EnumHasAnyFlags(m_windowStyle, EWindowStyle::Resizable);
}

GP_NODISCARD bool FSDL3Window::IsFullscreen() const noexcept { return m_windowMode == EWindowMode::Fullscreen; }

GP_NODISCARD bool FSDL3Window::IsBorderless() const noexcept
{
    return EnumHasAnyFlags(m_windowStyle, EWindowStyle::TitleBar) == false;
}

GP_NODISCARD bool FSDL3Window::IsTransparent() const noexcept
{
    return EnumHasAnyFlags(m_windowStyle, EWindowStyle::Transparent);
}

GP_NODISCARD bool FSDL3Window::IsAlwaysOnTop() const noexcept
{
    return EnumHasAnyFlags(m_windowStyle, EWindowStyle::AlwaysOnTop);
}

GP_NODISCARD bool FSDL3Window::IsHovered() const noexcept { return m_isHovered; }

GP_NODISCARD bool FSDL3Window::IsVisible() const noexcept { return m_windowState != EWindowState::Hidden; }

GP_NODISCARD bool FSDL3Window::IsHidden() const noexcept { return m_windowState == EWindowState::Hidden; }

GP_NODISCARD bool FSDL3Window::IsVSyncEnabled() const noexcept { return m_isVSync; }

GP_NODISCARD bool FSDL3Window::IsHighDPI() const noexcept { return m_isHighDPI; }

GP_NODISCARD bool FSDL3Window::IsInputGrabbed() const noexcept { return m_isMouseGrabbed || m_isKeyboardGrabbed; }

GP_NODISCARD bool FSDL3Window::IsMouseGrabbed() const noexcept { return m_isMouseGrabbed; }

GP_NODISCARD bool FSDL3Window::IsKeyboardGrabbed() const noexcept { return m_isKeyboardGrabbed; }

GP_NODISCARD bool FSDL3Window::IsCursorVisible() const noexcept { return m_cursorMode == ECursorMode::Normal; }

GP_NODISCARD bool FSDL3Window::IsCursorHidden() const noexcept
{
    return m_cursorMode == ECursorMode::Hidden || m_cursorMode == ECursorMode::Locked;
}

GP_NODISCARD bool FSDL3Window::IsCursorConfined() const noexcept { return m_cursorMode == ECursorMode::Confined; }

GP_NODISCARD bool FSDL3Window::IsOccluded() const noexcept { return m_isOccluded; }

///
/// @section Getters
///

GP_NODISCARD FIntPoint2D FSDL3Window::GetPosition() const noexcept { return m_position; }

GP_NODISCARD FIntPoint2D FSDL3Window::GetFullPosition() const noexcept
{
    // TODO: Implement retrieval of the full window position including non-client areas using SDL3 APIs.
    return m_position;
}

GP_NODISCARD FIntExtent2D FSDL3Window::GetSize() const noexcept { return m_contentSize; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetFullSize() const noexcept { return m_fullWindowSize; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetFramebufferSize() const noexcept { return m_framebufferSize; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetMinimumSize() const noexcept { return m_minSize; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetMaximumSize() const noexcept { return m_maxSize; }

GP_NODISCARD void* FSDL3Window::GetNativeHandle() const noexcept
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

GP_NODISCARD const IDisplay* FSDL3Window::GetCurrentDisplay() const noexcept
{
    // TODO: With the WindowingSystem retreive the current display of the window.
    return nullptr;
}

///
/// @section Setters
///

void FSDL3Window::SetPosition(const FIntPoint2D& position) noexcept
{
    if (!m_window) { return; }

    SDL_SetWindowPosition(m_window, position.x, position.y);
    m_position = position;
}

void FSDL3Window::SetSize(const FIntExtent2D& size) noexcept
{
    if (!m_window) { return; }

    SDL_SetWindowSize(m_window, static_cast<int>(size.width), static_cast<int>(size.height));
    m_contentSize = size;
}

void FSDL3Window::SetFullSize(const FIntExtent2D& size) noexcept
{
    if (!m_window) { return; }

    // TODO: Calculate the required client area size based on the desired full window size and the current window
    //       decorations (title bar, borders) using SDL3 APIs, then set the window size accordingly.
    SDL_SetWindowSize(m_window, static_cast<int>(size.width), static_cast<int>(size.height));
    m_fullWindowSize = size;
}

void FSDL3Window::SetMinimumSize(const FIntExtent2D& size) noexcept
{
    if (!m_window) { return; }

    SDL_SetWindowMinimumSize(m_window, static_cast<int>(size.width), static_cast<int>(size.height));
    m_minSize = size;
}

void FSDL3Window::SetMaximumSize(const FIntExtent2D& size) noexcept
{
    if (!m_window) { return; }

    SDL_SetWindowMaximumSize(m_window, static_cast<int>(size.width), static_cast<int>(size.height));
    m_maxSize = size;
}

///
/// @section Actions
///

void FSDL3Window::Open()
{
    if (m_window) { return; }

    // TODO: Implement window creation using SDL3 based on the provided FWindowDesc. This will involve translating the
    //       fields in FWindowDesc to the appropriate SDL3 window creation flags and parameters.
}

void FSDL3Window::Close()
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

    ResetToDefaultState();
}

void FSDL3Window::RequestClose()
{
    if (!m_window) { return; }

    SDL_Event closeEvent{};
    closeEvent.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    closeEvent.window.windowID = SDL_GetWindowID(m_window);
    SDL_PushEvent(&closeEvent);
}

void FSDL3Window::Show()
{
    if (!m_window) { return; }

    SDL_ShowWindow(m_window);
    m_windowState = EWindowState::Normal;
}

void FSDL3Window::Hide()
{
    if (!m_window) { return; }

    SDL_HideWindow(m_window);
    m_windowState = EWindowState::Hidden;
}

void FSDL3Window::Minimize()
{
    if (!m_window) { return; }

    SDL_MinimizeWindow(m_window);
    m_windowState = EWindowState::Minimized;
}

void FSDL3Window::Maximize()
{
    if (!m_window) { return; }

    SDL_MaximizeWindow(m_window);
    m_windowState = EWindowState::Maximized;
}

void FSDL3Window::Restore()
{
    if (!m_window) { return; }

    SDL_RestoreWindow(m_window);
    m_windowState = EWindowState::Normal;
}

void FSDL3Window::Focus()
{
    if (!m_window) { return; }

    if (SDL_RaiseWindow(m_window)) { m_isFocused = true; }
}

void FSDL3Window::RequestAttention(EWindowFlashMode mode)
{
    if (!m_window) { return; }

    switch (mode)
    {
    case EWindowFlashMode::None        : SDL_FlashWindow(m_window, SDL_FLASH_CANCEL); break;
    case EWindowFlashMode::UntilFocused: SDL_FlashWindow(m_window, SDL_FLASH_UNTIL_FOCUSED); break;
    case EWindowFlashMode::Briefly     : SDL_FlashWindow(m_window, SDL_FLASH_BRIEFLY); break;
    default                            : break;
    }
}

void FSDL3Window::CenterOnDisplay(const IDisplay* display)
{
    // TODO: Implement centering the window on the specified display using SDL3 APIs. This will involve retrieving the
    //       display's bounds and calculating the appropriate window position to center it.
    GP_UNUSED(display);
}

///
/// @section Private Helpers
///

void FSDL3Window::ResetToDefaultState() noexcept
{
    m_windowMode = m_desc.mode;
    m_windowState = !m_desc.startVisible ? EWindowState::Hidden : m_desc.initialState;
    m_windowStyle = m_desc.style;
    m_cursorMode = m_desc.cursorMode;
    m_cursorShape = m_desc.cursorShape;
    m_opacity = m_desc.opacity;
    m_minSize = m_desc.minimumSize;
    m_maxSize = m_desc.maximumSize;
    m_fullWindowSize = m_desc.size;
    m_contentSize = m_desc.size;
    m_framebufferSize = m_desc.size;
    m_position = m_desc.position;
    m_aspectRatio = static_cast<Float32>(m_desc.size.width) / static_cast<Float32>(m_desc.size.height);
    m_contentScale = 1.0f;
    m_isFocused = m_desc.startFocused;
    m_isHighDPI = m_desc.useHighDPI;
    m_isVSync = m_desc.useVSync;
    m_isKeyboardGrabbed = m_desc.grabKeyboard;
    m_isMouseGrabbed = m_desc.grabMouse;
    m_isHeadless = m_desc.useHeadless;
}

void FSDL3Window::SyncStateWithSDL() noexcept
{
    if (!m_window) { return; }

    Int32 x{}, y{}, width{}, height{}, fbWidth{}, fbHeight{};
    SDL_GetWindowPosition(m_window, &x, &y);
    SDL_GetWindowSize(m_window, &width, &height);
    SDL_GetWindowSizeInPixels(m_window, &fbWidth, &fbHeight);

    m_position = { x, y };
    m_contentSize = { static_cast<UInt32>(width), static_cast<UInt32>(height) };
    m_framebufferSize = { static_cast<UInt32>(fbWidth), static_cast<UInt32>(fbHeight) };

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

    m_isHighDPI = (fbWidth != width) || (fbHeight != height);
    m_contentScale = SDL_GetWindowDisplayScale(m_window);

    const SDL_WindowFlags sdlFlags = SDL_GetWindowFlags(m_window);
    m_isFocused = (sdlFlags & SDL_WINDOW_INPUT_FOCUS) != 0;
    m_isHovered = (sdlFlags & SDL_WINDOW_MOUSE_FOCUS) != 0;
    m_isMouseGrabbed = (sdlFlags & SDL_WINDOW_MOUSE_GRABBED) != 0;
    m_isKeyboardGrabbed = (sdlFlags & SDL_WINDOW_KEYBOARD_GRABBED) != 0;
    m_isOccluded = (sdlFlags & SDL_WINDOW_OCCLUDED) != 0;
}

void FSDL3Window::ApplySDLCursorShape(ECursorShape shape) noexcept
{
    GP_UNUSED(shape);
    // TODO: Implement cursor shape changes using SDL3's SDL_CreateSystemCursor and SDL_SetCursor functions, managing
    // the lifetime of the SDL_Cursor object to avoid leaks and ensure proper cleanup when changing shapes or closing
    // the window.
}

void FSDL3Window::ApplySDLCursorMode(ECursorMode mode) noexcept
{
    GP_UNUSED(mode);
    // TODO: Implement cursor mode changes using SDL3's SDL_SetRelativeMouseMode and SDL_SetWindowGrab functions to
    // handle normal, hidden, locked, and confined cursor behaviors.
}

///
/// @section Static Private Helpers
///

void FSDL3Window::BuildSDLFlags(const FWindowDesc& desc) noexcept
{
    GP_UNUSED(desc);
    // TODO: Implement translation of FWindowDesc fields to SDL_WindowFlags for window creation and updates.
}

}   // namespace GP
