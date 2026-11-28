// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/apple/system/AppleSharedLibrary.hpp"
#include "containers/arrays/Vector.hpp"
#include "platforms/apple/system/AppleSharedLibraryNaming.hpp"
#include <gtest/gtest.h>
#include <type_traits>

#if GP_PLATFORM_APPLE
    #include "containers/strings/FixedString.hpp"
    #include "platforms/base/system/Paths.hpp"
    #include "threading/atomics/Atomic.hpp"
    #include <thread>
    #include <unistd.h>
    #include <vector>
#endif

namespace gp::platform::apple::tests
{

static_assert(
    std::is_base_of_v<gp::platform::unix::SharedLibrary, gp::platform::apple::SharedLibrary>,
    "apple::SharedLibrary must derive from unix::SharedLibrary."
);
static_assert(!std::is_default_constructible_v<SharedLibrary>, "apple::SharedLibrary must not be constructible.");
static_assert(
    SharedLibrary::getDefaultExtension() == ".dylib", "apple::SharedLibrary default extension must be '.dylib'."
);

static_assert(SharedLibraryNaming::isDynamicLibraryFilename("libMoltenVK.dylib"), "Standard .dylib must match.");
static_assert(
    SharedLibraryNaming::isDynamicLibraryFilename("libvulkan.1.DYLIB"), "Case-insensitive .dylib must match."
);
static_assert(!SharedLibraryNaming::isDynamicLibraryFilename(".dylib"), "Empty stem before .dylib must not match.");
static_assert(!SharedLibraryNaming::isDynamicLibraryFilename("libvulkan.so"), "Unix .so must not match Apple .dylib.");

static_assert(SharedLibraryNaming::isFrameworkPath("Metal.framework"), "Bare .framework must match.");
static_assert(
    SharedLibraryNaming::isFrameworkPath("/System/Library/Frameworks/CoreFoundation.framework/"),
    "Trailing slash framework path must match."
);
static_assert(!SharedLibraryNaming::isFrameworkPath(".framework"), "Empty stem before .framework must not match.");

static_assert(
    SharedLibraryNaming::resolveFrameworkBinaryPath("/System/Library/Frameworks/Metal.framework").view() ==
        "/System/Library/Frameworks/Metal.framework/Metal",
    "resolveFrameworkBinaryPath must append the framework stem."
);
static_assert(
    SharedLibraryNaming::resolveBundleFrameworksDir("/Applications/MyGame.app/Contents/MacOS/").view() ==
        "/Applications/MyGame.app/Contents/Frameworks/",
    "resolveBundleFrameworksDir must map .app/Contents/MacOS/ to .app/Contents/Frameworks/."
);
static_assert(
    SharedLibraryNaming::resolveBundlePlugInsDir("/Applications/MyGame.app/Contents/MacOS").view() ==
        "/Applications/MyGame.app/Contents/PlugIns/",
    "resolveBundlePlugInsDir must map .app/Contents/MacOS to .app/Contents/PlugIns/."
);
static_assert(
    SharedLibraryNaming::resolveBundleFrameworksDir("/usr/local/bin/").isEmpty(),
    "Non-bundle executable directory must return an empty frameworks path."
);

TEST(AppleSharedLibraryNamingTest, ExpandsBareDylibAndFrameworkCandidates)
{
    gp::InlineVector<PathBuffer, 6u> candidates{};

    // Bare stem expands to exact, .dylib, lib*.dylib, and .framework binary.
    SharedLibraryNaming::buildCandidateFilenames("MoltenVK", candidates);
    ASSERT_EQ(candidates.size(), 4u);
    EXPECT_EQ(candidates[0].view(), gp::StringView("MoltenVK"));
    EXPECT_EQ(candidates[1].view(), gp::StringView("MoltenVK.dylib"));
    EXPECT_EQ(candidates[2].view(), gp::StringView("libMoltenVK.dylib"));
    EXPECT_EQ(candidates[3].view(), gp::StringView("MoltenVK.framework/MoltenVK"));

    // Prefixed stem omits redundant lib prefix and framework candidate.
    SharedLibraryNaming::buildCandidateFilenames("libMoltenVK", candidates);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0].view(), gp::StringView("libMoltenVK"));
    EXPECT_EQ(candidates[1].view(), gp::StringView("libMoltenVK.dylib"));

    // Unprefixed .dylib adds lib-prefixed variant.
    SharedLibraryNaming::buildCandidateFilenames("MoltenVK.dylib", candidates);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0].view(), gp::StringView("MoltenVK.dylib"));
    EXPECT_EQ(candidates[1].view(), gp::StringView("libMoltenVK.dylib"));

    // Explicit .framework expands inner binary first.
    SharedLibraryNaming::buildCandidateFilenames("Metal.framework", candidates);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0].view(), gp::StringView("Metal.framework/Metal"));
    EXPECT_EQ(candidates[1].view(), gp::StringView("Metal.framework"));
}

#if GP_PLATFORM_APPLE

class AppleSharedLibraryTest : public ::testing::Test
{
protected:
    void SetUp() noexcept override
    {
        SharedLibrary::resetSearchPaths();
    }

    void TearDown() noexcept override
    {
        SharedLibrary::resetSearchPaths();
    }
};

TEST_F(AppleSharedLibraryTest, LoadDyldSharedCacheLibraryAndFramework)
{
    void* const sysHandle = SharedLibrary::getHandle("/usr/lib/libSystem.B.dylib");
    ASSERT_NE(sysHandle, nullptr);

    using GetPidFn = pid_t (*)();
    void* const rawProc = SharedLibrary::getExport(sysHandle, "getpid");
    ASSERT_NE(rawProc, nullptr);

    const GetPidFn getPidFn = reinterpret_cast<GetPidFn>(rawProc);
    EXPECT_EQ(getPidFn(), ::getpid());

    const gp::StringView oversized("getpid_EXTRA_SUFFIX");
    EXPECT_EQ(SharedLibrary::getExport(sysHandle, oversized.substr(0u, 6u)), rawProc);
    EXPECT_EQ(SharedLibrary::getExport(sysHandle, "gp_nonexistent_apple_symbol_xyz"), nullptr);

    SharedLibrary::freeHandle(sysHandle);

    // System framework lookup via .framework bundle path.
    void* const cfHandle = SharedLibrary::getHandle("/System/Library/Frameworks/CoreFoundation.framework");
    ASSERT_NE(cfHandle, nullptr);
    EXPECT_NE(SharedLibrary::getExport(cfHandle, "CFGetTypeID"), nullptr);
    SharedLibrary::freeHandle(cfHandle);
}

TEST_F(AppleSharedLibraryTest, MultiThreadedConcurrentDyldLookupsAreRaceFree)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 64u;

    gp::thread::Atomic<bool> startLatch(false);
    gp::thread::Atomic<gp::UInt32> readyCount(0u);
    gp::thread::Atomic<gp::UInt32> successfulLookups(0u);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([&startLatch, &readyCount, &successfulLookups]() noexcept
        {
            readyCount.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
            while (!startLatch.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                void* const handle = SharedLibrary::getHandle("/usr/lib/libSystem.B.dylib");
                if (handle != nullptr)
                {
                    if (SharedLibrary::getExport(handle, "getpid") != nullptr)
                    {
                        successfulLookups.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
                    }
                    SharedLibrary::freeHandle(handle);
                }
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

    EXPECT_EQ(successfulLookups.load(gp::memory::MemoryOrder::Acquire), kThreadCount * kIterationsPerThread);
}

#endif   // GP_PLATFORM_APPLE

}   // namespace gp::platform::apple::tests
