// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/backends/Malloc.hpp"

namespace gp::memory
{

namespace detail
{

/// @brief Pointer to the global memory allocator instance.
extern GP_CORE_API Malloc* g_malloc;

#if !GP_IS_MONOLITHIC
/// @brief Pointer to the local shadow memory allocator instance, used to bypass DLL Import overhead.
extern Malloc* g_localShadowMalloc;
#endif

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
/// @details The module system is responsible for ensuring that the global memory allocator is initialized before this
/// function is called. It is
/// @return A pointer to the memory allocator, bypassing DLL Import overhead.
[[nodiscard]] GP_FORCEINLINE_HINT Malloc* getInlineMalloc()
{
#if !GP_IS_MONOLITHIC
    return detail::g_localShadowMalloc;
#else
    return detail::g_malloc;
#endif
}

}   // namespace gp::memory
