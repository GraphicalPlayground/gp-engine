// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

#if GP_PLATFORM_WINDOWS
    #include "miscellaneous/CommandLine.hpp"
    #include "platforms/windows/system/WindowsPaths.hpp"
    #include "threading/atomics/Atomic.hpp"
    #include <thread>
    #include <vector>

namespace gp::platform::windows::tests
{

class WindowsPathsTest : public ::testing::Test
{
protected:
    void TearDown() noexcept override
    {
        Paths::resetCachesForTesting();
        gp::CommandLine::shutdown();
    }

    static void expectValidNormalizedDirectory(const gp::StringView dir) noexcept
    {
        EXPECT_FALSE(dir.isEmpty());
        EXPECT_TRUE(Paths::isAbsolute(dir));
        EXPECT_FALSE(dir.contains('\\'));
        EXPECT_TRUE(dir.endsWith('/'));
    }
};

TEST_F(WindowsPathsTest, ExecutableAndWorkingDirectoriesAreValidAndCached)
{
    const gp::StringView exePath1 = Paths::executablePath();
    const gp::StringView exePath2 = Paths::executablePath();
    ASSERT_FALSE(exePath1.isEmpty());
    EXPECT_EQ(exePath1.data(), exePath2.data());
    EXPECT_TRUE(Paths::isAbsolute(exePath1));
    EXPECT_FALSE(exePath1.contains('\\'));

    const gp::StringView exeNameNoExt = Paths::executableName(true);
    const gp::StringView exeNameWithExt = Paths::executableName(false);
    EXPECT_FALSE(exeNameNoExt.isEmpty());
    EXPECT_FALSE(exeNameWithExt.isEmpty());
    EXPECT_GE(exeNameWithExt.size(), exeNameNoExt.size());

    const gp::StringView exeDir = Paths::executableDir();
    expectValidNormalizedDirectory(exeDir);
    EXPECT_EQ(Paths::baseDir(), exeDir);
    EXPECT_TRUE(Paths::isUnderDirectory(exePath1, exeDir));

    const PathBuffer cwd = Paths::workingDir();
    expectValidNormalizedDirectory(cwd.view());

    const gp::StringView launch = Paths::launchDir();
    expectValidNormalizedDirectory(launch);
}

TEST_F(WindowsPathsTest, KnownFoldersAreResolvedAndNormalizedInBssCache)
{
    const gp::StringView home = Paths::homeDir();
    const gp::StringView appData = Paths::appDataDir();
    const gp::StringView localAppData = Paths::localAppDataDir();
    const gp::StringView userConfig = Paths::userConfigDir();
    const gp::StringView documents = Paths::documentsDir();
    const gp::StringView temp = Paths::tempDir();

    expectValidNormalizedDirectory(home);
    expectValidNormalizedDirectory(appData);
    expectValidNormalizedDirectory(localAppData);
    expectValidNormalizedDirectory(userConfig);
    expectValidNormalizedDirectory(documents);
    expectValidNormalizedDirectory(temp);

    // Verify static .bss pointer stability across repeated invocations
    EXPECT_EQ(appData.data(), Paths::appDataDir().data());
    EXPECT_EQ(localAppData.data(), Paths::localAppDataDir().data());
    EXPECT_EQ(documents.data(), Paths::documentsDir().data());
    EXPECT_EQ(userConfig, localAppData);
    EXPECT_EQ(Paths::userSettingsDir(), userConfig);
    EXPECT_EQ(Paths::userDir(), documents);
}

TEST_F(WindowsPathsTest, ResolveRelativeAgainstWindowsBaseDirAndOverrides)
{
    Paths::setBaseDirForTesting("C:\\GP\\Engine\\Binaries\\Win64");
    EXPECT_EQ(Paths::baseDir(), gp::StringView("C:/GP/Engine/Binaries/Win64/"));

    const PathBuffer resolved = Paths::resolve("../../Content/Shaders/PBR.hlsl");
    EXPECT_EQ(resolved.view(), gp::StringView("C:/GP/Engine/Content/Shaders/PBR.hlsl"));
}

TEST_F(WindowsPathsTest, MultiThreadedConcurrentLazyCacheInitializationIsRaceFree)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 250u;

    Paths::resetCachesForTesting();

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
                const gp::StringView exePath = Paths::executablePath();
                const gp::StringView exeDir = Paths::executableDir();
                const gp::StringView base = Paths::baseDir();
                const gp::StringView home = Paths::homeDir();
                const gp::StringView appData = Paths::appDataDir();
                const gp::StringView localAppData = Paths::localAppDataDir();
                const gp::StringView docs = Paths::documentsDir();
                const gp::StringView temp = Paths::tempDir();
                const gp::StringView launch = Paths::launchDir();

                if (!exePath.isEmpty() && exeDir.endsWith('/') && base.endsWith('/') && home.endsWith('/') &&
                    appData.endsWith('/') && localAppData.endsWith('/') && docs.endsWith('/') && temp.endsWith('/') &&
                    launch.endsWith('/'))
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

}   // namespace gp::platform::windows::tests

#else

namespace gp::platform::windows::tests
{

TEST(WindowsPathsPlatformGuardTest, SkippedOnNonWindowsHost)
{
    SUCCEED();
}

}   // namespace gp::platform::windows::tests

#endif
