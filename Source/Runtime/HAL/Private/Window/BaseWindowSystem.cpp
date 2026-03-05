// Copyright (c) - Graphical Playground. All rights reserved.

#include "Window/BaseWindowSystem.hpp"

namespace GP
{

TSharedPtr<IWindow> FBaseWindowSystem::CreateWindow(const FWindowDesc& desc) noexcept
{
    GP_UNUSED(desc);
    return nullptr;
}

void FBaseWindowSystem::PollEvents() noexcept {}

void FBaseWindowSystem::DispatchEvents() noexcept {}

}   // namespace GP
