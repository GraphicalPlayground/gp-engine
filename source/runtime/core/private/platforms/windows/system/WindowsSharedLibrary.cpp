// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/WindowsSharedLibrary.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/String.hpp"
#include "hashing/PathHash.hpp"
#include "logging/LogCategory.hpp"
#include "logging/LogMacros.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platforms/base/system/FileEnums.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/FileTypes.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep
#include "profiling/Profiler.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <utility>

namespace gp::platform::windows
{

namespace detail
{

/// @brief Maximum UTF-16 `wchar_t` stack buffer capacity for Win32 module loader calls.
constexpr gp::USize kWin32WidePathCapacity = kMaxPathLength + 16u;

/// @brief Stack capacity for null-terminating exported symbol names passed to `GetProcAddress()`.
constexpr gp::USize kWin32ProcNameStackCapacity = 512u;

/// @brief Sentinel directory index indicating no matching registered directory was found.
constexpr gp::UInt32 kInvalidDirectoryIndex = static_cast<gp::UInt32>(-1);

/// @brief Cache entry associating an indexed `.dll` filename with its parent directory index in `searchPaths`.
struct CachedLibraryEntry
{
    gp::String fileName{};
    gp::UInt32 directoryIndex{ 0u };
};

/// @brief Process-wide Win32 shared library search-path state protected by a compact `gp::thread::Mutex`.
/// @details Uses a Structure-of-Arrays (SoA) hash array (`cacheNameHashes`) parallel to `cacheEntries` so filename
///          lookups scan 8 hashes per 64-byte CPU cache line before touching `gp::String` entries.
/// @note `mutex` must NEVER be held across `LoadLibraryW`, `LoadLibraryExW`, `FreeLibrary`, or filesystem I/O to
///       prevent AB-BA deadlocks with the Windows OS Loader Lock (`LdrpLoaderLock`).
struct SharedLibraryState
{
    gp::thread::Mutex mutex{};

    /// @brief LIFO stack compensating for Win32 `SetDllDirectoryW()` only storing a single active directory.
    gp::Vector<gp::String> searchPathsStack{};

    /// @brief Case-insensitive 64-bit path hashes parallel to `searchPaths` for O(1)-cost deduplication scans.
    gp::Vector<gp::UInt64> searchPathHashes{};

    /// @brief Persistent normalized UTF-8 directories registered via `addDirectory()`.
    gp::Vector<gp::String> searchPaths{};

    /// @brief Case-insensitive 64-bit filename hashes parallel to `cacheEntries` (SoA hot scan array).
    gp::Vector<gp::UInt64> cacheNameHashes{};

    /// @brief Cached `.dll` filenames discovered inside registered `searchPaths` directories.
    gp::Vector<CachedLibraryEntry> cacheEntries{};
};

/// @brief Returns the process-wide `SharedLibraryState` singleton without static initialization order hazards.
/// @return Reference to the global `SharedLibraryState`.
[[nodiscard]] static SharedLibraryState& getState() noexcept
{
    static SharedLibraryState s_state{};
    return s_state;
}

/// @brief RAII guard applying a temporary Win32 thread error mode and unconditionally restoring the previous mode.
class ScopedThreadErrorMode final
{
public:
    /// @brief Sets the calling thread's Win32 error mode via `SetThreadErrorMode()`.
    /// @param[in] newErrorMode Desired `SEM_*` bitmask for the duration of the scope.
    explicit ScopedThreadErrorMode(const DWORD newErrorMode) noexcept
        : m_hasPreviousMode(::SetThreadErrorMode(newErrorMode, &m_previousMode) != 0)
    {}

    /// @brief Restores the calling thread's previous Win32 error mode if it was successfully captured.
    ~ScopedThreadErrorMode() noexcept
    {
        if (m_hasPreviousMode)
        {
            ::SetThreadErrorMode(m_previousMode, nullptr);
        }
    }

    ScopedThreadErrorMode(const ScopedThreadErrorMode&) = delete;
    ScopedThreadErrorMode& operator=(const ScopedThreadErrorMode&) = delete;
    ScopedThreadErrorMode(ScopedThreadErrorMode&&) = delete;
    ScopedThreadErrorMode& operator=(ScopedThreadErrorMode&&) = delete;

private:
    DWORD m_previousMode{ 0u };
    bool m_hasPreviousMode{ false };
};

/// @brief Normalizes a candidate search directory into a canonical UTF-8 `PathBuffer` (resolved, `'/'` slashes, no
///        redundant trailing slash).
/// @param[in] directory Input UTF-8 directory view.
/// @param[out] outNormalized Receives the normalized directory path on success.
/// @return True if `directory` was valid and normalized to a non-empty path, false otherwise.
[[nodiscard]] static bool normalizeSearchDirectory(const gp::StringView directory, PathBuffer& outNormalized) noexcept
{
    outNormalized.clear();
    if (directory.isEmpty() || directory.size() > kMaxPathLength)
    {
        return false;
    }

    outNormalized = Paths::resolve(directory);
    Paths::normalizeDirectoryName(outNormalized);
    return !outNormalized.isEmpty();
}

/// @brief Converts a UTF-8 `gp::StringView` path into a null-terminated Win32 UTF-16 `wchar_t` buffer on the stack.
/// @param[in] utf8Path Input UTF-8 path or library name.
/// @param[out] outWideBuf Destination stack buffer of capacity `kWin32WidePathCapacity`.
/// @param[in] trimTrailingSlash If true, removes redundant trailing slashes prior to conversion.
/// @return True if conversion succeeded and produced a non-empty UTF-16 string, false otherwise.
[[nodiscard]] static bool toWin32WidePath(
    const gp::StringView utf8Path, wchar_t (&outWideBuf)[kWin32WidePathCapacity], const bool trimTrailingSlash = false
) noexcept
{
    outWideBuf[0] = L'\0';
    if (utf8Path.isEmpty() || utf8Path.size() > kMaxPathLength)
    {
        return false;
    }

    PathBuffer platformPath(utf8Path);
    if (trimTrailingSlash)
    {
        Paths::normalizeDirectoryName(platformPath);
    }
    Paths::makePlatformFilename(platformPath);
    if (platformPath.isEmpty())
    {
        return false;
    }

    const int wideLen = ::MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        platformPath.cString(),
        static_cast<int>(platformPath.size()),
        outWideBuf,
        static_cast<int>(kWin32WidePathCapacity - 1u)
    );

    if (wideLen <= 0 || static_cast<gp::USize>(wideLen) >= kWin32WidePathCapacity)
    {
        outWideBuf[0] = L'\0';
        return false;
    }

    outWideBuf[wideLen] = L'\0';
    return true;
}

/// @brief Computes a canonical case-insensitive 64-bit hash for a DLL filename or normalized directory path.
/// @param[in] text UTF-8 path or filename view.
/// @return 64-bit case-insensitive FNV-1a path digest.
[[nodiscard]] static gp::UInt64 hashCaseInsensitive(const gp::StringView text) noexcept
{
    return gp::hashing::PathHash64(text).value();
}

/// @brief Searches `state.searchPaths` for a previously registered directory matching `normalizedDir`.
/// @note Caller must hold `state.mutex`.
/// @param[in] state Locked shared-library state.
/// @param[in] dirHash Precomputed case-insensitive hash of `normalizedDir`.
/// @param[in] normalizedDir Normalized UTF-8 directory path to search for.
/// @return Index into `state.searchPaths`, or `kInvalidDirectoryIndex` if not registered.
[[nodiscard]] static gp::UInt32 findRegisteredDirectoryIndexLocked(
    const SharedLibraryState& state, const gp::UInt64 dirHash, const gp::StringView normalizedDir
) noexcept
{
    const gp::USize count = state.searchPathHashes.size();
    for (gp::USize i = 0u; i < count; ++i)
    {
        if (state.searchPathHashes[i] == dirHash && Paths::equals(state.searchPaths[i].view(), normalizedDir, false))
        {
            return static_cast<gp::UInt32>(i);
        }
    }
    return kInvalidDirectoryIndex;
}

/// @brief Checks whether a specific `(directoryIndex, fileName)` entry is already present in the DLL cache.
/// @note Caller must hold `state.mutex`.
/// @param[in] state Locked shared-library state.
/// @param[in] nameHash Precomputed case-insensitive hash of `fileName`.
/// @param[in] fileName Clean `.dll` filename.
/// @param[in] directoryIndex Parent directory index in `state.searchPaths`.
/// @return True if an equivalent entry already exists in the cache, false otherwise.
[[nodiscard]] static bool isCachedEntryPresentLocked(
    const SharedLibraryState& state,
    const gp::UInt64 nameHash,
    const gp::StringView fileName,
    const gp::UInt32 directoryIndex
) noexcept
{
    const gp::USize count = state.cacheNameHashes.size();
    for (gp::USize i = 0u; i < count; ++i)
    {
        if (state.cacheNameHashes[i] == nameHash && state.cacheEntries[i].directoryIndex == directoryIndex &&
            Paths::compareCaseInsensitive(state.cacheEntries[i].fileName.view(), fileName) == 0)
        {
            return true;
        }
    }
    return false;
}

/// @brief Appends a candidate full path to `candidates` only if no case-insensitively equal path is already queued.
/// @param[in,out] candidates Destination candidate path list.
/// @param[in] candidatePath Candidate full UTF-8 path to enqueue.
static void
    appendUniqueCandidate(gp::InlineVector<PathBuffer, 8u>& candidates, const PathBuffer& candidatePath) noexcept
{
    if (candidatePath.isEmpty())
    {
        return;
    }

    const gp::StringView candidateView = candidatePath.view();
    for (gp::USize i = 0u; i < candidates.size(); ++i)
    {
        if (Paths::equals(candidates[i].view(), candidateView, false))
        {
            return;
        }
    }

    candidates.pushBack(candidatePath);
}

/// @brief Loads a DLL located at an absolute UTF-8 path using the optimal Win32 search-order flag strategy.
/// @details When a directory is actively pushed on `searchPathsStack`, tries `LoadLibraryW()` first so the active
///          `SetDllDirectoryW()` directory is searched for transitive dependencies, falling back to
///          `LOAD_WITH_ALTERED_SEARCH_PATH`. When the stack is empty, tries `LOAD_WITH_ALTERED_SEARCH_PATH` first so
///          sibling dependencies residing alongside the target DLL resolve automatically, falling back to
///          `LoadLibraryW()`.
/// @note Must NEVER be called while holding `SharedLibraryState::mutex`.
/// @param[in] absolutePath Absolute UTF-8 path to the target DLL.
/// @param[in] hasActivePushedDir True if `searchPathsStack` was non-empty at lookup time.
/// @return Loaded `HMODULE` handle, or `nullptr` if loading failed.
[[nodiscard]] static HMODULE
    loadAbsoluteLibrary(const gp::StringView absolutePath, const bool hasActivePushedDir) noexcept
{
    wchar_t widePath[kWin32WidePathCapacity]{};
    if (!toWin32WidePath(absolutePath, widePath, false))
    {
        return nullptr;
    }

    HMODULE handle = nullptr;
    if (hasActivePushedDir)
    {
        handle = ::LoadLibraryW(widePath);
        if (handle == nullptr)
        {
            handle = ::LoadLibraryExW(widePath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
    }
    else
    {
        handle = ::LoadLibraryExW(widePath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (handle == nullptr)
        {
            handle = ::LoadLibraryW(widePath);
        }
    }

    return handle;
}

/// @brief Builds the `".dll"`-suffixed filename variant if `filename` has no extension and no trailing dot.
/// @param[in] filename Clean library leaf name.
/// @param[out] outWithExt Receives `filename + ".dll"` when an implicit extension applies.
/// @return True if `outWithExt` was populated with a distinct suffixed name, false otherwise.
[[nodiscard]] static bool tryBuildWithDefaultExtension(const gp::StringView filename, PathBuffer& outWithExt) noexcept
{
    outWithExt.clear();
    if (filename.isEmpty() || filename.endsWith('.') || !Paths::getExtension(filename).isEmpty())
    {
        return false;
    }

    constexpr gp::StringView kDllExt = SharedLibrary::getDefaultExtension();
    if ((filename.size() + kDllExt.size()) > kMaxPathLength)
    {
        return false;
    }

    outWithExt.assign(filename);
    outWithExt.append(kDllExt);
    return true;
}

/// @brief Checks whether a file exists at `basePath` or (if no extension is present) at `basePath + ".dll"`.
/// @param[in] basePath Candidate UTF-8 file path.
/// @param[out] outResolvedPath Receives the existing file path on success.
/// @return True if a regular file exists on disk, false otherwise.
[[nodiscard]] static bool
    tryResolveExistingFileOnDisk(const gp::StringView basePath, PathBuffer& outResolvedPath) noexcept
{
    if (FileSystem::isFile(basePath))
    {
        outResolvedPath.assign(basePath);
        return true;
    }

    PathBuffer withExt{};
    if (tryBuildWithDefaultExtension(basePath, withExt) && FileSystem::isFile(withExt.view()))
    {
        outResolvedPath = withExt;
        return true;
    }

    return false;
}

}   // namespace detail

void SharedLibrary::freeHandle(void* const handle) noexcept
{
    if (handle != nullptr)
    {
        ::FreeLibrary(static_cast<HMODULE>(handle));
    }
}

void* SharedLibrary::getExport(void* const handle, const gp::StringView procName) noexcept
{
    if (handle == nullptr || procName.isEmpty())
    {
        return nullptr;
    }

    // Win32 GetProcAddress requires a null-terminated narrow ASCII string; copy non-terminated views safely.
    if (procName.size() <= detail::kWin32ProcNameStackCapacity)
    {
        const gp::FixedString<detail::kWin32ProcNameStackCapacity> stackName(procName);
        const FARPROC proc = ::GetProcAddress(static_cast<HMODULE>(handle), stackName.cString());
        return reinterpret_cast<void*>(proc);
    }

    const gp::String heapName(procName);
    const FARPROC proc = ::GetProcAddress(static_cast<HMODULE>(handle), heapName.cString());
    return reinterpret_cast<void*>(proc);
}

void* SharedLibrary::loadLibraryFromSearchPaths(const gp::StringView filename) noexcept
{
    GP_PROFILE_SCOPE_NC("WindowsSharedLibrary::loadLibraryFromSearchPaths", GP_U32_COLOR_ORANGE);

    if (filename.isEmpty() || filename.size() > kMaxPathLength)
    {
        return nullptr;
    }

    detail::SharedLibraryState& state = detail::getState();

    // Case 1: Explicit path containing directory separators (absolute or relative path).
    if (!Paths::isPathLeaf(filename))
    {
        bool hasActivePushedDir = false;
        {
            const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
            hasActivePushedDir = !state.searchPathsStack.isEmpty();
        }

        if (Paths::isAbsolute(filename))
        {
            const PathBuffer standardized = Paths::createStandardFilename(filename);
            return static_cast<void*>(detail::loadAbsoluteLibrary(standardized.view(), hasActivePushedDir));
        }

        // Relative path with directory components: try working directory, then engine base directory, then OS default.
        const PathBuffer cwdCandidate = Paths::resolve(filename, Paths::workingDir().view());
        PathBuffer resolvedDiskPath{};
        if (detail::tryResolveExistingFileOnDisk(cwdCandidate.view(), resolvedDiskPath))
        {
            return static_cast<void*>(detail::loadAbsoluteLibrary(resolvedDiskPath.view(), hasActivePushedDir));
        }

        const PathBuffer baseCandidate = Paths::resolve(filename);
        if (detail::tryResolveExistingFileOnDisk(baseCandidate.view(), resolvedDiskPath))
        {
            return static_cast<void*>(detail::loadAbsoluteLibrary(resolvedDiskPath.view(), hasActivePushedDir));
        }

        wchar_t wideRelative[detail::kWin32WidePathCapacity]{};
        if (detail::toWin32WidePath(filename, wideRelative, false))
        {
            return static_cast<void*>(::LoadLibraryW(wideRelative));
        }
        return nullptr;
    }

    // Case 2: Bare library filename (e.g., "MyPlugin.dll" or "MyPlugin" or "kernel32.dll").
    PathBuffer filenameWithExt{};
    const bool hasImplicitExtVariant = detail::tryBuildWithDefaultExtension(filename, filenameWithExt);
    const gp::UInt64 primaryHash = detail::hashCaseInsensitive(filename);
    const gp::UInt64 extHash = hasImplicitExtVariant ? detail::hashCaseInsensitive(filenameWithExt.view()) : 0ULL;

    struct FallbackDirSnapshot
    {
        PathBuffer directory{};
        gp::UInt32 directoryIndex{ 0u };
    };

    gp::InlineVector<PathBuffer, 8u> prioritizedCandidates{};
    gp::InlineVector<FallbackDirSnapshot, 8u> fallbackDirs{};
    bool hasActivePushedDir = false;

    // Snapshot candidate paths under the lock, then release BEFORE any disk I/O or Win32 LoadLibrary* calls.
    {
        const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
        hasActivePushedDir = !state.searchPathsStack.isEmpty();

        // Priority 1: Active LIFO searchPathsStack (top-to-bottom so the most recently pushed directory wins).
        for (gp::USize i = state.searchPathsStack.size(); i > 0u; --i)
        {
            const gp::StringView stackDir = state.searchPathsStack[i - 1u].view();
            detail::appendUniqueCandidate(prioritizedCandidates, Paths::combine(stackDir, filename));
            if (hasImplicitExtVariant)
            {
                detail::appendUniqueCandidate(prioritizedCandidates, Paths::combine(stackDir, filenameWithExt.view()));
            }
        }

        // Priority 2: Pre-indexed SoA DLL cache from registered addDirectory() paths.
        const gp::USize cacheCount = state.cacheNameHashes.size();
        for (gp::USize i = 0u; i < cacheCount; ++i)
        {
            const gp::UInt64 entryHash = state.cacheNameHashes[i];
            const bool primaryMatch =
                (entryHash == primaryHash) &&
                (Paths::compareCaseInsensitive(state.cacheEntries[i].fileName.view(), filename) == 0);
            const bool extMatch =
                !primaryMatch && hasImplicitExtVariant && (entryHash == extHash) &&
                (Paths::compareCaseInsensitive(state.cacheEntries[i].fileName.view(), filenameWithExt.view()) == 0);

            if (primaryMatch || extMatch)
            {
                const gp::UInt32 dirIdx = state.cacheEntries[i].directoryIndex;
                if (dirIdx < state.searchPaths.size())
                {
                    const PathBuffer cachedFullPath =
                        Paths::combine(state.searchPaths[dirIdx].view(), state.cacheEntries[i].fileName.view());
                    detail::appendUniqueCandidate(prioritizedCandidates, cachedFullPath);
                }
            }
        }

        // Priority 3: Snapshot all registered directories for cache-miss fallback (handles newly dropped/hot-reloaded
        // DLLs).
        const gp::USize registeredCount = state.searchPaths.size();
        for (gp::USize i = 0u; i < registeredCount; ++i)
        {
            fallbackDirs.pushBack(
                FallbackDirSnapshot{ PathBuffer(state.searchPaths[i].view()), static_cast<gp::UInt32>(i) }
            );
        }
    }

    // Attempt loading prioritized candidates (stack entries first, then cache hits).
    for (gp::USize i = 0u; i < prioritizedCandidates.size(); ++i)
    {
        const gp::StringView candidateView = prioritizedCandidates[i].view();
        if (FileSystem::isFile(candidateView))
        {
            HMODULE const handle = detail::loadAbsoluteLibrary(candidateView, hasActivePushedDir);
            if (handle != nullptr)
            {
                return static_cast<void*>(handle);
            }
        }
    }

    // Cache-miss fallback: probe registered searchPaths directly on disk in case a DLL appeared after addDirectory().
    for (gp::USize i = 0u; i < fallbackDirs.size(); ++i)
    {
        const gp::StringView dirView = fallbackDirs[i].directory.view();
        const PathBuffer baseProbe = Paths::combine(dirView, filename);
        PathBuffer foundPath{};
        if (detail::tryResolveExistingFileOnDisk(baseProbe.view(), foundPath))
        {
            HMODULE const handle = detail::loadAbsoluteLibrary(foundPath.view(), hasActivePushedDir);
            if (handle != nullptr)
            {
                // Lazily populate the cache for subsequent loads if the directory is still registered at this index.
                const gp::StringView leafName = Paths::getCleanFilename(foundPath.view());
                if (!leafName.isEmpty())
                {
                    const gp::UInt64 leafHash = detail::hashCaseInsensitive(leafName);
                    const gp::UInt32 dirIdx = fallbackDirs[i].directoryIndex;
                    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
                    if (dirIdx < state.searchPaths.size() &&
                        Paths::equals(state.searchPaths[dirIdx].view(), dirView, false) &&
                        !detail::isCachedEntryPresentLocked(state, leafHash, leafName, dirIdx))
                    {
                        state.cacheNameHashes.pushBack(leafHash);
                        state.cacheEntries.pushBack(detail::CachedLibraryEntry{ gp::String(leafName), dirIdx });
                    }
                }
                return static_cast<void*>(handle);
            }
        }
    }

    // Priority 4: Standard Win32 already-loaded module check (incrementing refcount!) followed by OS LoadLibraryW().
    wchar_t wideName[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(filename, wideName, false))
    {
        return nullptr;
    }

    HMODULE existingModule = nullptr;
    if (::GetModuleHandleExW(0u, wideName, &existingModule) != 0 && existingModule != nullptr)
    {
        return static_cast<void*>(existingModule);
    }

    HMODULE const loadedModule = ::LoadLibraryW(wideName);
    return static_cast<void*>(loadedModule);
}

void* SharedLibrary::getHandle(const gp::StringView filename) noexcept
{
    GP_PROFILE_SCOPE_NC("WindowsSharedLibrary::getHandle", GP_U32_COLOR_ORANGE);

    if (filename.isEmpty())
    {
        return nullptr;
    }

    DWORD errorMode = 0u;
    if (!gp::CommandLine::getBool("dllerrors", false))
    {
        errorMode |= SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS;
        if (gp::CommandLine::getBool("unattended", false))
        {
            errorMode |= SEM_NOGPFAULTERRORBOX;
        }
    }

    const detail::ScopedThreadErrorMode scopedErrorMode(errorMode);
    return loadLibraryFromSearchPaths(filename);
}

void SharedLibrary::addDirectory(const gp::StringView directory) noexcept
{
    GP_PROFILE_SCOPE_NC("WindowsSharedLibrary::addDirectory", GP_U32_COLOR_ORANGE);

    PathBuffer normalizedDir{};
    if (!detail::normalizeSearchDirectory(directory, normalizedDir))
    {
        GP_ENSURE_MSG(false, "SharedLibrary::addDirectory() called with an empty or invalid directory path.");
        return;
    }

    const gp::StringView dirView = normalizedDir.view();
    const gp::UInt64 dirHash = detail::hashCaseInsensitive(dirView);
    detail::SharedLibraryState& state = detail::getState();

    // Fast deduplication check before touching the filesystem.
    {
        const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
        if (detail::findRegisteredDirectoryIndexLocked(state, dirHash, dirView) != detail::kInvalidDirectoryIndex)
        {
            return;
        }
    }

    // Enumerate immediate `.dll` files outside the mutex lock to avoid holding locks across OS I/O.
    gp::Vector<gp::String> discoveredDlls{};
    const auto collectDlls = [&discoveredDlls](const DirectoryEntry& entry) noexcept -> DirectoryVisitControl
    {
        if (entry.isFile())
        {
            const gp::StringView ext = Paths::getExtension(entry.name, PathFlags::None);
            if (Paths::compareCaseInsensitive(ext, "dll") == 0)
            {
                discoveredDlls.emplaceBack(entry.name);
            }
        }
        return DirectoryVisitControl::Continue;
    };
    (void)FileSystem::iterateDirectory(dirView, collectDlls);

    // Commit directory and discovered `.dll` cache entries atomically (re-verifying deduplication).
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    if (detail::findRegisteredDirectoryIndexLocked(state, dirHash, dirView) != detail::kInvalidDirectoryIndex)
    {
        return;
    }

    const gp::UInt32 dirIndex = static_cast<gp::UInt32>(state.searchPaths.size());
    state.searchPathHashes.pushBack(dirHash);
    state.searchPaths.emplaceBack(dirView);

    const gp::USize discoveredCount = discoveredDlls.size();
    for (gp::USize i = 0u; i < discoveredCount; ++i)
    {
        const gp::StringView dllName = discoveredDlls[i].view();
        const gp::UInt64 nameHash = detail::hashCaseInsensitive(dllName);
        state.cacheNameHashes.pushBack(nameHash);
        state.cacheEntries.pushBack(detail::CachedLibraryEntry{ std::move(discoveredDlls[i]), dirIndex });
    }
}

void SharedLibrary::pushDirectory(const gp::StringView directory) noexcept
{
    PathBuffer normalizedDir{};
    if (!detail::normalizeSearchDirectory(directory, normalizedDir))
    {
        GP_ENSURE_MSG(false, "SharedLibrary::pushDirectory() called with an empty or invalid directory path.");
        return;
    }

    wchar_t wideDir[detail::kWin32WidePathCapacity]{};
    if (!detail::toWin32WidePath(normalizedDir.view(), wideDir, true))
    {
        GP_LOG_WARNING(
            gp::log::LogCore, "SharedLibrary::pushDirectory() failed to convert '{}' to UTF-16.", normalizedDir.view()
        );
        return;
    }

    detail::SharedLibraryState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);

    if (::SetDllDirectoryW(wideDir) == 0)
    {
        GP_LOG_WARNING(
            gp::log::LogCore,
            "SharedLibrary::pushDirectory(): SetDllDirectoryW('{}') failed with Win32 error {}.",
            normalizedDir.view(),
            static_cast<gp::UInt32>(::GetLastError())
        );
        return;
    }

    state.searchPathsStack.emplaceBack(normalizedDir.view());
}

void SharedLibrary::popDirectory(const gp::StringView directory) noexcept
{
    PathBuffer normalizedDir{};
    if (!detail::normalizeSearchDirectory(directory, normalizedDir))
    {
        GP_ENSURE_MSG(false, "SharedLibrary::popDirectory() called with an empty or invalid directory path.");
        return;
    }

    detail::SharedLibraryState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);

    if (!GP_ENSURE_MSG(!state.searchPathsStack.isEmpty(), "Attempted to pop from an empty DLL directory stack."))
    {
        return;
    }

    const gp::StringView currentTop = state.searchPathsStack.back().view();
    if (!GP_ENSURE_MSG(
            Paths::equals(currentTop, normalizedDir.view(), false), "Mismatch in pushDirectory/popDirectory operations."
        ))
    {
        GP_LOG_WARNING(
            gp::log::LogCore,
            "SharedLibrary::popDirectory() mismatch: expected top '{}', got '{}'.",
            currentTop,
            normalizedDir.view()
        );
        return;
    }

    state.searchPathsStack.popBack();

    if (!state.searchPathsStack.isEmpty())
    {
        wchar_t widePreviousDir[detail::kWin32WidePathCapacity]{};
        if (detail::toWin32WidePath(state.searchPathsStack.back().view(), widePreviousDir, true))
        {
            ::SetDllDirectoryW(widePreviousDir);
        }
    }
    else
    {
        // Clear the active DLL directory and exclude the current working directory from default DLL search order.
        ::SetDllDirectoryW(L"");
    }
}

gp::USize SharedLibrary::getDirectoryStackDepth() noexcept
{
    detail::SharedLibraryState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    return state.searchPathsStack.size();
}

PathBuffer SharedLibrary::getActiveDirectory() noexcept
{
    detail::SharedLibraryState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    if (state.searchPathsStack.isEmpty())
    {
        return {};
    }
    return PathBuffer(state.searchPathsStack.back().view());
}

gp::USize SharedLibrary::getRegisteredDirectoryCount() noexcept
{
    detail::SharedLibraryState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    return state.searchPaths.size();
}

gp::USize SharedLibrary::getCachedLibraryCount() noexcept
{
    detail::SharedLibraryState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    return state.cacheEntries.size();
}

void SharedLibrary::resetSearchPaths() noexcept
{
    detail::SharedLibraryState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    state.searchPathsStack.clear();
    state.searchPathHashes.clear();
    state.searchPaths.clear();
    state.cacheNameHashes.clear();
    state.cacheEntries.clear();
    ::SetDllDirectoryW(L"");
}

}   // namespace gp::platform::windows
