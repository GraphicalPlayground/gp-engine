// Copyright (c) - Graphical Playground. All rights reserved.

#include "Window/SDL3/SDL3WindowSystem.hpp"
#include "Window/SDL3/SDL3Window.hpp"
#include <algorithm>
#include <SDL3/SDL.h>

namespace GP
{

GP_NODISCARD TSharedPtr<IWindow> FSDL3WindowSystem::CreateWindow(const FWindowDesc& desc) noexcept
{
    return m_windows.EmplaceBack(GP::MakeShared<FSDL3Window>(desc));
}

void FSDL3WindowSystem::RefreshDisplayList() noexcept
{
    // TODO: Implement display enumeration and management using SDL3 APIs.
}

GP_NODISCARD FStringView FSDL3WindowSystem::GetBackendName() const noexcept { return "SDL3"; }

void FSDL3WindowSystem::PollEvents() noexcept {}

void FSDL3WindowSystem::DispatchEvents() noexcept {}

}   // namespace GP
