// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixPaths.hpp"
#include <cstdlib>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

namespace gp::platform::unix
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

PathBuffer Paths::workingDir() noexcept
{
    PathBuffer result{};
    char cwdBuf[kMaxPathLength + 1u]{};
    if (::getcwd(cwdBuf, sizeof(cwdBuf)) != nullptr && cwdBuf[0] != '\0')
    {
        result.assign(cwdBuf);
        makeStandardFilename(result);
        ensureTrailingSeparator(result);
        return result;
    }

    result.assign("./");
    return result;
}

gp::StringView Paths::launchDir() noexcept
{
    static PathBuffer s_launchDirCache{};
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_launchDirCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        s_launchDirCache = workingDir();
        s_cachedGen = currentGen;
    }

    return s_launchDirCache.view();
}

gp::StringView Paths::homeDir() noexcept
{
    static PathBuffer s_homeDirCache{};
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_homeDirCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        const gp::StringView envHome = detail::getEnvView("HOME");
        if (!envHome.isEmpty() && isAbsolute(envHome))
        {
            s_homeDirCache.assign(envHome);
            makeStandardFilename(s_homeDirCache);
            ensureTrailingSeparator(s_homeDirCache);
        }
        else
        {
            struct passwd pwdEntry{};
            struct passwd* pwdResult = nullptr;
            char pwBuf[4096]{};
            if (::getpwuid_r(::geteuid(), &pwdEntry, pwBuf, sizeof(pwBuf), &pwdResult) == 0 && pwdResult != nullptr &&
                pwdResult->pw_dir != nullptr && pwdResult->pw_dir[0] != '\0')
            {
                s_homeDirCache.assign(pwdResult->pw_dir);
                makeStandardFilename(s_homeDirCache);
                ensureTrailingSeparator(s_homeDirCache);
            }
            else
            {
                s_homeDirCache = workingDir();
            }
        }
        s_cachedGen = currentGen;
    }

    return s_homeDirCache.view();
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
        s_appDataDirCache = resolveXdgDirectory(detail::getEnvView("XDG_DATA_HOME"), homeDir(), ".local/share");
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
        s_localAppDataDirCache = resolveXdgDirectory(detail::getEnvView("XDG_CACHE_HOME"), homeDir(), ".cache");
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
        s_userConfigDirCache = resolveXdgDirectory(detail::getEnvView("XDG_CONFIG_HOME"), homeDir(), ".config");
        s_cachedGen = currentGen;
    }

    return s_userConfigDirCache.view();
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

    static PathBuffer s_documentsDirCache{};
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_documentsDirCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        s_documentsDirCache = resolveXdgDirectory(detail::getEnvView("XDG_DOCUMENTS_DIR"), homeDir(), "Documents");
        s_cachedGen = currentGen;
    }

    return s_documentsDirCache.view();
}

gp::StringView Paths::userDir() noexcept
{
    return documentsDir();
}

gp::StringView Paths::tempDir() noexcept
{
    static PathBuffer s_tempDirCache{};
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_tempDirCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        const gp::StringView envTmp = detail::getEnvView("TMPDIR");
        if (!envTmp.isEmpty() && isAbsolute(envTmp))
        {
            s_tempDirCache.assign(envTmp);
            makeStandardFilename(s_tempDirCache);
            ensureTrailingSeparator(s_tempDirCache);
        }
        else
        {
            s_tempDirCache.assign("/var/tmp/");
        }
        s_cachedGen = currentGen;
    }

    return s_tempDirCache.view();
}

}   // namespace gp::platform::unix
