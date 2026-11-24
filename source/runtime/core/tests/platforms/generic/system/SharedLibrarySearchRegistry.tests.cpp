// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/system/SharedLibrarySearchRegistry.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/FixedString.hpp"
#include "diagnostics/Assert.hpp"
#include "platforms/apple/system/AppleSharedLibraryNaming.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/base/system/SharedLibraryTypes.hpp"
#include "platforms/unix/system/UnixSharedLibraryNaming.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::platform::generic::tests
{

using PlatformFileSystem = gp::platform::FileSystem;
using PlatformPaths = gp::platform::Paths;

class SharedLibrarySearchRegistryTest : public ::testing::Test
{
protected:
    PathBuffer m_sandboxRoot{};
    gp::diagnostics::AssertHandlerFn m_previousAssertHandler{ nullptr };
    static inline gp::thread::Atomic<gp::UInt32> s_ensureHitCount{ 0u };

protected:
    static gp::diagnostics::AssertAction testAssertHandler(const gp::diagnostics::AssertContext& context) noexcept
    {
        if (context.kind == gp::diagnostics::AssertKind::Ensure)
        {
            s_ensureHitCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        }
        return gp::diagnostics::AssertAction::Continue;
    }

    void SetUp() noexcept override
    {
        s_ensureHitCount.store(0u, gp::memory::MemoryOrder::Relaxed);
        m_previousAssertHandler =
            gp::diagnostics::setAssertHandler(&SharedLibrarySearchRegistryTest::testAssertHandler);

        static gp::thread::Atomic<gp::UInt32> s_sandboxCounter(1u);
        const gp::UInt32 id = s_sandboxCounter.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);

        gp::FixedString<64> folderName("gp_shlib_registry_ut_");
        char digits[16]{};
        gp::UInt32 tempId = id;
        gp::USize digitLen = 0u;
        do
        {
            digits[digitLen++] = static_cast<char>('0' + (tempId % 10u));
            tempId /= 10u;
        }
        while (tempId > 0u);
        for (gp::USize i = digitLen; i > 0u; --i)
        {
            folderName.pushBack(digits[i - 1u]);
        }

        m_sandboxRoot = PlatformPaths::combine(PlatformPaths::tempDir(), folderName.view());
        if (PlatformFileSystem::exists(m_sandboxRoot.view()))
        {
            (void)PlatformFileSystem::removeDirectoryTree(m_sandboxRoot.view());
        }
        const gp::Expected<bool, FileError> createRes = PlatformFileSystem::createDirectoryTree(m_sandboxRoot.view());
        ASSERT_TRUE(createRes.hasValue());
    }

    void TearDown() noexcept override
    {
        (void)gp::diagnostics::setAssertHandler(m_previousAssertHandler);
        if (!m_sandboxRoot.isEmpty() && PlatformFileSystem::exists(m_sandboxRoot.view()))
        {
            (void)PlatformFileSystem::removeDirectoryTree(m_sandboxRoot.view());
        }
    }
};

TEST_F(SharedLibrarySearchRegistryTest, LifoStackPushPopAndMismatchSafety)
{
    SharedLibrarySearchRegistry registry(
        SharedLibrarySearchPolicy{
          .defaultExtension = ".so",
          .fileFilter = &gp::platform::unix::SharedLibraryNaming::isSharedObjectFilename,
          .caseSensitive = true,
        }
    );

    const PathBuffer dirA = PlatformPaths::combine(m_sandboxRoot.view(), "DirA");
    const PathBuffer dirB = PlatformPaths::combine(m_sandboxRoot.view(), "DirB");

    EXPECT_FALSE(registry.hasActivePushedDirectory());
    EXPECT_EQ(registry.getDirectoryStackDepth(), 0u);
    EXPECT_TRUE(registry.getActiveDirectory().isEmpty());

    // Empty push and pop on empty stack must fail safely without corrupting state.
    EXPECT_FALSE(registry.pushDirectory(""));
    EXPECT_FALSE(registry.popDirectory(dirA.view()));
    EXPECT_EQ(registry.getDirectoryStackDepth(), 0u);

    PathBuffer normalizedOut{};
    EXPECT_TRUE(registry.pushDirectory(dirA.view(), &normalizedOut));
    EXPECT_EQ(registry.getDirectoryStackDepth(), 1u);
    EXPECT_TRUE(registry.hasActivePushedDirectory());
    EXPECT_TRUE(PlatformPaths::equals(registry.getActiveDirectory().view(), dirA.view(), true));
    EXPECT_TRUE(PlatformPaths::equals(normalizedOut.view(), dirA.view(), true));

    EXPECT_TRUE(registry.pushDirectory(dirB.view()));
    EXPECT_EQ(registry.getDirectoryStackDepth(), 2u);
    EXPECT_TRUE(PlatformPaths::equals(registry.getActiveDirectory().view(), dirB.view(), true));

    // Mismatched pop must leave the stack untouched.
    EXPECT_FALSE(registry.popDirectory(dirA.view()));
    EXPECT_EQ(registry.getDirectoryStackDepth(), 2u);
    EXPECT_TRUE(PlatformPaths::equals(registry.getActiveDirectory().view(), dirB.view(), true));

    // Popping B restores A.
    PathBuffer nextActive{};
    EXPECT_TRUE(registry.popDirectory(dirB.view(), &nextActive));
    EXPECT_EQ(registry.getDirectoryStackDepth(), 1u);
    EXPECT_TRUE(PlatformPaths::equals(nextActive.view(), dirA.view(), true));

    // Popping A empties the stack.
    EXPECT_TRUE(registry.popDirectory(dirA.view(), &nextActive));
    EXPECT_EQ(registry.getDirectoryStackDepth(), 0u);
    EXPECT_TRUE(nextActive.isEmpty());
    EXPECT_FALSE(registry.hasActivePushedDirectory());
}

TEST_F(SharedLibrarySearchRegistryTest, UnixPolicyIndexesSharedObjectsAndVersionedSonamesOnly)
{
    const PathBuffer libDir = PlatformPaths::combine(m_sandboxRoot.view(), "UnixLibs");
    ASSERT_TRUE(PlatformFileSystem::createDirectoryTree(libDir.view()).hasValue());

    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(libDir.view(), "libvulkan.so").view(), "elf").hasValue()
    );
    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(libDir.view(), "libvulkan.so.1").view(), "elf")
            .hasValue()
    );
    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(libDir.view(), "libGL.so.1.7.0").view(), "elf")
            .hasValue()
    );
    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(libDir.view(), "libvulkan.so.bak").view(), "bak")
            .hasValue()
    );
    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(libDir.view(), "notes.txt").view(), "txt").hasValue()
    );
    ASSERT_TRUE(
        PlatformFileSystem::createDirectory(PlatformPaths::combine(libDir.view(), "nested.so").view()).hasValue()
    );

    SharedLibrarySearchRegistry registry(
        SharedLibrarySearchPolicy{
          .defaultExtension = ".so",
          .fileFilter = &gp::platform::unix::SharedLibraryNaming::isSharedObjectFilename,
          .caseSensitive = true,
        }
    );

    EXPECT_TRUE(registry.addDirectory(libDir.view()));
    EXPECT_EQ(registry.getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(registry.getCachedLibraryCount(), 3u);

    // Re-adding the same normalized directory with trailing slash must deduplicate.
    PathBuffer withSlash = libDir;
    withSlash.pushBack('/');
    EXPECT_TRUE(registry.addDirectory(withSlash.view()));
    EXPECT_EQ(registry.getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(registry.getCachedLibraryCount(), 3u);

    // Candidate collection for "vulkan" should resolve cached "libvulkan.so".
    gp::InlineVector<PathBuffer, 4u> names{};
    gp::platform::unix::SharedLibraryNaming::buildCandidateFilenames("vulkan", names);

    SharedLibraryCandidateBatch batch{};
    registry.collectCandidates(names.data(), names.size(), batch);
    ASSERT_EQ(batch.prioritizedCandidates.size(), 1u);
    EXPECT_TRUE(
        PlatformPaths::equals(
            batch.prioritizedCandidates[0].view(), PlatformPaths::combine(libDir.view(), "libvulkan.so").view(), true
        )
    );
    EXPECT_EQ(batch.fallbackDirs.size(), 1u);
}

TEST_F(SharedLibrarySearchRegistryTest, ApplePolicyDeduplicatesCaseInsensitivelyAndIndexesDylibs)
{
    const PathBuffer fwDir = PlatformPaths::combine(m_sandboxRoot.view(), "AppleLibs");
    ASSERT_TRUE(PlatformFileSystem::createDirectoryTree(fwDir.view()).hasValue());

    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(fwDir.view(), "libMoltenVK.dylib").view(), "macho")
            .hasValue()
    );
    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(fwDir.view(), "LegacyPlugin.DYLIB").view(), "macho")
            .hasValue()
    );
    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(fwDir.view(), "libvulkan.so").view(), "elf").hasValue()
    );
    ASSERT_TRUE(
        PlatformFileSystem::createDirectory(PlatformPaths::combine(fwDir.view(), "Fake.dylib").view()).hasValue()
    );

    SharedLibrarySearchRegistry registry(
        SharedLibrarySearchPolicy{
          .defaultExtension = ".dylib",
          .fileFilter = &gp::platform::apple::SharedLibraryNaming::isDynamicLibraryFilename,
          .caseSensitive = false,
        }
    );

    EXPECT_TRUE(registry.addDirectory(fwDir.view()));
    EXPECT_EQ(registry.getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(registry.getCachedLibraryCount(), 2u);

    // Uppercase directory variant must deduplicate under Apple case-insensitive policy.
    PathBuffer upperDir = fwDir;
    for (gp::USize i = 0u; i < upperDir.size(); ++i)
    {
        upperDir[static_cast<PathBuffer::SizeType>(i)] = PlatformPaths::toUpperAscii(upperDir[i]);
    }
    EXPECT_TRUE(registry.addDirectory(upperDir.view()));
    EXPECT_EQ(registry.getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(registry.getCachedLibraryCount(), 2u);

    // Push and pop using different casing must succeed.
    EXPECT_TRUE(registry.pushDirectory(fwDir.view()));
    EXPECT_TRUE(registry.popDirectory(upperDir.view()));
    EXPECT_EQ(registry.getDirectoryStackDepth(), 0u);
}

TEST_F(SharedLibrarySearchRegistryTest, CandidatePriorityOrdersStackBeforeCacheAndSupportsLazyRegistration)
{
    const PathBuffer globalDir = PlatformPaths::combine(m_sandboxRoot.view(), "Global");
    const PathBuffer pushedDir = PlatformPaths::combine(m_sandboxRoot.view(), "Pushed");
    ASSERT_TRUE(PlatformFileSystem::createDirectoryTree(globalDir.view()).hasValue());
    ASSERT_TRUE(PlatformFileSystem::createDirectoryTree(pushedDir.view()).hasValue());
    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(globalDir.view(), "libTest.so").view(), "elf")
            .hasValue()
    );

    SharedLibrarySearchRegistry registry(
        SharedLibrarySearchPolicy{
          .defaultExtension = ".so",
          .fileFilter = &gp::platform::unix::SharedLibraryNaming::isSharedObjectFilename,
          .caseSensitive = true,
        }
    );

    ASSERT_TRUE(registry.addDirectory(globalDir.view()));
    ASSERT_TRUE(registry.pushDirectory(pushedDir.view()));

    const PathBuffer queryName("libTest.so");
    SharedLibraryCandidateBatch batch{};
    registry.collectCandidates(&queryName, 1u, batch);

    ASSERT_EQ(batch.prioritizedCandidates.size(), 2u);
    EXPECT_TRUE(batch.hasActivePushedDir);
    EXPECT_TRUE(
        PlatformPaths::equals(
            batch.prioritizedCandidates[0].view(), PlatformPaths::combine(pushedDir.view(), "libTest.so").view(), true
        )
    );
    EXPECT_TRUE(
        PlatformPaths::equals(
            batch.prioritizedCandidates[1].view(), PlatformPaths::combine(globalDir.view(), "libTest.so").view(), true
        )
    );

    // Lazy hot-reload registration inserts once and ignores duplicates or stale indices.
    ASSERT_EQ(batch.fallbackDirs.size(), 1u);
    registry.registerDiscoveredFile(
        batch.fallbackDirs[0].directoryIndex, batch.fallbackDirs[0].directory.view(), "libHot.so"
    );
    EXPECT_EQ(registry.getCachedLibraryCount(), 2u);

    registry.registerDiscoveredFile(
        batch.fallbackDirs[0].directoryIndex, batch.fallbackDirs[0].directory.view(), "libHot.so"
    );
    EXPECT_EQ(registry.getCachedLibraryCount(), 2u);

    registry.registerDiscoveredFile(99u, batch.fallbackDirs[0].directory.view(), "libStale.so");
    EXPECT_EQ(registry.getCachedLibraryCount(), 2u);

    registry.reset();
    EXPECT_EQ(registry.getDirectoryStackDepth(), 0u);
    EXPECT_EQ(registry.getRegisteredDirectoryCount(), 0u);
    EXPECT_EQ(registry.getCachedLibraryCount(), 0u);
}

TEST_F(SharedLibrarySearchRegistryTest, ConcurrentAddAndCollectAreRaceFree)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 64u;

    const PathBuffer sharedDir = PlatformPaths::combine(m_sandboxRoot.view(), "ConcurrentLibs");
    ASSERT_TRUE(PlatformFileSystem::createDirectoryTree(sharedDir.view()).hasValue());
    ASSERT_TRUE(
        PlatformFileSystem::writeAllText(PlatformPaths::combine(sharedDir.view(), "libConcurrent.so").view(), "elf")
            .hasValue()
    );

    SharedLibrarySearchRegistry registry(
        SharedLibrarySearchPolicy{
          .defaultExtension = ".so",
          .fileFilter = &gp::platform::unix::SharedLibraryNaming::isSharedObjectFilename,
          .caseSensitive = true,
        }
    );

    gp::thread::Atomic<bool> startLatch(false);
    gp::thread::Atomic<gp::UInt32> readyCount(0u);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([t, &registry, &sharedDir, &startLatch, &readyCount]() noexcept
        {
            readyCount.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
            while (!startLatch.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            const PathBuffer query("libConcurrent.so");
            SharedLibraryCandidateBatch batch{};

            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                if ((t % 2u) == 0u)
                {
                    (void)registry.addDirectory(sharedDir.view());
                }
                registry.collectCandidates(&query, 1u, batch);
            }
        });
    }

    while (readyCount.load(gp::memory::MemoryOrder::Acquire) < kThreadCount)
    {
        std::this_thread::yield();
    }
    startLatch.store(true, gp::memory::MemoryOrder::Release);

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(registry.getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(registry.getCachedLibraryCount(), 1u);
}

}   // namespace gp::platform::generic::tests
