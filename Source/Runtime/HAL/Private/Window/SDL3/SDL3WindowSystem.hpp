// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Events/EventQueue.hpp"
#include "Window/BaseWindowSystem.hpp"

namespace GP
{

/// @brief SDL3 implementation of IWindowSystem.
class FSDL3WindowSystem final : public FBaseWindowSystem
{
private:
    FEventQueue m_eventQueue;   //<! Queue for pending system events.
    // THashMap<SDL_WindowID, FSDL3Window*> m_windowLookup;

public:
    ///
    /// @section Construction & Destruction
    ///

    FSDL3WindowSystem() noexcept;
    ~FSDL3WindowSystem() noexcept override;

public:
    ///
    /// @section Window Management
    ///

    GP_NODISCARD virtual TSharedPtr<IWindow> CreateWindow(const FWindowDesc& desc) noexcept override;

    ///
    /// @section Display Management
    ///

    virtual void RefreshDisplayList() noexcept override;

    ///
    /// @section Event Handling
    ///

    virtual void PumpEvents() noexcept override;
    virtual void DispatchEvents() noexcept override;

    ///
    /// @section Utility
    ///

    GP_NODISCARD virtual FStringView GetBackendName() const noexcept override;

private:
    ///
    /// @section Private Helpers
    ///

    void DispatchSDL3WindowEvent(const SDL_Event& sdlEvent) noexcept;
    void DispatchSDL3DisplayEvent(const SDL_Event& sdlEvent) noexcept;
    void DispatchSDL3InputEvent(const SDL_Event& sdlEvent) noexcept;
    void DispatchSDL3OtherEvent(const SDL_Event& sdlEvent) noexcept;
    void PumpSDL3Events() noexcept;
};

}   // namespace GP
