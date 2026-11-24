// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "platforms/unix/system/UnixSharedLibrary.hpp"

namespace gp::platform::apple
{

/// @brief Apple (Darwin / macOS) implementation of dynamic shared library (`.dylib` and `.framework`) loading and
///        search-path management.
/// @details Inherits POSIX `dlopen`/`dlsym`/`dlclose` primitives from `gp::platform::unix::SharedLibrary` and adds
///          Apple-specific resolution: case-insensitive `.dylib` search-path indexing, `.framework/<Name>` bundle
///          binary lookup, `.app/Contents/Frameworks/` and `.app/Contents/PlugIns/` bundle probing, and direct
///          `dlopen()` for macOS 11+ dyld shared cache libraries that do not exist as files on disk.
struct GP_CORE_API SharedLibrary : public gp::platform::unix::SharedLibrary
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
    /// @note Resolves `.dylib` libraries, `.framework` bundles, `.app` bundle `Frameworks/` and `PlugIns/` directories,
    ///       and dyld shared cache system libraries. Never holds internal locks across `dlopen()`.
    [[nodiscard]] static void* getHandle(const gp::StringView filename) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::addDirectory()
    /// @note Deduplicates `directory` case-insensitively and indexes immediate `.dylib` files outside the mutex.
    static void addDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::pushDirectory()
    /// @note Pushes the normalized directory onto Apple's process-wide LIFO search-path stack.
    static void pushDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::popDirectory()
    /// @note Verifies the stack is non-empty and matches `directory` case-insensitively before popping.
    static void popDirectory(const gp::StringView directory) noexcept;

    /// @copydoc gp::platform::generic::SharedLibrary::getDefaultExtension()
    [[nodiscard]] static constexpr gp::StringView getDefaultExtension() noexcept
    {
        return ".dylib";
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

private:
    /// @brief Resolves and loads an Apple dynamic library or framework across search paths, bundle dirs, and dyld.
    /// @param[in] filename Shared library filename, framework name, or path (UTF-8).
    /// @return Opaque `dlopen` handle on success, or `nullptr` if no candidate loaded.
    [[nodiscard]] static void* loadLibraryFromSearchPaths(const gp::StringView filename) noexcept;
};

}   // namespace gp::platform::apple
