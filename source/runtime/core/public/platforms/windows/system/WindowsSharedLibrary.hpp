// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "platforms/generic/system/GenericSharedLibrary.hpp"

namespace gp::platform::windows
{

/// @brief Win32 implementation of dynamic shared library (`.dll`) loading, symbol export lookup, and search-path
///        management.
/// @details Bridges Win32's single-slot `SetDllDirectoryW()` API into a thread-safe LIFO search-path stack
///          (`searchPathsStack`) paired with a persistent Structure-of-Arrays (SoA) directory & DLL filename cache:
///          - **UTF-8 to UTF-16 Stack Bridging:** Converts all UTF-8 paths to stack-allocated UTF-16 buffers and
///            invokes explicit wide Win32 APIs (`LoadLibraryW`, `LoadLibraryExW`, `GetModuleHandleExW`,
///            `SetDllDirectoryW`, `FreeLibrary`) so non-null-terminated `gp::StringView` slices and Unicode paths work
///            without heap churn.
///          - **Refcount-Safe Module Acquisition:** Uses `GetModuleHandleExW(0, ...)` (which atomically increments the
///            module reference count) and `LoadLibrary*` so every handle returned by `getHandle()` is strictly balanced
///            with `freeHandle()`.
///          - **OS Loader-Lock Deadlock Immunity:** Never holds the internal search-path mutex across `LoadLibrary*`,
///            `FreeLibrary`, or filesystem directory enumeration, preventing AB-BA lock inversions with
///            `LdrpLoaderLock` and `DllMain`.
///          - **Modal Error-Box Suppression:** Scopes `SetThreadErrorMode()` to suppress blocking OS DLL error dialogs
///            by default (honoring `-dllerrors` and `-unattended` command-line flags).
struct GP_CORE_API SharedLibrary final : public gp::platform::generic::SharedLibrary
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
    /// @note Suppresses Win32 missing-DLL error dialog boxes via `SetThreadErrorMode()` unless `-dllerrors` is set on
    ///       the command line. Thread-safe; never holds internal locks across Win32 loader calls.
    [[nodiscard]] static void* getHandle(const gp::StringView filename) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::getExport()
    /// @note Safely null-terminates `procName` on the stack before invoking Win32 `GetProcAddress()`.
    [[nodiscard]] static void* getExport(void* const handle, const gp::StringView procName) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::freeHandle()
    /// @note Invokes Win32 `FreeLibrary()`. Passing `nullptr` is a safe no-op.
    static void freeHandle(void* const handle) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::addDirectory()
    /// @note Normalizes and deduplicates `directory` (case-insensitively), then indexes all immediate `.dll` files into
    ///       the internal SoA search-path cache outside the mutex lock.
    static void addDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::pushDirectory()
    /// @note Pushes the normalized directory onto the LIFO stack and updates Win32 `SetDllDirectoryW()`.
    static void pushDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::popDirectory()
    /// @note Verifies the stack is non-empty and its top matches `directory` (after normalization, case-insensitively),
    ///       pops the entry, and restores the previous directory via `SetDllDirectoryW()` (or `L""` when empty to keep
    ///       the current working directory out of the DLL search order).
    static void popDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::getDefaultExtension()
    [[nodiscard]] static constexpr gp::StringView getDefaultExtension() noexcept
    {
        return ".dll";
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
    /// @note Also resets the Win32 process DLL directory via `SetDllDirectoryW(L"")`.
    static void resetSearchPaths() noexcept;

private:
    /// @brief Resolves and loads a shared library across the active search-path stack, cached search directories, and
    ///        Win32 loader fallbacks.
    /// @param[in] filename Shared library filename or path (UTF-8).
    /// @return Opaque `HMODULE` cast to `void*` on success, or `nullptr` if the library could not be loaded.
    [[nodiscard]] static void* loadLibraryFromSearchPaths(const gp::StringView filename) noexcept;
};

}   // namespace gp::platform::windows
