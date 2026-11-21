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
inline Malloc* g_localShadowMalloc = nullptr;
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
/// function is called.
/// @return A pointer to the memory allocator, bypassing DLL Import overhead.
[[nodiscard]] GP_FORCEINLINE_HINT Malloc* getInlineMalloc() noexcept
{
#if !GP_IS_MONOLITHIC
    return detail::g_localShadowMalloc;
#else
    return detail::g_malloc;
#endif
}

/// @brief Resolves the active global `Malloc` backend, preferring the zero-IAT inline shadow pointer when populated.
/// @details Checks the image-local shadow pointer (`getInlineMalloc()`) first with a likely branch prediction hint,
///          falling back to the cross-DLL `getGlobalMalloc()` initializer only if the local shadow pointer has not yet
///          been bound (for example, during pre-main static initialization).
/// @return Non-null pointer to the active `Malloc` instance.
[[nodiscard]] GP_FORCEINLINE_HINT Malloc* getActiveMalloc() noexcept
{
    Malloc* const inlineMalloc = getInlineMalloc();
    if (inlineMalloc != nullptr) [[likely]]
    {
        return inlineMalloc;
    }

    return getGlobalMalloc();
}

/// @brief Synchronizes the calling module's image-local shadow allocator pointer (`detail::g_localShadowMalloc`).
/// @details Because `setupLocalShadowMalloc` is force-inlined into the calling translation unit, invoking it inside a
///          dynamic shared library's entry point (`gpInitializeModule`) writes directly to that specific
///          `.dll`/`.so`/`.dylib` binary's private `.bss` `detail::g_localShadowMalloc` instance ([ADR-0061]).
///          Subsequent calls to `getInlineMalloc()`, `getActiveMalloc()`, `HeapAllocator`, `AlignedHeapAllocator`, and
///          per-module `operator new`/`delete` inside that shared library bypass the OS Import Address Table (IAT) /
///          PLT-GOT completely.
/// @param[in] hostMalloc Optional pointer to the host process's canonical `Malloc` instance. When `nullptr`, resolves
///                       the canonical instance via `getGlobalMalloc()`.
/// @return The synchronized non-null `Malloc*` pointer now bound for the calling module image.
GP_FORCEINLINE_HINT Malloc* setupLocalShadowMalloc(Malloc* const hostMalloc = nullptr) noexcept
{
    Malloc* const resolved = (hostMalloc != nullptr) ? hostMalloc : getGlobalMalloc();
#if !GP_IS_MONOLITHIC
    detail::g_localShadowMalloc = resolved;
#endif
    return resolved;
}

/// @brief Clears the calling module's image-local shadow allocator pointer prior to dynamic module unload.
/// @note In monolithic builds (`GP_IS_MONOLITHIC == 1`), this function is a safe no-op.
GP_FORCEINLINE_HINT void resetLocalShadowMalloc() noexcept
{
#if !GP_IS_MONOLITHIC
    detail::g_localShadowMalloc = nullptr;
#endif
}

/// @brief Queries whether the calling module's image-local shadow allocator pointer is currently bound.
/// @return True if `getInlineMalloc() != nullptr`, false otherwise.
[[nodiscard]] GP_FORCEINLINE_HINT bool isLocalShadowMallocInitialized() noexcept
{
    return getInlineMalloc() != nullptr;
}

}   // namespace gp::memory
