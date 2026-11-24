// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "platforms/generic/system/GenericSharedLibrary.hpp"

namespace gp::platform::unix
{

/// @brief POSIX implementation of dynamic shared library (`.so`) loading, symbol lookup, and search-path management.
/// @details Uses `dlopen(RTLD_LAZY | RTLD_LOCAL)`, `dlsym()`, and `dlclose()` backed by a thread-safe
///          `SharedLibrarySearchRegistry`. Search-path snapshots are taken under the registry lock and released before
///          any filesystem probing or `dlopen()` call so library constructors cannot deadlock against the engine mutex.
struct GP_CORE_API SharedLibrary : public gp::platform::generic::SharedLibrary
{
public:
    SharedLibrary() = delete;
    ~SharedLibrary() = delete;
    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;
    SharedLibrary(SharedLibrary&&) = delete;
    SharedLibrary& operator=(SharedLibrary&&) = delete;

public:
    /// @copydoc gp::platform::generic::SharedLibrary::getHandle()
    /// @note Loads with `RTLD_LAZY | RTLD_LOCAL` to keep plugin symbols out of the global process namespace.
    ///       Thread-safe; never holds internal locks across `dlopen()` calls.
    [[nodiscard]] static void* getHandle(const gp::StringView filename) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::getExport()
    /// @note Null-terminates `procName` on the stack (with heap fallback for long names) before calling `dlsym()`.
    [[nodiscard]] static void* getExport(void* const handle, const gp::StringView procName) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::freeHandle()
    /// @note Calls `dlclose()`. Passing `nullptr` is a safe no-op.
    static void freeHandle(void* const handle) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::addDirectory()
    /// @note Deduplicates `directory` case-sensitively and indexes immediate `.so` and versioned `.so.<N>` files
    ///       outside the registry mutex.
    static void addDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::pushDirectory()
    /// @note Pushes the normalized directory onto the process-wide LIFO search-path stack.
    static void pushDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::popDirectory()
    /// @note Verifies the stack is non-empty and matches `directory` case-sensitively before popping.
    static void popDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::getDefaultExtension()
    [[nodiscard]] static constexpr gp::StringView getDefaultExtension() noexcept
    {
        return ".so";
    }

    /// @copydoc gp::platform::generic::SharedLibrary::getDirectoryStackDepth()
    [[nodiscard]] static gp::USize getDirectoryStackDepth() noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::getActiveDirectory()
    [[nodiscard]] static PathBuffer getActiveDirectory() noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::getRegisteredDirectoryCount()
    [[nodiscard]] static gp::USize getRegisteredDirectoryCount() noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::getCachedLibraryCount()
    [[nodiscard]] static gp::USize getCachedLibraryCount() noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::resetSearchPaths()
    static void resetSearchPaths() noexcept;

protected:
    /// @brief Calls POSIX `dlopen(path, RTLD_LAZY | RTLD_LOCAL)` on a stack-allocated null-terminated path.
    /// @note Must never be called while holding a `SharedLibrarySearchRegistry` mutex.
    /// @param[in] path UTF-8 library path or soname.
    /// @return Opaque `dlopen` handle on success, or `nullptr` on failure.
    [[nodiscard]] static void* openNativeLibrary(const gp::StringView path) noexcept;

    /// @brief Calls POSIX `dlsym(handle, procName)` using a null-terminated stack or heap buffer.
    /// @param[in] handle Opaque `dlopen` handle.
    /// @param[in] procName Exported symbol name.
    /// @return Raw symbol address, or `nullptr` if `handle` is null, `procName` is empty, or the symbol is missing.
    [[nodiscard]] static void* lookupNativeSymbol(void* const handle, const gp::StringView procName) noexcept;

    /// @brief Calls POSIX `dlclose(handle)` when `handle` is non-null.
    /// @param[in] handle Opaque `dlopen` handle, or `nullptr`.
    static void closeNativeLibrary(void* const handle) noexcept;

    /// @brief Captures the most recent `dlerror()` message for diagnostic logging on load failure.
    /// @return View of the thread-local error string, or `"unknown dlopen error"` if `dlerror()` returned null.
    [[nodiscard]] static gp::StringView getLastLoaderError() noexcept;

private:
    /// @brief Resolves and loads a shared library across the LIFO stack, SoA cache, runtime dirs, and system linker.
    /// @param[in] filename Shared library filename or path (UTF-8).
    /// @return Opaque `dlopen` handle on success, or `nullptr` if no candidate loaded.
    [[nodiscard]] static void* loadLibraryFromSearchPaths(const gp::StringView filename) noexcept;
};

}   // namespace gp::platform::unix
