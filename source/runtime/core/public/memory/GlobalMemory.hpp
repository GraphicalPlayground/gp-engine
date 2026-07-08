// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/backends/Malloc.hpp"

namespace gp::memory
{

/// @brief Pointer to the global memory allocator instance.
extern GP_CORE_API Malloc* g_malloc;

/// @brief Retrieves the global memory allocator instance.
/// @return A pointer to the global memory allocator instance.
GP_NODISCARD GP_CORE_API Malloc* getGlobalMalloc();

}   // namespace gp::memory
