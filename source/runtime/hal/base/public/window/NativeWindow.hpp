// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Supported native window backends platform capabilities.
enum class NativeWindowBackend : gp::UInt8
{
    Unknown,
    Win32,     // Windows Desktop
    Cocoa,     // macOS
    UIKit,     // iOS / tvOS
    Wayland,   // Linux (Modern)
    X11,       // Linux (Legacy)
    Android,   // Android
    KMSDRM,    // Linux Console / Embedded (Direct to display)
    Vivante    // Embedded Linux
};

namespace detail
{

/// @brief A structure that holds the native window handles for Win32.
struct Win32WindowHandles final
{
    void* hwnd;        // HWND
    void* hinstance;   // HINSTANCE
};

/// @brief A structure that holds the native window handles for Cocoa (macOS).
struct CocoaWindowHandles final
{
    void* nsWindow;   // NSWindow*
};

/// @brief A structure that holds the native window handles for UIKit (iOS/tvOS).
struct UIKitWindowHandles final
{
    void* uiWindow;             // UIWindow*
    gp::UIntPtr metalViewTag;   // Often required to query the MTKView from SDL on iOS
};

/// @brief A structure that holds the native window handles for Wayland.
struct WaylandWindowHandles final
{
    void* display;   // wl_display*
    void* surface;   // wl_surface*
};

/// @brief A structure that holds the native window handles for X11.
struct X11WindowHandles final
{
    void* display;        // Display*
    gp::UIntPtr window;   // Window (XID)
};

/// @brief A structure that holds the native window handles for Android.
struct AndroidWindowHandles final
{
    void* window;    // ANativeWindow*
    void* surface;   // EGLSurface
};

/// @brief A structure that holds the native window handles for KMS/DRM.
struct KMSDRMWindowHandles final
{
    void* gbmDevice;          // gbm_device*
    void* gbmSurface;         // gbm_surface*
    gp::Int32 drmFd;          // DRM file descriptor
    gp::UInt32 deviceIndex;   // Device index number
};

/// @brief A structure that holds the native window handles for Vivante systems.
struct VivanteWindowHandles final
{
    void* display;   // EGLNativeDisplayType
    void* window;    // EGLNativeWindowType
    void* surface;   // EGLSurface
};

}   // namespace detail

/// @brief A structure that holds the native window handles for various platforms.
/// The actual type and meaning of these handles is platform-specific. It may be used for advanced operations
/// that are not covered by this interface, but the caller must know the expected type and semantics of the
/// handle for the specific platform. This may return nullptr if no native handle is available.
struct NativeWindowHandle final
{
public:
    NativeWindowBackend backend{ NativeWindowBackend::Unknown };

    union
    {
        detail::Win32WindowHandles win32;
        detail::CocoaWindowHandles cocoa;
        detail::UIKitWindowHandles uikit;
        detail::WaylandWindowHandles wayland;
        detail::X11WindowHandles x11;
        detail::AndroidWindowHandles android;
        detail::KMSDRMWindowHandles kmsdrm;
        detail::VivanteWindowHandles vivante;
        void* _storage[4];
    };

public:
    /// @brief Default constructor initializes the backend to Unknown and clears the storage.
    constexpr NativeWindowHandle(NativeWindowBackend backend = NativeWindowBackend::Unknown) noexcept
        : backend(backend)
        , _storage{}
    {}
};

}   // namespace gp::hal
