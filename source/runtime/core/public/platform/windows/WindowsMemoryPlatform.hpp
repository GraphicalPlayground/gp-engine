// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platform/generic/GenericMemoryPlatform.hpp"

namespace gp::memory::impl
{

/// @brief Windows-specific implementation of the memory management platform.
class WindowsMemoryPlatform final : public GenericPlatformMemory
{};

}   // namespace gp::memory::impl
