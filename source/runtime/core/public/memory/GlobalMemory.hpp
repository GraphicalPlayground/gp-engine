// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/backends/Malloc.hpp"
#include <atomic>

namespace gp::memory
{

namespace detail
{

/// @brief Pointer to the global memory allocator instance.
extern GP_CORE_API Malloc* g_malloc;

}   // namespace detail

/// @brief Retrieves the global memory allocator instance.
/// @details This function is garanted to be thread-safe from being called in the bootstrapping phase of the engine, and
/// is safe to call from any thread after the engine has been initialized.
/// @return A pointer to the global memory allocator instance.
[[nodiscard]] GP_CORE_API Malloc* getGlobalMalloc();

/// @brief Gets the memory allocator while completely bypassing DLL Import (IAT) overhead.
/// @note This function is intended for use in performance-critical code where the overhead of DLL Import Table (IAT)
/// lookups can be avoided. It provides a direct access to the memory allocator without the usual DLL import overhead,
/// which can be beneficial in scenarios where memory allocation is a frequent operation and performance is paramount.
/// @return A pointer to the memory allocator, bypassing DLL Import overhead.
[[nodiscard]] GP_FORCEINLINE_HINT Malloc* getInlineMalloc()
{
#ifdef GP_MONOLITHIC_BUILD
    return detail::g_malloc;
#else
    // TODO: Maybe consider using the magic statics feature of C++11 to avoid the overhead of the atomic load/store.
    static std::atomic<Malloc*> localShadowMalloc{ nullptr };

    Malloc* cached = localShadowMalloc.load(std::memory_order_relaxed);

    if (cached == nullptr) [[unlikely]]
    {
        cached = getGlobalMalloc();
        localShadowMalloc.store(cached, std::memory_order_relaxed);
    }

    return cached;
#endif
}

}   // namespace gp::memory
