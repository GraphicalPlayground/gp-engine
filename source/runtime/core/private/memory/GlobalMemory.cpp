// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/GlobalMemory.hpp"
#include "platforms/base/PlatformMemory.hpp"

namespace gp::memory
{

Malloc* g_malloc = nullptr;

Malloc* getGlobalMalloc()
{
    if (GP_LIKELY(g_malloc != nullptr))
    {
        return g_malloc;
    }

    Malloc* newAllocator = gp::platform::Memory::getDefaultAllocator();
    GP_ASSERT(newAllocator != nullptr, "Failed to create a global memory allocator instance.");
    g_malloc = newAllocator;
    return g_malloc;
}

}   // namespace gp::memory
