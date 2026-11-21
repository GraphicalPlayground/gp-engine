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
    if (detail::g_malloc != nullptr) [[likely]]
    {
#if !GP_IS_MONOLITHIC
        if (detail::g_localShadowMalloc == nullptr) [[unlikely]]
        {
            detail::g_localShadowMalloc = detail::g_malloc;
        }
#endif
        return detail::g_malloc;
    }

    Malloc* const newAllocator = gp::platform::Memory::getDefaultAllocator();
    GP_ASSERT(newAllocator != nullptr, "Failed to create a global memory allocator instance.");
    detail::g_malloc = newAllocator;
#if !GP_IS_MONOLITHIC
    detail::g_localShadowMalloc = newAllocator;
#endif
    return detail::g_malloc;
}

}   // namespace gp::memory
