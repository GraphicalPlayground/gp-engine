// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "window/SDL3Window.hpp"
#include "window/NativeWindow.hpp"
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

void Window::cacheNativeHandle() noexcept
{
    m_nativeHandle = NativeWindowHandle{};

    if (!isValid())
    {
        return;
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

[[nodiscard]] NativeWindowHandle Window::getNativeHandle() const noexcept
{
    return m_nativeHandle;
}

}   // namespace gp::hal::sdl3
