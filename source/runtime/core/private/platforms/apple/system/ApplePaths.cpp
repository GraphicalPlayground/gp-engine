// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/apple/system/ApplePaths.hpp"
#include "threading/atomics/Atomic.hpp"
#include <cstdlib>
#include <limits.h>
#include <mach-o/dyld.h>

namespace gp::platform::apple
{

namespace detail
{

/// @brief Reads an environment variable as a `gp::StringView` (empty if unset).
/// @param[in] varName Null-terminated environment variable name.
/// @return Non-owning `StringView` of the environment variable value, or empty view.
[[nodiscard]] static gp::StringView getEnvView(const char* const varName) noexcept
{
    const char* const raw = std::getenv(varName);
    return (raw != nullptr) ? gp::StringView(raw) : gp::StringView{};
}

}   // namespace detail

gp::StringView Paths::executablePath() noexcept
{
    const gp::StringView overridePath = tryGetExecutablePathOverride();
    if (!overridePath.isEmpty())
    {
        return overridePath;
    }

    static PathBuffer s_exePathCache{};
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        PathBuffer computed{};
        char rawExecBuf[kMaxPathLength + 1u]{};
        gp::UInt32 bufSize = static_cast<gp::UInt32>(kMaxPathLength);
        if (::_NSGetExecutablePath(rawExecBuf, &bufSize) == 0 && rawExecBuf[0] != '\0')
        {
            char realPathBuf[PATH_MAX]{};
            const char* const canonical = ::realpath(rawExecBuf, realPathBuf);
            const gp::StringView chosenView((canonical != nullptr) ? canonical : rawExecBuf);
            const gp::USize clampedLen = (chosenView.size() <= kMaxPathLength) ? chosenView.size() : kMaxPathLength;
            computed.assign(chosenView.substr(0u, clampedLen));
            makeStandardFilename(computed);
        }

        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_exePathCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }

    return s_exePathCache.view();
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
    static PathBuffer s_exeDirCache{};
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const gp::StringView exePath = executablePath();
        const gp::StringView dirPart = getPath(exePath);
        PathBuffer computed{};
        if (!dirPart.isEmpty())
        {
            const gp::USize clampedLen = (dirPart.size() <= kMaxPathLength) ? dirPart.size() : kMaxPathLength;
            computed.assign(dirPart.substr(0u, clampedLen));
            makeStandardFilename(computed);
            ensureTrailingSeparator(computed);
        }
        else
        {
            computed = workingDir();
        }

        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_exeDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }

    return s_exeDirCache.view();
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
    const gp::StringView overrideDir = tryGetBaseDirOverride();
    if (!overrideDir.isEmpty())
    {
        return overrideDir;
    }

    static PathBuffer s_resourcesDirCache{};
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const PathBuffer computed = resolveMacOsBundleResourcesDir(executableDir());
        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_resourcesDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }

    return s_resourcesDirCache.view();
}

gp::StringView Paths::appDataDir() noexcept
{
    const gp::StringView overrideDir = tryGetAppDataDirOverride();
    if (!overrideDir.isEmpty())
    {
        return overrideDir;
    }

    static PathBuffer s_appDataDirCache{};
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const PathBuffer computed =
            resolveXdgDirectory(detail::getEnvView("XDG_DATA_HOME"), homeDir(), "Library/Application Support");
        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_appDataDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }

    return s_appDataDirCache.view();
}

gp::StringView Paths::localAppDataDir() noexcept
{
    static PathBuffer s_localAppDataDirCache{};
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const PathBuffer computed =
            resolveXdgDirectory(detail::getEnvView("XDG_CACHE_HOME"), homeDir(), "Library/Caches");
        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_localAppDataDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }

    return s_localAppDataDirCache.view();
}

gp::StringView Paths::userConfigDir() noexcept
{
    static PathBuffer s_userConfigDirCache{};
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const PathBuffer computed =
            resolveXdgDirectory(detail::getEnvView("XDG_CONFIG_HOME"), homeDir(), "Library/Preferences");
        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_userConfigDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }

    return s_userConfigDirCache.view();
}

gp::StringView Paths::userSettingsDir() noexcept
{
    return userConfigDir();
}

PathBuffer Paths::resolve(const gp::StringView path) noexcept
{
    return resolve(path, baseDir());
}

PathBuffer Paths::convertRelativePathToFull(const gp::StringView inPath) noexcept
{
    return resolve(inPath, baseDir());
}

}   // namespace gp::platform::apple
