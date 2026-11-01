// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/WindowsPaths.hpp"
#include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep
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
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_exePathCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        wchar_t wideModulePath[detail::kWin32WideBufferCapacity]{};
        const DWORD len =
            ::GetModuleFileNameW(nullptr, wideModulePath, static_cast<DWORD>(detail::kWin32WideBufferCapacity));
        if (len > 0u && len < detail::kWin32WideBufferCapacity)
        {
            (void)detail::wideToNormalizedUtf8(wideModulePath, s_exePathCache, false);
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
            s_exeDirCache.assign("./");
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
        if (!detail::queryShellKnownFolder(FOLDERID_Profile, s_homeDirCache))
        {
            if (!detail::queryWideEnvDirectory(L"USERPROFILE", s_homeDirCache))
            {
                s_homeDirCache = executableDir();
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
        if (!detail::queryShellKnownFolder(FOLDERID_RoamingAppData, s_appDataDirCache))
        {
            if (!detail::queryWideEnvDirectory(L"APPDATA", s_appDataDirCache))
            {
                s_appDataDirCache = homeDir();
            }
        }
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
        if (!detail::queryShellKnownFolder(FOLDERID_LocalAppData, s_localAppDataDirCache))
        {
            if (!detail::queryWideEnvDirectory(L"LOCALAPPDATA", s_localAppDataDirCache))
            {
                s_localAppDataDirCache = appDataDir();
            }
        }
        s_cachedGen = currentGen;
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
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_documentsDirCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        if (!detail::queryShellKnownFolder(FOLDERID_Documents, s_documentsDirCache))
        {
            s_documentsDirCache = combine(homeDir(), "Documents");
            ensureTrailingSeparator(s_documentsDirCache);
        }
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
        wchar_t shortTempBuf[detail::kWin32WideBufferCapacity]{};
        const DWORD tempLen = ::GetTempPathW(static_cast<DWORD>(detail::kWin32WideBufferCapacity), shortTempBuf);
        if (tempLen > 0u && tempLen < detail::kWin32WideBufferCapacity)
        {
            wchar_t longTempBuf[detail::kWin32WideBufferCapacity]{};
            const DWORD longLen =
                ::GetLongPathNameW(shortTempBuf, longTempBuf, static_cast<DWORD>(detail::kWin32WideBufferCapacity));
            const wchar_t* const chosenBuf =
                (longLen > 0u && longLen < detail::kWin32WideBufferCapacity) ? longTempBuf : shortTempBuf;
            (void)detail::wideToNormalizedUtf8(chosenBuf, s_tempDirCache, true);
        }

        if (s_tempDirCache.isEmpty())
        {
            s_tempDirCache = combine(localAppDataDir(), "Temp");
            ensureTrailingSeparator(s_tempDirCache);
        }
        s_cachedGen = currentGen;
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
