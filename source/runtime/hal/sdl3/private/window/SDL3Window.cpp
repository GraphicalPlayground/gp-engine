// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "window/SDL3Window.hpp"
#include "templates/Enums.hpp"
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

namespace
{

/// @brief Maps an engine `CursorShape` to an `SDL_SystemCursor` enumerator.
/// @param[in] shape Engine cursor shape.
/// @return Corresponding `SDL_SystemCursor` value.
[[nodiscard]] constexpr SDL_SystemCursor toSdlSystemCursor(CursorShape shape) noexcept
{
    switch (shape)
    {
    case CursorShape::Default:
    case CursorShape::Arrow:
        return SDL_SYSTEM_CURSOR_DEFAULT;
    case CursorShape::IBeam:
        return SDL_SYSTEM_CURSOR_TEXT;
    case CursorShape::Wait:
        return SDL_SYSTEM_CURSOR_WAIT;
    case CursorShape::Crosshair:
        return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case CursorShape::WaitArrow:
        return SDL_SYSTEM_CURSOR_PROGRESS;
    case CursorShape::ResizeNWSE:
        return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case CursorShape::ResizeNESW:
        return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case CursorShape::ResizeWE:
        return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case CursorShape::ResizeNS:
        return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case CursorShape::ResizeAll:
    case CursorShape::Window:
        return SDL_SYSTEM_CURSOR_MOVE;
    case CursorShape::No:
        return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    case CursorShape::Hand:
        return SDL_SYSTEM_CURSOR_POINTER;
    case CursorShape::Count:
    default:
        return SDL_SYSTEM_CURSOR_DEFAULT;
    }
}

/// @brief Maps an engine `WindowFlashMode` to an `SDL_FlashOperation` enumerator.
/// @param[in] mode Engine window flash mode.
/// @return Corresponding `SDL_FlashOperation` value.
[[nodiscard]] constexpr SDL_FlashOperation toSdlFlashOperation(WindowFlashMode mode) noexcept
{
    switch (mode)
    {
    case WindowFlashMode::None:
        return SDL_FLASH_CANCEL;
    case WindowFlashMode::Briefly:
        return SDL_FLASH_BRIEFLY;
    case WindowFlashMode::UntilFocused:
    default:
        return SDL_FLASH_UNTIL_FOCUSED;
    }
}

}   // namespace

Window::Window(const WindowDesc& desc) noexcept
    : gp::hal::generic::Window(desc)
{
    if (m_desc.startOpened)
    {
        open();
    }
}

Window::~Window() noexcept
{
    close();
}

void Window::open() noexcept
{
    if (m_window != nullptr)
    {
        return;   // Window is already open.
    }

    if (m_desc.useHeadless)
    {
        m_desc.startVisible = false;
        m_desc.startFocused = false;
        m_desc.mode = WindowMode::Windowed;
        m_desc.initialState = WindowState::Hidden;
        m_desc.opacity = 1.0f;
    }

    const SDL_PropertiesID props = SDL_CreateProperties();

    // m_titleBuffer is guaranteed null-terminated by GenericWindow::cacheTitleString()
    (void)SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, m_titleBuffer);
    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, static_cast<Sint64>(m_desc.size.width));
    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, static_cast<Sint64>(m_desc.size.height));
    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, static_cast<Sint64>(m_desc.position.x));
    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, static_cast<Sint64>(m_desc.position.y));

    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;   // Visibility is applied explicitly after configuration.

    if (m_desc.useHighDPI)
    {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    if ((m_desc.style & WindowStyle::Resizable) != WindowStyle::None)
    {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if ((m_desc.style & WindowStyle::NoTaskbar) != WindowStyle::None)
    {
        flags |= SDL_WINDOW_UTILITY;
    }
    if ((m_desc.style & WindowStyle::AlwaysOnTop) != WindowStyle::None)
    {
        flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }
    if ((m_desc.style & WindowStyle::Transparent) != WindowStyle::None)
    {
        flags |= SDL_WINDOW_TRANSPARENT;
    }

    const bool hasTitleBar = gp::enums::hasAllFlags(m_desc.style, WindowStyle::TitleBar);
    if (!hasTitleBar || m_desc.mode == WindowMode::Borderless)
    {
        flags |= SDL_WINDOW_BORDERLESS;
    }

    if (m_desc.mode == WindowMode::Fullscreen || m_desc.mode == WindowMode::FullscreenDesktop)
    {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    if (m_desc.initialState == WindowState::Minimized)
    {
        flags |= SDL_WINDOW_MINIMIZED;
    }
    else if (m_desc.initialState == WindowState::Maximized)
    {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

    if (!m_desc.focusOnShow)
    {
        flags |= SDL_WINDOW_NOT_FOCUSABLE;
    }

    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, static_cast<Sint64>(flags));

    // Support embedding the engine viewport inside an external editor/tool parent window
    switch (m_desc.nativeParentHandle.backend)
    {
    case NativeWindowBackend::Win32:
        if (m_desc.nativeParentHandle.win32.hwnd != nullptr)
        {
            (void)SDL_SetPointerProperty(
                props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, m_desc.nativeParentHandle.win32.hwnd
            );
        }
        break;
    case NativeWindowBackend::Cocoa:
        if (m_desc.nativeParentHandle.cocoa.nsWindow != nullptr)
        {
            (void)SDL_SetPointerProperty(
                props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, m_desc.nativeParentHandle.cocoa.nsWindow
            );
        }
        break;
    case NativeWindowBackend::X11:
        if (m_desc.nativeParentHandle.x11.window != 0u)
        {
            (void)SDL_SetNumberProperty(
                props,
                SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER,
                static_cast<Sint64>(m_desc.nativeParentHandle.x11.window)
            );
        }
        break;
    case NativeWindowBackend::Wayland:
        if (m_desc.nativeParentHandle.wayland.surface != nullptr)
        {
            (void)SDL_SetPointerProperty(
                props, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, m_desc.nativeParentHandle.wayland.surface
            );
        }
        break;
    default:
        break;
    }

    m_window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if (m_window == nullptr) [[unlikely]]
    {
        m_isOpen = false;
        m_windowId = kInvalidWindowId;
        return;
    }

    m_windowId = static_cast<WindowId>(SDL_GetWindowID(m_window));
    m_isOpen = true;
    m_isCloseRequested = false;

    cacheNativeHandle();

    if (m_desc.minimumSize.width > 0u || m_desc.minimumSize.height > 0u)
    {
        (void)SDL_SetWindowMinimumSize(
            m_window, static_cast<int>(m_desc.minimumSize.width), static_cast<int>(m_desc.minimumSize.height)
        );
    }

    if (m_desc.maximumSize.width > 0u || m_desc.maximumSize.height > 0u)
    {
        (void)SDL_SetWindowMaximumSize(
            m_window, static_cast<int>(m_desc.maximumSize.width), static_cast<int>(m_desc.maximumSize.height)
        );
    }

    if (m_desc.opacity >= 0.0f && m_desc.opacity < 1.0f)
    {
        (void)SDL_SetWindowOpacity(m_window, m_desc.opacity);
    }

    setCursorShape(m_desc.cursorShape);
    setCursorMode(m_desc.cursorMode);

    if (m_desc.grabMouse)
    {
        (void)SDL_SetWindowMouseGrab(m_window, true);
    }
    if (m_desc.grabKeyboard)
    {
        (void)SDL_SetWindowKeyboardGrab(m_window, true);
    }

    if (!m_desc.useHeadless && m_desc.startVisible && m_desc.initialState != WindowState::Hidden)
    {
        if (m_desc.startFocused)
        {
            (void)SDL_ShowWindow(m_window);
            (void)SDL_RaiseWindow(m_window);
            m_isFocused = true;
        }
        else
        {
            (void)SDL_SetWindowFocusable(m_window, false);
            (void)SDL_ShowWindow(m_window);
            if (m_desc.focusOnShow)
            {
                (void)SDL_SetWindowFocusable(m_window, true);
            }
            m_isFocused = false;
        }
    }

    refreshShadowMetrics();
}

void Window::close() noexcept
{
    if (m_cursor != nullptr)
    {
        SDL_DestroyCursor(m_cursor);
        m_cursor = nullptr;
    }

    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    gp::hal::generic::Window::close();
}

void Window::show() noexcept
{
    if (m_window == nullptr || m_desc.useHeadless)
    {
        return;
    }

    (void)SDL_ShowWindow(m_window);
    gp::hal::generic::Window::show();
}

void Window::hide() noexcept
{
    if (m_window == nullptr)
    {
        return;
    }

    (void)SDL_HideWindow(m_window);
    gp::hal::generic::Window::hide();
}

void Window::minimize() noexcept
{
    if (m_window == nullptr)
    {
        return;
    }

    (void)SDL_MinimizeWindow(m_window);
    gp::hal::generic::Window::minimize();
}

void Window::maximize() noexcept
{
    if (m_window == nullptr)
    {
        return;
    }

    (void)SDL_MaximizeWindow(m_window);
    gp::hal::generic::Window::maximize();
}

void Window::restore() noexcept
{
    if (m_window == nullptr)
    {
        return;
    }

    (void)SDL_RestoreWindow(m_window);
    gp::hal::generic::Window::restore();
}

void Window::focus() noexcept
{
    if (m_window == nullptr || m_desc.useHeadless)
    {
        return;
    }

    (void)SDL_RaiseWindow(m_window);
    gp::hal::generic::Window::focus();
}

void Window::requestAttention(WindowFlashMode mode) noexcept
{
    if (m_window == nullptr || m_desc.useHeadless)
    {
        return;
    }

    (void)SDL_FlashWindow(m_window, toSdlFlashOperation(mode));
}

void Window::centerOnDisplay([[maybe_unused]] const IDisplay* display) noexcept
{
    if (m_window == nullptr)
    {
        return;
    }

    const SDL_DisplayID displayId = (m_currentDisplayId != 0u) ? m_currentDisplayId : SDL_GetDisplayForWindow(m_window);
    const int centeredPos = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(displayId));
    (void)SDL_SetWindowPosition(m_window, centeredPos, centeredPos);

    int actualX = 0;
    int actualY = 0;
    if (SDL_GetWindowPosition(m_window, &actualX, &actualY))
    {
        m_desc.position = Point2D(actualX, actualY);
    }
}

void Window::setPosition(const Point2D& position) noexcept
{
    gp::hal::generic::Window::setPosition(position);

    if (m_window != nullptr)
    {
        (void)SDL_SetWindowPosition(m_window, position.x, position.y);
    }
}

void Window::setPosition(Int32 x, Int32 y) noexcept
{
    setPosition(Point2D(x, y));
}

void Window::setSize(const Extent2D& size) noexcept
{
    gp::hal::generic::Window::setSize(size);

    if (m_window != nullptr)
    {
        (void)SDL_SetWindowSize(m_window, static_cast<int>(size.width), static_cast<int>(size.height));
        refreshShadowMetrics();
    }
}

void Window::setSize(UInt32 width, UInt32 height) noexcept
{
    setSize(Extent2D(width, height));
}

void Window::setFullSize(const Extent2D& size) noexcept
{
    if (m_window != nullptr)
    {
        (void)SDL_GetWindowBordersSize(m_window, &m_borderTop, &m_borderLeft, &m_borderBottom, &m_borderRight);
    }

    gp::hal::generic::Window::setFullSize(size);
}

void Window::setFullSize(UInt32 width, UInt32 height) noexcept
{
    setFullSize(Extent2D(width, height));
}

void Window::setSizeLimits(const Extent2D& minimum, const Extent2D& maximum) noexcept
{
    setMinimumSize(minimum);
    setMaximumSize(maximum);
}

void Window::setMinimumSize(const Extent2D& size) noexcept
{
    gp::hal::generic::Window::setMinimumSize(size);

    if (m_window != nullptr)
    {
        (void)SDL_SetWindowMinimumSize(m_window, static_cast<int>(size.width), static_cast<int>(size.height));
    }
}

void Window::setMinimumSize(UInt32 width, UInt32 height) noexcept
{
    setMinimumSize(Extent2D(width, height));
}

void Window::setMaximumSize(const Extent2D& size) noexcept
{
    gp::hal::generic::Window::setMaximumSize(size);

    if (m_window != nullptr)
    {
        (void)SDL_SetWindowMaximumSize(m_window, static_cast<int>(size.width), static_cast<int>(size.height));
    }
}

void Window::setMaximumSize(UInt32 width, UInt32 height) noexcept
{
    setMaximumSize(Extent2D(width, height));
}

void Window::setTitle(StringView title) noexcept
{
    gp::hal::generic::Window::setTitle(title);

    if (m_window != nullptr)
    {
        (void)SDL_SetWindowTitle(m_window, m_titleBuffer);
    }
}

void Window::setOpacity(Float32 opacity) noexcept
{
    gp::hal::generic::Window::setOpacity(opacity);

    if (m_window != nullptr && !m_desc.useHeadless)
    {
        (void)SDL_SetWindowOpacity(m_window, m_desc.opacity);
    }
}

void Window::setResizable(bool resizable) noexcept
{
    gp::hal::generic::Window::setResizable(resizable);

    if (m_window != nullptr)
    {
        (void)SDL_SetWindowResizable(m_window, resizable);
    }
}

void Window::setAlwaysOnTop(bool alwaysOnTop) noexcept
{
    gp::hal::generic::Window::setAlwaysOnTop(alwaysOnTop);

    if (m_window != nullptr)
    {
        (void)SDL_SetWindowAlwaysOnTop(m_window, alwaysOnTop);
    }
}

void Window::setWindowMode(WindowMode mode, const IDisplay* targetDisplay) noexcept
{
    gp::hal::generic::Window::setWindowMode(mode, targetDisplay);

    if (m_window == nullptr || m_desc.useHeadless)
    {
        return;
    }

    switch (mode)
    {
    case WindowMode::Windowed:
        (void)SDL_SetWindowFullscreen(m_window, false);
        (void)SDL_SetWindowBordered(m_window, gp::enums::hasAllFlags(m_desc.style, WindowStyle::TitleBar));
        break;

    case WindowMode::Borderless:
        (void)SDL_SetWindowFullscreen(m_window, false);
        (void)SDL_SetWindowBordered(m_window, false);
        break;

    case WindowMode::Fullscreen:
    case WindowMode::FullscreenDesktop:
        (void)SDL_SetWindowFullscreen(m_window, true);
        break;

    default:
        break;
    }

    refreshShadowMetrics();
}

void Window::setCursorMode(CursorMode mode) noexcept
{
    gp::hal::generic::Window::setCursorMode(mode);

    if (m_window == nullptr || m_desc.useHeadless)
    {
        return;
    }

    switch (mode)
    {
    case CursorMode::Normal:
        (void)SDL_SetWindowRelativeMouseMode(m_window, false);
        (void)SDL_SetWindowMouseGrab(m_window, m_desc.grabMouse);
        (void)SDL_ShowCursor();
        break;

    case CursorMode::Hidden:
        (void)SDL_SetWindowRelativeMouseMode(m_window, false);
        (void)SDL_SetWindowMouseGrab(m_window, m_desc.grabMouse);
        (void)SDL_HideCursor();
        break;

    case CursorMode::Locked:
        // SDL3 relative mouse mode hides the cursor, locks it to the window, and emits raw relative deltas.
        (void)SDL_SetWindowRelativeMouseMode(m_window, true);
        break;

    case CursorMode::Confined:
        (void)SDL_SetWindowRelativeMouseMode(m_window, false);
        (void)SDL_SetWindowMouseGrab(m_window, true);
        (void)SDL_ShowCursor();
        break;

    default:
        break;
    }
}

void Window::setMouseGrabbed(bool grabbed) noexcept
{
    gp::hal::generic::Window::setMouseGrabbed(grabbed);

    if (m_window != nullptr && m_desc.cursorMode != CursorMode::Confined && m_desc.cursorMode != CursorMode::Locked)
    {
        (void)SDL_SetWindowMouseGrab(m_window, grabbed);
    }
}

void Window::setKeyboardGrabbed(bool grabbed) noexcept
{
    gp::hal::generic::Window::setKeyboardGrabbed(grabbed);

    if (m_window != nullptr)
    {
        (void)SDL_SetWindowKeyboardGrab(m_window, grabbed);
    }
}

void Window::setCursorShape(CursorShape shape) noexcept
{
    gp::hal::generic::Window::setCursorShape(shape);

    if (m_window == nullptr || m_desc.useHeadless)
    {
        return;
    }

    SDL_Cursor* newCursor = SDL_CreateSystemCursor(toSdlSystemCursor(shape));
    if (newCursor != nullptr)
    {
        (void)SDL_SetCursor(newCursor);
        if (m_cursor != nullptr)
        {
            SDL_DestroyCursor(m_cursor);
        }
        m_cursor = newCursor;
    }
}

void Window::setCursorPosition(const Point2D& position) noexcept
{
    if (m_window != nullptr && !m_desc.useHeadless)
    {
        SDL_WarpMouseInWindow(m_window, static_cast<float>(position.x), static_cast<float>(position.y));
    }
}

void Window::refreshShadowMetrics() noexcept
{
    if (m_window == nullptr)
    {
        return;
    }

    int posX = 0;
    int posY = 0;
    if (SDL_GetWindowPosition(m_window, &posX, &posY))
    {
        m_desc.position = Point2D(posX, posY);
    }

    int logicalW = 0;
    int logicalH = 0;
    if (SDL_GetWindowSize(m_window, &logicalW, &logicalH))
    {
        m_desc.size = Extent2D(
            static_cast<UInt32>(logicalW > 0 ? logicalW : 0), static_cast<UInt32>(logicalH > 0 ? logicalH : 0)
        );
    }

    int pixelW = 0;
    int pixelH = 0;
    if (SDL_GetWindowSizeInPixels(m_window, &pixelW, &pixelH))
    {
        m_framebufferSize =
            Extent2D(static_cast<UInt32>(pixelW > 0 ? pixelW : 0), static_cast<UInt32>(pixelH > 0 ? pixelH : 0));
    }
    else
    {
        m_framebufferSize = m_desc.size;
    }

    (void)SDL_GetWindowBordersSize(m_window, &m_borderTop, &m_borderLeft, &m_borderBottom, &m_borderRight);

    const float scale = SDL_GetWindowDisplayScale(m_window);
    m_contentScale = (scale > 0.0f) ? scale : 1.0f;

    m_currentDisplayId = static_cast<gp::UInt32>(SDL_GetDisplayForWindow(m_window));
}

void Window::cacheNativeHandle() noexcept
{
    m_nativeHandle = NativeWindowHandle{ NativeWindowBackend::Unknown };

    if (m_window == nullptr)
    {
        return;
    }

    const SDL_PropertiesID props = SDL_GetWindowProperties(m_window);

    if (void* hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr))
    {
        m_nativeHandle.backend = NativeWindowBackend::Win32;
        m_nativeHandle.win32.hwnd = hwnd;
        m_nativeHandle.win32.hinstance = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
        return;
    }

    if (void* wlSurface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr))
    {
        m_nativeHandle.backend = NativeWindowBackend::Wayland;
        m_nativeHandle.wayland.surface = wlSurface;
        m_nativeHandle.wayland.display =
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        return;
    }

    if (const Sint64 x11Window = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0))
    {
        m_nativeHandle.backend = NativeWindowBackend::X11;
        m_nativeHandle.x11.window = static_cast<gp::UIntPtr>(x11Window);
        m_nativeHandle.x11.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        return;
    }

    if (void* nsWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr))
    {
        m_nativeHandle.backend = NativeWindowBackend::Cocoa;
        m_nativeHandle.cocoa.nsWindow = nsWindow;
        return;
    }

    if (void* androidWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr))
    {
        m_nativeHandle.backend = NativeWindowBackend::Android;
        m_nativeHandle.android.window = androidWindow;
        m_nativeHandle.android.surface =
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_SURFACE_POINTER, nullptr);
        return;
    }

    if (void* uiWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr))
    {
        m_nativeHandle.backend = NativeWindowBackend::UIKit;
        m_nativeHandle.uikit.uiWindow = uiWindow;
        m_nativeHandle.uikit.metalViewTag =
            static_cast<gp::UIntPtr>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_UIKIT_METAL_VIEW_TAG_NUMBER, 0));
        return;
    }

    if (void* gbmDevice = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_KMSDRM_GBM_DEVICE_POINTER, nullptr))
    {
        m_nativeHandle.backend = NativeWindowBackend::KMSDRM;
        m_nativeHandle.kmsdrm.gbmDevice = gbmDevice;
        m_nativeHandle.kmsdrm.drmFd =
            static_cast<gp::Int32>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_KMSDRM_DRM_FD_NUMBER, -1));
        m_nativeHandle.kmsdrm.deviceIndex =
            static_cast<gp::UInt32>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_KMSDRM_DEVICE_INDEX_NUMBER, 0));
        return;
    }

    if (void* vivanteWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_VIVANTE_WINDOW_POINTER, nullptr))
    {
        m_nativeHandle.backend = NativeWindowBackend::Vivante;
        m_nativeHandle.vivante.window = vivanteWindow;
        m_nativeHandle.vivante.display =
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_VIVANTE_DISPLAY_POINTER, nullptr);
        m_nativeHandle.vivante.surface =
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_VIVANTE_SURFACE_POINTER, nullptr);
        return;
    }
}

}   // namespace gp::hal::sdl3
