// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/WindowsPaths.hpp"
#include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"
#include <initguid.h>
#include <KnownFolders.h>

#if GP_COMPILER_MSVC
    #pragma comment(lib, "Shell32.lib")
    #pragma comment(lib, "Ole32.lib")
#endif

extern "C"
{
    /// @brief Retrieves the full path of a known folder identified by the folder's `KNOWNFOLDERID` (Shell32.dll).
    HRESULT __stdcall SHGetKnownFolderPath(const GUID& rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath);

    /// @brief Frees a block of task memory previously allocated through `SHGetKnownFolderPath` (Ole32.dll).
    void __stdcall CoTaskMemFree(LPVOID pv);
}

namespace gp::platform::windows
{

namespace detail
{

/// @brief Maximum UTF-16 `wchar_t` buffer capacity used for Win32 path queries.
constexpr gp::USize kWin32WideBufferCapacity = kMaxPathLength + 1u;

/// @brief Converts a null-terminated UTF-16 Win32 path into a normalized UTF-8 `PathBuffer`.
/// @param[in] widePath Null-terminated UTF-16 string from a Win32 API.
/// @param[out] outBuffer Destination `PathBuffer` to populate.
/// @param[in] isDirectory If true, ensures `outBuffer` ends with a trailing `'/'`.
/// @return True if conversion succeeded and produced a non-empty path, false otherwise.
[[nodiscard]] static bool
    wideToNormalizedUtf8(const wchar_t* const widePath, PathBuffer& outBuffer, const bool isDirectory) noexcept
{
    outBuffer.clear();
    if (widePath == nullptr || widePath[0] == L'\0')
    {
        return false;
    }

    char utf8Buf[kWin32WideBufferCapacity]{};
    const int written = ::WideCharToMultiByte(
        CP_UTF8, 0u, widePath, -1, utf8Buf, static_cast<int>(kWin32WideBufferCapacity), nullptr, nullptr
    );

    if (written <= 1)
    {
        return false;
    }

    outBuffer.assign(gp::StringView(utf8Buf, static_cast<gp::USize>(written - 1)));
    Paths::makeStandardFilename(outBuffer);
    if (isDirectory)
    {
        Paths::ensureTrailingSeparator(outBuffer);
    }

    return !outBuffer.isEmpty();
}

/// @brief Queries a Windows Shell Known Folder (`FOLDERID_*`) and converts the result into a normalized UTF-8
///        `PathBuffer`.
/// @param[in] folderId Known Folder GUID (`FOLDERID_Profile`, `FOLDERID_RoamingAppData`, etc.).
/// @param[out] outBuffer Destination `PathBuffer` to populate (guaranteed trailing `'/'`).
/// @return True if `SHGetKnownFolderPath` succeeded, false otherwise.
[[nodiscard]] static bool queryShellKnownFolder(const GUID& folderId, PathBuffer& outBuffer) noexcept
{
    PWSTR rawWidePath = nullptr;
    const HRESULT hr = ::SHGetKnownFolderPath(folderId, 0u, nullptr, &rawWidePath);
    const bool ok = SUCCEEDED(hr) && wideToNormalizedUtf8(rawWidePath, outBuffer, true);

    if (rawWidePath != nullptr)
    {
        ::CoTaskMemFree(rawWidePath);
    }

    return ok;
}

/// @brief Queries a Win32 UTF-16 environment variable and converts it into a normalized UTF-8 directory `PathBuffer`.
/// @param[in] envName Null-terminated wide environment variable name (e.g., `L"USERPROFILE"`).
/// @param[out] outBuffer Destination `PathBuffer` to populate (guaranteed trailing `'/'`).
/// @return True if the environment variable exists and is non-empty, false otherwise.
[[nodiscard]] static bool queryWideEnvDirectory(const wchar_t* const envName, PathBuffer& outBuffer) noexcept
{
    wchar_t wideBuf[kWin32WideBufferCapacity]{};
    const DWORD len = ::GetEnvironmentVariableW(envName, wideBuf, static_cast<DWORD>(kWin32WideBufferCapacity));
    if (len == 0u || len >= kWin32WideBufferCapacity)
    {
        return false;
    }

    return wideToNormalizedUtf8(wideBuf, outBuffer, true);
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
        wchar_t wideModulePath[detail::kWin32WideBufferCapacity]{};
        const DWORD len =
            ::GetModuleFileNameW(nullptr, wideModulePath, static_cast<DWORD>(detail::kWin32WideBufferCapacity));
        if (len > 0u && len < detail::kWin32WideBufferCapacity)
        {
            (void)detail::wideToNormalizedUtf8(wideModulePath, computed, false);
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
            computed.assign("./");
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

PathBuffer Paths::workingDir() noexcept
{
    PathBuffer result{};
    wchar_t wideCwd[detail::kWin32WideBufferCapacity]{};
    const DWORD len = ::GetCurrentDirectoryW(static_cast<DWORD>(detail::kWin32WideBufferCapacity), wideCwd);
    if (len > 0u && len < detail::kWin32WideBufferCapacity)
    {
        if (detail::wideToNormalizedUtf8(wideCwd, result, true))
        {
            return result;
        }
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
        if (!detail::queryShellKnownFolder(FOLDERID_Profile, computed))
        {
            if (!detail::queryWideEnvDirectory(L"USERPROFILE", computed))
            {
                computed.assign(executableDir());
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
        PathBuffer computed{};
        if (!detail::queryShellKnownFolder(FOLDERID_RoamingAppData, computed))
        {
            if (!detail::queryWideEnvDirectory(L"APPDATA", computed))
            {
                computed.assign(homeDir());
            }
        }

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
        PathBuffer computed{};
        if (!detail::queryShellKnownFolder(FOLDERID_LocalAppData, computed))
        {
            if (!detail::queryWideEnvDirectory(L"LOCALAPPDATA", computed))
            {
                computed.assign(appDataDir());
            }
        }

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
    return localAppDataDir();
}

gp::StringView Paths::userSettingsDir() noexcept
{
    return localAppDataDir();
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
        PathBuffer computed{};
        if (!detail::queryShellKnownFolder(FOLDERID_Documents, computed))
        {
            computed = combine(homeDir(), "Documents");
            ensureTrailingSeparator(computed);
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
        wchar_t shortTempBuf[detail::kWin32WideBufferCapacity]{};
        const DWORD tempLen = ::GetTempPathW(static_cast<DWORD>(detail::kWin32WideBufferCapacity), shortTempBuf);
        if (tempLen > 0u && tempLen < detail::kWin32WideBufferCapacity)
        {
            wchar_t longTempBuf[detail::kWin32WideBufferCapacity]{};
            const DWORD longLen =
                ::GetLongPathNameW(shortTempBuf, longTempBuf, static_cast<DWORD>(detail::kWin32WideBufferCapacity));
            const wchar_t* const chosenBuf =
                (longLen > 0u && longLen < detail::kWin32WideBufferCapacity) ? longTempBuf : shortTempBuf;
            (void)detail::wideToNormalizedUtf8(chosenBuf, computed, true);
        }

        if (computed.isEmpty())
        {
            computed = combine(localAppDataDir(), "Temp");
            ensureTrailingSeparator(computed);
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

PathBuffer Paths::resolve(const gp::StringView path) noexcept
{
    return resolve(path, baseDir());
}

PathBuffer Paths::convertRelativePathToFull(const gp::StringView inPath) noexcept
{
    return resolve(inPath, baseDir());
}

}   // namespace gp::platform::windows
