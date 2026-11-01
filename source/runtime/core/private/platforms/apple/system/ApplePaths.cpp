// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/apple/system/ApplePaths.hpp"
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
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_exePathCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        char rawExecBuf[kMaxPathLength + 1u]{};
        gp::UInt32 bufSize = static_cast<gp::UInt32>(kMaxPathLength);
        if (::_NSGetExecutablePath(rawExecBuf, &bufSize) == 0 && rawExecBuf[0] != '\0')
        {
            char realPathBuf[PATH_MAX]{};
            const char* const canonical = ::realpath(rawExecBuf, realPathBuf);
            s_exePathCache.assign((canonical != nullptr) ? canonical : rawExecBuf);
            makeStandardFilename(s_exePathCache);
        }
        s_cachedGen = currentGen;
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
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    const gp::StringView exePath = executablePath();
    if (s_exeDirCache.isEmpty() || s_cachedGen != currentGen || !tryGetExecutablePathOverride().isEmpty())
    {
        const gp::StringView dirPart = getPath(exePath);
        if (!dirPart.isEmpty())
        {
            s_exeDirCache.assign(dirPart);
            makeStandardFilename(s_exeDirCache);
            ensureTrailingSeparator(s_exeDirCache);
        }
        else
        {
            s_exeDirCache = workingDir();
        }
        s_cachedGen = currentGen;
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

gp::StringView Paths::appDataDir() noexcept
{
    const gp::StringView overrideDir = tryGetAppDataDirOverride();
    if (!overrideDir.isEmpty())
    {
        return overrideDir;
    }

    static PathBuffer s_appDataDirCache{};
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_appDataDirCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        s_appDataDirCache =
            resolveXdgDirectory(detail::getEnvView("XDG_DATA_HOME"), homeDir(), "Library/Application Support");
        s_cachedGen = currentGen;
    }

    return s_appDataDirCache.view();
}

gp::StringView Paths::localAppDataDir() noexcept
{
    static PathBuffer s_localAppDataDirCache{};
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_localAppDataDirCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        s_localAppDataDirCache = resolveXdgDirectory(detail::getEnvView("XDG_CACHE_HOME"), homeDir(), "Library/Caches");
        s_cachedGen = currentGen;
    }

    return s_localAppDataDirCache.view();
}

gp::StringView Paths::userConfigDir() noexcept
{
    static PathBuffer s_userConfigDirCache{};
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_userConfigDirCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        s_userConfigDirCache =
            resolveXdgDirectory(detail::getEnvView("XDG_CONFIG_HOME"), homeDir(), "Library/Preferences");
        s_cachedGen = currentGen;
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
