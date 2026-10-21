// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "window/SDL3Window.hpp"
#include "templates/Enums.hpp"
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

void Window::open()
{
    if (m_window != nullptr)
    {
        return;   // Window is already open, no action needed.
    }

    if (m_desc.useHeadless)
    {
        m_desc.startVisible = false;
        m_desc.startFocused = false;
        m_desc.mode = WindowMode::Windowed;
        m_desc.initialState = WindowState::Normal;
        m_desc.opacity = 1.0f;
    }

    SDL_PropertiesID props = SDL_CreateProperties();

    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, m_desc.title.data());

    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, static_cast<Sint64>(m_desc.size.width));
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, static_cast<Sint64>(m_desc.size.height));

    int posX = m_desc.position.x;
    int posY = m_desc.position.y;

    if (m_desc.targetDisplay)
    {
        // Assuming Display class has a way to get the native SDL_DisplayID
        // SDL_DisplayID displayId = m_desc.targetDisplay->getNativeId();
        // posX = SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayId);
        // posY = SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayId);
    }

    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, posX);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, posY);

    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;   // We control visibility manually after creation.

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

    bool hasTitleBar = gp::enums::hasAllFlags(m_desc.style, WindowStyle::TitleBar);
    if (!hasTitleBar || m_desc.mode == WindowMode::Borderless)
    {
        flags |= SDL_WINDOW_BORDERLESS;
    }

    if (m_desc.mode == WindowMode::Fullscreen || m_desc.mode == WindowMode::FullscreenDesktop)
    {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    // Depending on your definition of WindowState
    if (m_desc.initialState == WindowState::Minimized)
    {
        flags |= SDL_WINDOW_MINIMIZED;
    }
    if (m_desc.initialState == WindowState::Maximized)
    {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, static_cast<Sint64>(flags));

    if (m_desc.nativeParentHandle.backend != NativeWindowBackend::Unknown)
    {
        // TODO: Set the native parent handle for the window if provided.
    }

    m_window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if (!m_window)
    {
        // GP_LOG_ERROR("Failed to create SDL3 Window: %s", SDL_GetError());
        return;
    }

    m_isOpen = true;
    m_vSyncEnabled = m_desc.useVSync;

    cacheNativeHandle();

    if (m_desc.minimumSize.width > 0 || m_desc.minimumSize.height > 0)
    {
        SDL_SetWindowMinimumSize(
            m_window, static_cast<int>(m_desc.minimumSize.width), static_cast<int>(m_desc.minimumSize.height)
        );
    }

    if (m_desc.maximumSize.width > 0 || m_desc.maximumSize.height > 0)
    {
        SDL_SetWindowMaximumSize(
            m_window, static_cast<int>(m_desc.maximumSize.width), static_cast<int>(m_desc.maximumSize.height)
        );
    }

    if (m_desc.opacity >= 0.0f && m_desc.opacity < 1.0f)
    {
        SDL_SetWindowOpacity(m_window, m_desc.opacity);
    }

    if (m_desc.grabMouse)
    {
        SDL_SetWindowMouseGrab(m_window, true);
    }
    if (m_desc.grabKeyboard)
    {
        SDL_SetWindowKeyboardGrab(m_window, true);
    }

    // Ensure cursor shape is initialized properly
    // setCursorShape(m_desc.cursorShape);

    if (!m_desc.useHeadless && m_desc.startVisible)
    {
        if (m_desc.startFocused)
        {
            SDL_ShowWindow(m_window);
            SDL_RaiseWindow(m_window);
        }
        else
        {
            SDL_SetWindowFocusable(m_window, false);
            SDL_ShowWindow(m_window);
            SDL_SetWindowFocusable(m_window, true);
        }
    }
}

void Window::close()
{
    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    m_nativeHandle = NativeWindowHandle{};
    m_isOpen = false;
}

void Window::cacheNativeHandle() noexcept
{
    m_nativeHandle = NativeWindowHandle{ NativeWindowBackend::Unknown };

    if (m_window == nullptr)
    {
        return;   // No window to cache handles from.
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(m_window);

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

    if (Sint64 x11Window = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0))
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
