// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/system/SharedLibrarySearchRegistry.hpp"
#include "diagnostics/Assert.hpp"   // IWYU pragma: keep
#include "hashing/HashingEnums.hpp"
#include "hashing/PathHash.hpp"
#include "logging/LogCategory.hpp"
#include "logging/LogMacros.hpp"
#include "platforms/base/system/FileEnums.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/FileTypes.hpp"
#include "platforms/base/system/PathEnums.hpp"
#include "platforms/base/system/Paths.hpp"
#include "profiling/Profiler.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <utility>

namespace gp::platform::generic
{

SharedLibrarySearchRegistry::SharedLibrarySearchRegistry(const SharedLibrarySearchPolicy& policy) noexcept
    : m_policy(policy)
{}

bool SharedLibrarySearchRegistry::normalizeSearchDirectory(
    const gp::StringView directory, PathBuffer& outNormalized
) noexcept
{
    outNormalized.clear();
    if (directory.isEmpty() || directory.size() > kMaxPathLength)
    {
        return false;
    }

    outNormalized = gp::platform::Paths::resolve(directory);
    gp::platform::Paths::normalizeDirectoryName(outNormalized);
    return !outNormalized.isEmpty();
}

gp::UInt64 SharedLibrarySearchRegistry::computeHash(const gp::StringView text) const noexcept
{
    if (m_policy.caseSensitive)
    {
        return gp::hashing::hashPath<gp::hashing::Fnv1a64>(text, gp::hashing::HashCase::CaseSensitive);
    }

    return gp::hashing::PathHash64(text).value();
}

bool SharedLibrarySearchRegistry::pathsEqual(const gp::StringView lhs, const gp::StringView rhs) const noexcept
{
    return gp::platform::Paths::equals(lhs, rhs, m_policy.caseSensitive);
}

bool SharedLibrarySearchRegistry::namesEqual(const gp::StringView lhs, const gp::StringView rhs) const noexcept
{
    if (m_policy.caseSensitive)
    {
        return lhs == rhs;
    }

    return gp::platform::Paths::compareCaseInsensitive(lhs, rhs) == 0;
}

bool SharedLibrarySearchRegistry::matchesFileFilter(const gp::StringView fileName) const noexcept
{
    if (fileName.isEmpty())
    {
        return false;
    }

    if (m_policy.fileFilter != nullptr)
    {
        return m_policy.fileFilter(fileName);
    }

    if (m_policy.defaultExtension.isEmpty())
    {
        return true;
    }

    gp::StringView targetExt = m_policy.defaultExtension;
    if (targetExt.startsWith('.'))
    {
        targetExt.removePrefix(1u);
    }

    const gp::StringView actualExt = gp::platform::Paths::getExtension(fileName, PathFlags::None);
    return namesEqual(actualExt, targetExt);
}

gp::UInt32 SharedLibrarySearchRegistry::findRegisteredDirectoryIndexLocked(
    const gp::UInt64 dirHash, const gp::StringView normalizedDir
) const noexcept
{
    const gp::USize count = m_searchPathHashes.size();
    for (gp::USize i = 0u; i < count; ++i)
    {
        if (m_searchPathHashes[i] == dirHash && pathsEqual(m_searchPaths[i].view(), normalizedDir))
        {
            return static_cast<gp::UInt32>(i);
        }
    }

    return kInvalidSearchDirectoryIndex;
}

bool SharedLibrarySearchRegistry::isCachedEntryPresentLocked(
    const gp::UInt64 nameHash, const gp::StringView fileName, const gp::UInt32 directoryIndex
) const noexcept
{
    const gp::USize count = m_cacheNameHashes.size();
    for (gp::USize i = 0u; i < count; ++i)
    {
        if (m_cacheNameHashes[i] == nameHash && m_cacheEntries[i].directoryIndex == directoryIndex &&
            namesEqual(m_cacheEntries[i].fileName.view(), fileName))
        {
            return true;
        }
    }

    return false;
}

void SharedLibrarySearchRegistry::appendUniqueCandidate(
    gp::InlineVector<PathBuffer, 16u>& candidates, const PathBuffer& candidatePath
) const noexcept
{
    if (candidatePath.isEmpty())
    {
        return;
    }

    const gp::StringView candidateView = candidatePath.view();
    for (gp::USize i = 0u; i < candidates.size(); ++i)
    {
        if (pathsEqual(candidates[i].view(), candidateView))
        {
            return;
        }
    }

    candidates.pushBack(candidatePath);
}

bool SharedLibrarySearchRegistry::addDirectory(const gp::StringView directory) noexcept
{
    GP_PROFILE_SCOPE_NC("SharedLibrarySearchRegistry::addDirectory", GP_U32_COLOR_ORANGE);

    PathBuffer normalizedDir{};
    if (!normalizeSearchDirectory(directory, normalizedDir))
    {
        GP_ENSURE_MSG(false, "SharedLibrary::addDirectory() called with an empty or invalid directory path.");
        return false;
    }

    const gp::StringView dirView = normalizedDir.view();
    const gp::UInt64 dirHash = computeHash(dirView);

    // Check for an existing registration before touching the filesystem.
    {
        const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        if (findRegisteredDirectoryIndexLocked(dirHash, dirView) != kInvalidSearchDirectoryIndex)
        {
            return true;
        }
    }

    // Enumerate immediate library files outside the mutex to avoid holding locks across OS I/O.
    gp::Vector<gp::String> discoveredFiles{};
    const auto collectLibraries = [this,
                                   &discoveredFiles](const DirectoryEntry& entry) noexcept -> DirectoryVisitControl
    {
        if (entry.isFile() && matchesFileFilter(entry.name))
        {
            discoveredFiles.emplaceBack(entry.name);
        }
        return DirectoryVisitControl::Continue;
    };
    (void)gp::platform::FileSystem::iterateDirectory(dirView, collectLibraries);

    // Commit the directory and its discovered files under the lock after re-checking deduplication.
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (findRegisteredDirectoryIndexLocked(dirHash, dirView) != kInvalidSearchDirectoryIndex)
    {
        return true;
    }

    const gp::UInt32 dirIndex = static_cast<gp::UInt32>(m_searchPaths.size());
    m_searchPathHashes.pushBack(dirHash);
    m_searchPaths.emplaceBack(dirView);

    const gp::USize discoveredCount = discoveredFiles.size();
    for (gp::USize i = 0u; i < discoveredCount; ++i)
    {
        const gp::StringView fileName = discoveredFiles[i].view();
        const gp::UInt64 nameHash = computeHash(fileName);
        m_cacheNameHashes.pushBack(nameHash);
        m_cacheEntries.pushBack(CachedLibraryEntry{ std::move(discoveredFiles[i]), dirIndex });
    }

    return true;
}

bool SharedLibrarySearchRegistry::pushDirectory(
    const gp::StringView directory, PathBuffer* const outNormalizedDir
) noexcept
{
    PathBuffer normalizedDir{};
    if (!normalizeSearchDirectory(directory, normalizedDir))
    {
        GP_ENSURE_MSG(false, "SharedLibrary::pushDirectory() called with an empty or invalid directory path.");
        return false;
    }

    {
        const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        m_searchPathsStack.emplaceBack(normalizedDir.view());
    }

    if (outNormalizedDir != nullptr)
    {
        *outNormalizedDir = normalizedDir;
    }
    return true;
}

bool SharedLibrarySearchRegistry::popDirectory(
    const gp::StringView directory, PathBuffer* const outNewActiveDir
) noexcept
{
    PathBuffer normalizedDir{};
    if (!normalizeSearchDirectory(directory, normalizedDir))
    {
        GP_ENSURE_MSG(false, "SharedLibrary::popDirectory() called with an empty or invalid directory path.");
        return false;
    }

    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    if (!GP_ENSURE_MSG(!m_searchPathsStack.isEmpty(), "Attempted to pop from an empty shared library directory stack."))
    {
        return false;
    }

    const gp::StringView currentTop = m_searchPathsStack.back().view();
    if (!GP_ENSURE_MSG(
            pathsEqual(currentTop, normalizedDir.view()), "Mismatch in pushDirectory/popDirectory operations."
        ))
    {
        GP_LOG_WARNING(
            gp::log::LogCore,
            "SharedLibrary::popDirectory() mismatch: expected top '{}', got '{}'.",
            currentTop,
            normalizedDir.view()
        );
        return false;
    }

    m_searchPathsStack.popBack();

    if (outNewActiveDir != nullptr)
    {
        if (!m_searchPathsStack.isEmpty())
        {
            outNewActiveDir->assign(m_searchPathsStack.back().view());
        }
        else
        {
            outNewActiveDir->clear();
        }
    }

    return true;
}

void SharedLibrarySearchRegistry::collectCandidates(
    const PathBuffer* const candidateNames, const gp::USize candidateCount, SharedLibraryCandidateBatch& outBatch
) const noexcept
{
    outBatch.prioritizedCandidates.clear();
    outBatch.fallbackDirs.clear();
    outBatch.hasActivePushedDir = false;

    if (candidateNames == nullptr || candidateCount == 0u)
    {
        return;
    }

    gp::InlineVector<gp::UInt64, 8u> candidateHashes{};
    for (gp::USize i = 0u; i < candidateCount; ++i)
    {
        candidateHashes.pushBack(computeHash(candidateNames[i].view()));
    }

    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    outBatch.hasActivePushedDir = !m_searchPathsStack.isEmpty();

    // Priority 1: Active LIFO stack directories, searched top to bottom.
    for (gp::USize stackIdx = m_searchPathsStack.size(); stackIdx > 0u; --stackIdx)
    {
        const gp::StringView stackDir = m_searchPathsStack[stackIdx - 1u].view();
        for (gp::USize nameIdx = 0u; nameIdx < candidateCount; ++nameIdx)
        {
            appendUniqueCandidate(
                outBatch.prioritizedCandidates, gp::platform::Paths::combine(stackDir, candidateNames[nameIdx].view())
            );
        }
    }

    // Priority 2: Pre-indexed SoA cache hits from registered directories.
    const gp::USize cacheCount = m_cacheNameHashes.size();
    for (gp::USize nameIdx = 0u; nameIdx < candidateCount; ++nameIdx)
    {
        const gp::UInt64 targetHash = candidateHashes[nameIdx];
        const gp::StringView targetName = candidateNames[nameIdx].view();

        for (gp::USize cacheIdx = 0u; cacheIdx < cacheCount; ++cacheIdx)
        {
            if (m_cacheNameHashes[cacheIdx] == targetHash &&
                namesEqual(m_cacheEntries[cacheIdx].fileName.view(), targetName))
            {
                const gp::UInt32 dirIdx = m_cacheEntries[cacheIdx].directoryIndex;
                if (dirIdx < m_searchPaths.size())
                {
                    const PathBuffer fullPath = gp::platform::Paths::combine(
                        m_searchPaths[dirIdx].view(), m_cacheEntries[cacheIdx].fileName.view()
                    );
                    appendUniqueCandidate(outBatch.prioritizedCandidates, fullPath);
                }
            }
        }
    }

    // Priority 3: Snapshot all registered directories for cache-miss disk probing.
    const gp::USize registeredCount = m_searchPaths.size();
    for (gp::USize i = 0u; i < registeredCount; ++i)
    {
        outBatch.fallbackDirs.pushBack(
            SharedLibraryFallbackDir{ PathBuffer(m_searchPaths[i].view()), static_cast<gp::UInt32>(i) }
        );
    }
}

void SharedLibrarySearchRegistry::registerDiscoveredFile(
    const gp::UInt32 directoryIndex, const gp::StringView expectedDirectory, const gp::StringView fileName
) noexcept
{
    if (fileName.isEmpty() || expectedDirectory.isEmpty())
    {
        return;
    }

    const gp::UInt64 nameHash = computeHash(fileName);
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    if (directoryIndex < m_searchPaths.size() && pathsEqual(m_searchPaths[directoryIndex].view(), expectedDirectory) &&
        !isCachedEntryPresentLocked(nameHash, fileName, directoryIndex))
    {
        m_cacheNameHashes.pushBack(nameHash);
        m_cacheEntries.pushBack(CachedLibraryEntry{ gp::String(fileName), directoryIndex });
    }
}

bool SharedLibrarySearchRegistry::hasActivePushedDirectory() const noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return !m_searchPathsStack.isEmpty();
}

gp::USize SharedLibrarySearchRegistry::getDirectoryStackDepth() const noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_searchPathsStack.size();
}

PathBuffer SharedLibrarySearchRegistry::getActiveDirectory() const noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (m_searchPathsStack.isEmpty())
    {
        return {};
    }

    return PathBuffer(m_searchPathsStack.back().view());
}

gp::USize SharedLibrarySearchRegistry::getRegisteredDirectoryCount() const noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_searchPaths.size();
}

gp::USize SharedLibrarySearchRegistry::getCachedLibraryCount() const noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_cacheEntries.size();
}

void SharedLibrarySearchRegistry::reset() noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_searchPathsStack.clear();
    m_searchPathHashes.clear();
    m_searchPaths.clear();
    m_cacheNameHashes.clear();
    m_cacheEntries.clear();
}

}   // namespace gp::platform::generic
