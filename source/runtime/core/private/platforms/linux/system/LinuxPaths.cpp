// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/linux/system/LinuxPaths.hpp"
#include "threading/atomics/Atomic.hpp"
#include <unistd.h>

namespace gp::platform::linux
{

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
        char linkBuf[kMaxPathLength + 1u]{};
        const ssize_t readBytes = ::readlink("/proc/self/exe", linkBuf, kMaxPathLength);
        if (readBytes > 0 && static_cast<gp::USize>(readBytes) <= kMaxPathLength)
        {
            linkBuf[readBytes] = '\0';
            computed.assign(gp::StringView(linkBuf, static_cast<gp::USize>(readBytes)));
            makeStandardFilename(computed);
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
            computed = workingDir();
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

PathBuffer Paths::resolve(const gp::StringView path) noexcept
{
    return resolve(path, baseDir());
}

PathBuffer Paths::convertRelativePathToFull(const gp::StringView inPath) noexcept
{
    return resolve(inPath, baseDir());
}

}   // namespace gp::platform::linux
