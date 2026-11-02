// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/system/Paths.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::platform::tests
{

class PlatformPathsTest : public ::testing::Test
{
protected:
    void TearDown() noexcept override
    {
        Paths::resetCachesForTesting();
        gp::CommandLine::shutdown();
    }
};

TEST_F(PlatformPathsTest, UnifiedPlatformDirectoriesAreAbsoluteNormalizedAndTrailingSlashed)
{
    const gp::StringView exePath = Paths::executablePath();
    const gp::StringView exeDir = Paths::executableDir();
    const gp::StringView baseDir = Paths::baseDir();
    const PathBuffer cwd = Paths::workingDir();
    const gp::StringView launchDir = Paths::launchDir();
    const gp::StringView homeDir = Paths::homeDir();
    const gp::StringView appDataDir = Paths::appDataDir();
    const gp::StringView localAppDataDir = Paths::localAppDataDir();
    const gp::StringView userConfigDir = Paths::userConfigDir();
    const gp::StringView documentsDir = Paths::documentsDir();
    const gp::StringView tempDir = Paths::tempDir();

    EXPECT_FALSE(exePath.isEmpty());
    EXPECT_TRUE(Paths::isAbsolute(exePath));
    EXPECT_FALSE(exePath.contains('\\'));

    EXPECT_TRUE(Paths::isAbsolute(exeDir));
    EXPECT_TRUE(exeDir.endsWith('/'));
    EXPECT_EQ(baseDir, exeDir);

    EXPECT_TRUE(Paths::isAbsolute(cwd.view()));
    EXPECT_TRUE(cwd.view().endsWith('/'));

    EXPECT_TRUE(Paths::isAbsolute(launchDir));
    EXPECT_TRUE(launchDir.endsWith('/'));

    EXPECT_TRUE(Paths::isAbsolute(homeDir));
    EXPECT_TRUE(homeDir.endsWith('/'));

    EXPECT_TRUE(Paths::isAbsolute(appDataDir));
    EXPECT_TRUE(appDataDir.endsWith('/'));

    EXPECT_TRUE(Paths::isAbsolute(localAppDataDir));
    EXPECT_TRUE(localAppDataDir.endsWith('/'));

    EXPECT_TRUE(Paths::isAbsolute(userConfigDir));
    EXPECT_TRUE(userConfigDir.endsWith('/'));

    EXPECT_TRUE(Paths::isAbsolute(documentsDir));
    EXPECT_TRUE(documentsDir.endsWith('/'));

    EXPECT_TRUE(Paths::isAbsolute(tempDir));
    EXPECT_TRUE(tempDir.endsWith('/'));
}

TEST_F(PlatformPathsTest, EndToEndVfsMountRootConstructionAndRelativeResolution)
{
    const PathBuffer engineShaders = Paths::combine(Paths::baseDir(), "../Content/Shaders");
    const PathBuffer resolvedShaders = Paths::resolve("../Content/Shaders");
    EXPECT_EQ(Paths::createStandardFilename(engineShaders.view()).view(), resolvedShaders.view());

    const PathBuffer saveGamesDir = Paths::combine(Paths::documentsDir(), "GraphicalPlayground", "SaveGames");
    EXPECT_TRUE(Paths::isUnderDirectory(saveGamesDir.view(), Paths::documentsDir()));

    const PathBuffer shaderCacheDir = Paths::combine(Paths::localAppDataDir(), "GraphicalPlayground", "ShaderCache");
    EXPECT_TRUE(Paths::isUnderDirectory(shaderCacheDir.view(), Paths::localAppDataDir()));

    gp::StringView relativeSaveSubpath{};
    EXPECT_TRUE(
        Paths::tryMakeChildPathRelativeTo(saveGamesDir.view(), Paths::documentsDir(), relativeSaveSubpath, false)
    );
    EXPECT_EQ(relativeSaveSubpath, gp::StringView("GraphicalPlayground/SaveGames"));
}

TEST_F(PlatformPathsTest, MultiThreadedConcurrentCommandLineOverridesAreRaceFree)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 250u;

    const char* const argv[] = {
        "gp-editor.exe", "-basedir=D:\\OverrideBase", "-appdata=D:\\OverrideAppData", "-userdir=D:\\OverrideDocs"
    };
    gp::CommandLine::initialize(4, argv);
    Paths::setExecutablePathForTesting("C:\\CustomBin\\gp-editor.exe");

    gp::thread::Atomic<bool> startLatch(false);
    gp::thread::Atomic<gp::UInt32> readyThreads(0u);
    gp::thread::Atomic<gp::UInt32> validChecks(0u);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([&startLatch, &readyThreads, &validChecks]() noexcept
        {
            readyThreads.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
            while (!startLatch.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                const gp::StringView exeDir = Paths::executableDir();
                const gp::StringView base = Paths::baseDir();
                const gp::StringView appData = Paths::appDataDir();
                const gp::StringView docs = Paths::documentsDir();

                if (exeDir == "C:/CustomBin/" && base == "D:/OverrideBase/" && appData == "D:/OverrideAppData/" &&
                    docs == "D:/OverrideDocs/")
                {
                    validChecks.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
                }
            }
        });
    }

    while (readyThreads.load(gp::memory::MemoryOrder::Acquire) < kThreadCount)
    {
        std::this_thread::yield();
    }
    startLatch.store(true, gp::memory::MemoryOrder::Release);

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(validChecks.load(gp::memory::MemoryOrder::Acquire), kThreadCount * kIterationsPerThread);
}

}   // namespace gp::platform::tests
