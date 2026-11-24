// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "platforms/base/system/SharedLibraryTypes.hpp"
#include "threading/sync/Mutex.hpp"

namespace gp::platform::generic
{

/// @brief Thread-safe LIFO search-path stack and Structure-of-Arrays (SoA) library filename cache.
/// @details Mirrors Unreal Engine's `PushDllDirectory`, `PopDllDirectory`, and `AddDllDirectory` search state while
///          keeping directory scans and OS dynamic loader calls outside the internal mutex to prevent loader-lock
///          deadlocks. Filename hashes live in a parallel array (`m_cacheNameHashes`) so cache scans inspect eight
///          64-bit hashes per 64-byte cache line before reading any `gp::String`.
class GP_CORE_API SharedLibrarySearchRegistry final
{
public:
    /// @brief Constructs a search registry configured with the given platform matching policy.
    /// @param[in] policy Case-sensitivity and filename filtering rules for this platform.
    explicit SharedLibrarySearchRegistry(const SharedLibrarySearchPolicy& policy) noexcept;

    /// @brief Destroys the registry and releases all stored search paths and cache entries.
    ~SharedLibrarySearchRegistry() noexcept = default;

    SharedLibrarySearchRegistry(const SharedLibrarySearchRegistry&) = delete;
    SharedLibrarySearchRegistry& operator=(const SharedLibrarySearchRegistry&) = delete;
    SharedLibrarySearchRegistry(SharedLibrarySearchRegistry&&) = delete;
    SharedLibrarySearchRegistry& operator=(SharedLibrarySearchRegistry&&) = delete;

public:
    /// @brief Registers a persistent search directory and indexes its immediate matching library files.
    /// @details Deduplicates the normalized path under the lock, enumerates the directory outside the lock via
    ///          `FileSystem::iterateDirectory()`, and commits the discovered entries atomically.
    /// @param[in] directory UTF-8 directory path to register.
    /// @return True if the directory was valid (newly added or already present); false if empty or invalid.
    bool addDirectory(const gp::StringView directory) noexcept;

    /// @brief Pushes a normalized directory onto the LIFO search-path stack.
    /// @param[in] directory UTF-8 directory path to push.
    /// @param[out] outNormalizedDir Optional pointer receiving the normalized directory path on success.
    /// @return True if the directory was pushed; false if empty or invalid.
    bool pushDirectory(const gp::StringView directory, PathBuffer* const outNormalizedDir = nullptr) noexcept;

    /// @brief Pops the top directory from the LIFO search-path stack after verifying it matches `directory`.
    /// @param[in] directory Expected top directory path (UTF-8).
    /// @param[out] outNewActiveDir Optional pointer receiving the new top directory after popping, or empty if empty.
    /// @return True if the top entry matched and was popped; false on empty stack, invalid path, or mismatch.
    bool popDirectory(const gp::StringView directory, PathBuffer* const outNewActiveDir = nullptr) noexcept;

    /// @brief Snapshots prioritized candidate full paths and fallback directories for a set of bare filename variants.
    /// @details Priority order in `outBatch.prioritizedCandidates`:
    ///          1. Active LIFO stack directories (top to bottom) combined with each candidate filename.
    ///          2. Pre-indexed SoA cache hits from registered `addDirectory()` paths.
    ///          All registered directories are also copied into `outBatch.fallbackDirs` for post-registration probing.
    /// @param[in] candidateNames Ordered bare filename variants (for example `"vulkan"`, `"vulkan.so"`,
    /// `"libvulkan.so"`).
    /// @param[in] candidateCount Number of valid entries in `candidateNames`.
    /// @param[out] outBatch Receives the lock-free snapshot of candidates and fallback directories.
    void collectCandidates(
        const PathBuffer* const candidateNames, const gp::USize candidateCount, SharedLibraryCandidateBatch& outBatch
    ) const noexcept;

    /// @brief Lazily inserts a library filename discovered on disk into the SoA cache if the directory is still valid.
    /// @param[in] directoryIndex Registered directory index captured in `SharedLibraryFallbackDir`.
    /// @param[in] expectedDirectory Normalized directory string captured when the snapshot was taken.
    /// @param[in] fileName Clean library filename discovered inside `expectedDirectory`.
    void registerDiscoveredFile(
        const gp::UInt32 directoryIndex, const gp::StringView expectedDirectory, const gp::StringView fileName
    ) noexcept;

    /// @brief Reports whether the LIFO search-path stack currently contains at least one directory.
    /// @return True if at least one directory is pushed; false if empty.
    [[nodiscard]] bool hasActivePushedDirectory() const noexcept;

    /// @brief Returns the number of directories currently on the LIFO search-path stack.
    /// @return Stack depth.
    [[nodiscard]] gp::USize getDirectoryStackDepth() const noexcept;

    /// @brief Returns the normalized directory at the top of the LIFO search-path stack.
    /// @return Active top directory, or an empty `PathBuffer` when the stack is empty.
    [[nodiscard]] PathBuffer getActiveDirectory() const noexcept;

    /// @brief Returns the number of persistent search directories registered via `addDirectory()`.
    /// @return Registered directory count.
    [[nodiscard]] gp::USize getRegisteredDirectoryCount() const noexcept;

    /// @brief Returns the total number of library filenames currently indexed in the SoA cache.
    /// @return Cached filename entry count.
    [[nodiscard]] gp::USize getCachedLibraryCount() const noexcept;

    /// @brief Clears the LIFO stack, registered search directories, and SoA filename cache.
    void reset() noexcept;

    /// @brief Normalizes a candidate search directory into a resolved UTF-8 `PathBuffer` without trailing slash.
    /// @param[in] directory Input UTF-8 directory path.
    /// @param[out] outNormalized Receives the normalized path on success.
    /// @return True if `directory` normalized to a non-empty path; false otherwise.
    [[nodiscard]] static bool
        normalizeSearchDirectory(const gp::StringView directory, PathBuffer& outNormalized) noexcept;

private:
    /// @brief Cache entry associating an indexed library filename with its parent directory index in `m_searchPaths`.
    struct CachedLibraryEntry
    {
        gp::String fileName{};
        gp::UInt32 directoryIndex{ 0u };
    };

    /// @brief Computes a 64-bit path hash honoring `m_policy.caseSensitive`.
    /// @param[in] text UTF-8 path or filename.
    /// @return 64-bit FNV-1a path digest.
    [[nodiscard]] gp::UInt64 computeHash(const gp::StringView text) const noexcept;

    /// @brief Compares two normalized paths honoring `m_policy.caseSensitive`.
    /// @param[in] lhs First path view.
    /// @param[in] rhs Second path view.
    /// @return True if equal under the active policy.
    [[nodiscard]] bool pathsEqual(const gp::StringView lhs, const gp::StringView rhs) const noexcept;

    /// @brief Compares two bare filenames honoring `m_policy.caseSensitive`.
    /// @param[in] lhs First filename.
    /// @param[in] rhs Second filename.
    /// @return True if equal under the active policy.
    [[nodiscard]] bool namesEqual(const gp::StringView lhs, const gp::StringView rhs) const noexcept;

    /// @brief Tests whether `fileName` matches the registry's file filter or default extension.
    /// @param[in] fileName Bare filename from directory enumeration.
    /// @return True if the file should be indexed.
    [[nodiscard]] bool matchesFileFilter(const gp::StringView fileName) const noexcept;

    /// @brief Finds the index of `normalizedDir` inside `m_searchPaths`. Caller must hold `m_mutex`.
    /// @param[in] dirHash Precomputed hash of `normalizedDir`.
    /// @param[in] normalizedDir Normalized UTF-8 directory path.
    /// @return Matching index, or `kInvalidSearchDirectoryIndex` if absent.
    [[nodiscard]] gp::UInt32
        findRegisteredDirectoryIndexLocked(const gp::UInt64 dirHash, const gp::StringView normalizedDir) const noexcept;

    /// @brief Checks whether `(directoryIndex, fileName)` is already cached. Caller must hold `m_mutex`.
    /// @param[in] nameHash Precomputed hash of `fileName`.
    /// @param[in] fileName Clean filename.
    /// @param[in] directoryIndex Parent directory index.
    /// @return True if present; false otherwise.
    [[nodiscard]] bool isCachedEntryPresentLocked(
        const gp::UInt64 nameHash, const gp::StringView fileName, const gp::UInt32 directoryIndex
    ) const noexcept;

    /// @brief Appends `candidatePath` to `candidates` unless an equivalent path is already present.
    /// @param[in,out] candidates Target candidate list.
    /// @param[in] candidatePath Candidate full path to append.
    void appendUniqueCandidate(
        gp::InlineVector<PathBuffer, 16u>& candidates, const PathBuffer& candidatePath
    ) const noexcept;

private:
    SharedLibrarySearchPolicy m_policy{};
    mutable gp::thread::Mutex m_mutex{};
    gp::Vector<gp::String> m_searchPathsStack{};
    gp::Vector<gp::UInt64> m_searchPathHashes{};
    gp::Vector<gp::String> m_searchPaths{};
    gp::Vector<gp::UInt64> m_cacheNameHashes{};
    gp::Vector<CachedLibraryEntry> m_cacheEntries{};
};

}   // namespace gp::platform::generic
