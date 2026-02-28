// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/ContainerForward.hpp"
#include "Container/Span.hpp"
#include "Events/MulticastDelegate.hpp"
#include "Events/ScopedDelegateHandle.hpp"
#include "HALBuild.hpp"
#include "HALForward.hpp"
#include "Monitor/IMonitor.hpp"
#include "Monitor/MonitorEvents.hpp"
#include "Windowing/IWindow.hpp"
#include "Windowing/WindowDesc.hpp"
#include <memory>   // TODO: Replace std::unique_ptr with GP::UniquePtr when available.

namespace GP::HAL
{

/// \brief Central HAL subsystem responsible for monitor enumeration, window creation, and OS event dispatching.
///
/// IWindowingSystem is the single entry point for all display-related operations in the GP HAL layer. It owns the
/// lifetime of every IWindow and IMonitor object vended to the application, and drives their event loops via
/// PollEvents(). The concrete implementation (SDL3, Win32, etc.) is selected at link time; client code always
/// programs against this interface.
///
/// ### Lifecycle
/// \code
/// // 1. Create the system (implementation chosen at link time).
/// TUniquePtr<IWindowingSystem> windowingSystem = IWindowingSystem::Create();
///
/// // 2. Create one or more windows.
/// FWindowDesc desc;
/// desc.title  = "My App";
/// desc.width  = 1920;
/// desc.height = 1080;
/// IWindow* window = windowingSystem->CreateWindow(desc);
///
/// // 3. Run the main loop.
/// while (window->IsOpen())
/// {
///     windowingSystem->PollEvents();
///     // ... render ...
/// }
///
/// // 4. Explicit destruction is optional; IWindowingSystem::~IWindowingSystem destroys all owned windows.
/// windowingSystem->DestroyWindow(window);
/// \endcode
///
/// ### Ownership model
/// - IWindowingSystem owns all IWindow and IMonitor instances it creates.
/// - Pointers returned by CreateWindow() and GetMonitor*() are valid until the corresponding
///   DestroyWindow() call or until the IWindowingSystem itself is destroyed.
/// - Storing raw pointers beyond those lifetimes is undefined behaviour.
///
/// ### Threading
/// All methods must be called from the thread that created the IWindowingSystem (typically the main thread).
/// PollEvents() is not thread-safe.
class IWindowingSystem
{
public:
    using FEventHandle = GP::Events::FDelegateHandle;
    using FScopedHandle = GP::Events::FScopedDelegateHandle;
    template <typename Signature>
    using TEvent = GP::Events::TMulticastDelegate<Signature>;

public:
    TEvent<void(const FMonitorEvents::Connected&)> OnMonitorConnected;
    TEvent<void(const FMonitorEvents::Disconnected&)> OnMonitorDisconnected;
    TEvent<void(const FMonitorEvents::PrimaryChanged&)> OnPrimaryMonitorChanged;

public:
    /// \brief Virtual destructor. Destroys all windows and releases all platform resources.
    virtual ~IWindowingSystem() = default;

public:
    /// \brief Creates and initializes the platform windowing system.
    /// The concrete implementation is determined at link time (e.g., SDL3, Win32, Cocoa). Only one instance
    /// should exist per process; creating a second instance before destroying the first is undefined behaviour
    /// on most backends.
    /// \return A TUniquePtr owning the newly constructed windowing system.
    /// \note This function is implemented in the private platform translation unit (e.g., SDL3WindowingSystem.cpp). The
    /// public header never includes any platform-specific headers.
    GP_NODISCARD static std::unique_ptr<IWindowingSystem> Create();

public:
    /// \brief Creates a new window according to the given descriptor.
    /// The returned pointer is owned by this IWindowingSystem and remains valid until DestroyWindow() is called
    /// or the system itself is destroyed.
    /// \param desc A fully populated FWindowDesc describing the desired window configuration.
    /// \return A non-owning raw pointer to the created IWindow, or nullptr if creation failed.
    GP_NODISCARD virtual IWindow* CreateWindow(const FWindowDesc& desc) = 0;

    /// \brief Destroys a window that was previously created by this system.
    /// Fires IWindow::OnClosed before releasing resources. After this call the pointer is invalid.
    /// \param window The window to destroy. Passing nullptr is a no-op.
    virtual void DestroyWindow(IWindow* window) = 0;

    /// \brief Returns a view of all currently alive windows owned by this system.
    /// The span is invalidated by any call to CreateWindow() or DestroyWindow().
    /// \return A read-only span of raw IWindow pointers.
    GP_NODISCARD virtual Container::TSpan<IWindow* const> GetWindows() const noexcept = 0;

    /// \brief Returns the number of windows currently alive.
    /// \return Window count.
    GP_NODISCARD virtual UInt32 GetWindowCount() const noexcept = 0;

    /// \brief Checks whether any window created by this system is currently alive.
    /// \return True if at least one window exists, false otherwise.
    GP_NODISCARD virtual bool HasWindows() const noexcept = 0;

    /// \brief Looks up an IWindow by its native OS handle.
    /// Useful when integrating with OS-level message hooks that only provide the native handle.
    /// \param nativeHandle The platform-specific native window handle (e.g., HWND, NSWindow*, SDL_Window*).
    /// \return A pointer to the matching IWindow, or nullptr if not found.
    GP_NODISCARD virtual IWindow* FindWindowByNativeHandle(void* nativeHandle) const noexcept = 0;

    /// \brief Returns the OS-designated primary monitor.
    /// \return A non-owning raw pointer to the primary IMonitor. Never nullptr on a system with at least one display.
    GP_NODISCARD virtual const IMonitor* GetPrimaryMonitor() const noexcept = 0;

    /// \brief Returns a non-owning mutable pointer to the primary monitor.
    /// \return Mutable pointer to the primary IMonitor.
    GP_NODISCARD virtual IMonitor* GetPrimaryMonitor() noexcept = 0;

    /// \brief Returns a read-only view of all currently connected monitors.
    /// The primary monitor is always the first element. The span is invalidated when a monitor is connected or
    /// disconnected (i.e., after a PollEvents() call that processes a display change event).
    /// \return A read-only span of raw IMonitor pointers, sorted so index 0 is the primary display.
    GP_NODISCARD virtual Container::TSpan<IMonitor* const> GetMonitors() const noexcept = 0;

    /// \brief Returns a mutable view of all currently connected monitors.
    /// \return A mutable span of IMonitor pointers.
    GP_NODISCARD virtual Container::TSpan<IMonitor*> GetMonitors() noexcept = 0;

    /// \brief Returns the number of currently connected monitors.
    /// \return Monitor count.
    GP_NODISCARD virtual UInt32 GetMonitorCount() const noexcept = 0;

    /// \brief Looks up an IMonitor by its OS-assigned unique identifier string.
    /// \param uniqueID The platform-specific monitor ID (from IMonitor::GetUniqueID()).
    /// \return A pointer to the matching IMonitor, or nullptr if not found.
    GP_NODISCARD virtual const IMonitor* FindMonitorByID(FStringView uniqueID) const noexcept = 0;

    /// \brief Looks up an IMonitor by its native OS handle.
    /// \param nativeHandle Platform handle (HMONITOR, CGDirectDisplayID cast, RROutput, wl_output*).
    /// \return A pointer to the matching IMonitor, or nullptr if not found.
    GP_NODISCARD virtual const IMonitor* FindMonitorByNativeHandle(void* nativeHandle) const noexcept = 0;

    /// \brief Finds the monitor that contains the largest area of the given window's content rectangle.
    /// When the window spans multiple monitors, the one with the greatest overlap is returned.
    /// \param window A valid, non-null IWindow pointer.
    /// \return A pointer to the best-fit IMonitor. Returns the primary monitor as a fallback. Never nullptr.
    GP_NODISCARD virtual const IMonitor* FindMonitorForWindow(const IWindow* window) const noexcept = 0;

    /// \brief Forces a re-enumeration of the connected monitor list from the OS.
    /// Call this after a WM_DISPLAYCHANGE (Windows) or RRScreenChangeNotify (Linux/X11) event if you bypass
    /// the standard PollEvents() mechanism.
    virtual void RefreshMonitors() = 0;

    /// \brief Processes all pending OS events and dispatches them to the appropriate IWindow or IMonitor delegates.
    /// Must be called once per frame from the main thread before reading any window or input state.
    virtual void PollEvents() = 0;

    /// \brief Blocks the calling thread until at least one OS event is available, then processes all pending events.
    /// Useful for non-game applications (editors, tools) that should not busy-wait when idle.
    virtual void WaitEvents() = 0;

    /// \brief Blocks the calling thread for at most \p timeoutSeconds seconds waiting for OS events.
    /// Returns immediately if an event arrives before the timeout expires.
    /// \param timeoutSeconds Maximum time to block in seconds. A value of 0.0 is equivalent to PollEvents().
    virtual void WaitEventsTimeout(Float64 timeoutSeconds) = 0;

    /// \brief Posts a synthetic wake-up event to unblock a thread currently sleeping in WaitEvents() or
    /// WaitEventsTimeout(). Safe to call from any thread.
    virtual void PostEmptyEvent() = 0;

    /// \brief Returns a human-readable string identifying the backend in use (e.g., "SDL3", "Win32", "Cocoa").
    /// \return A string view into a statically allocated literal. Never nullptr.
    GP_NODISCARD virtual FStringView GetBackendName() const noexcept = 0;

    /// \brief Returns a human-readable string describing the underlying windowing protocol in use
    /// (e.g., "Windows GDI", "X11", "Wayland", "Cocoa", "UIKit", "WebGL").
    /// Useful for logging, crash reports, and conditional feature enablement.
    /// \return A string view into a statically allocated or internally owned string.
    GP_NODISCARD virtual FStringView GetPlatformName() const noexcept = 0;
};

}   // namespace GP::HAL
