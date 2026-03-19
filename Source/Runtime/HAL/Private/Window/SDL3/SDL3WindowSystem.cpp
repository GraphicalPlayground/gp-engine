// Copyright (c) - Graphical Playground. All rights reserved.

#include "Window/SDL3/SDL3WindowSystem.hpp"
#include "Display/SDL3/SDL3Display.hpp"
#include "Window/SDL3/SDL3Window.hpp"
#include <algorithm>
#include <SDL3/SDL.h>

namespace GP
{

FSDL3WindowSystem::FSDL3WindowSystem() noexcept
{
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0 && SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        // TODO: Handle initialization failure (e.g. log the error, throw an exception, etc.).
    }

    RefreshDisplayList();
}

FSDL3WindowSystem::~FSDL3WindowSystem() noexcept
{
    // Destroy all windows before quitting SDL.
    m_windows.Clear();
    m_displays.Clear();

    // TODO: Maybe find another way to init/destroy SDL3 system
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();
}

GP_NODISCARD TSharedPtr<IWindow> FSDL3WindowSystem::CreateWindow(const FWindowDesc& desc) noexcept
{
    return m_windows.EmplaceBack(GP::MakeShared<FSDL3Window>(desc));
}

void FSDL3WindowSystem::RefreshDisplayList() noexcept
{
    int sdlCount = 0;
    SDL_DisplayID* sdlIds = SDL_GetDisplays(&sdlCount);

    if (!sdlIds || sdlCount <= 0)
    {
        if (sdlIds) { SDL_free(sdlIds); }
        return;
    }

    const SizeT incomingCount = static_cast<SizeT>(sdlCount);
    TArray<bool> existingMatched(m_displays.Size(), false);

    // For each incoming SDL ID: if we already have a matching FSDL3Display, refresh
    // it and mark it as matched. Track which incoming IDs are genuinely new.
    TArray<FSDL3Display::FDisplayId> newIds;
    newIds.Reserve(incomingCount);

    for (SizeT i = 0; i < incomingCount; ++i)
    {
        const FSDL3Display::FDisplayId incomingId = sdlIds[i];

        bool found = false;
        for (SizeT j = 0; j < m_displays.Size(); ++j)
        {
            FSDL3Display* display = static_cast<FSDL3Display*>(m_displays[j].Get());
            if (display && display->GetSDLDisplayId() == incomingId)
            {
                // Display already tracked — refresh its cached state in-place.
                display->Refresh();
                existingMatched[j] = true;
                found = true;
                break;
            }
        }

        if (!found) { newIds.PushBack(incomingId); }
    }

    SDL_free(sdlIds);
    sdlIds = nullptr;

    for (SSizeT j = static_cast<SSizeT>(m_displays.Size()) - 1; j >= 0; --j)
    {
        if (!existingMatched[static_cast<SizeT>(j)]) { m_displays.RemoveAt(static_cast<SizeT>(j)); }
    }

    m_displays.Reserve(m_displays.Size() + newIds.Size());

    for (const FSDL3Display::FDisplayId id: newIds) { m_displays.PushBack(MakeShared<FSDL3Display>(id)); }

    const SDL_DisplayID primarySDLId = SDL_GetPrimaryDisplay();

    TSharedPtr<IDisplay>* first = m_displays.Data();
    TSharedPtr<IDisplay>* last = first + m_displays.Size();

    std::sort(
        first,
        last,
        [primarySDLId](const TSharedPtr<IDisplay>& a, const TSharedPtr<IDisplay>& b) noexcept
        {
            FSDL3Display* displayA = static_cast<FSDL3Display*>(a.Get());
            FSDL3Display* displayB = static_cast<FSDL3Display*>(b.Get());

            const bool aIsPrimary = (displayA->GetSDLDisplayId() == primarySDLId);
            const bool bIsPrimary = (displayB->GetSDLDisplayId() == primarySDLId);

            if (aIsPrimary != bIsPrimary)
            {
                return aIsPrimary;   // primary sorts before everything else
            }

            return displayA->GetSDLDisplayId() < displayB->GetSDLDisplayId();
        }
    );
}

GP_NODISCARD FStringView FSDL3WindowSystem::GetBackendName() const noexcept { return "SDL3"; }

void FSDL3WindowSystem::PumpEvents() noexcept { PumpSDL3Events(); }

void FSDL3WindowSystem::DispatchEvents() noexcept
{
    // System-level first: display topology changes must be stable before
    // any window interprets its own resize/DPI events.
    m_eventQueue.Swap();
    m_eventQueue.Dispatch();

    // Per-window second.
    for (const auto& window: m_windows)
    {
        if (window && window->IsValid()) { window->DispatchEvents(); }
    }
}

void FSDL3WindowSystem::DispatchSDL3WindowEvent(const SDL_Event& sdlEvent) noexcept
{
    GP_UNUSED(sdlEvent);
    // TODO: Implement dispatching of SDL window events to the appropriate FSDL3Window instances.
}

void FSDL3WindowSystem::DispatchSDL3DisplayEvent(const SDL_Event& sdlEvent) noexcept
{
    GP_UNUSED(sdlEvent);
    // TODO: Implement dispatching of SDL display events to the appropriate FSDL3Display instances.
}

void FSDL3WindowSystem::DispatchSDL3InputEvent(const SDL_Event& sdlEvent) noexcept
{
    GP_UNUSED(sdlEvent);
    // TODO: Implement dispatching of SDL input events (keyboard, mouse, etc.) to the appropriate FSDL3Window instances
    // based on event data.
}

void FSDL3WindowSystem::DispatchSDL3OtherEvent(const SDL_Event& sdlEvent) noexcept
{
    GP_UNUSED(sdlEvent);
    // TODO: Implement handling of non-window/display/input SDL events as needed (e.g. quit events, user events, etc.).
}

void FSDL3WindowSystem::PumpSDL3Events() noexcept
{
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent))
    {
        const SDL_EventType type = static_cast<SDL_EventType>(sdlEvent.type);

        if (type >= SDL_EVENT_WINDOW_FIRST && type <= SDL_EVENT_WINDOW_LAST) { DispatchSDL3WindowEvent(sdlEvent); }
        else if (type >= SDL_EVENT_DISPLAY_FIRST && type <= SDL_EVENT_DISPLAY_LAST)
        {
            DispatchSDL3DisplayEvent(sdlEvent);
        }
        else if ((type >= SDL_EVENT_KEY_DOWN && type <= SDL_EVENT_TEXT_EDITING_CANDIDATES) ||
                 (type >= SDL_EVENT_MOUSE_MOTION && type <= SDL_EVENT_MOUSE_REMOVED) ||
                 (type >= SDL_EVENT_JOYSTICK_AXIS_MOTION && type <= SDL_EVENT_JOYSTICK_REMOVED) ||
                 (type >= SDL_EVENT_GAMEPAD_AXIS_MOTION && type <= SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED) ||
                 (type >= SDL_EVENT_FINGER_DOWN && type <= SDL_EVENT_FINGER_CANCELED) ||
                 (type >= SDL_EVENT_PEN_PROXIMITY_IN && type <= SDL_EVENT_PEN_BUTTON_UP))
        {
            DispatchSDL3InputEvent(sdlEvent);
        }
        else { DispatchSDL3OtherEvent(sdlEvent); }
    }
}

}   // namespace GP
