// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"

namespace gp::platform::generic
{

/// @brief Platform-independent fallback contract for dynamic shared library loading and symbol resolution.
/// @details Provides the canonical static interface inherited and specialized by concrete platform implementations
///          (`windows::SharedLibrary`, `unix::SharedLibrary`, `linux::SharedLibrary`, `apple::SharedLibrary`,
///          `macos::SharedLibrary`). On unsupported platforms, all loading and export lookups safely return `nullptr`
///          and search-path operations compile down to no-ops.
/// @see gp::platform::windows::SharedLibrary, gp::platform::linux::SharedLibrary, gp::platform::macos::SharedLibrary
struct SharedLibrary
{
public:
    SharedLibrary() = delete;
    ~SharedLibrary() = delete;
    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;
    SharedLibrary(SharedLibrary&&) = delete;
    SharedLibrary& operator=(SharedLibrary&&) = delete;

public:
    /// @brief Loads a shared library or acquires an additional reference to an already-loaded library.
    /// @details Every non-null handle returned by `getHandle()` carries an incremented OS module reference count and
    ///          must be paired with a corresponding call to `freeHandle()`.
    /// @param[in] filename Shared library filename or path (relative or absolute, UTF-8).
    /// @return Opaque platform module handle on success, or `nullptr` if the library could not be located or loaded.
    [[nodiscard]] static GP_CORE_API void* getHandle(const gp::StringView filename) noexcept;

    /// @brief Resolves the address of an exported symbol from a loaded shared library.
    /// @param[in] handle Opaque module handle previously returned by `getHandle()`.
    /// @param[in] procName Exported symbol name (UTF-8 / ASCII; does not need to be null-terminated).
    /// @return Raw pointer to the exported symbol address, or `nullptr` if `handle` is null or the symbol is absent.
    [[nodiscard]] static GP_CORE_API void* getExport(void* const handle, const gp::StringView procName) noexcept;

    /// @brief Releases a shared library handle previously acquired via `getHandle()`.
    /// @details Decrements the OS loader reference count and unloads the module when its count reaches zero.
    ///          Passing `nullptr` is a safe no-op.
    /// @param[in] handle Opaque module handle to release, or `nullptr`.
    static GP_CORE_API void freeHandle(void* const handle) noexcept;

    /// @brief Registers a persistent directory to be searched when resolving unqualified shared library names.
    /// @param[in] directory Directory path to register (UTF-8).
    GP_FORCEINLINE_HINT static void addDirectory([[maybe_unused]] const gp::StringView directory) noexcept
    {}

    /// @brief Pushes a directory onto the process-wide LIFO search-path stack for scoped dependency resolution.
    /// @param[in] directory Directory path to push onto the stack (UTF-8).
    GP_FORCEINLINE_HINT static void pushDirectory([[maybe_unused]] const gp::StringView directory) noexcept
    {}

    /// @brief Pops the top directory from the LIFO search-path stack and restores the previous active search directory.
    /// @param[in] directory Expected directory path matching the current top of the stack (UTF-8).
    GP_FORCEINLINE_HINT static void popDirectory([[maybe_unused]] const gp::StringView directory) noexcept
    {}

    /// @brief Returns the default platform shared library filename extension (including leading dot).
    /// @return Platform extension (e.g., `".dll"`, `".so"`, `".dylib"`), or empty view on generic fallback.
    [[nodiscard]] static constexpr gp::StringView getDefaultExtension() noexcept
    {
        return {};
    }

    /// @brief Queries the current depth of the active LIFO search-path stack.
    /// @return Number of pushed directories currently on the stack (`0` on generic fallback).
    [[nodiscard]] GP_FORCEINLINE_HINT static gp::USize getDirectoryStackDepth() noexcept
    {
        return 0u;
    }

    /// @brief Queries the normalized directory currently at the top of the LIFO search-path stack.
    /// @return Normalized `PathBuffer` of the active top directory, or an empty buffer if the stack is empty.
    [[nodiscard]] GP_FORCEINLINE_HINT static PathBuffer getActiveDirectory() noexcept
    {
        return {};
    }

    /// @brief Queries the number of persistent search directories registered via `addDirectory()`.
    /// @return Count of registered search directories (`0` on generic fallback).
    [[nodiscard]] GP_FORCEINLINE_HINT static gp::USize getRegisteredDirectoryCount() noexcept
    {
        return 0u;
    }

    /// @brief Queries the total number of shared library filenames currently indexed in the search-path cache.
    /// @return Count of cached library entries (`0` on generic fallback).
    [[nodiscard]] GP_FORCEINLINE_HINT static gp::USize getCachedLibraryCount() noexcept
    {
        return 0u;
    }

    /// @brief Clears all pushed stack entries, registered search directories, and cached library entries.
    /// @note Primarily intended for deterministic teardown between unit tests and engine subsystem shutdown.
    GP_FORCEINLINE_HINT static void resetSearchPaths() noexcept
    {}
};

}   // namespace gp::platform::generic
