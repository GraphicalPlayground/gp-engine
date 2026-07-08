// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/GlobalMemory.hpp"
#include "platforms/base/PlatformMemory.hpp"

namespace gp::memory
{

namespace detail
{

Malloc* g_malloc = nullptr;

}   // namespace detail

Malloc* getGlobalMalloc()
{
    if (GP_LIKELY(detail::g_malloc != nullptr))
    {
        return detail::g_malloc;
    }

    Malloc* newAllocator = gp::platform::Memory::getDefaultAllocator();
    GP_ASSERT(newAllocator != nullptr, "Failed to create a global memory allocator instance.");
    detail::g_malloc = newAllocator;
    return detail::g_malloc;
}

}   // namespace gp::memory
