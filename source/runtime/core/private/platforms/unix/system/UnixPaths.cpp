// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixPaths.hpp"
#include "threading/atomics/Atomic.hpp"
#include <cstdlib>
#include <fcntl.h>
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

/// @brief Attempts to resolve a user directory key from `$XDG_CONFIG_HOME/user-dirs.dirs`.
/// @param[in] configDir Normalized user configuration directory (`$XDG_CONFIG_HOME` / `~/.config/`).
/// @param[in] homeDir Normalized user home directory.
/// @param[in] keyName Target XDG user-dirs key (e.g., `"XDG_DOCUMENTS_DIR"`).
/// @return Normalized directory `PathBuffer` if found and valid, or empty `PathBuffer`.
[[nodiscard]] static PathBuffer tryResolveFromUserDirsConfig(
    const gp::StringView configDir, const gp::StringView homeDir, const gp::StringView keyName
) noexcept
{
    const PathBuffer configFilePath = Paths::combine(configDir, "user-dirs.dirs");
    const int fd = ::open(configFilePath.cString(), O_RDONLY);
    if (fd < 0)
    {
        return {};
    }

    char fileBuf[4096]{};
    const ssize_t bytesRead = ::read(fd, fileBuf, sizeof(fileBuf) - 1u);
    (void)::close(fd);

    if (bytesRead <= 0)
    {
        return {};
    }

    fileBuf[bytesRead] = '\0';
    return Paths::parseXdgUserDirsEntry(gp::StringView(fileBuf, static_cast<gp::USize>(bytesRead)), keyName, homeDir);
}

}   // namespace detail

PathBuffer Paths::workingDir() noexcept
{
    PathBuffer result{};
    char cwdBuf[kMaxPathLength + 1u]{};
    if (::getcwd(cwdBuf, sizeof(cwdBuf)) != nullptr && cwdBuf[0] != '\0')
    {
        const gp::StringView rawView(cwdBuf);
        const gp::USize clampedLen = (rawView.size() <= kMaxPathLength) ? rawView.size() : kMaxPathLength;
        result.assign(rawView.substr(0u, clampedLen));
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
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const PathBuffer computed = workingDir();
        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_launchDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }

    return s_launchDirCache.view();
}

gp::StringView Paths::homeDir() noexcept
{
    static PathBuffer s_homeDirCache{};
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        PathBuffer computed{};
        const gp::StringView envHome = detail::getEnvView("HOME");
        if (!envHome.isEmpty() && isAbsolute(envHome))
        {
            const gp::USize clampedLen = (envHome.size() <= kMaxPathLength) ? envHome.size() : kMaxPathLength;
            computed.assign(envHome.substr(0u, clampedLen));
            makeStandardFilename(computed);
            ensureTrailingSeparator(computed);
        }
        else
        {
            struct passwd pwdEntry{};
            struct passwd* pwdResult = nullptr;
            char pwBuf[4096]{};
            if (::getpwuid_r(::geteuid(), &pwdEntry, pwBuf, sizeof(pwBuf), &pwdResult) == 0 && pwdResult != nullptr &&
                pwdResult->pw_dir != nullptr && pwdResult->pw_dir[0] != '\0')
            {
                const gp::StringView pwView(pwdResult->pw_dir);
                const gp::USize clampedLen = (pwView.size() <= kMaxPathLength) ? pwView.size() : kMaxPathLength;
                computed.assign(pwView.substr(0u, clampedLen));
                makeStandardFilename(computed);
                ensureTrailingSeparator(computed);
            }
            else
            {
                computed = workingDir();
            }
        }

        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_homeDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
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
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const PathBuffer computed = resolveXdgDirectory(detail::getEnvView("XDG_DATA_HOME"), homeDir(), ".local/share");
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
        const PathBuffer computed = resolveXdgDirectory(detail::getEnvView("XDG_CACHE_HOME"), homeDir(), ".cache");
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
        const PathBuffer computed = resolveXdgDirectory(detail::getEnvView("XDG_CONFIG_HOME"), homeDir(), ".config");
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

gp::StringView Paths::documentsDir() noexcept
{
    const gp::StringView overrideDir = tryGetDocumentsDirOverride();
    if (!overrideDir.isEmpty())
    {
        return overrideDir;
    }

    static PathBuffer s_documentsDirCache{};
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        const gp::StringView home = homeDir();
        const gp::StringView envDocs = detail::getEnvView("XDG_DOCUMENTS_DIR");
        PathBuffer computed{};
        if (!envDocs.isEmpty() && isAbsolute(envDocs))
        {
            computed = resolveXdgDirectory(envDocs, home, "Documents");
        }
        else
        {
            computed = detail::tryResolveFromUserDirsConfig(userConfigDir(), home, "XDG_DOCUMENTS_DIR");
            if (computed.isEmpty())
            {
                computed = resolveXdgDirectory({}, home, "Documents");
            }
        }

        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_documentsDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
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
    static gp::thread::Atomic<gp::UInt32> s_cachedGen(0u);
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_cachedGen.load(gp::memory::MemoryOrder::Acquire) != currentGen) [[unlikely]]
    {
        PathBuffer computed{};
        const gp::StringView envTmp = detail::getEnvView("TMPDIR");
        if (!envTmp.isEmpty() && isAbsolute(envTmp))
        {
            const gp::USize clampedLen = (envTmp.size() <= kMaxPathLength) ? envTmp.size() : kMaxPathLength;
            computed.assign(envTmp.substr(0u, clampedLen));
            makeStandardFilename(computed);
            ensureTrailingSeparator(computed);
        }
        else
        {
            computed.assign("/var/tmp/");
        }

        const ScopedCacheLock lock;
        if (s_cachedGen.load(gp::memory::MemoryOrder::Relaxed) != currentGen)
        {
            s_tempDirCache = computed;
            s_cachedGen.store(currentGen, gp::memory::MemoryOrder::Release);
        }
    }

    return s_tempDirCache.view();
}

}   // namespace gp::platform::unix
