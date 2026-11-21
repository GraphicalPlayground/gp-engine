// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "vfs/VirtualFileSystem.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "containers/strings/FixedString.hpp"
#include "hashing/Fnv1a.hpp"
#include "logging/LogFormatter.hpp"
#include "platforms/base/system/Paths.hpp"
#include "threading/sync/SharedLock.hpp"
#include "threading/sync/SharedMutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "vfs/PhysicalMountBackend.hpp"

namespace gp::vfs
{

namespace detail
{

/// @brief Internal slot inside the zero-allocation `.bss` VFS mount table.
struct MountTableSlot
{
    SchemeBuffer scheme{};
    IMountBackend* backend{ nullptr };
    MountPriority priority{ MountPriority::Base };
    MountAccess access{ MountAccess::ReadOnly };
    gp::UInt32 registrationOrder{ 0u };
    bool ownsPhysicalSlot{ false };
    gp::USize physicalSlotIndex{ 0u };
    bool active{ false };
};

/// @brief Zero-allocation `.bss` state for `gp::vfs::VirtualFileSystem`.
struct VfsGlobalState
{
    gp::thread::SharedMutex mutex{};
    MountTableSlot mounts[kMaxMountPoints]{};
    PhysicalMountBackend physicalBackends[kMaxMountPoints]{};
    bool physicalSlotUsed[kMaxMountPoints]{};
    gp::UInt32 nextRegistrationOrder{ 1u };
};

/// @brief Returns the function-local static `.bss` VFS state instance.
/// @return Reference to `VfsGlobalState`.
[[nodiscard]] static VfsGlobalState& getState() noexcept
{
    static VfsGlobalState s_state{};
    return s_state;
}

/// @brief Gathers active mount slots matching `normalizedScheme` sorted in descending priority order (and descending
///        registration order as a tie-breaker so later mounts at the same priority win).
/// @param[in] state Reference to `VfsGlobalState` (caller must hold shared or exclusive `state.mutex`).
/// @param[in] normalizedScheme Canonical lowercase scheme (e.g., `"game://"`).
/// @param[out] outIndices Receives sorted slot indices into `state.mounts`.
/// @return Number of matching mounts written to `outIndices`.
[[nodiscard]] static gp::USize collectSortedMountsForSchemeUnlocked(
    const VfsGlobalState& state, const gp::StringView normalizedScheme, gp::USize (&outIndices)[kMaxMountPoints]
) noexcept
{
    gp::USize count = 0u;
    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active && state.mounts[i].backend != nullptr &&
            state.mounts[i].scheme.view() == normalizedScheme)
        {
            outIndices[count] = i;
            ++count;
        }
    }

    // Insertion sort by descending priority, then descending registrationOrder (most recent mount wins ties).
    for (gp::USize i = 1u; i < count; ++i)
    {
        const gp::USize keyIdx = outIndices[i];
        const gp::UInt16 keyPrio = static_cast<gp::UInt16>(state.mounts[keyIdx].priority);
        const gp::UInt32 keyOrder = state.mounts[keyIdx].registrationOrder;

        gp::USize j = i;
        while (j > 0u)
        {
            const gp::USize prevIdx = outIndices[j - 1u];
            const gp::UInt16 prevPrio = static_cast<gp::UInt16>(state.mounts[prevIdx].priority);
            const gp::UInt32 prevOrder = state.mounts[prevIdx].registrationOrder;

            if (keyPrio > prevPrio || (keyPrio == prevPrio && keyOrder > prevOrder))
            {
                outIndices[j] = prevIdx;
                --j;
            }
            else
            {
                break;
            }
        }
        outIndices[j] = keyIdx;
    }

    return count;
}

/// @brief Finds the highest-priority `MountAccess::ReadWrite` backend for `normalizedScheme` (caller holds lock).
/// @param[in] state Reference to `VfsGlobalState`.
/// @param[in] normalizedScheme Canonical lowercase scheme.
/// @param[out] outError Receives `SchemeNotMounted` if no mounts exist for the scheme, or `ReadOnlyMount` if all
///                      mounts for the scheme are `ReadOnly`.
/// @return Pointer to the winning writable `IMountBackend`, or `nullptr` on error.
[[nodiscard]] static IMountBackend* findWritableBackendUnlocked(
    const VfsGlobalState& state, const gp::StringView normalizedScheme, VfsError& outError
) noexcept
{
    gp::USize sorted[kMaxMountPoints]{};
    const gp::USize count = collectSortedMountsForSchemeUnlocked(state, normalizedScheme, sorted);
    if (count == 0u)
    {
        outError = VfsError::SchemeNotMounted;
        return nullptr;
    }

    for (gp::USize i = 0u; i < count; ++i)
    {
        const MountTableSlot& slot = state.mounts[sorted[i]];
        if (slot.access == MountAccess::ReadWrite)
        {
            outError = VfsError::None;
            return slot.backend;
        }
    }

    outError = VfsError::ReadOnlyMount;
    return nullptr;
}

/// @brief Releases a mount slot and its owned `PhysicalMountBackend` slot if applicable (caller holds exclusive lock).
/// @param[in,out] state Reference to `VfsGlobalState`.
/// @param[in] slotIndex Index in `state.mounts` to clear.
static void releaseMountSlotUnlocked(VfsGlobalState& state, const gp::USize slotIndex) noexcept
{
    MountTableSlot& slot = state.mounts[slotIndex];
    if (slot.ownsPhysicalSlot && slot.physicalSlotIndex < kMaxMountPoints)
    {
        state.physicalBackends[slot.physicalSlotIndex].setRootPath("");
        state.physicalSlotUsed[slot.physicalSlotIndex] = false;
    }

    slot.scheme.clear();
    slot.backend = nullptr;
    slot.priority = MountPriority::Base;
    slot.access = MountAccess::ReadOnly;
    slot.registrationOrder = 0u;
    slot.ownsPhysicalSlot = false;
    slot.physicalSlotIndex = 0u;
    slot.active = false;
}

/// @brief Formats a canonical VFS URI from a scheme (`"game://"`) and relative path (`"textures/hero.ktx2"`).
/// @param[in] scheme Canonical scheme including `"://"`.
/// @param[in] relativePath Normalized relative path without leading slash.
/// @return Combined `PathBuffer` holding `"scheme://relativePath"`.
[[nodiscard]] static gp::PathBuffer
    buildCanonicalUri(const gp::StringView scheme, const gp::StringView relativePath) noexcept
{
    gp::PathBuffer uri(scheme);
    gp::StringView rel = relativePath;
    while (!rel.isEmpty() && gp::platform::Paths::isSeparator(rel.front()))
    {
        rel = rel.substr(1u);
    }
    uri.append(rel);
    return uri;
}

/// @brief Recursive implementation for `VirtualFileSystem::iterateDirectoryRecursive`.
/// @param[in] directoryUri Current VFS directory URI.
/// @param[in] visitor User callback.
/// @param[in] userData User context pointer.
/// @param[in] depth Current recursion depth (`0` at root).
/// @param[out] outStopped Set to true if `visitor` returned `DirectoryVisitControl::Stop`.
/// @return `Expected<bool, VfsError>`.
[[nodiscard]] static gp::Expected<bool, VfsError> iterateDirectoryRecursiveInternal(
    const gp::StringView directoryUri,
    const VfsDirectoryVisitorFn visitor,
    void* const userData,
    const gp::UInt32 depth,
    bool& outStopped
) noexcept
{
    if (visitor == nullptr)
    {
        return gp::makeUnexpected(VfsError::InvalidArgument);
    }
    if (depth > gp::platform::kMaxDirectoryRecursionDepth)
    {
        return gp::makeUnexpected(VfsError::MaxRecursionDepthExceeded);
    }

    struct VfsRecursiveState
    {
        VfsDirectoryVisitorFn userVisitor{ nullptr };
        void* userContext{ nullptr };
        gp::UInt32 currentDepth{ 0u };
        VfsError childError{ VfsError::None };
        bool stopped{ false };
    };

    VfsRecursiveState state{};
    state.userVisitor = visitor;
    state.userContext = userData;
    state.currentDepth = depth;

    const VfsDirectoryVisitorFn stepVisitor = [](const VfsDirectoryEntry& rawEntry,
                                                 void* const rawState) -> gp::platform::DirectoryVisitControl
    {
        VfsRecursiveState* const walk = static_cast<VfsRecursiveState*>(rawState);
        VfsDirectoryEntry entryWithDepth = rawEntry;
        entryWithDepth.depth = walk->currentDepth;

        const gp::platform::DirectoryVisitControl ctrl = walk->userVisitor(entryWithDepth, walk->userContext);
        if (ctrl == gp::platform::DirectoryVisitControl::Stop)
        {
            walk->stopped = true;
            return gp::platform::DirectoryVisitControl::Stop;
        }

        if (entryWithDepth.isDirectory() && !entryWithDepth.stats.isSymbolicLink() &&
            ctrl != gp::platform::DirectoryVisitControl::SkipSubdirectory)
        {
            const gp::Expected<bool, VfsError> childRes = iterateDirectoryRecursiveInternal(
                entryWithDepth.uri, walk->userVisitor, walk->userContext, walk->currentDepth + 1u, walk->stopped
            );
            if (!childRes.hasValue())
            {
                walk->childError = childRes.error();
                return gp::platform::DirectoryVisitControl::Stop;
            }
            if (walk->stopped)
            {
                return gp::platform::DirectoryVisitControl::Stop;
            }
        }

        return gp::platform::DirectoryVisitControl::Continue;
    };

    const gp::Expected<bool, VfsError> iterRes = VirtualFileSystem::iterateDirectory(directoryUri, stepVisitor, &state);
    if (!iterRes.hasValue())
    {
        return iterRes;
    }
    if (state.childError != VfsError::None)
    {
        return gp::makeUnexpected(state.childError);
    }

    outStopped = state.stopped;
    return true;
}

}   // namespace detail

gp::Expected<bool, VfsError> VirtualFileSystem::initializeDefaultMounts(const gp::StringView appName) noexcept
{
    const gp::StringView effectiveApp = !appName.isEmpty() ? appName
                                                           : (!gp::platform::Paths::executableName(true).isEmpty()
                                                                  ? gp::platform::Paths::executableName(true)
                                                                  : gp::StringView("gp-engine"));

    const gp::PathBuffer engineRoot = gp::platform::Paths::combine(gp::platform::Paths::baseDir(), "engine");
    const gp::PathBuffer gameRoot = gp::platform::Paths::combine(gp::platform::Paths::baseDir(), "content");
    const gp::PathBuffer userRoot = gp::platform::Paths::combine(gp::platform::Paths::documentsDir(), effectiveApp);
    const gp::PathBuffer appDataRoot = gp::platform::Paths::combine(gp::platform::Paths::appDataDir(), effectiveApp);
    const gp::PathBuffer cacheRoot =
        gp::platform::Paths::combine(gp::platform::Paths::localAppDataDir(), effectiveApp, "cache");
    const gp::PathBuffer tempRoot = gp::platform::Paths::combine(gp::platform::Paths::tempDir(), effectiveApp);

    const gp::Expected<bool, VfsError> r1 =
        mount(kEngineScheme, engineRoot.view(), MountPriority::Base, MountAccess::ReadOnly);
    if (!r1.hasValue())
    {
        return r1;
    }

    const gp::Expected<bool, VfsError> r2 =
        mount(kGameScheme, gameRoot.view(), MountPriority::Base, MountAccess::ReadOnly);
    if (!r2.hasValue())
    {
        return r2;
    }

    const gp::Expected<bool, VfsError> r3 =
        mount(kUserScheme, userRoot.view(), MountPriority::Base, MountAccess::ReadWrite);
    if (!r3.hasValue())
    {
        return r3;
    }

    const gp::Expected<bool, VfsError> r4 =
        mount(kAppDataScheme, appDataRoot.view(), MountPriority::Base, MountAccess::ReadWrite);
    if (!r4.hasValue())
    {
        return r4;
    }

    const gp::Expected<bool, VfsError> r5 =
        mount(kCacheScheme, cacheRoot.view(), MountPriority::Base, MountAccess::ReadWrite);
    if (!r5.hasValue())
    {
        return r5;
    }

    const gp::Expected<bool, VfsError> r6 =
        mount(kTempScheme, tempRoot.view(), MountPriority::Base, MountAccess::ReadWrite);
    if (!r6.hasValue())
    {
        return r6;
    }

    return true;
}

void VirtualFileSystem::shutdown() noexcept
{
    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(state.mutex);

    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active)
        {
            detail::releaseMountSlotUnlocked(state, i);
        }
    }
    state.nextRegistrationOrder = 1u;
}

bool VirtualFileSystem::normalizeScheme(const gp::StringView rawScheme, SchemeBuffer& outScheme) noexcept
{
    outScheme.clear();
    if (rawScheme.isEmpty())
    {
        return false;
    }

    gp::StringView trimmed = rawScheme;
    if (trimmed.endsWith("://"))
    {
        trimmed = trimmed.substr(0u, trimmed.size() - 3u);
    }
    else if (trimmed.endsWith(":"))
    {
        trimmed = trimmed.substr(0u, trimmed.size() - 1u);
    }

    // Reject single-character schemes (e.g., Windows drive letters "C:") or schemes exceeding buffer limits
    if (trimmed.size() < 2u || (trimmed.size() + 3u) > kMaxSchemeLength)
    {
        return false;
    }
    if (!gp::platform::Paths::isAsciiAlpha(trimmed[0]))
    {
        return false;
    }

    for (gp::USize i = 0u; i < trimmed.size(); ++i)
    {
        const char ch = trimmed[i];
        if (!gp::platform::Paths::isAsciiAlphaNumeric(ch) && ch != '+' && ch != '-' && ch != '.')
        {
            return false;
        }
        outScheme.pushBack(gp::platform::Paths::toLowerAscii(ch));
    }

    outScheme.append("://");
    return true;
}

gp::Expected<ParsedUri, VfsError> VirtualFileSystem::parseUri(const gp::StringView uri) noexcept
{
    if (uri.isEmpty() || uri.size() > gp::platform::kMaxPathLength)
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }

    const gp::USize schemeDelimPos = uri.find("://");
    if (schemeDelimPos == gp::StringView::npos || schemeDelimPos < 2u)
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }

    ParsedUri parsed{};
    if (!normalizeScheme(uri.substr(0u, schemeDelimPos + 3u), parsed.scheme))
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }

    gp::StringView rawSubPath = uri.substr(schemeDelimPos + 3u);
    while (!rawSubPath.isEmpty() && gp::platform::Paths::isSeparator(rawSubPath.front()))
    {
        rawSubPath = rawSubPath.substr(1u);
    }

    if (rawSubPath.isEmpty())
    {
        return parsed;
    }

    // Reject drive specifiers or embedded colons inside the relative sub-path (e.g. "game://C:/Windows")
    for (gp::USize i = 0u; i < rawSubPath.size(); ++i)
    {
        if (rawSubPath[i] == ':')
        {
            return gp::makeUnexpected(VfsError::SandboxEscapeViolation);
        }
    }

    gp::PathBuffer collapsed(rawSubPath);
    gp::platform::Paths::normalizeDirectoryName(collapsed);
    if (!gp::platform::Paths::collapseRelativeDirectories(collapsed))
    {
        return gp::makeUnexpected(VfsError::SandboxEscapeViolation);
    }

    gp::StringView cleanView = collapsed.view();
    while (!cleanView.isEmpty() && gp::platform::Paths::isSeparator(cleanView.front()))
    {
        cleanView = cleanView.substr(1u);
    }
    while (!cleanView.isEmpty() && gp::platform::Paths::isSeparator(cleanView.back()))
    {
        cleanView = cleanView.substr(0u, cleanView.size() - 1u);
    }

    if (cleanView == ".." || cleanView.startsWith("../"))
    {
        return gp::makeUnexpected(VfsError::SandboxEscapeViolation);
    }
    if (cleanView == ".")
    {
        cleanView = {};
    }

    parsed.relativePath.assign(cleanView);
    return parsed;
}

gp::Expected<bool, VfsError> VirtualFileSystem::mount(
    const gp::StringView scheme,
    const gp::StringView physicalRootDir,
    const MountPriority priority,
    const MountAccess access
) noexcept
{
    SchemeBuffer normScheme{};
    if (!normalizeScheme(scheme, normScheme) || physicalRootDir.isEmpty())
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }

    gp::PathBuffer normalizedRoot(physicalRootDir);
    gp::platform::Paths::makeStandardFilename(normalizedRoot);
    gp::platform::Paths::ensureTrailingSeparator(normalizedRoot);

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(state.mutex);

    // If an identical scheme + physical root mount already exists, update its priority and access in place.
    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active && state.mounts[i].scheme.view() == normScheme.view() &&
            state.mounts[i].backend != nullptr &&
            gp::platform::Paths::equals(state.mounts[i].backend->rootPath(), normalizedRoot.view()))
        {
            state.mounts[i].priority = priority;
            state.mounts[i].access = access;
            state.mounts[i].registrationOrder = state.nextRegistrationOrder++;
            return true;
        }
    }

    gp::USize mountSlot = kMaxMountPoints;
    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (!state.mounts[i].active)
        {
            mountSlot = i;
            break;
        }
    }

    gp::USize physSlot = kMaxMountPoints;
    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (!state.physicalSlotUsed[i])
        {
            physSlot = i;
            break;
        }
    }

    if (mountSlot >= kMaxMountPoints || physSlot >= kMaxMountPoints)
    {
        return gp::makeUnexpected(VfsError::MountTableFull);
    }

    if (!state.physicalBackends[physSlot].setRootPath(normalizedRoot.view()))
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }
    state.physicalSlotUsed[physSlot] = true;

    detail::MountTableSlot& slot = state.mounts[mountSlot];
    slot.scheme = normScheme;
    slot.backend = &state.physicalBackends[physSlot];
    slot.priority = priority;
    slot.access = access;
    slot.registrationOrder = state.nextRegistrationOrder++;
    slot.ownsPhysicalSlot = true;
    slot.physicalSlotIndex = physSlot;
    slot.active = true;

    return true;
}

gp::Expected<bool, VfsError> VirtualFileSystem::mountBackend(
    const gp::StringView scheme, IMountBackend* const backend, const MountPriority priority, const MountAccess access
) noexcept
{
    if (backend == nullptr)
    {
        return gp::makeUnexpected(VfsError::InvalidArgument);
    }

    SchemeBuffer normScheme{};
    if (!normalizeScheme(scheme, normScheme))
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(state.mutex);

    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active && state.mounts[i].scheme.view() == normScheme.view() &&
            state.mounts[i].backend == backend)
        {
            state.mounts[i].priority = priority;
            state.mounts[i].access = access;
            state.mounts[i].registrationOrder = state.nextRegistrationOrder++;
            return true;
        }
    }

    gp::USize mountSlot = kMaxMountPoints;
    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (!state.mounts[i].active)
        {
            mountSlot = i;
            break;
        }
    }

    if (mountSlot >= kMaxMountPoints)
    {
        return gp::makeUnexpected(VfsError::MountTableFull);
    }

    detail::MountTableSlot& slot = state.mounts[mountSlot];
    slot.scheme = normScheme;
    slot.backend = backend;
    slot.priority = priority;
    slot.access = access;
    slot.registrationOrder = state.nextRegistrationOrder++;
    slot.ownsPhysicalSlot = false;
    slot.physicalSlotIndex = 0u;
    slot.active = true;

    return true;
}

bool VirtualFileSystem::unmount(const gp::StringView scheme, const gp::StringView rootPath) noexcept
{
    SchemeBuffer normScheme{};
    if (!normalizeScheme(scheme, normScheme) || rootPath.isEmpty())
    {
        return false;
    }

    gp::PathBuffer normalizedTarget(rootPath);
    gp::platform::Paths::makeStandardFilename(normalizedTarget);
    gp::platform::Paths::ensureTrailingSeparator(normalizedTarget);

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(state.mutex);

    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active && state.mounts[i].scheme.view() == normScheme.view() &&
            state.mounts[i].backend != nullptr)
        {
            const gp::StringView backendRoot = state.mounts[i].backend->rootPath();
            if (backendRoot == rootPath || gp::platform::Paths::equals(backendRoot, normalizedTarget.view()))
            {
                detail::releaseMountSlotUnlocked(state, i);
                return true;
            }
        }
    }

    return false;
}

bool VirtualFileSystem::unmountBackend(const gp::StringView scheme, const IMountBackend* const backend) noexcept
{
    SchemeBuffer normScheme{};
    if (backend == nullptr || !normalizeScheme(scheme, normScheme))
    {
        return false;
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(state.mutex);

    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active && state.mounts[i].scheme.view() == normScheme.view() &&
            state.mounts[i].backend == backend)
        {
            detail::releaseMountSlotUnlocked(state, i);
            return true;
        }
    }

    return false;
}

gp::USize VirtualFileSystem::unmountScheme(const gp::StringView scheme) noexcept
{
    SchemeBuffer normScheme{};
    if (!normalizeScheme(scheme, normScheme))
    {
        return 0u;
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize removedCount = 0u;
    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active && state.mounts[i].scheme.view() == normScheme.view())
        {
            detail::releaseMountSlotUnlocked(state, i);
            ++removedCount;
        }
    }
    return removedCount;
}

bool VirtualFileSystem::isSchemeMounted(const gp::StringView scheme) noexcept
{
    return getMountCount(scheme) > 0u;
}

gp::USize VirtualFileSystem::getMountCount(const gp::StringView scheme) noexcept
{
    SchemeBuffer normScheme{};
    if (!scheme.isEmpty() && !normalizeScheme(scheme, normScheme))
    {
        return 0u;
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize count = 0u;
    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active)
        {
            if (normScheme.isEmpty() || state.mounts[i].scheme.view() == normScheme.view())
            {
                ++count;
            }
        }
    }
    return count;
}

gp::USize VirtualFileSystem::enumerateMounts(
    const gp::StringView scheme, const MountPointVisitorFn visitor, void* const userData
) noexcept
{
    if (visitor == nullptr)
    {
        return 0u;
    }

    SchemeBuffer normScheme{};
    if (!scheme.isEmpty() && !normalizeScheme(scheme, normScheme))
    {
        return 0u;
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize matchingIndices[kMaxMountPoints]{};
    gp::USize count = 0u;
    for (gp::USize i = 0u; i < kMaxMountPoints; ++i)
    {
        if (state.mounts[i].active && state.mounts[i].backend != nullptr)
        {
            if (normScheme.isEmpty() || state.mounts[i].scheme.view() == normScheme.view())
            {
                matchingIndices[count] = i;
                ++count;
            }
        }
    }

    // Sort deterministically by ascending scheme name, then descending priority, then descending registration order.
    for (gp::USize i = 1u; i < count; ++i)
    {
        const gp::USize keyIdx = matchingIndices[i];
        const detail::MountTableSlot& keySlot = state.mounts[keyIdx];
        const gp::UInt16 keyPrio = static_cast<gp::UInt16>(keySlot.priority);

        gp::USize j = i;
        while (j > 0u)
        {
            const gp::USize prevIdx = matchingIndices[j - 1u];
            const detail::MountTableSlot& prevSlot = state.mounts[prevIdx];
            const gp::Int32 schemeCmp = keySlot.scheme.view().compare(prevSlot.scheme.view());
            const gp::UInt16 prevPrio = static_cast<gp::UInt16>(prevSlot.priority);

            const bool shouldComeBefore =
                (schemeCmp < 0) ||
                (schemeCmp == 0 && (keyPrio > prevPrio ||
                                    (keyPrio == prevPrio && keySlot.registrationOrder > prevSlot.registrationOrder)));
            if (shouldComeBefore)
            {
                matchingIndices[j] = prevIdx;
                --j;
            }
            else
            {
                break;
            }
        }
        matchingIndices[j] = keyIdx;
    }

    gp::USize visited = 0u;
    MountPointDesc desc{};
    for (gp::USize i = 0u; i < count; ++i)
    {
        const detail::MountTableSlot& slot = state.mounts[matchingIndices[i]];
        desc.scheme.assign(slot.scheme.view());
        desc.rootPath.assign(slot.backend->rootPath());
        desc.priority = slot.priority;
        desc.access = slot.access;

        ++visited;
        if (!visitor(desc, userData))
        {
            break;
        }
    }

    return visited;
}

gp::Expected<gp::PathBuffer, VfsError> VirtualFileSystem::resolvePhysicalPath(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize sorted[kMaxMountPoints]{};
    const gp::USize count = detail::collectSortedMountsForSchemeUnlocked(state, parsed.value().scheme.view(), sorted);
    if (count == 0u)
    {
        return gp::makeUnexpected(VfsError::SchemeNotMounted);
    }

    IMountBackend* highestPhysical = nullptr;
    for (gp::USize i = 0u; i < count; ++i)
    {
        IMountBackend* const backend = state.mounts[sorted[i]].backend;
        if (backend->isPhysical())
        {
            if (highestPhysical == nullptr)
            {
                highestPhysical = backend;
            }
            if (backend->exists(parsed.value().relativePath.view()))
            {
                return backend->resolvePhysicalPath(parsed.value().relativePath.view());
            }
        }
    }

    if (highestPhysical != nullptr)
    {
        return highestPhysical->resolvePhysicalPath(parsed.value().relativePath.view());
    }

    return gp::makeUnexpected(VfsError::NotSupported);
}

gp::Expected<gp::PathBuffer, VfsError> VirtualFileSystem::resolveWritablePhysicalPath(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->resolvePhysicalPath(parsed.value().relativePath.view());
}

bool VirtualFileSystem::exists(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return false;
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize sorted[kMaxMountPoints]{};
    const gp::USize count = detail::collectSortedMountsForSchemeUnlocked(state, parsed.value().scheme.view(), sorted);
    for (gp::USize i = 0u; i < count; ++i)
    {
        if (state.mounts[sorted[i]].backend->exists(parsed.value().relativePath.view()))
        {
            return true;
        }
    }
    return false;
}

bool VirtualFileSystem::isFile(const gp::StringView uri) noexcept
{
    const gp::Expected<gp::platform::FileStats, VfsError> stats = getStats(uri);
    return stats.hasValue() && stats.value().isFile();
}

bool VirtualFileSystem::isDirectory(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return false;
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize sorted[kMaxMountPoints]{};
    const gp::USize count = detail::collectSortedMountsForSchemeUnlocked(state, parsed.value().scheme.view(), sorted);
    for (gp::USize i = 0u; i < count; ++i)
    {
        const gp::Expected<gp::platform::FileStats, VfsError> stats =
            state.mounts[sorted[i]].backend->getStats(parsed.value().relativePath.view());
        if (stats.hasValue())
        {
            return stats.value().isDirectory();
        }
    }
    return false;
}

gp::Expected<gp::platform::FileStats, VfsError> VirtualFileSystem::getStats(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize sorted[kMaxMountPoints]{};
    const gp::USize count = detail::collectSortedMountsForSchemeUnlocked(state, parsed.value().scheme.view(), sorted);
    if (count == 0u)
    {
        return gp::makeUnexpected(VfsError::SchemeNotMounted);
    }

    for (gp::USize i = 0u; i < count; ++i)
    {
        const gp::Expected<gp::platform::FileStats, VfsError> stats =
            state.mounts[sorted[i]].backend->getStats(parsed.value().relativePath.view());
        if (stats.hasValue())
        {
            return stats.value();
        }
    }

    return gp::makeUnexpected(VfsError::NotFound);
}

gp::Expected<gp::UInt64, VfsError> VirtualFileSystem::fileSize(const gp::StringView uri) noexcept
{
    const gp::Expected<gp::platform::FileStats, VfsError> stats = getStats(uri);
    if (!stats.hasValue())
    {
        return gp::makeUnexpected(stats.error());
    }
    if (stats.value().isDirectory())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }
    return stats.value().size;
}

gp::Expected<gp::platform::FileHandle, VfsError>
    VirtualFileSystem::openRead(const gp::StringView uri, const gp::platform::FileShareMode shareMode) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }
    if (parsed.value().isSchemeRoot())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize sorted[kMaxMountPoints]{};
    const gp::USize count = detail::collectSortedMountsForSchemeUnlocked(state, parsed.value().scheme.view(), sorted);
    if (count == 0u)
    {
        return gp::makeUnexpected(VfsError::SchemeNotMounted);
    }

    VfsError lastErr = VfsError::NotFound;
    for (gp::USize i = 0u; i < count; ++i)
    {
        IMountBackend* const backend = state.mounts[sorted[i]].backend;
        if (!backend->exists(parsed.value().relativePath.view()))
        {
            continue;
        }

        gp::Expected<gp::platform::FileHandle, VfsError> openRes = backend->openFileHandle(
            parsed.value().relativePath.view(),
            gp::platform::FileAccessMode::Read,
            gp::platform::FileOpenMode::OpenExisting,
            shareMode
        );
        if (openRes.hasValue())
        {
            return std::move(openRes.value());
        }
        lastErr = openRes.error();
        if (lastErr != VfsError::NotFound)
        {
            break;
        }
    }

    return gp::makeUnexpected(lastErr);
}

gp::Expected<gp::platform::FileHandle, VfsError> VirtualFileSystem::openWrite(
    const gp::StringView uri, const bool append, const bool allowRead, const gp::platform::FileShareMode shareMode
) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }
    if (parsed.value().isSchemeRoot())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    const gp::platform::FileAccessMode access =
        allowRead ? gp::platform::FileAccessMode::ReadWrite : gp::platform::FileAccessMode::Write;
    const gp::platform::FileOpenMode mode =
        append ? gp::platform::FileOpenMode::Append : gp::platform::FileOpenMode::CreateAlways;

    return writable->openFileHandle(parsed.value().relativePath.view(), access, mode, shareMode);
}

gp::Expected<bool, VfsError> VirtualFileSystem::createDirectory(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->createDirectory(parsed.value().relativePath.view());
}

gp::Expected<bool, VfsError> VirtualFileSystem::createDirectoryTree(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->createDirectoryTree(parsed.value().relativePath.view());
}

gp::Expected<bool, VfsError> VirtualFileSystem::removeFile(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->removeFile(parsed.value().relativePath.view());
}

gp::Expected<bool, VfsError> VirtualFileSystem::removeDirectory(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->removeDirectory(parsed.value().relativePath.view());
}

gp::Expected<bool, VfsError> VirtualFileSystem::removeDirectoryTree(const gp::StringView uri) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->removeDirectoryTree(parsed.value().relativePath.view());
}

gp::Expected<bool, VfsError> VirtualFileSystem::move(
    const gp::StringView sourceUri, const gp::StringView destinationUri, const bool overwrite
) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsedSrc = parseUri(sourceUri);
    if (!parsedSrc.hasValue())
    {
        return gp::makeUnexpected(parsedSrc.error());
    }
    const gp::Expected<ParsedUri, VfsError> parsedDst = parseUri(destinationUri);
    if (!parsedDst.hasValue())
    {
        return gp::makeUnexpected(parsedDst.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError srcErr = VfsError::None;
    IMountBackend* const srcWritable =
        detail::findWritableBackendUnlocked(state, parsedSrc.value().scheme.view(), srcErr);
    if (srcWritable == nullptr)
    {
        return gp::makeUnexpected(srcErr);
    }

    VfsError dstErr = VfsError::None;
    IMountBackend* const dstWritable =
        detail::findWritableBackendUnlocked(state, parsedDst.value().scheme.view(), dstErr);
    if (dstWritable == nullptr)
    {
        return gp::makeUnexpected(dstErr);
    }

    if (srcWritable != dstWritable)
    {
        return gp::makeUnexpected(VfsError::CrossMountOperation);
    }

    return srcWritable->move(parsedSrc.value().relativePath.view(), parsedDst.value().relativePath.view(), overwrite);
}

gp::Expected<bool, VfsError> VirtualFileSystem::copyFile(
    const gp::StringView sourceUri, const gp::StringView destinationUri, const bool overwrite
) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsedSrc = parseUri(sourceUri);
    if (!parsedSrc.hasValue())
    {
        return gp::makeUnexpected(parsedSrc.error());
    }
    const gp::Expected<ParsedUri, VfsError> parsedDst = parseUri(destinationUri);
    if (!parsedDst.hasValue())
    {
        return gp::makeUnexpected(parsedDst.error());
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize sortedSrc[kMaxMountPoints]{};
    const gp::USize srcCount =
        detail::collectSortedMountsForSchemeUnlocked(state, parsedSrc.value().scheme.view(), sortedSrc);
    if (srcCount == 0u)
    {
        return gp::makeUnexpected(VfsError::SchemeNotMounted);
    }

    IMountBackend* srcBackend = nullptr;
    for (gp::USize i = 0u; i < srcCount; ++i)
    {
        if (state.mounts[sortedSrc[i]].backend->exists(parsedSrc.value().relativePath.view()))
        {
            srcBackend = state.mounts[sortedSrc[i]].backend;
            break;
        }
    }
    if (srcBackend == nullptr)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }

    VfsError dstErr = VfsError::None;
    IMountBackend* const dstWritable =
        detail::findWritableBackendUnlocked(state, parsedDst.value().scheme.view(), dstErr);
    if (dstWritable == nullptr)
    {
        return gp::makeUnexpected(dstErr);
    }

    if (srcBackend == dstWritable)
    {
        return dstWritable->copyFile(
            parsedSrc.value().relativePath.view(), parsedDst.value().relativePath.view(), overwrite
        );
    }

    if (!overwrite && dstWritable->exists(parsedDst.value().relativePath.view()))
    {
        return gp::makeUnexpected(VfsError::AlreadyExists);
    }

    const gp::Expected<gp::platform::FileStats, VfsError> srcStats =
        srcBackend->getStats(parsedSrc.value().relativePath.view());
    if (!srcStats.hasValue())
    {
        return gp::makeUnexpected(srcStats.error());
    }
    if (srcStats.value().isDirectory())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::UInt64 totalBytes = srcStats.value().size;
    if (totalBytes == 0ULL)
    {
        return dstWritable->writeBytes(parsedDst.value().relativePath.view(), nullptr, 0u, false);
    }

    // Cross-mount copy: if both source and destination are physical mounts, keep both handles open across all chunks
    // (or open a single destination handle when copying from a non-physical backend) instead of reopening per 16 KiB.
    if (dstWritable->isPhysical())
    {
        gp::Expected<gp::platform::FileHandle, VfsError> dstHandleRes = dstWritable->openFileHandle(
            parsedDst.value().relativePath.view(),
            gp::platform::FileAccessMode::Write,
            overwrite ? gp::platform::FileOpenMode::CreateAlways : gp::platform::FileOpenMode::CreateNew,
            gp::platform::FileShareMode::None
        );
        if (!dstHandleRes.hasValue())
        {
            return gp::makeUnexpected(dstHandleRes.error());
        }

        gp::platform::FileHandle& dstHandle = dstHandleRes.value();
        gp::Expected<gp::platform::FileHandle, VfsError> srcHandleRes = gp::makeUnexpected(VfsError::NotSupported);
        if (srcBackend->isPhysical())
        {
            srcHandleRes = srcBackend->openFileHandle(
                parsedSrc.value().relativePath.view(),
                gp::platform::FileAccessMode::Read,
                gp::platform::FileOpenMode::OpenExisting,
                gp::platform::FileShareMode::ReadWrite
            );
            if (!srcHandleRes.hasValue())
            {
                return gp::makeUnexpected(srcHandleRes.error());
            }
        }

        gp::Byte chunkBuf[gp::platform::kFileCopyChunkSize]{};
        gp::UInt64 offset = 0ULL;

        while (offset < totalBytes)
        {
            const gp::UInt64 remaining = totalBytes - offset;
            const gp::USize toRead = (remaining > static_cast<gp::UInt64>(gp::platform::kFileCopyChunkSize))
                                         ? gp::platform::kFileCopyChunkSize
                                         : static_cast<gp::USize>(remaining);

            gp::USize bytesRead = 0u;
            if (srcHandleRes.hasValue())
            {
                const gp::Expected<gp::USize, gp::platform::FileError> readRes =
                    srcHandleRes.value().read(chunkBuf, toRead);
                if (!readRes.hasValue())
                {
                    return gp::makeUnexpected(mapFileErrorToVfsError(readRes.error()));
                }
                bytesRead = readRes.value();
            }
            else
            {
                const gp::Expected<gp::USize, VfsError> readRes =
                    srcBackend->readBytesAt(parsedSrc.value().relativePath.view(), offset, chunkBuf, toRead);
                if (!readRes.hasValue())
                {
                    return gp::makeUnexpected(readRes.error());
                }
                bytesRead = readRes.value();
            }

            if (bytesRead == 0u)
            {
                break;
            }

            const gp::Expected<gp::USize, gp::platform::FileError> writeRes = dstHandle.write(chunkBuf, bytesRead);
            if (!writeRes.hasValue())
            {
                return gp::makeUnexpected(mapFileErrorToVfsError(writeRes.error()));
            }
            if (writeRes.value() != bytesRead)
            {
                return gp::makeUnexpected(VfsError::DiskFull);
            }

            offset += static_cast<gp::UInt64>(bytesRead);
        }
        return true;
    }

    gp::Byte chunkBuf[gp::platform::kFileCopyChunkSize]{};
    gp::UInt64 offset = 0ULL;
    bool firstChunk = true;

    while (offset < totalBytes)
    {
        const gp::UInt64 remaining = totalBytes - offset;
        const gp::USize toRead = (remaining > static_cast<gp::UInt64>(gp::platform::kFileCopyChunkSize))
                                     ? gp::platform::kFileCopyChunkSize
                                     : static_cast<gp::USize>(remaining);

        const gp::Expected<gp::USize, VfsError> readRes =
            srcBackend->readBytesAt(parsedSrc.value().relativePath.view(), offset, chunkBuf, toRead);
        if (!readRes.hasValue())
        {
            return gp::makeUnexpected(readRes.error());
        }
        if (readRes.value() == 0u)
        {
            break;
        }

        const gp::Expected<bool, VfsError> writeRes =
            dstWritable->writeBytes(parsedDst.value().relativePath.view(), chunkBuf, readRes.value(), !firstChunk);
        if (!writeRes.hasValue())
        {
            return gp::makeUnexpected(writeRes.error());
        }

        offset += static_cast<gp::UInt64>(readRes.value());
        firstChunk = false;
    }

    return true;
}

gp::Expected<bool, VfsError> VirtualFileSystem::iterateDirectory(
    const gp::StringView directoryUri, const VfsDirectoryVisitorFn visitor, void* const userData
) noexcept
{
    if (visitor == nullptr)
    {
        return gp::makeUnexpected(VfsError::InvalidArgument);
    }

    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(directoryUri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }

    IMountBackend* activeBackends[kMaxMountPoints]{};
    MountPriority activePriorities[kMaxMountPoints]{};
    gp::USize mountCount = 0u;

    {
        detail::VfsGlobalState& state = detail::getState();
        const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

        gp::USize sorted[kMaxMountPoints]{};
        mountCount = detail::collectSortedMountsForSchemeUnlocked(state, parsed.value().scheme.view(), sorted);
        if (mountCount == 0u)
        {
            return gp::makeUnexpected(VfsError::SchemeNotMounted);
        }

        for (gp::USize i = 0u; i < mountCount; ++i)
        {
            activeBackends[i] = state.mounts[sorted[i]].backend;
            activePriorities[i] = state.mounts[sorted[i]].priority;
        }
    }

    constexpr gp::USize kSeenTableCapacity = kVfsMergedDirectoryHashSlots;
    constexpr gp::USize kSeenTableMask = kSeenTableCapacity - 1u;

    /// @brief Compact open-addressed 64-bit FNV-1a hash set (`512` slots = `4 KiB` on stack) for O(1) shadowed-entry
    ///        deduplication during multi-mount VFS directory iteration, replacing a `32 KiB` string array so recursive
    ///        VFS walks stay safely within `64 KiB` standard job-system fiber stacks (ADR-0049, ADR-0060).
    struct MergedIterateState
    {
        SchemeBuffer scheme{};
        gp::PathBuffer baseRelativeDir{};
        gp::UInt64 seenHashes[kSeenTableCapacity]{};
        gp::USize seenCount{ 0u };
        MountPriority currentPriority{ MountPriority::Base };
        VfsDirectoryVisitorFn userVisitor{ nullptr };
        void* userContext{ nullptr };
        bool stopped{ false };

        /// @brief Computes a non-zero 64-bit FNV-1a hash of a leaf filename for O(1) deduplication.
        /// @param[in] name Leaf filename view.
        /// @return Non-zero 64-bit hash (`0ULL` is reserved for empty table slots).
        [[nodiscard]] static gp::UInt64 hashLeafName(const gp::StringView name) noexcept
        {
            gp::hashing::Fnv1a64 hasher{};
            hasher.update(name.data(), name.size());
            const gp::UInt64 digest = hasher.finalize();
            return (digest == 0ULL) ? 1ULL : digest;
        }

        /// @brief Checks whether `name` was already emitted by a higher-priority layer and inserts it if unseen.
        /// @param[in] name Leaf filename view.
        /// @return True if `name` is newly inserted (caller should emit); false if already seen (caller skips).
        [[nodiscard]] bool tryInsertSeen(const gp::StringView name) noexcept
        {
            const gp::UInt64 h = hashLeafName(name);
            gp::USize slot = static_cast<gp::USize>(h) & kSeenTableMask;
            for (gp::USize probe = 0u; probe < kSeenTableCapacity; ++probe)
            {
                const gp::UInt64 existing = seenHashes[slot];
                if (existing == h)
                {
                    return false;
                }
                if (existing == 0ULL)
                {
                    seenHashes[slot] = h;
                    ++seenCount;
                    return true;
                }
                slot = (slot + 1u) & kSeenTableMask;
            }
            return true;
        }
    };

    MergedIterateState mergedState{};
    mergedState.scheme = parsed.value().scheme;
    mergedState.baseRelativeDir = parsed.value().relativePath;
    mergedState.userVisitor = visitor;
    mergedState.userContext = userData;

    const gp::platform::DirectoryVisitorFn backendVisitor =
        [](const gp::platform::DirectoryEntry& rawEntry, void* const rawCtx) -> gp::platform::DirectoryVisitControl
    {
        MergedIterateState* const st = static_cast<MergedIterateState*>(rawCtx);

        // Deduplicate shadowed entries already emitted by a higher-priority mount layer in O(1).
        if (!st->tryInsertSeen(rawEntry.name))
        {
            return gp::platform::DirectoryVisitControl::Continue;
        }

        const gp::PathBuffer relChild = st->baseRelativeDir.isEmpty()
                                            ? gp::PathBuffer(rawEntry.name)
                                            : gp::platform::Paths::combine(st->baseRelativeDir.view(), rawEntry.name);
        const gp::PathBuffer fullUri = detail::buildCanonicalUri(st->scheme.view(), relChild.view());

        VfsDirectoryEntry vfsEntry{};
        vfsEntry.uri = fullUri.view();
        vfsEntry.name = rawEntry.name;
        vfsEntry.stats = rawEntry.stats;
        vfsEntry.sourcePriority = st->currentPriority;
        vfsEntry.depth = 0u;

        const gp::platform::DirectoryVisitControl ctrl = st->userVisitor(vfsEntry, st->userContext);
        if (ctrl == gp::platform::DirectoryVisitControl::Stop)
        {
            st->stopped = true;
            return gp::platform::DirectoryVisitControl::Stop;
        }
        return gp::platform::DirectoryVisitControl::Continue;
    };

    bool anyDirectoryFound = false;
    VfsError firstNonNotFoundError = VfsError::NotFound;

    for (gp::USize i = 0u; i < mountCount; ++i)
    {
        mergedState.currentPriority = activePriorities[i];
        const gp::Expected<bool, VfsError> res =
            activeBackends[i]->iterateDirectory(parsed.value().relativePath.view(), backendVisitor, &mergedState);

        if (res.hasValue())
        {
            anyDirectoryFound = true;
            if (mergedState.stopped)
            {
                break;
            }
        }
        else if (res.error() == VfsError::NotDirectory)
        {
            // A higher-priority mount shadows this URI with a regular file (not a directory): stop immediately so
            // lower-priority directories are not leaked behind a shadowing file.
            if (!anyDirectoryFound)
            {
                return gp::makeUnexpected(VfsError::NotDirectory);
            }
        }
        else if (res.error() != VfsError::NotFound && firstNonNotFoundError == VfsError::NotFound)
        {
            firstNonNotFoundError = res.error();
        }
    }

    if (!anyDirectoryFound)
    {
        return gp::makeUnexpected(firstNonNotFoundError);
    }

    return true;
}

gp::Expected<bool, VfsError> VirtualFileSystem::iterateDirectoryRecursive(
    const gp::StringView directoryUri, const VfsDirectoryVisitorFn visitor, void* const userData
) noexcept
{
    bool stopped = false;
    return detail::iterateDirectoryRecursiveInternal(directoryUri, visitor, userData, 0u, stopped);
}

gp::Expected<gp::USize, VfsError> VirtualFileSystem::readBytesAt(
    const gp::StringView uri, const gp::UInt64 offset, void* const outBuffer, const gp::USize bytesToRead
) noexcept
{
    if (bytesToRead > 0u && outBuffer == nullptr)
    {
        return gp::makeUnexpected(VfsError::InvalidArgument);
    }

    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }
    if (parsed.value().isSchemeRoot())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    gp::USize sorted[kMaxMountPoints]{};
    const gp::USize count = detail::collectSortedMountsForSchemeUnlocked(state, parsed.value().scheme.view(), sorted);
    if (count == 0u)
    {
        return gp::makeUnexpected(VfsError::SchemeNotMounted);
    }

    VfsError lastErr = VfsError::NotFound;
    for (gp::USize i = 0u; i < count; ++i)
    {
        IMountBackend* const backend = state.mounts[sorted[i]].backend;
        if (!backend->exists(parsed.value().relativePath.view()))
        {
            continue;
        }

        const gp::Expected<gp::USize, VfsError> readRes =
            backend->readBytesAt(parsed.value().relativePath.view(), offset, outBuffer, bytesToRead);
        if (readRes.hasValue())
        {
            return readRes.value();
        }
        lastErr = readRes.error();
        if (lastErr != VfsError::NotFound)
        {
            break;
        }
    }

    return gp::makeUnexpected(lastErr);
}

gp::Expected<gp::USize, VfsError> VirtualFileSystem::readBytes(
    const gp::StringView uri, void* const outBuffer, const gp::USize bufferCapacity
) noexcept
{
    return readBytesAt(uri, 0ULL, outBuffer, bufferCapacity);
}

gp::Expected<bool, VfsError> VirtualFileSystem::writeAllBytes(
    const gp::StringView uri, const void* const data, const gp::USize sizeInBytes
) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }
    if (parsed.value().isSchemeRoot())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->writeBytes(parsed.value().relativePath.view(), data, sizeInBytes, false);
}

gp::Expected<bool, VfsError>
    VirtualFileSystem::writeAllText(const gp::StringView uri, const gp::StringView text) noexcept
{
    return writeAllBytes(uri, text.data(), text.size());
}

gp::Expected<bool, VfsError> VirtualFileSystem::appendAllBytes(
    const gp::StringView uri, const void* const data, const gp::USize sizeInBytes
) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }
    if (parsed.value().isSchemeRoot())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->writeBytes(parsed.value().relativePath.view(), data, sizeInBytes, true);
}

gp::Expected<bool, VfsError>
    VirtualFileSystem::appendAllText(const gp::StringView uri, const gp::StringView text) noexcept
{
    return appendAllBytes(uri, text.data(), text.size());
}

gp::Expected<bool, VfsError> VirtualFileSystem::writeAtomic(
    const gp::StringView uri, const void* const data, const gp::USize sizeInBytes
) noexcept
{
    const gp::Expected<ParsedUri, VfsError> parsed = parseUri(uri);
    if (!parsed.hasValue())
    {
        return gp::makeUnexpected(parsed.error());
    }
    if (parsed.value().isSchemeRoot())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    detail::VfsGlobalState& state = detail::getState();
    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(state.mutex);

    VfsError err = VfsError::None;
    IMountBackend* const writable = detail::findWritableBackendUnlocked(state, parsed.value().scheme.view(), err);
    if (writable == nullptr)
    {
        return gp::makeUnexpected(err);
    }

    return writable->writeAtomic(parsed.value().relativePath.view(), data, sizeInBytes);
}

gp::Expected<bool, VfsError>
    VirtualFileSystem::writeAtomicText(const gp::StringView uri, const gp::StringView text) noexcept
{
    return writeAtomic(uri, text.data(), text.size());
}

namespace
{

/// @brief Maximum entries emitted by a single `vfs.Ls` console command invocation to avoid terminal flooding.
constexpr gp::USize kMaxConsoleLsEntries = 256u;

gp::console::AutoConsoleCommand g_cmdVfsStatus(
    "vfs.Status",
    "Prints active VirtualFileSystem mount counts across the canonical ADR-0044 URI schemes.",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    gp::FixedString<383> line{};
    gp::log::formatTo(
        line,
        "VFS[total={}/{}] engine={} game={} user={} appdata={} cache={} temp={}",
        VirtualFileSystem::getMountCount(),
        kMaxMountPoints,
        VirtualFileSystem::getMountCount(kEngineScheme),
        VirtualFileSystem::getMountCount(kGameScheme),
        VirtualFileSystem::getMountCount(kUserScheme),
        VirtualFileSystem::getMountCount(kAppDataScheme),
        VirtualFileSystem::getMountCount(kCacheScheme),
        VirtualFileSystem::getMountCount(kTempScheme)
    );
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleCommand g_cmdVfsMounts(
    "vfs.Mounts",
    "Lists active VFS mount points in lookup precedence order. Usage: vfs.Mounts [scheme]",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    const gp::StringView schemeFilter = (args.getArgCount() >= 1u) ? args.getArg(0u) : gp::StringView{};
    const gp::USize totalMatches = VirtualFileSystem::getMountCount(schemeFilter);

    gp::FixedString<511> line{};
    if (totalMatches == 0u)
    {
        if (schemeFilter.isEmpty())
        {
            args.writeLine("VFS Mounts: none registered.");
        }
        else
        {
            gp::log::formatTo(line, "VFS Mounts: no mounts registered for '{}'.", schemeFilter);
            args.writeLine(line.view());
        }
        return;
    }

    if (schemeFilter.isEmpty())
    {
        gp::log::formatTo(line, "VFS Mounts ({} active):", totalMatches);
    }
    else
    {
        gp::log::formatTo(line, "VFS Mounts for '{}' ({} active):", schemeFilter, totalMatches);
    }
    args.writeLine(line.view());

    gp::USize index = 0u;
    VirtualFileSystem::enumerateMounts(
        schemeFilter,
        [&args, &index](const MountPointDesc& desc) noexcept -> bool
    {
        gp::FixedString<511> entryLine{};
        gp::log::formatTo(
            entryLine,
            "  [{}] {} | prio={}({}) | access={} | root={}",
            index,
            desc.scheme.view(),
            toString(desc.priority),
            static_cast<gp::UInt32>(desc.priority),
            toString(desc.access),
            desc.rootPath.view()
        );
        args.writeLine(entryLine.view());
        ++index;
        return true;
    }
    );
},
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleCommand g_cmdVfsResolve(
    "vfs.Resolve",
    "Parses a logical VFS URI and resolves its read and writable physical OS paths. Usage: vfs.Resolve <uri>",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    if (args.getArgCount() < 1u)
    {
        args.writeLine("Usage: vfs.Resolve <uri>  (e.g., vfs.Resolve user://saves/slot01.sav)");
        return;
    }

    const gp::StringView uri = args.getArg(0u);
    const gp::Expected<ParsedUri, VfsError> parsed = VirtualFileSystem::parseUri(uri);
    gp::FixedString<511> line{};
    if (!parsed.hasValue())
    {
        gp::log::formatTo(line, "Resolve '{}': error={}", uri, toString(parsed.error()));
        args.writeLine(line.view());
        return;
    }

    gp::log::formatTo(line, "Canonical: {}{}", parsed.value().scheme.view(), parsed.value().relativePath.view());
    args.writeLine(line.view());

    const gp::Expected<gp::PathBuffer, VfsError> readRes = VirtualFileSystem::resolvePhysicalPath(uri);
    line.clear();
    if (readRes.hasValue())
    {
        gp::log::formatTo(line, "  ReadPath : {}", readRes.value().view());
    }
    else
    {
        gp::log::formatTo(line, "  ReadPath : <{}>", toString(readRes.error()));
    }
    args.writeLine(line.view());

    const gp::Expected<gp::PathBuffer, VfsError> writeRes = VirtualFileSystem::resolveWritablePhysicalPath(uri);
    line.clear();
    if (writeRes.hasValue())
    {
        gp::log::formatTo(line, "  WritePath: {}", writeRes.value().view());
    }
    else
    {
        gp::log::formatTo(line, "  WritePath: <{}>", toString(writeRes.error()));
    }
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleCommand g_cmdVfsExists(
    "vfs.Exists",
    "Checks whether a logical VFS URI exists and reports whether it is a file or directory. Usage: vfs.Exists <uri>",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    if (args.getArgCount() < 1u)
    {
        args.writeLine("Usage: vfs.Exists <uri>  (e.g., vfs.Exists game://configs/default.ini)");
        return;
    }

    const gp::StringView uri = args.getArg(0u);
    const gp::Expected<ParsedUri, VfsError> parsed = VirtualFileSystem::parseUri(uri);
    gp::FixedString<511> line{};
    if (!parsed.hasValue())
    {
        gp::log::formatTo(line, "Exists '{}': error={}", uri, toString(parsed.error()));
        args.writeLine(line.view());
        return;
    }

    if (!VirtualFileSystem::isSchemeMounted(parsed.value().scheme.view()))
    {
        gp::log::formatTo(line, "Exists '{}': error={}", uri, toString(VfsError::SchemeNotMounted));
        args.writeLine(line.view());
        return;
    }

    const bool exists = VirtualFileSystem::exists(uri);
    const bool isFile = exists && VirtualFileSystem::isFile(uri);
    const bool isDir = exists && VirtualFileSystem::isDirectory(uri);
    gp::log::formatTo(line, "Exists '{}': exists={} isFile={} isDirectory={}", uri, exists, isFile, isDir);
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleCommand g_cmdVfsStat(
    "vfs.Stat",
    "Prints size, attributes, and timestamps for a logical VFS URI. Usage: vfs.Stat <uri>",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    if (args.getArgCount() < 1u)
    {
        args.writeLine("Usage: vfs.Stat <uri>  (e.g., vfs.Stat user://saves/slot01.sav)");
        return;
    }

    const gp::StringView uri = args.getArg(0u);
    const gp::Expected<gp::platform::FileStats, VfsError> statRes = VirtualFileSystem::getStats(uri);
    gp::FixedString<511> line{};
    if (!statRes.hasValue())
    {
        gp::log::formatTo(line, "Stat '{}': error={}", uri, toString(statRes.error()));
        args.writeLine(line.view());
        return;
    }

    const gp::platform::FileStats& stats = statRes.value();
    const gp::StringView typeStr = stats.isDirectory() ? "Directory" : (stats.isFile() ? "File" : "Other");
    gp::log::formatTo(
        line,
        "Stat '{}': type={} size={}B readOnly={} hidden={} symLink={} createdNs={} modifiedNs={} accessedNs={}",
        uri,
        typeStr,
        stats.size,
        stats.isReadOnly(),
        stats.isHidden(),
        stats.isSymbolicLink(),
        stats.creationTime.nanoseconds,
        stats.lastWriteTime.nanoseconds,
        stats.lastAccessTime.nanoseconds
    );
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleCommand g_cmdVfsLs(
    "vfs.Ls",
    "Lists merged VFS directory entries in overlay priority order. Usage: vfs.Ls <uri> [-r]",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    if (args.getArgCount() < 1u)
    {
        args.writeLine("Usage: vfs.Ls <uri> [-r]  (e.g., vfs.Ls game:// -r)");
        return;
    }

    const gp::StringView uri = args.getArg(0u);
    bool recursive = false;
    if (args.getArgCount() >= 2u)
    {
        const gp::StringView flag = args.getArg(1u);
        recursive = flag.equalsIgnoreCase("-r") || flag.equalsIgnoreCase("--recursive");
    }

    gp::USize printedCount = 0u;
    bool truncated = false;
    const auto visitor = [&args,
                          &printedCount,
                          &truncated](const VfsDirectoryEntry& entry) noexcept -> gp::platform::DirectoryVisitControl
    {
        if (printedCount >= kMaxConsoleLsEntries)
        {
            truncated = true;
            return gp::platform::DirectoryVisitControl::Stop;
        }

        const gp::StringView kindTag = entry.isDirectory() ? "[DIR ]" : "[FILE]";
        gp::FixedString<511> entryLine{};
        gp::log::formatTo(
            entryLine,
            "  {} {:10}B | prio={} | {}",
            kindTag,
            entry.stats.size,
            toString(entry.sourcePriority),
            entry.uri
        );
        args.writeLine(entryLine.view());
        ++printedCount;
        return gp::platform::DirectoryVisitControl::Continue;
    };

    const gp::Expected<bool, VfsError> iterRes = recursive ? VirtualFileSystem::iterateDirectoryRecursive(uri, visitor)
                                                           : VirtualFileSystem::iterateDirectory(uri, visitor);
    gp::FixedString<511> summary{};
    if (!iterRes.hasValue())
    {
        gp::log::formatTo(summary, "Ls '{}': error={}", uri, toString(iterRes.error()));
        args.writeLine(summary.view());
        return;
    }

    if (truncated)
    {
        gp::log::formatTo(summary, "... (truncated at {} entries)", kMaxConsoleLsEntries);
    }
    else
    {
        gp::log::formatTo(summary, "Total: {} entries", printedCount);
    }
    args.writeLine(summary.view());
},
    gp::console::ConsoleVarFlags::None
);

}   // namespace

}   // namespace gp::vfs
