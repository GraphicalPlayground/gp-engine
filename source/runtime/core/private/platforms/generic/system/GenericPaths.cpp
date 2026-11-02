// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/system/GenericPaths.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::platform::generic
{

namespace detail
{

/// @brief Monotonic atomic generation counter incremented whenever `resetCachesForTesting()` or a test override runs.
static gp::thread::Atomic<gp::UInt32> s_cacheGeneration(1u);

/// @brief Lightweight atomic spinlock protecting `.bss` path cache writes during lazy initialization.
static gp::thread::Atomic<gp::UInt32> s_cacheSpinLock(0u);

/// @brief Explicit unit-test override buffer for `executablePath()`.
static PathBuffer s_executablePathOverride{};

/// @brief Explicit unit-test override buffer for `baseDir()`.
static PathBuffer s_baseDirOverride{};

/// @brief Explicit unit-test override buffer for `appDataDir()`.
static PathBuffer s_appDataDirOverride{};

/// @brief Explicit unit-test override buffer for `documentsDir()`.
static PathBuffer s_documentsDirOverride{};

/// @brief Normalized `.bss` cache for the `-basedir=` command-line override.
static PathBuffer s_cliBaseDirCache{};
static const char* s_cliBaseDirSourcePtr = nullptr;

/// @brief Normalized `.bss` cache for the `-appdata=` command-line override.
static PathBuffer s_cliAppDataDirCache{};
static const char* s_cliAppDataDirSourcePtr = nullptr;

/// @brief Normalized `.bss` cache for the `-userdir=` command-line override.
static PathBuffer s_cliDocumentsDirCache{};
static const char* s_cliDocumentsDirSourcePtr = nullptr;

/// @brief Normalized `.bss` cache for `generic::Paths::executablePath()`.
static PathBuffer s_genericExecutablePathCache{};
static const char* s_genericExecutablePathSourcePtr = nullptr;

/// @brief Normalized `.bss` cache for `generic::Paths::executableDir()`.
static PathBuffer s_genericExecutableDirCache{};
static gp::thread::Atomic<gp::UInt32> s_genericExecutableDirGen(0u);

/// @brief Normalized `.bss` cache for `generic::Paths::launchDir()`.
static PathBuffer s_genericLaunchDirCache{};
static gp::thread::Atomic<gp::UInt32> s_genericLaunchDirGen(0u);

/// @brief Formats a directory override into `outBuffer` with normalized forward slashes and a trailing `'/'`.
/// @param[in] rawDir Raw directory string view.
/// @param[out] outBuffer Target `PathBuffer` to populate.
static void formatDirectoryOverride(const gp::StringView rawDir, PathBuffer& outBuffer) noexcept
{
    const gp::USize clampedLen = (rawDir.size() <= kMaxPathLength) ? rawDir.size() : kMaxPathLength;
    outBuffer.assign(rawDir.substr(0u, clampedLen));
    Paths::makeStandardFilename(outBuffer);
    Paths::ensureTrailingSeparator(outBuffer);
}

}   // namespace detail

void Paths::acquireCacheLock() noexcept
{
    while (detail::s_cacheSpinLock.exchange(1u, gp::memory::MemoryOrder::Acquire) != 0u)
    {
        while (detail::s_cacheSpinLock.load(gp::memory::MemoryOrder::Relaxed) != 0u)
        {
            gp::platform::Atomics::compilerBarrier();
        }
    }
}

void Paths::releaseCacheLock() noexcept
{
    detail::s_cacheSpinLock.store(0u, gp::memory::MemoryOrder::Release);
}

void Paths::setExecutablePathForTesting(const gp::StringView path) noexcept
{
    const ScopedCacheLock lock;
    if (path.isEmpty())
    {
        detail::s_executablePathOverride.clear();
    }
    else
    {
        const gp::USize clampedLen = (path.size() <= kMaxPathLength) ? path.size() : kMaxPathLength;
        detail::s_executablePathOverride.assign(path.substr(0u, clampedLen));
        makeStandardFilename(detail::s_executablePathOverride);
    }
    detail::s_cacheGeneration.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
}

void Paths::setBaseDirForTesting(const gp::StringView dir) noexcept
{
    const ScopedCacheLock lock;
    if (dir.isEmpty())
    {
        detail::s_baseDirOverride.clear();
    }
    else
    {
        detail::formatDirectoryOverride(dir, detail::s_baseDirOverride);
    }
    detail::s_cacheGeneration.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
}

void Paths::setAppDataDirForTesting(const gp::StringView dir) noexcept
{
    const ScopedCacheLock lock;
    if (dir.isEmpty())
    {
        detail::s_appDataDirOverride.clear();
    }
    else
    {
        detail::formatDirectoryOverride(dir, detail::s_appDataDirOverride);
    }
    detail::s_cacheGeneration.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
}

void Paths::setDocumentsDirForTesting(const gp::StringView dir) noexcept
{
    const ScopedCacheLock lock;
    if (dir.isEmpty())
    {
        detail::s_documentsDirOverride.clear();
    }
    else
    {
        detail::formatDirectoryOverride(dir, detail::s_documentsDirOverride);
    }
    detail::s_cacheGeneration.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
}

void Paths::resetCachesForTesting() noexcept
{
    const ScopedCacheLock lock;
    detail::s_executablePathOverride.clear();
    detail::s_baseDirOverride.clear();
    detail::s_appDataDirOverride.clear();
    detail::s_documentsDirOverride.clear();
    detail::s_cliBaseDirCache.clear();
    detail::s_cliBaseDirSourcePtr = nullptr;
    detail::s_cliAppDataDirCache.clear();
    detail::s_cliAppDataDirSourcePtr = nullptr;
    detail::s_cliDocumentsDirCache.clear();
    detail::s_cliDocumentsDirSourcePtr = nullptr;
    detail::s_genericExecutablePathCache.clear();
    detail::s_genericExecutablePathSourcePtr = nullptr;
    detail::s_genericExecutableDirCache.clear();
    detail::s_genericExecutableDirGen.store(0u, gp::memory::MemoryOrder::Relaxed);
    detail::s_genericLaunchDirCache.clear();
    detail::s_genericLaunchDirGen.store(0u, gp::memory::MemoryOrder::Relaxed);
    detail::s_cacheGeneration.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
}

gp::StringView Paths::tryGetExecutablePathOverride() noexcept
{
    return detail::s_executablePathOverride.view();
}

gp::StringView Paths::tryGetBaseDirOverride() noexcept
{
    if (!detail::s_baseDirOverride.isEmpty())
    {
        return detail::s_baseDirOverride.view();
    }

    if (gp::CommandLine::isInitialized())
    {
        const gp::Optional<gp::StringView> cliVal = gp::CommandLine::tryGetString("-basedir");
        if (cliVal.hasValue() && !cliVal->isEmpty())
        {
            const ScopedCacheLock lock;
            if (detail::s_cliBaseDirSourcePtr != cliVal->data() || detail::s_cliBaseDirCache.isEmpty())
            {
                detail::formatDirectoryOverride(*cliVal, detail::s_cliBaseDirCache);
                detail::s_cliBaseDirSourcePtr = cliVal->data();
            }
            return detail::s_cliBaseDirCache.view();
        }
    }

    return {};
}

gp::StringView Paths::tryGetAppDataDirOverride() noexcept
{
    if (!detail::s_appDataDirOverride.isEmpty())
    {
        return detail::s_appDataDirOverride.view();
    }

    if (gp::CommandLine::isInitialized())
    {
        const gp::Optional<gp::StringView> cliVal = gp::CommandLine::tryGetString("-appdata");
        if (cliVal.hasValue() && !cliVal->isEmpty())
        {
            const ScopedCacheLock lock;
            if (detail::s_cliAppDataDirSourcePtr != cliVal->data() || detail::s_cliAppDataDirCache.isEmpty())
            {
                detail::formatDirectoryOverride(*cliVal, detail::s_cliAppDataDirCache);
                detail::s_cliAppDataDirSourcePtr = cliVal->data();
            }
            return detail::s_cliAppDataDirCache.view();
        }
    }

    return {};
}

gp::StringView Paths::tryGetDocumentsDirOverride() noexcept
{
    if (!detail::s_documentsDirOverride.isEmpty())
    {
        return detail::s_documentsDirOverride.view();
    }

    if (gp::CommandLine::isInitialized())
    {
        const gp::Optional<gp::StringView> cliVal = gp::CommandLine::tryGetString("-userdir");
        if (cliVal.hasValue() && !cliVal->isEmpty())
        {
            const ScopedCacheLock lock;
            if (detail::s_cliDocumentsDirSourcePtr != cliVal->data() || detail::s_cliDocumentsDirCache.isEmpty())
            {
                detail::formatDirectoryOverride(*cliVal, detail::s_cliDocumentsDirCache);
                detail::s_cliDocumentsDirSourcePtr = cliVal->data();
            }
            return detail::s_cliDocumentsDirCache.view();
        }
    }

    return {};
}

gp::UInt32 Paths::getCacheGeneration() noexcept
{
    return detail::s_cacheGeneration.load(gp::memory::MemoryOrder::Acquire);
}

gp::StringView Paths::executablePath() noexcept
{
    const gp::StringView overridePath = tryGetExecutablePathOverride();
    if (!overridePath.isEmpty())
    {
        return overridePath;
    }

    if (gp::CommandLine::isInitialized())
    {
        const gp::StringView cliExe = gp::CommandLine::getExecutablePath();
        if (!cliExe.isEmpty())
        {
            const ScopedCacheLock lock;
            if (detail::s_genericExecutablePathSourcePtr != cliExe.data() ||
                detail::s_genericExecutablePathCache.isEmpty())
            {
                const gp::USize clampedLen = (cliExe.size() <= kMaxPathLength) ? cliExe.size() : kMaxPathLength;
                detail::s_genericExecutablePathCache.assign(cliExe.substr(0u, clampedLen));
                makeStandardFilename(detail::s_genericExecutablePathCache);
                detail::s_genericExecutablePathSourcePtr = cliExe.data();
            }
            return detail::s_genericExecutablePathCache.view();
        }
    }

    return {};
}

gp::StringView Paths::executableName(const bool removeExtension) noexcept
{
    const gp::StringView exePath = executablePath();
    if (exePath.isEmpty())
    {
        return {};
    }

    return removeExtension ? getBaseFilename(exePath, true) : getCleanFilename(exePath);
}

gp::StringView Paths::executableDir() noexcept
{
    const gp::UInt32 currentGen = getCacheGeneration();
    const gp::StringView exePath = executablePath();
    if (!exePath.isEmpty())
    {
        if (detail::s_genericExecutableDirGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
        {
            const gp::StringView dirView = getPath(exePath);
            if (!dirView.isEmpty())
            {
                PathBuffer formatted{};
                detail::formatDirectoryOverride(dirView, formatted);
                const ScopedCacheLock lock;
                detail::s_genericExecutableDirCache = formatted;
                detail::s_genericExecutableDirGen.store(currentGen, gp::memory::MemoryOrder::Release);
            }
        }
        if (!detail::s_genericExecutableDirCache.isEmpty())
        {
            return detail::s_genericExecutableDirCache.view();
        }
    }

    return "./";
}

gp::StringView Paths::baseDir() noexcept
{
    const gp::StringView overrideDir = tryGetBaseDirOverride();
    if (!overrideDir.isEmpty())
    {
        return overrideDir;
    }

    return executableDir();
}

gp::StringView Paths::appResourcesDir() noexcept
{
    return baseDir();
}

PathBuffer Paths::workingDir() noexcept
{
    return PathBuffer("./");
}

gp::StringView Paths::launchDir() noexcept
{
    const gp::UInt32 currentGen = getCacheGeneration();
    if (detail::s_genericLaunchDirGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const PathBuffer cwd = workingDir();
        const ScopedCacheLock lock;
        if (detail::s_genericLaunchDirGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            detail::s_genericLaunchDirCache = cwd;
            detail::s_genericLaunchDirGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }
    return detail::s_genericLaunchDirCache.view();
}

gp::StringView Paths::homeDir() noexcept
{
    return "./";
}

gp::StringView Paths::appDataDir() noexcept
{
    const gp::StringView overrideDir = tryGetAppDataDirOverride();
    if (!overrideDir.isEmpty())
    {
        return overrideDir;
    }

    return "./";
}

gp::StringView Paths::localAppDataDir() noexcept
{
    return "./";
}

gp::StringView Paths::userConfigDir() noexcept
{
    return "./";
}

gp::StringView Paths::userSettingsDir() noexcept
{
    return userConfigDir();
}

gp::StringView Paths::documentsDir() noexcept
{
    const gp::StringView overrideDir = tryGetDocumentsDirOverride();
    if (!overrideDir.isEmpty())
    {
        return overrideDir;
    }

    return "./";
}

gp::StringView Paths::userDir() noexcept
{
    return documentsDir();
}

gp::StringView Paths::tempDir() noexcept
{
    return "/tmp/";
}

PathBuffer Paths::resolve(const gp::StringView path) noexcept
{
    return resolve(path, baseDir());
}

PathBuffer Paths::convertRelativePathToFull(const gp::StringView inPath) noexcept
{
    return resolve(inPath, baseDir());
}

}   // namespace gp::platform::generic
