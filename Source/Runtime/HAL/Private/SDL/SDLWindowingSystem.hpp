// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Array.hpp"
#include "Container/ContainerForward.hpp"
#include "HALBuild.hpp"
#include "Windowing/IWindowingSystem.hpp"
#include <SDL3/SDL.h>

namespace GP::HAL
{

// Forward declarations
class SDLWindow;
class SDLMonitor;

/// \brief SDL3 backend implementation of IWindowingSystem.
/// Manages SDL3 initialization/teardown, routes SDL events to individual FSDL3Window instances, and maintains
/// the live collections of windows and monitors. All methods must be called from the main thread.
class SDLWindowingSystem final : public IWindowingSystem
{
private:
    Container::TArray<std::unique_ptr<SDLWindow>> m_windows;
    Container::TArray<std::unique_ptr<SDLMonitor>> m_monitors;
    Container::TArray<IWindow*> m_windowPtrs;
    Container::TArray<IMonitor*> m_monitorPtrs;

public:
    SDLWindowingSystem();
    ~SDLWindowingSystem() override;

    SDLWindowingSystem(const SDLWindowingSystem&) = delete;
    SDLWindowingSystem& operator=(const SDLWindowingSystem&) = delete;
    SDLWindowingSystem(SDLWindowingSystem&&) = delete;
    SDLWindowingSystem& operator=(SDLWindowingSystem&&) = delete;

public:
    GP_NODISCARD IWindow* CreateWindow(const FWindowDesc& desc) override;
    void DestroyWindow(IWindow* window) override;
    GP_NODISCARD Container::TSpan<IWindow* const> GetWindows() const noexcept override;
    GP_NODISCARD UInt32 GetWindowCount() const noexcept override;
    GP_NODISCARD bool HasWindows() const noexcept override;
    GP_NODISCARD IWindow* FindWindowByNativeHandle(void*) const noexcept override;
    GP_NODISCARD const IMonitor* GetPrimaryMonitor() const noexcept override;
    GP_NODISCARD IMonitor* GetPrimaryMonitor() noexcept override;
    GP_NODISCARD Container::TSpan<IMonitor* const> GetMonitors() const noexcept override;
    GP_NODISCARD Container::TSpan<IMonitor*> GetMonitors() noexcept override;
    GP_NODISCARD UInt32 GetMonitorCount() const noexcept override;
    GP_NODISCARD const IMonitor* FindMonitorByID(FStringView uniqueID) const noexcept override;
    GP_NODISCARD const IMonitor* FindMonitorByNativeHandle(void*) const noexcept override;
    GP_NODISCARD const IMonitor* FindMonitorForWindow(const IWindow*) const noexcept override;
    GP_NODISCARD const IMonitor* FindMonitorBySDLDisplayID(SDL_DisplayID displayID) const noexcept;
    void RefreshMonitors() override;
    void PollEvents() override;
    void WaitEvents() override;
    void WaitEventsTimeout(Float64 timeoutSec) override;
    void PostEmptyEvent() override;
    GP_NODISCARD FStringView GetBackendName() const noexcept override;
    GP_NODISCARD FStringView GetPlatformName() const noexcept override;

private:
    /// \brief Processes a single SDL_Event, routing it to the correct window or handling system-level events.
    /// \param event The SDL_Event to process.
    void ProcessEvent(const SDL_Event& event);

    /// \brief Handles SDL_EVENT_WINDOW_* events by forwarding to the target FSDL3Window.
    /// \param event The SDL_Event to process.
    void ProcessWindowEvent(const SDL_Event& event);

    /// \brief Handles SDL_EVENT_DISPLAY_* events by updating the monitor list and firing system-level delegates.
    /// \param event The SDL_Event to process.
    void ProcessDisplayEvent(const SDL_Event& event);

    /// \brief Returns the FSDL3Window that owns the given SDL_WindowID, or nullptr if not found.
    /// \param id The SDL_WindowID to search for.
    /// \return A pointer to the owning FSDL3Window, or nullptr if no match is found.
    GP_NODISCARD SDLWindow* FindSDLWindow(SDL_WindowID id) const noexcept;

    /// \brief Builds the initial monitor list from SDL_GetDisplays().
    void EnumerateMonitors();

    /// \brief Creates or refreshes a single FSDL3Monitor for the given SDL_DisplayID.
    /// \param displayID The SDL_DisplayID to upsert.
    void UpsertMonitor(SDL_DisplayID displayID);

    /// \brief Removes and fires OnMonitorDisconnected for the given SDL_DisplayID.
    /// \param displayID The SDL_DisplayID to remove from the monitor list.
    void RemoveMonitor(SDL_DisplayID displayID);

    /// \brief Rebuilds the flat raw-pointer caches (m_windowPtrs, m_monitorPtrs) after structural changes.
    void RebuildPointerCaches();
};

}   // namespace GP::HAL
