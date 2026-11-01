// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/system/GenericPaths.hpp"
#include "miscellaneous/CommandLine.hpp"

namespace gp::platform::generic
{

namespace detail
{

/// @brief Monotonic generation counter incremented whenever `resetCachesForTesting()` is invoked.
static gp::UInt32 s_cacheGeneration = 1u;

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

/// @brief Normalized `.bss` cache for the `-appdata=` command-line override.
static PathBuffer s_cliAppDataDirCache{};

/// @brief Normalized `.bss` cache for the `-userdir=` command-line override.
static PathBuffer s_cliDocumentsDirCache{};

/// @brief Normalized `.bss` cache for `generic::Paths::executablePath()`.
static PathBuffer s_genericExecutablePathCache{};

/// @brief Normalized `.bss` cache for `generic::Paths::executableDir()`.
static PathBuffer s_genericExecutableDirCache{};

/// @brief Normalized `.bss` cache for `generic::Paths::launchDir()`.
static PathBuffer s_genericLaunchDirCache{};

/// @brief Formats a directory override into `outBuffer` with normalized forward slashes and a trailing `'/'`.
/// @param[in] rawDir Raw directory string view.
/// @param[out] outBuffer Target `PathBuffer` to populate.
static void formatDirectoryOverride(const gp::StringView rawDir, PathBuffer& outBuffer) noexcept
{
    outBuffer.assign(rawDir);
    Paths::makeStandardFilename(outBuffer);
    Paths::ensureTrailingSeparator(outBuffer);
}

}   // namespace detail

void Paths::setExecutablePathForTesting(const gp::StringView path) noexcept
{
    if (path.isEmpty())
    {
        detail::s_executablePathOverride.clear();
    }
    else
    {
        detail::s_executablePathOverride.assign(path);
        makeStandardFilename(detail::s_executablePathOverride);
    }
    ++detail::s_cacheGeneration;
}

void Paths::setBaseDirForTesting(const gp::StringView dir) noexcept
{
    if (dir.isEmpty())
    {
        detail::s_baseDirOverride.clear();
    }
    else
    {
        detail::formatDirectoryOverride(dir, detail::s_baseDirOverride);
    }
    ++detail::s_cacheGeneration;
}

void Paths::setAppDataDirForTesting(const gp::StringView dir) noexcept
{
    if (dir.isEmpty())
    {
        detail::s_appDataDirOverride.clear();
    }
    else
    {
        detail::formatDirectoryOverride(dir, detail::s_appDataDirOverride);
    }
    ++detail::s_cacheGeneration;
}

void Paths::setDocumentsDirForTesting(const gp::StringView dir) noexcept
{
    if (dir.isEmpty())
    {
        detail::s_documentsDirOverride.clear();
    }
    else
    {
        detail::formatDirectoryOverride(dir, detail::s_documentsDirOverride);
    }
    ++detail::s_cacheGeneration;
}

void Paths::resetCachesForTesting() noexcept
{
    detail::s_executablePathOverride.clear();
    detail::s_baseDirOverride.clear();
    detail::s_appDataDirOverride.clear();
    detail::s_documentsDirOverride.clear();
    detail::s_cliBaseDirCache.clear();
    detail::s_cliAppDataDirCache.clear();
    detail::s_cliDocumentsDirCache.clear();
    detail::s_genericExecutablePathCache.clear();
    detail::s_genericExecutableDirCache.clear();
    detail::s_genericLaunchDirCache.clear();
    ++detail::s_cacheGeneration;
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
            detail::formatDirectoryOverride(*cliVal, detail::s_cliBaseDirCache);
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
            detail::formatDirectoryOverride(*cliVal, detail::s_cliAppDataDirCache);
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
            detail::formatDirectoryOverride(*cliVal, detail::s_cliDocumentsDirCache);
            return detail::s_cliDocumentsDirCache.view();
        }
    }

    return {};
}

gp::UInt32 Paths::getCacheGeneration() noexcept
{
    return detail::s_cacheGeneration;
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
            detail::s_genericExecutablePathCache.assign(cliExe);
            makeStandardFilename(detail::s_genericExecutablePathCache);
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
    const gp::StringView exePath = executablePath();
    if (!exePath.isEmpty())
    {
        const gp::StringView dirView = getPath(exePath);
        if (!dirView.isEmpty())
        {
            detail::formatDirectoryOverride(dirView, detail::s_genericExecutableDirCache);
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

PathBuffer Paths::workingDir() noexcept
{
    return PathBuffer("./");
}

gp::StringView Paths::launchDir() noexcept
{
    static gp::UInt32 s_cachedGen = 0u;
    if (detail::s_genericLaunchDirCache.isEmpty() || s_cachedGen != getCacheGeneration())
    {
        detail::s_genericLaunchDirCache = workingDir();
        s_cachedGen = getCacheGeneration();
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
