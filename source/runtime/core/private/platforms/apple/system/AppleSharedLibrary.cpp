// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/apple/system/AppleSharedLibrary.hpp"
#include "containers/arrays/Vector.hpp"
#include "logging/LogCategory.hpp"
#include "logging/LogMacros.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/AppleSharedLibraryNaming.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/base/system/SharedLibraryTypes.hpp"
#include "platforms/generic/system/SharedLibrarySearchRegistry.hpp"
#include "profiling/Profiler.hpp"

namespace gp::platform::apple
{

using PlatformFileSystem = gp::platform::FileSystem;
using PlatformPaths = gp::platform::Paths;

namespace detail
{

/// @brief Returns the process-wide Apple search-path registry configured for case-insensitive `.dylib` matching.
/// @return Reference to the singleton `SharedLibrarySearchRegistry`.
[[nodiscard]] static gp::platform::generic::SharedLibrarySearchRegistry& getRegistry() noexcept
{
    static gp::platform::generic::SharedLibrarySearchRegistry s_registry(
        SharedLibrarySearchPolicy{
          .defaultExtension = SharedLibrary::getDefaultExtension(),
          .fileFilter = &SharedLibraryNaming::isDynamicLibraryFilename,
          .caseSensitive = false,
        }
    );
    return s_registry;
}

/// @brief Probes a candidate file path (or `.framework` bundle / `.dylib` variant) on disk.
/// @param[in] basePath Candidate UTF-8 path.
/// @param[out] outResolvedPath Receives the existing regular file path on success.
/// @return True if an existing regular file was found; false otherwise.
[[nodiscard]] static bool
    tryResolveExistingFileOnDisk(const gp::StringView basePath, PathBuffer& outResolvedPath) noexcept
{
    if (SharedLibraryNaming::isFrameworkPath(basePath))
    {
        const PathBuffer frameworkBinary = SharedLibraryNaming::resolveFrameworkBinaryPath(basePath);
        if (!frameworkBinary.isEmpty() && PlatformFileSystem::isFile(frameworkBinary.view()))
        {
            outResolvedPath = frameworkBinary;
            return true;
        }
    }

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

/// @brief Attempts to load any candidate filename located inside `directory` via disk probe followed by `dlopen()`.
/// @param[in] directory Base directory path.
/// @param[in] candidateNames Ordered candidate filenames.
/// @return Opaque `dlopen` handle on success, or `nullptr` if none loaded.
[[nodiscard]] static void* tryLoadFromDirectory(
    const gp::StringView directory,
    const gp::InlineVector<PathBuffer, 6u>& candidateNames,
    void* (*const openFn)(gp::StringView) noexcept
) noexcept
{
    if (directory.isEmpty())
    {
        return nullptr;
    }

    for (gp::USize i = 0u; i < candidateNames.size(); ++i)
    {
        const PathBuffer probePath = PlatformPaths::combine(directory, candidateNames[i].view());
        if (PlatformFileSystem::isFile(probePath.view()))
        {
            void* const handle = openFn(probePath.view());
            if (handle != nullptr)
            {
                return handle;
            }
        }
    }

    return nullptr;
}

}   // namespace detail

void* SharedLibrary::loadLibraryFromSearchPaths(const gp::StringView filename) noexcept
{
    GP_PROFILE_SCOPE_NC("AppleSharedLibrary::loadLibraryFromSearchPaths", GP_U32_COLOR_ORANGE);

    if (filename.isEmpty() || filename.size() > kMaxPathLength)
    {
        return nullptr;
    }

    const gp::StringView exeDir = PlatformPaths::executableDir();
    const PathBuffer bundleFrameworksDir = SharedLibraryNaming::resolveBundleFrameworksDir(exeDir);
    const PathBuffer bundlePlugInsDir = SharedLibraryNaming::resolveBundlePlugInsDir(exeDir);

    // Case 1: Explicit path containing directory separators (absolute or relative).
    if (!PlatformPaths::isPathLeaf(filename))
    {
        if (PlatformPaths::isAbsolute(filename))
        {
            const PathBuffer standardized = PlatformPaths::createStandardFilename(filename);

            // Framework bundle path (for example "/System/Library/Frameworks/CoreFoundation.framework").
            // Pass directly to dlopen without FileSystem::isFile() so macOS 11+ dyld shared cache frameworks load.
            if (SharedLibraryNaming::isFrameworkPath(standardized.view()))
            {
                const PathBuffer innerBinary = SharedLibraryNaming::resolveFrameworkBinaryPath(standardized.view());
                if (!innerBinary.isEmpty())
                {
                    void* const frameworkHandle = openNativeLibrary(innerBinary.view());
                    if (frameworkHandle != nullptr)
                    {
                        return frameworkHandle;
                    }
                }
            }

            // Direct dlopen first so dyld shared cache libraries (such as /usr/lib/libSystem.B.dylib) succeed even
            // when absent from the filesystem.
            void* const handle = openNativeLibrary(standardized.view());
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

        // Relative path: probe working dir, base dir, executable dir, .app bundle dirs, then raw dyld lookup.
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

        if (!exeDir.isEmpty())
        {
            const PathBuffer exeCandidate = PlatformPaths::resolve(filename, exeDir);
            if (detail::tryResolveExistingFileOnDisk(exeCandidate.view(), resolvedDiskPath))
            {
                void* const handle = openNativeLibrary(resolvedDiskPath.view());
                if (handle != nullptr)
                {
                    return handle;
                }
            }
        }

        if (!bundleFrameworksDir.isEmpty())
        {
            const PathBuffer fwCandidate = PlatformPaths::resolve(filename, bundleFrameworksDir.view());
            if (detail::tryResolveExistingFileOnDisk(fwCandidate.view(), resolvedDiskPath))
            {
                void* const handle = openNativeLibrary(resolvedDiskPath.view());
                if (handle != nullptr)
                {
                    return handle;
                }
            }
        }

        if (!bundlePlugInsDir.isEmpty())
        {
            const PathBuffer pluginCandidate = PlatformPaths::resolve(filename, bundlePlugInsDir.view());
            if (detail::tryResolveExistingFileOnDisk(pluginCandidate.view(), resolvedDiskPath))
            {
                void* const handle = openNativeLibrary(resolvedDiskPath.view());
                if (handle != nullptr)
                {
                    return handle;
                }
            }
        }

        return openNativeLibrary(filename);
    }

    // Case 2: Bare library or framework name (for example "libMoltenVK.dylib", "MoltenVK", or "Metal.framework").
    gp::InlineVector<PathBuffer, 6u> candidateNames{};
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
                    const gp::StringView leafName = PlatformPaths::getCleanFilename(probePath.view());
                    if (SharedLibraryNaming::isDynamicLibraryFilename(leafName))
                    {
                        registry.registerDiscoveredFile(batch.fallbackDirs[dirIdx].directoryIndex, dirView, leafName);
                    }
                    return handle;
                }
            }
        }
    }

    // Priority 4: macOS .app bundle Contents/Frameworks/, Contents/PlugIns/, executable dir, and base dir.
    if (void* const handle =
            detail::tryLoadFromDirectory(bundleFrameworksDir.view(), candidateNames, &openNativeLibrary);
        handle != nullptr)
    {
        return handle;
    }

    if (void* const handle = detail::tryLoadFromDirectory(bundlePlugInsDir.view(), candidateNames, &openNativeLibrary);
        handle != nullptr)
    {
        return handle;
    }

    if (void* const handle = detail::tryLoadFromDirectory(exeDir, candidateNames, &openNativeLibrary);
        handle != nullptr)
    {
        return handle;
    }

    const gp::StringView baseDir = PlatformPaths::baseDir();
    if (!baseDir.isEmpty() && !PlatformPaths::equals(baseDir, exeDir, false))
    {
        if (void* const handle = detail::tryLoadFromDirectory(baseDir, candidateNames, &openNativeLibrary);
            handle != nullptr)
        {
            return handle;
        }
    }

    // Priority 5: dyld search (@rpath, @loader_path, DYLD_LIBRARY_PATH, /usr/lib, /System/Library/Frameworks,
    // and the dyld shared cache).
    for (gp::USize nameIdx = 0u; nameIdx < candidateNames.size(); ++nameIdx)
    {
        void* const handle = openNativeLibrary(candidateNames[nameIdx].view());
        if (handle != nullptr)
        {
            return handle;
        }
    }

    // System framework fallback for bare framework names like "CoreFoundation.framework" or "CoreFoundation".
    for (gp::USize nameIdx = 0u; nameIdx < candidateNames.size(); ++nameIdx)
    {
        const gp::StringView candidateView = candidateNames[nameIdx].view();
        if (candidateView.find(".framework/") != gp::StringView::npos)
        {
            const PathBuffer systemFrameworkPath = PlatformPaths::combine("/System/Library/Frameworks", candidateView);
            void* const handle = openNativeLibrary(systemFrameworkPath.view());
            if (handle != nullptr)
            {
                return handle;
            }
        }
    }

    return nullptr;
}

void* SharedLibrary::getHandle(const gp::StringView filename) noexcept
{
    GP_PROFILE_SCOPE_NC("AppleSharedLibrary::getHandle", GP_U32_COLOR_ORANGE);

    if (filename.isEmpty())
    {
        return nullptr;
    }

    void* const handle = loadLibraryFromSearchPaths(filename);
    if (handle == nullptr)
    {
        GP_LOG_DEBUG(
            gp::log::LogCore,
            "AppleSharedLibrary::getHandle('{}') failed to load library: {}",
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

}   // namespace gp::platform::apple
