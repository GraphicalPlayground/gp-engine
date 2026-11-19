// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "vfs/MemoryMountBackend.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/base/system/Time.hpp"
#include "threading/sync/SharedLock.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <cstring>

namespace gp::vfs
{

MemoryMountBackend::MemoryMountBackend() noexcept
{
    clear();
}

MemoryMountBackend::MemoryMountBackend(const gp::StringView identifier) noexcept
{
    if (!identifier.isEmpty())
    {
        m_identifier.assign(identifier);
    }
    clear();
}

void MemoryMountBackend::clear() noexcept
{
    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    for (gp::USize i = 0u; i < kMaxMemoryMountNodes; ++i)
    {
        m_nodes[i].relativePath.clear();
        m_nodes[i].content.reset();
        m_nodes[i].stats = {};
        m_nodes[i].active = false;
    }

    const gp::time::Timestamp nowTs(gp::platform::Time::utcTimestampMicroseconds() * 1000ULL);
    m_nodes[0].relativePath.clear();
    m_nodes[0].stats.size = 0ULL;
    m_nodes[0].stats.creationTime = nowTs;
    m_nodes[0].stats.lastWriteTime = nowTs;
    m_nodes[0].stats.lastAccessTime = nowTs;
    m_nodes[0].stats.attributes = gp::platform::FileAttributeFlags::Directory;
    m_nodes[0].active = true;
}

VfsError MemoryMountBackend::normalizeRelativeKey(
    const gp::StringView rawRelativePath, gp::PathBuffer& outNormalized
) noexcept
{
    outNormalized.clear();
    if (rawRelativePath.isEmpty())
    {
        return VfsError::None;
    }
    if (rawRelativePath.size() > gp::platform::kMaxPathLength)
    {
        return VfsError::InvalidUri;
    }

    gp::PathBuffer temp(rawRelativePath);
    gp::platform::Paths::normalizeDirectoryName(temp);
    if (!gp::platform::Paths::collapseRelativeDirectories(temp))
    {
        return VfsError::SandboxEscapeViolation;
    }

    gp::StringView view = temp.view();
    while (!view.isEmpty() && gp::platform::Paths::isSeparator(view.front()))
    {
        view = view.substr(1u);
    }
    while (!view.isEmpty() && gp::platform::Paths::isSeparator(view.back()))
    {
        view = view.substr(0u, view.size() - 1u);
    }

    if (view == ".." || view.startsWith("../"))
    {
        return VfsError::SandboxEscapeViolation;
    }
    if (view == ".")
    {
        return VfsError::None;
    }

    outNormalized.assign(view);
    return VfsError::None;
}

gp::USize MemoryMountBackend::findNodeIndexUnlocked(const gp::StringView normalizedKey) const noexcept
{
    for (gp::USize i = 0u; i < kMaxMemoryMountNodes; ++i)
    {
        if (m_nodes[i].active && m_nodes[i].relativePath.view() == normalizedKey)
        {
            return i;
        }
    }
    return kMaxMemoryMountNodes;
}

gp::USize MemoryMountBackend::allocateNodeSlotUnlocked() noexcept
{
    for (gp::USize i = 0u; i < kMaxMemoryMountNodes; ++i)
    {
        if (!m_nodes[i].active)
        {
            return i;
        }
    }
    return kMaxMemoryMountNodes;
}

gp::Expected<bool, VfsError> MemoryMountBackend::createDirectoryUnlocked(const gp::StringView normalizedKey) noexcept
{
    if (normalizedKey.isEmpty())
    {
        return true;
    }

    const gp::USize existingIdx = findNodeIndexUnlocked(normalizedKey);
    if (existingIdx < kMaxMemoryMountNodes)
    {
        if (m_nodes[existingIdx].stats.isDirectory())
        {
            return true;
        }
        return gp::makeUnexpected(VfsError::AlreadyExists);
    }

    const gp::StringView parentDir = gp::platform::Paths::getPath(normalizedKey);
    const gp::USize parentIdx = findNodeIndexUnlocked(parentDir);
    if (parentIdx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (!m_nodes[parentIdx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::NotDirectory);
    }

    const gp::USize slot = allocateNodeSlotUnlocked();
    if (slot >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::MountTableFull);
    }

    const gp::time::Timestamp nowTs(gp::platform::Time::utcTimestampMicroseconds() * 1000ULL);
    m_nodes[slot].relativePath.assign(normalizedKey);
    m_nodes[slot].content.clear();
    m_nodes[slot].stats.size = 0ULL;
    m_nodes[slot].stats.creationTime = nowTs;
    m_nodes[slot].stats.lastWriteTime = nowTs;
    m_nodes[slot].stats.lastAccessTime = nowTs;
    m_nodes[slot].stats.attributes = gp::platform::FileAttributeFlags::Directory;
    m_nodes[slot].active = true;
    return true;
}

gp::StringView MemoryMountBackend::rootPath() const noexcept
{
    return m_identifier.view();
}

bool MemoryMountBackend::isPhysical() const noexcept
{
    return false;
}

gp::Expected<gp::PathBuffer, VfsError>
    MemoryMountBackend::resolvePhysicalPath(const gp::StringView relativePath) const noexcept
{
    (void)relativePath;
    return gp::makeUnexpected(VfsError::NotSupported);
}

bool MemoryMountBackend::exists(const gp::StringView relativePath) const noexcept
{
    gp::PathBuffer key{};
    if (normalizeRelativeKey(relativePath, key) != VfsError::None)
    {
        return false;
    }

    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_mutex);
    return findNodeIndexUnlocked(key.view()) < kMaxMemoryMountNodes;
}

gp::Expected<gp::platform::FileStats, VfsError>
    MemoryMountBackend::getStats(const gp::StringView relativePath) const noexcept
{
    gp::PathBuffer key{};
    const VfsError normErr = normalizeRelativeKey(relativePath, key);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }

    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::USize idx = findNodeIndexUnlocked(key.view());
    if (idx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    return m_nodes[idx].stats;
}

gp::Expected<gp::platform::FileHandle, VfsError> MemoryMountBackend::openFileHandle(
    const gp::StringView relativePath,
    const gp::platform::FileAccessMode access,
    const gp::platform::FileOpenMode openMode,
    const gp::platform::FileShareMode shareMode
) noexcept
{
    (void)relativePath;
    (void)access;
    (void)openMode;
    (void)shareMode;
    return gp::makeUnexpected(VfsError::NotSupported);
}

gp::Expected<gp::USize, VfsError> MemoryMountBackend::readBytesAt(
    const gp::StringView relativePath, const gp::UInt64 offset, void* const outBuffer, const gp::USize bytesToRead
) const noexcept
{
    if (bytesToRead > 0u && outBuffer == nullptr)
    {
        return gp::makeUnexpected(VfsError::InvalidArgument);
    }

    gp::PathBuffer key{};
    const VfsError normErr = normalizeRelativeKey(relativePath, key);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }

    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::USize idx = findNodeIndexUnlocked(key.view());
    if (idx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (m_nodes[idx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::String& content = m_nodes[idx].content;
    if (offset >= static_cast<gp::UInt64>(content.size()) || bytesToRead == 0u)
    {
        return 0u;
    }

    const gp::USize startOffset = static_cast<gp::USize>(offset);
    const gp::USize available = content.size() - startOffset;
    const gp::USize toCopy = (bytesToRead < available) ? bytesToRead : available;
    std::memcpy(outBuffer, content.data() + startOffset, toCopy);
    return toCopy;
}

gp::Expected<bool, VfsError> MemoryMountBackend::writeBytes(
    const gp::StringView relativePath, const void* const data, const gp::USize sizeInBytes, const bool append
) noexcept
{
    if (sizeInBytes > 0u && data == nullptr)
    {
        return gp::makeUnexpected(VfsError::InvalidArgument);
    }

    gp::PathBuffer key{};
    const VfsError normErr = normalizeRelativeKey(relativePath, key);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }
    if (key.isEmpty())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::StringView parentDir = gp::platform::Paths::getPath(key.view());
    const gp::USize parentIdx = findNodeIndexUnlocked(parentDir);
    if (parentIdx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (!m_nodes[parentIdx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::NotDirectory);
    }

    gp::USize idx = findNodeIndexUnlocked(key.view());
    const gp::time::Timestamp nowTs(gp::platform::Time::utcTimestampMicroseconds() * 1000ULL);

    if (idx < kMaxMemoryMountNodes)
    {
        if (m_nodes[idx].stats.isDirectory())
        {
            return gp::makeUnexpected(VfsError::IsDirectory);
        }
        if (m_nodes[idx].stats.isReadOnly())
        {
            return gp::makeUnexpected(VfsError::AccessDenied);
        }
        if (!append)
        {
            m_nodes[idx].content.clear();
        }
    }
    else
    {
        idx = allocateNodeSlotUnlocked();
        if (idx >= kMaxMemoryMountNodes)
        {
            return gp::makeUnexpected(VfsError::MountTableFull);
        }
        m_nodes[idx].relativePath = key;
        m_nodes[idx].content.clear();
        m_nodes[idx].stats.creationTime = nowTs;
        m_nodes[idx].stats.attributes = gp::platform::FileAttributeFlags::RegularFile;
        m_nodes[idx].active = true;
    }

    if (sizeInBytes > 0u)
    {
        m_nodes[idx].content.append(static_cast<const char*>(data), sizeInBytes);
    }
    m_nodes[idx].stats.size = static_cast<gp::UInt64>(m_nodes[idx].content.size());
    m_nodes[idx].stats.lastWriteTime = nowTs;
    m_nodes[idx].stats.lastAccessTime = nowTs;
    return true;
}

gp::Expected<bool, VfsError> MemoryMountBackend::writeAtomic(
    const gp::StringView relativePath, const void* const data, const gp::USize sizeInBytes
) noexcept
{
    return writeBytes(relativePath, data, sizeInBytes, false);
}

gp::Expected<bool, VfsError> MemoryMountBackend::createDirectory(const gp::StringView relativePath) noexcept
{
    gp::PathBuffer key{};
    const VfsError normErr = normalizeRelativeKey(relativePath, key);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }

    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    return createDirectoryUnlocked(key.view());
}

gp::Expected<bool, VfsError> MemoryMountBackend::createDirectoryTree(const gp::StringView relativePath) noexcept
{
    gp::PathBuffer key{};
    const VfsError normErr = normalizeRelativeKey(relativePath, key);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }
    if (key.isEmpty())
    {
        return true;
    }

    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::StringView keyView = key.view();
    gp::USize scanPos = 0u;
    while (scanPos < keyView.size())
    {
        gp::USize nextSep = scanPos;
        while (nextSep < keyView.size() && !gp::platform::Paths::isSeparator(keyView[nextSep]))
        {
            ++nextSep;
        }

        const gp::StringView prefix = keyView.substr(0u, nextSep);
        const gp::Expected<bool, VfsError> res = createDirectoryUnlocked(prefix);
        if (!res.hasValue())
        {
            return res;
        }

        scanPos = nextSep + 1u;
    }
    return true;
}

gp::Expected<bool, VfsError> MemoryMountBackend::removeFile(const gp::StringView relativePath) noexcept
{
    gp::PathBuffer key{};
    const VfsError normErr = normalizeRelativeKey(relativePath, key);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }
    if (key.isEmpty())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::USize idx = findNodeIndexUnlocked(key.view());
    if (idx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (m_nodes[idx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    m_nodes[idx].relativePath.clear();
    m_nodes[idx].content.reset();
    m_nodes[idx].stats = {};
    m_nodes[idx].active = false;
    return true;
}

gp::Expected<bool, VfsError> MemoryMountBackend::removeDirectory(const gp::StringView relativePath) noexcept
{
    gp::PathBuffer key{};
    const VfsError normErr = normalizeRelativeKey(relativePath, key);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }
    if (key.isEmpty())
    {
        return gp::makeUnexpected(VfsError::AccessDenied);
    }

    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::USize idx = findNodeIndexUnlocked(key.view());
    if (idx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (!m_nodes[idx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::NotDirectory);
    }

    for (gp::USize i = 0u; i < kMaxMemoryMountNodes; ++i)
    {
        if (m_nodes[i].active && i != idx &&
            gp::platform::Paths::isUnderDirectory(m_nodes[i].relativePath.view(), key.view()))
        {
            return gp::makeUnexpected(VfsError::DirectoryNotEmpty);
        }
    }

    m_nodes[idx].relativePath.clear();
    m_nodes[idx].content.reset();
    m_nodes[idx].stats = {};
    m_nodes[idx].active = false;
    return true;
}

gp::Expected<bool, VfsError> MemoryMountBackend::removeDirectoryTree(const gp::StringView relativePath) noexcept
{
    gp::PathBuffer key{};
    const VfsError normErr = normalizeRelativeKey(relativePath, key);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }
    if (key.isEmpty())
    {
        return gp::makeUnexpected(VfsError::AccessDenied);
    }

    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::USize idx = findNodeIndexUnlocked(key.view());
    if (idx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (!m_nodes[idx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::NotDirectory);
    }

    for (gp::USize i = 0u; i < kMaxMemoryMountNodes; ++i)
    {
        if (m_nodes[i].active &&
            (i == idx || gp::platform::Paths::isUnderDirectory(m_nodes[i].relativePath.view(), key.view())))
        {
            m_nodes[i].relativePath.clear();
            m_nodes[i].content.reset();
            m_nodes[i].stats = {};
            m_nodes[i].active = false;
        }
    }
    return true;
}

gp::Expected<bool, VfsError> MemoryMountBackend::move(
    const gp::StringView sourceRelativePath, const gp::StringView destinationRelativePath, const bool overwrite
) noexcept
{
    gp::PathBuffer srcKey{};
    gp::PathBuffer dstKey{};
    const VfsError srcErr = normalizeRelativeKey(sourceRelativePath, srcKey);
    const VfsError dstErr = normalizeRelativeKey(destinationRelativePath, dstKey);
    if (srcErr != VfsError::None)
    {
        return gp::makeUnexpected(srcErr);
    }
    if (dstErr != VfsError::None)
    {
        return gp::makeUnexpected(dstErr);
    }
    if (srcKey.isEmpty() || dstKey.isEmpty())
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }

    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::USize srcIdx = findNodeIndexUnlocked(srcKey.view());
    if (srcIdx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (srcKey == dstKey)
    {
        return true;
    }

    const gp::StringView dstParent = gp::platform::Paths::getPath(dstKey.view());
    const gp::USize dstParentIdx = findNodeIndexUnlocked(dstParent);
    if (dstParentIdx >= kMaxMemoryMountNodes || !m_nodes[dstParentIdx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }

    const gp::USize dstIdx = findNodeIndexUnlocked(dstKey.view());
    if (dstIdx < kMaxMemoryMountNodes)
    {
        if (!overwrite || m_nodes[dstIdx].stats.isDirectory())
        {
            return gp::makeUnexpected(VfsError::AlreadyExists);
        }
        m_nodes[dstIdx].relativePath.clear();
        m_nodes[dstIdx].content.reset();
        m_nodes[dstIdx].stats = {};
        m_nodes[dstIdx].active = false;
    }

    const bool movingDirectory = m_nodes[srcIdx].stats.isDirectory();
    if (movingDirectory)
    {
        // Prevent moving a directory into its own descendant subtree
        if (gp::platform::Paths::isUnderDirectory(dstKey.view(), srcKey.view()))
        {
            return gp::makeUnexpected(VfsError::InvalidArgument);
        }

        // Reparent all active descendants under srcKey to the new dstKey prefix
        for (gp::USize i = 0u; i < kMaxMemoryMountNodes; ++i)
        {
            if (!m_nodes[i].active || i == srcIdx)
            {
                continue;
            }
            const gp::StringView nodePath = m_nodes[i].relativePath.view();
            if (gp::platform::Paths::isUnderDirectory(nodePath, srcKey.view()))
            {
                const gp::StringView suffix = nodePath.substr(srcKey.size() + 1u);
                const gp::PathBuffer reparented = gp::platform::Paths::combine(dstKey.view(), suffix);
                m_nodes[i].relativePath = reparented;
            }
        }
    }

    m_nodes[srcIdx].relativePath = dstKey;
    return true;
}

gp::Expected<bool, VfsError> MemoryMountBackend::copyFile(
    const gp::StringView sourceRelativePath, const gp::StringView destinationRelativePath, const bool overwrite
) noexcept
{
    gp::PathBuffer srcKey{};
    gp::PathBuffer dstKey{};
    const VfsError srcErr = normalizeRelativeKey(sourceRelativePath, srcKey);
    const VfsError dstErr = normalizeRelativeKey(destinationRelativePath, dstKey);
    if (srcErr != VfsError::None)
    {
        return gp::makeUnexpected(srcErr);
    }
    if (dstErr != VfsError::None)
    {
        return gp::makeUnexpected(dstErr);
    }
    if (srcKey.isEmpty() || dstKey.isEmpty())
    {
        return gp::makeUnexpected(VfsError::InvalidUri);
    }
    if (srcKey == dstKey)
    {
        return gp::makeUnexpected(VfsError::InvalidArgument);
    }

    const gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::USize srcIdx = findNodeIndexUnlocked(srcKey.view());
    if (srcIdx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (m_nodes[srcIdx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::IsDirectory);
    }

    const gp::StringView dstParent = gp::platform::Paths::getPath(dstKey.view());
    const gp::USize dstParentIdx = findNodeIndexUnlocked(dstParent);
    if (dstParentIdx >= kMaxMemoryMountNodes || !m_nodes[dstParentIdx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }

    gp::USize dstIdx = findNodeIndexUnlocked(dstKey.view());
    if (dstIdx < kMaxMemoryMountNodes)
    {
        if (!overwrite || m_nodes[dstIdx].stats.isDirectory())
        {
            return gp::makeUnexpected(VfsError::AlreadyExists);
        }
    }
    else
    {
        dstIdx = allocateNodeSlotUnlocked();
        if (dstIdx >= kMaxMemoryMountNodes)
        {
            return gp::makeUnexpected(VfsError::MountTableFull);
        }
    }

    const gp::time::Timestamp nowTs(gp::platform::Time::utcTimestampMicroseconds() * 1000ULL);
    m_nodes[dstIdx].relativePath = dstKey;
    m_nodes[dstIdx].content = m_nodes[srcIdx].content;
    m_nodes[dstIdx].stats = m_nodes[srcIdx].stats;
    m_nodes[dstIdx].stats.creationTime = nowTs;
    m_nodes[dstIdx].stats.lastWriteTime = nowTs;
    m_nodes[dstIdx].stats.lastAccessTime = nowTs;
    m_nodes[dstIdx].active = true;
    return true;
}

gp::Expected<bool, VfsError> MemoryMountBackend::iterateDirectory(
    const gp::StringView relativeDirectoryPath, const gp::platform::DirectoryVisitorFn visitor, void* const userData
) const noexcept
{
    if (visitor == nullptr)
    {
        return gp::makeUnexpected(VfsError::InvalidArgument);
    }

    gp::PathBuffer dirKey{};
    const VfsError normErr = normalizeRelativeKey(relativeDirectoryPath, dirKey);
    if (normErr != VfsError::None)
    {
        return gp::makeUnexpected(normErr);
    }

    const gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_mutex);
    const gp::USize dirIdx = findNodeIndexUnlocked(dirKey.view());
    if (dirIdx >= kMaxMemoryMountNodes)
    {
        return gp::makeUnexpected(VfsError::NotFound);
    }
    if (!m_nodes[dirIdx].stats.isDirectory())
    {
        return gp::makeUnexpected(VfsError::NotDirectory);
    }

    for (gp::USize i = 0u; i < kMaxMemoryMountNodes; ++i)
    {
        if (!m_nodes[i].active || i == dirIdx || m_nodes[i].relativePath.isEmpty())
        {
            continue;
        }

        const gp::StringView nodeParent = gp::platform::Paths::getPath(m_nodes[i].relativePath.view());
        if (nodeParent != dirKey.view())
        {
            continue;
        }

        gp::platform::DirectoryEntry entry{};
        entry.path = m_nodes[i].relativePath.view();
        entry.name = gp::platform::Paths::getCleanFilename(entry.path);
        entry.stats = m_nodes[i].stats;
        entry.depth = 0u;

        const gp::platform::DirectoryVisitControl ctrl = visitor(entry, userData);
        if (ctrl == gp::platform::DirectoryVisitControl::Stop)
        {
            break;
        }
    }

    return true;
}

}   // namespace gp::vfs
