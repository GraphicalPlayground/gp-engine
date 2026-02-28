// Copyright (c) - Graphical Playground. All rights reserved.

#include "SDL/SDLWindowingSystem.hpp"
#include "SDL/SDLMonitor.hpp"
#include "SDL/SDLWindow.hpp"
#include "Windowing/IWindowingSystem.hpp"
#include <memory>   // Replace with GP::UniquePtr when available.
#include <SDL3/SDL.h>

namespace GP::HAL
{

std::unique_ptr<IWindowingSystem> IWindowingSystem::Create()
{
    if constexpr (Build::HAL::HasSDL3) { return std::make_unique<SDLWindowingSystem>(); }
    return nullptr;
}

SDLWindowingSystem::SDLWindowingSystem()
{
    const bool initOk = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    GP_ASSERT(initOk, "SDL3 failed to initialize: {}", SDL_GetError());

    EnumerateMonitors();
}

SDLWindowingSystem::~SDLWindowingSystem()
{
    m_windows.Clear();
    m_monitors.Clear();
    SDL_Quit();
}

IWindow* SDLWindowingSystem::CreateWindow(const FWindowDesc& desc)
{
    auto& owned = m_windows.EmplaceBack(std::make_unique<SDLWindow>(desc, this));
    RebuildPointerCaches();
    return owned.get();
}

void SDLWindowingSystem::DestroyWindow(IWindow* window)
{
    if (!window) { return; }

    const auto it = m_windows.FindIf([window](const std::unique_ptr<SDLWindow>& w) { return w.get() == window; });

    if (it != m_windows.End())
    {
        m_windows.EraseSwapBack(it);
        RebuildPointerCaches();
    }
}

Container::TSpan<IWindow* const> SDLWindowingSystem::GetWindows() const noexcept
{
    return Container::TSpan<IWindow* const>{ m_windowPtrs.Data(), m_windowPtrs.Size() };
}

UInt32 SDLWindowingSystem::GetWindowCount() const noexcept { return static_cast<UInt32>(m_windows.Size()); }

bool SDLWindowingSystem::HasWindows() const noexcept { return !m_windows.IsEmpty(); }

IWindow* SDLWindowingSystem::FindWindowByNativeHandle(void* nativeHandle) const noexcept
{
    for (const auto& w: m_windows)
    {
        if (w->GetNativeHandle() == nativeHandle) { return w.get(); }
    }
    return nullptr;
}

const IMonitor* SDLWindowingSystem::GetPrimaryMonitor() const noexcept
{
    return m_monitors.IsEmpty() ? nullptr : m_monitors[0].get();
}

IMonitor* SDLWindowingSystem::GetPrimaryMonitor() noexcept
{
    return m_monitors.IsEmpty() ? nullptr : m_monitors[0].get();
}

Container::TSpan<IMonitor* const> SDLWindowingSystem::GetMonitors() const noexcept
{
    return Container::TSpan<IMonitor* const>{ m_monitorPtrs.Data(), m_monitorPtrs.Size() };
}

Container::TSpan<IMonitor*> SDLWindowingSystem::GetMonitors() noexcept
{
    return Container::TSpan<IMonitor*>{ m_monitorPtrs.Data(), m_monitorPtrs.Size() };
}

UInt32 SDLWindowingSystem::GetMonitorCount() const noexcept { return static_cast<UInt32>(m_monitors.Size()); }

const IMonitor* SDLWindowingSystem::FindMonitorByID(FStringView uniqueID) const noexcept
{
    for (const auto& m: m_monitors)
    {
        if (m->GetUniqueID() == uniqueID) { return m.get(); }
    }
    return nullptr;
}

const IMonitor* SDLWindowingSystem::FindMonitorByNativeHandle(void* nativeHandle) const noexcept
{
    for (const auto& m: m_monitors)
    {
        if (m->GetNativeHandle() == nativeHandle) { return m.get(); }
    }
    return nullptr;
}

const IMonitor* SDLWindowingSystem::FindMonitorForWindow(const IWindow* window) const noexcept
{
    GP_ASSERT(window, "FindMonitorForWindow: window must not be null");

    const auto* sdlWindow = static_cast<const SDLWindow*>(window);
    const SDL_DisplayID displayID = SDL_GetDisplayForWindow(sdlWindow->GetSDLWindow());

    for (const auto& m: m_monitors)
    {
        if (m->GetDisplayID() == displayID) { return m.get(); }
    }

    // Fallback: return primary.
    return GetPrimaryMonitor();
}

void SDLWindowingSystem::RefreshMonitors() { EnumerateMonitors(); }

void SDLWindowingSystem::PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) { ProcessEvent(event); }
}

void SDLWindowingSystem::WaitEvents()
{
    SDL_WaitEvent(nullptr);
    PollEvents();
}

void SDLWindowingSystem::WaitEventsTimeout(Float64 timeoutSec)
{
    SDL_WaitEventTimeout(nullptr, static_cast<Int32>(timeoutSec * 1000.0));
    PollEvents();
}

void SDLWindowingSystem::PostEmptyEvent()
{
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    SDL_PushEvent(&event);
}

FStringView SDLWindowingSystem::GetBackendName() const noexcept { return "SDL3"; }

FStringView SDLWindowingSystem::GetPlatformName() const noexcept { return FStringView{ SDL_GetPlatform() }; }

void SDLWindowingSystem::ProcessEvent(const SDL_Event& event)
{
    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) { ProcessWindowEvent(event); }
    else if (event.type >= SDL_EVENT_DISPLAY_FIRST && event.type <= SDL_EVENT_DISPLAY_LAST)
    {
        ProcessDisplayEvent(event);
    }
}

void SDLWindowingSystem::ProcessWindowEvent(const SDL_Event& event)
{
    SDLWindow* target = FindSDLWindow(event.window.windowID);
    if (target) { target->DispatchSDLEvent(event); }
}

void SDLWindowingSystem::ProcessDisplayEvent(const SDL_Event& event)
{
    const SDL_DisplayID displayID = event.display.displayID;

    switch (event.display.type)
    {
    case SDL_EVENT_DISPLAY_ADDED:
    {
        UpsertMonitor(displayID);
        const IMonitor* newMonitor =
            FindMonitorByNativeHandle(reinterpret_cast<void*>(static_cast<UIntPtr>(displayID)));

        OnMonitorConnected.Broadcast(FMonitorEvents::Connected{ const_cast<IMonitor*>(newMonitor) });
        break;
    }
    case SDL_EVENT_DISPLAY_REMOVED:
    {
        const IMonitor* gone = FindMonitorByNativeHandle(reinterpret_cast<void*>(static_cast<UIntPtr>(displayID)));

        OnMonitorDisconnected.Broadcast(FMonitorEvents::Disconnected{ const_cast<IMonitor*>(gone) });
        RemoveMonitor(displayID);
        break;
    }
    case SDL_EVENT_DISPLAY_MOVED:
    case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    {
        UpsertMonitor(displayID);   // Refresh cached properties.
        break;
    }
    default: break;
    }
}

SDLWindow* SDLWindowingSystem::FindSDLWindow(SDL_WindowID id) const noexcept
{
    for (const auto& w: m_windows)
    {
        if (w->GetSDLWindowID() == id) { return w.get(); }
    }
    return nullptr;
}

void SDLWindowingSystem::EnumerateMonitors()
{
    m_monitors.Clear();

    Int32 displayCount = 0;
    const SDL_DisplayID* displays = SDL_GetDisplays(&displayCount);

    if (!displays || displayCount <= 0) { return; }

    m_monitors.Reserve(static_cast<UInt32>(displayCount));
    for (Int32 i = 0; i < displayCount; ++i) { m_monitors.EmplaceBack(std::make_unique<SDLMonitor>(displays[i])); }

    RebuildPointerCaches();
}

void SDLWindowingSystem::UpsertMonitor(SDL_DisplayID displayID)
{
    for (const auto& m: m_monitors)
    {
        if (m->GetDisplayID() == displayID)
        {
            m->Refresh();
            return;
        }
    }
    m_monitors.EmplaceBack(std::make_unique<SDLMonitor>(displayID));
    RebuildPointerCaches();
}

void SDLWindowingSystem::RemoveMonitor(SDL_DisplayID displayID)
{
    const auto it =
        m_monitors.FindIf([displayID](const std::unique_ptr<SDLMonitor>& m) { return m->GetDisplayID() == displayID; });

    if (it != m_monitors.End())
    {
        m_monitors.EraseSwapBack(it);
        RebuildPointerCaches();
    }
}

void SDLWindowingSystem::RebuildPointerCaches()
{
    m_windowPtrs.Clear();
    m_windowPtrs.Reserve(static_cast<UInt32>(m_windows.Size()));
    for (const auto& w: m_windows) { m_windowPtrs.PushBack(w.get()); }

    m_monitorPtrs.Clear();
    m_monitorPtrs.Reserve(static_cast<UInt32>(m_monitors.Size()));
    for (const auto& m: m_monitors) { m_monitorPtrs.PushBack(m.get()); }
}

}   // namespace GP::HAL
