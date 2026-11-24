// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixSharedLibrary.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/String.hpp"
#include "logging/LogCategory.hpp"
#include "logging/LogMacros.hpp"   // IWYU pragma: keep
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/base/system/SharedLibraryTypes.hpp"
#include "platforms/generic/system/SharedLibrarySearchRegistry.hpp"
#include "platforms/unix/system/UnixSharedLibraryNaming.hpp"
#include "profiling/Profiler.hpp"
#include <dlfcn.h>

namespace gp::platform::unix
{

using PlatformFileSystem = gp::platform::FileSystem;
using PlatformPaths = gp::platform::Paths;

namespace detail
{

/// @brief Returns the process-wide POSIX search-path registry configured for case-sensitive `.so` matching.
/// @return Reference to the singleton `SharedLibrarySearchRegistry`.
[[nodiscard]] static gp::platform::generic::SharedLibrarySearchRegistry& getRegistry() noexcept
{
    static gp::platform::generic::SharedLibrarySearchRegistry s_registry(
        SharedLibrarySearchPolicy{
          .defaultExtension = SharedLibrary::getDefaultExtension(),
          .fileFilter = &SharedLibraryNaming::isSharedObjectFilename,
          .caseSensitive = true,
        }
    );
    return s_registry;
}

/// @brief Checks whether `basePath` or `basePath + ".so"` exists as a regular file on disk.
/// @param[in] basePath Candidate UTF-8 path.
/// @param[out] outResolvedPath Receives the existing path on success.
/// @return True if an existing regular file was found; false otherwise.
[[nodiscard]] static bool
    tryResolveExistingFileOnDisk(const gp::StringView basePath, PathBuffer& outResolvedPath) noexcept
{
    if (PlatformFileSystem::isFile(basePath))
    {
        outResolvedPath.assign(basePath);
        return true;
    }

    PathBuffer withExt{};
    if (SharedLibraryNaming::tryBuildWithDefaultExtension(basePath, withExt) &&
        PlatformFileSystem::isFile(withExt.view()))
    {
        outResolvedPath = withExt;
        return true;
    }

    return false;
}

}   // namespace detail

void* SharedLibrary::openNativeLibrary(const gp::StringView path) noexcept
{
    if (path.isEmpty() || path.size() > kMaxPathLength)
    {
        return nullptr;
    }

    const PathBuffer nullTerminatedPath(path);
    return ::dlopen(nullTerminatedPath.cString(), RTLD_LAZY | RTLD_LOCAL);
}

void* SharedLibrary::lookupNativeSymbol(void* const handle, const gp::StringView procName) noexcept
{
    if (handle == nullptr || procName.isEmpty())
    {
        return nullptr;
    }

    // Clear stale loader errors before querying dlsym.
    (void)::dlerror();

    if (procName.size() <= kSharedLibraryProcNameStackCapacity)
    {
        const gp::FixedString<kSharedLibraryProcNameStackCapacity> stackName(procName);
        return ::dlsym(handle, stackName.cString());
    }

    const gp::String heapName(procName);
    return ::dlsym(handle, heapName.cString());
}

void SharedLibrary::closeNativeLibrary(void* const handle) noexcept
{
    if (handle != nullptr)
    {
        (void)::dlclose(handle);
    }
}

gp::StringView SharedLibrary::getLastLoaderError() noexcept
{
    const char* const errorMsg = ::dlerror();
    if (errorMsg == nullptr || errorMsg[0] == '\0')
    {
        return "unknown dlopen error";
    }
    return gp::StringView(errorMsg);
}

void SharedLibrary::freeHandle(void* const handle) noexcept
{
    closeNativeLibrary(handle);
}

void* SharedLibrary::getExport(void* const handle, const gp::StringView procName) noexcept
{
    return lookupNativeSymbol(handle, procName);
}

void* SharedLibrary::loadLibraryFromSearchPaths(const gp::StringView filename) noexcept
{
    GP_PROFILE_SCOPE_NC("UnixSharedLibrary::loadLibraryFromSearchPaths", GP_U32_COLOR_ORANGE);

    if (filename.isEmpty() || filename.size() > kMaxPathLength)
    {
        return nullptr;
    }

    // Case 1: Explicit path containing directory separators (absolute or relative).
    if (!PlatformPaths::isPathLeaf(filename))
    {
        if (PlatformPaths::isAbsolute(filename))
        {
            const PathBuffer standardized = PlatformPaths::createStandardFilename(filename);
            void* handle = openNativeLibrary(standardized.view());
            if (handle != nullptr)
            {
                return handle;
            }

            PathBuffer withExt{};
            if (SharedLibraryNaming::tryBuildWithDefaultExtension(standardized.view(), withExt))
            {
                return openNativeLibrary(withExt.view());
            }
            return nullptr;
        }

        // Relative path: probe working directory, engine base directory, executable directory, then raw dlopen.
        PathBuffer resolvedDiskPath{};
        const PathBuffer cwdCandidate = PlatformPaths::resolve(filename, PlatformPaths::workingDir().view());
        if (detail::tryResolveExistingFileOnDisk(cwdCandidate.view(), resolvedDiskPath))
        {
            void* const handle = openNativeLibrary(resolvedDiskPath.view());
            if (handle != nullptr)
            {
                return handle;
            }
        }

        const PathBuffer baseCandidate = PlatformPaths::resolve(filename);
        if (detail::tryResolveExistingFileOnDisk(baseCandidate.view(), resolvedDiskPath))
        {
            void* const handle = openNativeLibrary(resolvedDiskPath.view());
            if (handle != nullptr)
            {
                return handle;
            }
        }

        const PathBuffer exeCandidate = PlatformPaths::resolve(filename, PlatformPaths::executableDir());
        if (detail::tryResolveExistingFileOnDisk(exeCandidate.view(), resolvedDiskPath))
        {
            void* const handle = openNativeLibrary(resolvedDiskPath.view());
            if (handle != nullptr)
            {
                return handle;
            }
        }

        return openNativeLibrary(filename);
    }

    // Case 2: Bare library name (for example "libvulkan.so.1" or "vulkan").
    gp::InlineVector<PathBuffer, 4u> candidateNames{};
    SharedLibraryNaming::buildCandidateFilenames(filename, candidateNames);
    if (candidateNames.isEmpty())
    {
        return nullptr;
    }

    gp::platform::generic::SharedLibrarySearchRegistry& registry = detail::getRegistry();
    SharedLibraryCandidateBatch batch{};
    registry.collectCandidates(candidateNames.data(), candidateNames.size(), batch);

    // Priority 1 & 2: LIFO stack directories first, followed by pre-indexed SoA cache hits.
    for (gp::USize i = 0u; i < batch.prioritizedCandidates.size(); ++i)
    {
        const gp::StringView candidatePath = batch.prioritizedCandidates[i].view();
        if (PlatformFileSystem::isFile(candidatePath))
        {
            void* const handle = openNativeLibrary(candidatePath);
            if (handle != nullptr)
            {
                return handle;
            }
        }
    }

    // Priority 3: Cache-miss probe across registered directories for libraries added after addDirectory().
    for (gp::USize dirIdx = 0u; dirIdx < batch.fallbackDirs.size(); ++dirIdx)
    {
        const gp::StringView dirView = batch.fallbackDirs[dirIdx].directory.view();
        for (gp::USize nameIdx = 0u; nameIdx < candidateNames.size(); ++nameIdx)
        {
            const PathBuffer probePath = PlatformPaths::combine(dirView, candidateNames[nameIdx].view());
            if (PlatformFileSystem::isFile(probePath.view()))
            {
                void* const handle = openNativeLibrary(probePath.view());
                if (handle != nullptr)
                {
                    registry.registerDiscoveredFile(
                        batch.fallbackDirs[dirIdx].directoryIndex, dirView, candidateNames[nameIdx].view()
                    );
                    return handle;
                }
            }
        }
    }

    // Priority 4: Engine executable and base directories (POSIX dlopen does not search the binary directory by
    // default).
    const gp::StringView exeDir = PlatformPaths::executableDir();
    if (!exeDir.isEmpty())
    {
        for (gp::USize nameIdx = 0u; nameIdx < candidateNames.size(); ++nameIdx)
        {
            const PathBuffer probePath = PlatformPaths::combine(exeDir, candidateNames[nameIdx].view());
            if (PlatformFileSystem::isFile(probePath.view()))
            {
                void* const handle = openNativeLibrary(probePath.view());
                if (handle != nullptr)
                {
                    return handle;
                }
            }
        }
    }

    const gp::StringView baseDir = PlatformPaths::baseDir();
    if (!baseDir.isEmpty() && !PlatformPaths::equals(baseDir, exeDir, true))
    {
        for (gp::USize nameIdx = 0u; nameIdx < candidateNames.size(); ++nameIdx)
        {
            const PathBuffer probePath = PlatformPaths::combine(baseDir, candidateNames[nameIdx].view());
            if (PlatformFileSystem::isFile(probePath.view()))
            {
                void* const handle = openNativeLibrary(probePath.view());
                if (handle != nullptr)
                {
                    return handle;
                }
            }
        }
    }

    // Priority 5: System dynamic linker fallback (RPATH/RUNPATH, LD_LIBRARY_PATH, /etc/ld.so.cache, /usr/lib).
    for (gp::USize nameIdx = 0u; nameIdx < candidateNames.size(); ++nameIdx)
    {
        void* const handle = openNativeLibrary(candidateNames[nameIdx].view());
        if (handle != nullptr)
        {
            return handle;
        }
    }

    return nullptr;
}

void* SharedLibrary::getHandle(const gp::StringView filename) noexcept
{
    GP_PROFILE_SCOPE_NC("UnixSharedLibrary::getHandle", GP_U32_COLOR_ORANGE);

    if (filename.isEmpty())
    {
        return nullptr;
    }

    void* const handle = loadLibraryFromSearchPaths(filename);
    if (handle == nullptr)
    {
        GP_LOG_DEBUG(
            gp::log::LogCore,
            "UnixSharedLibrary::getHandle('{}') failed to load library: {}",
            filename,
            getLastLoaderError()
        );
    }

    return handle;
}

void SharedLibrary::addDirectory(const gp::StringView directory) noexcept
{
    (void)detail::getRegistry().addDirectory(directory);
}

void SharedLibrary::pushDirectory(const gp::StringView directory) noexcept
{
    (void)detail::getRegistry().pushDirectory(directory);
}

void SharedLibrary::popDirectory(const gp::StringView directory) noexcept
{
    (void)detail::getRegistry().popDirectory(directory);
}

gp::USize SharedLibrary::getDirectoryStackDepth() noexcept
{
    return detail::getRegistry().getDirectoryStackDepth();
}

PathBuffer SharedLibrary::getActiveDirectory() noexcept
{
    return detail::getRegistry().getActiveDirectory();
}

gp::USize SharedLibrary::getRegisteredDirectoryCount() noexcept
{
    return detail::getRegistry().getRegisteredDirectoryCount();
}

gp::USize SharedLibrary::getCachedLibraryCount() noexcept
{
    return detail::getRegistry().getCachedLibraryCount();
}

void SharedLibrary::resetSearchPaths() noexcept
{
    detail::getRegistry().reset();
}

}   // namespace gp::platform::unix
