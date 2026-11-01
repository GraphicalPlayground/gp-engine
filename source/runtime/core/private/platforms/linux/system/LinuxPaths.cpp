// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/linux/system/LinuxPaths.hpp"
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
    static gp::UInt32 s_cachedGen = 0u;
    const gp::UInt32 currentGen = getCacheGeneration();

    if (s_exePathCache.isEmpty() || s_cachedGen != currentGen) [[unlikely]]
    {
        char linkBuf[kMaxPathLength + 1u]{};
        const ssize_t readBytes = ::readlink("/proc/self/exe", linkBuf, kMaxPathLength);
        if (readBytes > 0 && static_cast<gp::USize>(readBytes) <= kMaxPathLength)
        {
            linkBuf[readBytes] = '\0';
            s_exePathCache.assign(gp::StringView(linkBuf, static_cast<gp::USize>(readBytes)));
            makeStandardFilename(s_exePathCache);
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
            s_exeDirCache = workingDir();
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

PathBuffer Paths::resolve(const gp::StringView path) noexcept
{
    return resolve(path, baseDir());
}

PathBuffer Paths::convertRelativePathToFull(const gp::StringView inPath) noexcept
{
    return resolve(inPath, baseDir());
}

}   // namespace gp::platform::linux
