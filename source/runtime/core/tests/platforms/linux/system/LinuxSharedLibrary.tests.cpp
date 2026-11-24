// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/linux/system/LinuxSharedLibrary.hpp"
#include <gtest/gtest.h>
#include <type_traits>

#if GP_PLATFORM_LINUX
    #include "containers/strings/FixedString.hpp"
    #include "platforms/base/system/FileSystem.hpp"
    #include "platforms/base/system/Paths.hpp"
    #include "threading/atomics/Atomic.hpp"
    #include <thread>
    #include <unistd.h>
    #include <vector>
#endif

namespace gp::platform::linux::tests
{

static_assert(
    std::is_base_of_v<gp::platform::unix::SharedLibrary, gp::platform::linux::SharedLibrary>,
    "linux::SharedLibrary must derive from unix::SharedLibrary."
);
static_assert(std::is_final_v<SharedLibrary>, "linux::SharedLibrary must be final.");
static_assert(SharedLibrary::getDefaultExtension() == ".so", "linux::SharedLibrary default extension must be '.so'.");

#if GP_PLATFORM_LINUX

class LinuxSharedLibraryTest : public ::testing::Test
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

TEST_F(LinuxSharedLibraryTest, LoadSystemLibraryAndResolveExportsSafely)
{
    void* const libcHandle = SharedLibrary::getHandle("libc.so.6");
    ASSERT_NE(libcHandle, nullptr);

    using GetPidFn = pid_t (*)();
    void* const rawProc = SharedLibrary::getExport(libcHandle, "getpid");
    ASSERT_NE(rawProc, nullptr);

    const GetPidFn getPidFn = reinterpret_cast<GetPidFn>(rawProc);
    EXPECT_EQ(getPidFn(), ::getpid());

    // Non-null-terminated StringView slice must resolve identically without reading past the slice.
    const gp::StringView oversized("getpid_TRAILING_BYTES");
    EXPECT_EQ(SharedLibrary::getExport(libcHandle, oversized.substr(0u, 6u)), rawProc);

    // Missing and empty symbols return nullptr.
    EXPECT_EQ(SharedLibrary::getExport(libcHandle, "gp_nonexistent_export_symbol_xyz"), nullptr);
    EXPECT_EQ(SharedLibrary::getExport(libcHandle, ""), nullptr);

    // Long non-existent symbol exercises the heap allocation fallback.
    gp::FixedString<600> longSymbol{};
    for (gp::USize i = 0u; i < 580u; ++i)
    {
        longSymbol.pushBack('A');
    }
    EXPECT_EQ(SharedLibrary::getExport(libcHandle, longSymbol.view()), nullptr);

    SharedLibrary::freeHandle(libcHandle);
}

TEST_F(LinuxSharedLibraryTest, MultiThreadedConcurrentLookupsAreRaceFree)
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
                void* const handle = SharedLibrary::getHandle("libc.so.6");
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

#endif   // GP_PLATFORM_LINUX

}   // namespace gp::platform::linux::tests
