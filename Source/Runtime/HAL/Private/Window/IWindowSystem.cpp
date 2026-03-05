// Copyright (c) - Graphical Playground. All rights reserved.

#include "Window/IWindowSystem.hpp"
#include "Window/SDL3/SDL3WindowSystem.hpp"

namespace GP
{

TUniquePtr<IWindowSystem> IWindowSystem::Create() noexcept { return GP::MakeUnique<FSDL3WindowSystem>(); }

}   // namespace GP
