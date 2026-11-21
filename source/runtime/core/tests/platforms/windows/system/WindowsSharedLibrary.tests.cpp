// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

#if GP_PLATFORM_WINDOWS
    #include "containers/strings/FixedString.hpp"
    #include "diagnostics/Assert.hpp"
    #include "miscellaneous/CommandLine.hpp"
    #include "platforms/base/system/FileSystem.hpp"
    #include "platforms/base/system/Paths.hpp"
    #include "platforms/windows/system/WindowsSharedLibrary.hpp"
    #include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep
    #include "threading/atomics/Atomic.hpp"
    #include <thread>
    #include <vector>

namespace gp::platform::windows::tests
{

namespace
{

/// @brief Queries Win32 `GetDllDirectoryW()` and returns it normalized to UTF-8 with `'/'` separators.
[[nodiscard]] PathBuffer queryOsDllDirectoryUtf8() noexcept
{
    wchar_t wideBuf[kMaxPathLength + 16u]{};
    const DWORD len = ::GetDllDirectoryW(static_cast<DWORD>(kMaxPathLength + 16u), wideBuf);
    if (len == 0u || len >= (kMaxPathLength + 16u))
    {
        return {};
    }

    char utf8Buf[kMaxPathLength + 16u]{};
    const int written = ::WideCharToMultiByte(
        CP_UTF8, 0u, wideBuf, static_cast<int>(len), utf8Buf, static_cast<int>(kMaxPathLength + 15u), nullptr, nullptr
    );
    if (written <= 0)
    {
        return {};
    }

    PathBuffer result(gp::StringView(utf8Buf, static_cast<gp::USize>(written)));
    Paths::normalizeDirectoryName(result);
    return result;
}

/// @brief Queries Win32 `GetModuleFileNameW()` for a loaded `HMODULE` and normalizes it to UTF-8.
[[nodiscard]] PathBuffer queryModulePathUtf8(void* const handle) noexcept
{
    if (handle == nullptr)
    {
        return {};
    }

    wchar_t wideBuf[kMaxPathLength + 16u]{};
    const DWORD len =
        ::GetModuleFileNameW(static_cast<HMODULE>(handle), wideBuf, static_cast<DWORD>(kMaxPathLength + 16u));
    if (len == 0u || len >= (kMaxPathLength + 16u))
    {
        return {};
    }

    char utf8Buf[kMaxPathLength + 16u]{};
    const int written = ::WideCharToMultiByte(
        CP_UTF8, 0u, wideBuf, static_cast<int>(len), utf8Buf, static_cast<int>(kMaxPathLength + 15u), nullptr, nullptr
    );
    if (written <= 0)
    {
        return {};
    }

    PathBuffer result(gp::StringView(utf8Buf, static_cast<gp::USize>(written)));
    Paths::makeStandardFilename(result);
    return result;
}

/// @brief Locates a real, self-contained DLL on disk that can be copied under unique test filenames to exercise real
///        Windows loader discovery without relying on `KnownDLLs` redirection.
/// @param[out] outDllPath Receives the source DLL path on disk.
/// @param[out] outExportName Receives the name of an exported function inside that DLL.
/// @return True if a suitable template DLL was found, false otherwise.
[[nodiscard]] bool findCopyableTestDll(PathBuffer& outDllPath, gp::StringView& outExportName) noexcept
{
    // Prefer tbbmalloc.dll shipped alongside test executables (zero system KnownDLL redirection).
    const PathBuffer localTbb = Paths::combine(Paths::executableDir(), "tbbmalloc.dll");
    if (FileSystem::isFile(localTbb.view()))
    {
        outDllPath = localTbb;
        outExportName = "scalable_malloc";
        return true;
    }

    // Fallback: locate a small non-KnownDLL system library in System32.
    wchar_t sysDirWide[kMaxPathLength + 16u]{};
    const UINT sysDirLen = ::GetSystemDirectoryW(sysDirWide, static_cast<UINT>(kMaxPathLength + 16u));
    if (sysDirLen > 0u && sysDirLen < (kMaxPathLength + 16u))
    {
        char sysDirUtf8[kMaxPathLength + 16u]{};
        const int written = ::WideCharToMultiByte(
            CP_UTF8,
            0u,
            sysDirWide,
            static_cast<int>(sysDirLen),
            sysDirUtf8,
            static_cast<int>(kMaxPathLength + 15u),
            nullptr,
            nullptr
        );
        if (written > 0)
        {
            const gp::StringView sysDirView(sysDirUtf8, static_cast<gp::USize>(written));
            const PathBuffer versionDll = Paths::combine(sysDirView, "version.dll");
            if (FileSystem::isFile(versionDll.view()))
            {
                outDllPath = versionDll;
                outExportName = "GetFileVersionInfoSizeW";
                return true;
            }
        }
    }

    return false;
}

}   // namespace

class WindowsSharedLibraryTest : public ::testing::Test
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
            return gp::diagnostics::AssertAction::Continue;
        }
        return gp::diagnostics::AssertAction::Continue;
    }

    void SetUp() noexcept override
    {
        SharedLibrary::resetSearchPaths();
        gp::CommandLine::shutdown();
        s_ensureHitCount.store(0u, gp::memory::MemoryOrder::Relaxed);
        m_previousAssertHandler = gp::diagnostics::setAssertHandler(&WindowsSharedLibraryTest::testAssertHandler);

        static gp::thread::Atomic<gp::UInt32> s_sandboxCounter(1u);
        const gp::UInt32 id = s_sandboxCounter.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);

        gp::FixedString<64> folderName("gp_shlib_test_sandbox_");
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

        m_sandboxRoot = Paths::combine(Paths::tempDir(), folderName.view());
        if (FileSystem::exists(m_sandboxRoot.view()))
        {
            (void)FileSystem::removeDirectoryTree(m_sandboxRoot.view());
        }
        const gp::Expected<bool, FileError> createRes = FileSystem::createDirectoryTree(m_sandboxRoot.view());
        ASSERT_TRUE(createRes.hasValue());
    }

    void TearDown() noexcept override
    {
        SharedLibrary::resetSearchPaths();
        gp::CommandLine::shutdown();
        (void)gp::diagnostics::setAssertHandler(m_previousAssertHandler);

        if (!m_sandboxRoot.isEmpty() && FileSystem::exists(m_sandboxRoot.view()))
        {
            (void)FileSystem::removeDirectoryTree(m_sandboxRoot.view());
        }
    }
};

TEST_F(WindowsSharedLibraryTest, LoadSystemLibraryAndResolveExportsSafely)
{
    void* const kernelHandle = SharedLibrary::getHandle("kernel32.dll");
    ASSERT_NE(kernelHandle, nullptr);

    // Valid exported symbol lookup and real invocation.
    using GetCurrentProcessIdFn = DWORD(__stdcall*)();
    void* const rawProc = SharedLibrary::getExport(kernelHandle, "GetCurrentProcessId");
    ASSERT_NE(rawProc, nullptr);

    const GetCurrentProcessIdFn getPid = reinterpret_cast<GetCurrentProcessIdFn>(rawProc);
    EXPECT_EQ(getPid(), ::GetCurrentProcessId());

    // Non-null-terminated StringView slice must resolve identically without buffer over-read.
    const gp::StringView oversizedLiteral("GetCurrentProcessId_TRAILING_BYTES_NOT_PART_OF_NAME");
    const gp::StringView nonTerminatedSlice = oversizedLiteral.substr(0u, 19u);
    void* const sliceProc = SharedLibrary::getExport(kernelHandle, nonTerminatedSlice);
    EXPECT_EQ(sliceProc, rawProc);

    // Missing symbol and empty symbol must return nullptr.
    EXPECT_EQ(SharedLibrary::getExport(kernelHandle, "GP_NonExistent_Export_Symbol_XYZ"), nullptr);
    EXPECT_EQ(SharedLibrary::getExport(kernelHandle, ""), nullptr);

    // Very long non-existent symbol exercises heap fallback path safely.
    gp::FixedString<600> longSymbol{};
    for (gp::USize i = 0u; i < 580u; ++i)
    {
        longSymbol.pushBack('A');
    }
    EXPECT_EQ(SharedLibrary::getExport(kernelHandle, longSymbol.view()), nullptr);

    SharedLibrary::freeHandle(kernelHandle);
}

TEST_F(WindowsSharedLibraryTest, SystemLibraryImplicitExtensionCaseInsensitivityAndRefcountBalance)
{
    // Implicit ".dll" extension and uppercase lookup.
    void* const handleNoExt = SharedLibrary::getHandle("kernel32");
    ASSERT_NE(handleNoExt, nullptr);

    void* const handleUpper = SharedLibrary::getHandle("KERNEL32.DLL");
    ASSERT_NE(handleUpper, nullptr);
    EXPECT_EQ(handleNoExt, handleUpper);

    // Non-null-terminated StringView slice for filename.
    const gp::StringView oversizedFilename("kernel32.dll_EXTRA_GARBAGE");
    void* const handleSlice = SharedLibrary::getHandle(oversizedFilename.substr(0u, 12u));
    ASSERT_NE(handleSlice, nullptr);
    EXPECT_EQ(handleSlice, handleNoExt);

    // Repeated getHandle / freeHandle pairs must keep OS loader refcounts strictly balanced.
    constexpr gp::UInt32 kIterations = 32u;
    for (gp::UInt32 i = 0u; i < kIterations; ++i)
    {
        void* const tempHandle = SharedLibrary::getHandle("kernel32.dll");
        ASSERT_EQ(tempHandle, handleNoExt);
        SharedLibrary::freeHandle(tempHandle);
    }

    // Export resolution must still succeed after all balanced frees.
    EXPECT_NE(SharedLibrary::getExport(handleNoExt, "GetCurrentProcessId"), nullptr);

    SharedLibrary::freeHandle(handleSlice);
    SharedLibrary::freeHandle(handleUpper);
    SharedLibrary::freeHandle(handleNoExt);
}

TEST_F(WindowsSharedLibraryTest, PushAndPopDirectoryMaintainLifoStackAndUpdateWin32DllDirectory)
{
    const PathBuffer dirA = Paths::combine(m_sandboxRoot.view(), "DirA");
    const PathBuffer dirB = Paths::combine(m_sandboxRoot.view(), "DirB");
    const PathBuffer dirC = Paths::combine(m_sandboxRoot.view(), "DirC");

    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);
    EXPECT_TRUE(SharedLibrary::getActiveDirectory().isEmpty());

    // Push A
    SharedLibrary::pushDirectory(dirA.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 1u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), dirA.view(), false));
    EXPECT_TRUE(Paths::equals(queryOsDllDirectoryUtf8().view(), dirA.view(), false));

    // Push B
    SharedLibrary::pushDirectory(dirB.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 2u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), dirB.view(), false));
    EXPECT_TRUE(Paths::equals(queryOsDllDirectoryUtf8().view(), dirB.view(), false));

    // Push C
    SharedLibrary::pushDirectory(dirC.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 3u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), dirC.view(), false));
    EXPECT_TRUE(Paths::equals(queryOsDllDirectoryUtf8().view(), dirC.view(), false));

    // Pop C -> restores B
    SharedLibrary::popDirectory(dirC.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 2u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), dirB.view(), false));
    EXPECT_TRUE(Paths::equals(queryOsDllDirectoryUtf8().view(), dirB.view(), false));

    // Pop B -> restores A
    SharedLibrary::popDirectory(dirB.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 1u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), dirA.view(), false));
    EXPECT_TRUE(Paths::equals(queryOsDllDirectoryUtf8().view(), dirA.view(), false));

    // Pop A -> clears OS DLL directory
    SharedLibrary::popDirectory(dirA.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);
    EXPECT_TRUE(SharedLibrary::getActiveDirectory().isEmpty());
    EXPECT_TRUE(queryOsDllDirectoryUtf8().isEmpty());
}

TEST_F(WindowsSharedLibraryTest, PushAndPopDirectorySupportReentrantDuplicatesAndPathNormalization)
{
    PathBuffer backslashDir = Paths::combine(m_sandboxRoot.view(), "Nested/SubDir");
    Paths::makePlatformFilename(backslashDir);
    backslashDir.pushBack('\\');

    const PathBuffer collapsedEquivalent = Paths::combine(m_sandboxRoot.view(), "Nested/Other/../SubDir/.");
    const PathBuffer canonicalDir = Paths::combine(m_sandboxRoot.view(), "Nested/SubDir");

    // Re-entrant nested pushes of the same logical directory via differing formatting.
    SharedLibrary::pushDirectory(backslashDir.view());
    SharedLibrary::pushDirectory(collapsedEquivalent.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 2u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), canonicalDir.view(), false));

    // Pop using uppercase/canonical variant must match top entry.
    PathBuffer upperVariant = canonicalDir;
    for (gp::USize i = 0u; i < upperVariant.size(); ++i)
    {
        upperVariant[static_cast<PathBuffer::SizeType>(i)] = Paths::toUpperAscii(upperVariant[i]);
    }
    SharedLibrary::popDirectory(upperVariant.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 1u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), canonicalDir.view(), false));

    // Second pop empties the stack cleanly.
    SharedLibrary::popDirectory(canonicalDir.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);
    EXPECT_TRUE(queryOsDllDirectoryUtf8().isEmpty());
}

TEST_F(WindowsSharedLibraryTest, PopDirectoryMismatchAndEmptyStackDoNotCorruptState)
{
    const PathBuffer dirA = Paths::combine(m_sandboxRoot.view(), "SafeA");
    const PathBuffer dirB = Paths::combine(m_sandboxRoot.view(), "SafeB");

    // Pop on empty stack must be a safe no-op.
    SharedLibrary::popDirectory(dirA.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);

    // Push empty directory must be rejected safely.
    SharedLibrary::pushDirectory("");
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);

    // Push A, then attempt mismatched pop with B -> A must remain untouched on top.
    SharedLibrary::pushDirectory(dirA.view());
    ASSERT_EQ(SharedLibrary::getDirectoryStackDepth(), 1u);

    SharedLibrary::popDirectory(dirB.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 1u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), dirA.view(), false));
    EXPECT_TRUE(Paths::equals(queryOsDllDirectoryUtf8().view(), dirA.view(), false));

    // Pop with empty path while stack is non-empty -> A must remain untouched.
    SharedLibrary::popDirectory("");
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 1u);

    // Matching pop cleans up properly.
    SharedLibrary::popDirectory(dirA.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);
}

TEST_F(WindowsSharedLibraryTest, AddDirectoryIndexesDllFilesOnlyAndDeduplicatesCaseInsensitively)
{
    const PathBuffer pluginDir = Paths::combine(m_sandboxRoot.view(), "Plugins");
    ASSERT_TRUE(FileSystem::createDirectoryTree(pluginDir.view()).hasValue());

    // Populate with a mix of .dll, .DLL, non-DLL files, and a subdirectory named "Fake.dll".
    ASSERT_TRUE(FileSystem::writeAllText(Paths::combine(pluginDir.view(), "Alpha.dll").view(), "stub").hasValue());
    ASSERT_TRUE(FileSystem::writeAllText(Paths::combine(pluginDir.view(), "Beta.DLL").view(), "stub").hasValue());
    ASSERT_TRUE(FileSystem::writeAllText(Paths::combine(pluginDir.view(), "Readme.txt").view(), "text").hasValue());
    ASSERT_TRUE(FileSystem::writeAllText(Paths::combine(pluginDir.view(), "Symbols.pdb").view(), "pdb").hasValue());
    ASSERT_TRUE(FileSystem::createDirectory(Paths::combine(pluginDir.view(), "SubFolder.dll").view()).hasValue());

    SharedLibrary::addDirectory(pluginDir.view());
    EXPECT_EQ(SharedLibrary::getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(SharedLibrary::getCachedLibraryCount(), 2u);

    // Re-adding with backslashes, trailing separator, and uppercase must deduplicate completely.
    PathBuffer altVariant = pluginDir;
    Paths::makePlatformFilename(altVariant);
    altVariant.pushBack('\\');
    for (gp::USize i = 0u; i < altVariant.size(); ++i)
    {
        altVariant[static_cast<PathBuffer::SizeType>(i)] = Paths::toUpperAscii(altVariant[i]);
    }
    SharedLibrary::addDirectory(altVariant.view());
    EXPECT_EQ(SharedLibrary::getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(SharedLibrary::getCachedLibraryCount(), 2u);

    // Empty path must be rejected safely.
    SharedLibrary::addDirectory("");
    EXPECT_EQ(SharedLibrary::getRegisteredDirectoryCount(), 1u);
}

TEST_F(WindowsSharedLibraryTest, LoadRealDllFromRegisteredSearchPathCacheAndHotReloadFallback)
{
    PathBuffer sourceDllPath{};
    gp::StringView expectedExport{};
    if (!findCopyableTestDll(sourceDllPath, expectedExport))
    {
        GTEST_SKIP() << "No copyable test DLL found on this host.";
    }

    const PathBuffer dirCached = Paths::combine(m_sandboxRoot.view(), "CachedDir");
    const PathBuffer dirHotReload = Paths::combine(m_sandboxRoot.view(), "HotReloadDir");
    ASSERT_TRUE(FileSystem::createDirectoryTree(dirCached.view()).hasValue());
    ASSERT_TRUE(FileSystem::createDirectoryTree(dirHotReload.view()).hasValue());

    // Stage a uniquely named DLL in dirCached BEFORE addDirectory().
    const PathBuffer cachedDllPath = Paths::combine(dirCached.view(), "gp_ut_cached_lib.dll");
    ASSERT_TRUE(FileSystem::copyFile(sourceDllPath.view(), cachedDllPath.view(), true).hasValue());

    // Without registering the directory, loading by bare filename must fail.
    EXPECT_EQ(SharedLibrary::getHandle("gp_ut_cached_lib.dll"), nullptr);

    // Register dirCached -> indexes gp_ut_cached_lib.dll immediately.
    SharedLibrary::addDirectory(dirCached.view());
    EXPECT_EQ(SharedLibrary::getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(SharedLibrary::getCachedLibraryCount(), 1u);

    // Load via cached bare filename, implicit extension, and uppercase casing.
    void* const handleExact = SharedLibrary::getHandle("gp_ut_cached_lib.dll");
    ASSERT_NE(handleExact, nullptr);
    EXPECT_TRUE(Paths::equals(queryModulePathUtf8(handleExact).view(), cachedDllPath.view(), false));
    EXPECT_NE(SharedLibrary::getExport(handleExact, expectedExport), nullptr);

    void* const handleNoExt = SharedLibrary::getHandle("gp_ut_cached_lib");
    ASSERT_EQ(handleNoExt, handleExact);

    void* const handleUpper = SharedLibrary::getHandle("GP_UT_CACHED_LIB.DLL");
    ASSERT_EQ(handleUpper, handleExact);

    SharedLibrary::freeHandle(handleUpper);
    SharedLibrary::freeHandle(handleNoExt);
    SharedLibrary::freeHandle(handleExact);

    // Verify FreeLibrary released the OS image lock so the file can be removed cleanly.
    EXPECT_TRUE(FileSystem::removeFile(cachedDllPath.view()).hasValue());

    // Hot-reload / post-registration cache-miss fallback: register empty dirHotReload first, then drop a DLL in it.
    SharedLibrary::addDirectory(dirHotReload.view());
    EXPECT_EQ(SharedLibrary::getRegisteredDirectoryCount(), 2u);
    EXPECT_EQ(SharedLibrary::getCachedLibraryCount(), 1u);

    const PathBuffer hotDllPath = Paths::combine(dirHotReload.view(), "gp_ut_hotreload_lib.dll");
    ASSERT_TRUE(FileSystem::copyFile(sourceDllPath.view(), hotDllPath.view(), true).hasValue());

    void* const hotHandle = SharedLibrary::getHandle("gp_ut_hotreload_lib.dll");
    ASSERT_NE(hotHandle, nullptr);
    EXPECT_TRUE(Paths::equals(queryModulePathUtf8(hotHandle).view(), hotDllPath.view(), false));
    EXPECT_NE(SharedLibrary::getExport(hotHandle, expectedExport), nullptr);

    // Lazy cache insertion should now include the newly discovered DLL.
    EXPECT_EQ(SharedLibrary::getCachedLibraryCount(), 2u);

    SharedLibrary::freeHandle(hotHandle);
    EXPECT_TRUE(FileSystem::removeFile(hotDllPath.view()).hasValue());
}

TEST_F(WindowsSharedLibraryTest, PushDirectoryStackTakesPrecedenceOverRegisteredSearchPaths)
{
    PathBuffer sourceDllPath{};
    gp::StringView expectedExport{};
    if (!findCopyableTestDll(sourceDllPath, expectedExport))
    {
        GTEST_SKIP() << "No copyable test DLL found on this host.";
    }

    const PathBuffer registeredDir = Paths::combine(m_sandboxRoot.view(), "GlobalSearchDir");
    const PathBuffer pushedDir = Paths::combine(m_sandboxRoot.view(), "ScopedPushDir");
    ASSERT_TRUE(FileSystem::createDirectoryTree(registeredDir.view()).hasValue());
    ASSERT_TRUE(FileSystem::createDirectoryTree(pushedDir.view()).hasValue());

    const PathBuffer globalDllPath = Paths::combine(registeredDir.view(), "gp_ut_shadow_lib.dll");
    const PathBuffer scopedDllPath = Paths::combine(pushedDir.view(), "gp_ut_shadow_lib.dll");
    ASSERT_TRUE(FileSystem::copyFile(sourceDllPath.view(), globalDllPath.view(), true).hasValue());
    ASSERT_TRUE(FileSystem::copyFile(sourceDllPath.view(), scopedDllPath.view(), true).hasValue());

    // Register GlobalSearchDir persistently, then push ScopedPushDir on top of the LIFO stack.
    SharedLibrary::addDirectory(registeredDir.view());
    SharedLibrary::pushDirectory(pushedDir.view());

    void* const scopedHandle = SharedLibrary::getHandle("gp_ut_shadow_lib.dll");
    ASSERT_NE(scopedHandle, nullptr);
    EXPECT_TRUE(Paths::equals(queryModulePathUtf8(scopedHandle).view(), scopedDllPath.view(), false));
    EXPECT_NE(SharedLibrary::getExport(scopedHandle, expectedExport), nullptr);
    SharedLibrary::freeHandle(scopedHandle);

    // Pop ScopedPushDir -> subsequent lookup must resolve from GlobalSearchDir.
    SharedLibrary::popDirectory(pushedDir.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);

    void* const globalHandle = SharedLibrary::getHandle("gp_ut_shadow_lib.dll");
    ASSERT_NE(globalHandle, nullptr);
    EXPECT_TRUE(Paths::equals(queryModulePathUtf8(globalHandle).view(), globalDllPath.view(), false));
    EXPECT_NE(SharedLibrary::getExport(globalHandle, expectedExport), nullptr);
    SharedLibrary::freeHandle(globalHandle);
}

TEST_F(WindowsSharedLibraryTest, LoadRealDllByExplicitAbsolutePathWithoutSearchPaths)
{
    PathBuffer sourceDllPath{};
    gp::StringView expectedExport{};
    if (!findCopyableTestDll(sourceDllPath, expectedExport))
    {
        GTEST_SKIP() << "No copyable test DLL found on this host.";
    }

    const PathBuffer directDllPath = Paths::combine(m_sandboxRoot.view(), "DirectFolder/gp_ut_direct_lib.dll");
    ASSERT_TRUE(FileSystem::createDirectoryTree(Paths::getPath(directDllPath.view())).hasValue());
    ASSERT_TRUE(FileSystem::copyFile(sourceDllPath.view(), directDllPath.view(), true).hasValue());

    // Load with forward slashes directly (no addDirectory or pushDirectory).
    void* const handle = SharedLibrary::getHandle(directDllPath.view());
    ASSERT_NE(handle, nullptr);
    EXPECT_TRUE(Paths::equals(queryModulePathUtf8(handle).view(), directDllPath.view(), false));
    EXPECT_NE(SharedLibrary::getExport(handle, expectedExport), nullptr);
    SharedLibrary::freeHandle(handle);

    // Load via a relative path resolved against overridden Paths::baseDir().
    Paths::setBaseDirForTesting(m_sandboxRoot.view());
    void* const relativeHandle = SharedLibrary::getHandle("DirectFolder/gp_ut_direct_lib.dll");
    Paths::resetCachesForTesting();

    ASSERT_NE(relativeHandle, nullptr);
    EXPECT_TRUE(Paths::equals(queryModulePathUtf8(relativeHandle).view(), directDllPath.view(), false));
    SharedLibrary::freeHandle(relativeHandle);
}

TEST_F(WindowsSharedLibraryTest, NestedPushedDirectoriesResolveLibrariesAcrossFullStack)
{
    PathBuffer sourceDllPath{};
    gp::StringView expectedExport{};
    if (!findCopyableTestDll(sourceDllPath, expectedExport))
    {
        GTEST_SKIP() << "No copyable test DLL found on this host.";
    }

    const PathBuffer dirLower = Paths::combine(m_sandboxRoot.view(), "StackLower");
    const PathBuffer dirUpper = Paths::combine(m_sandboxRoot.view(), "StackUpper");
    ASSERT_TRUE(FileSystem::createDirectoryTree(dirLower.view()).hasValue());
    ASSERT_TRUE(FileSystem::createDirectoryTree(dirUpper.view()).hasValue());

    const PathBuffer lowerDllPath = Paths::combine(dirLower.view(), "gp_ut_stack_lower.dll");
    const PathBuffer upperDllPath = Paths::combine(dirUpper.view(), "gp_ut_stack_upper.dll");
    ASSERT_TRUE(FileSystem::copyFile(sourceDllPath.view(), lowerDllPath.view(), true).hasValue());
    ASSERT_TRUE(FileSystem::copyFile(sourceDllPath.view(), upperDllPath.view(), true).hasValue());

    // Push Lower then Upper: Win32 SetDllDirectoryW only holds Upper, yet both stack entries must resolve.
    SharedLibrary::pushDirectory(dirLower.view());
    SharedLibrary::pushDirectory(dirUpper.view());
    ASSERT_EQ(SharedLibrary::getDirectoryStackDepth(), 2u);

    void* const upperHandle = SharedLibrary::getHandle("gp_ut_stack_upper.dll");
    ASSERT_NE(upperHandle, nullptr);
    EXPECT_TRUE(Paths::equals(queryModulePathUtf8(upperHandle).view(), upperDllPath.view(), false));

    void* const lowerHandle = SharedLibrary::getHandle("gp_ut_stack_lower.dll");
    ASSERT_NE(lowerHandle, nullptr);
    EXPECT_TRUE(Paths::equals(queryModulePathUtf8(lowerHandle).view(), lowerDllPath.view(), false));

    SharedLibrary::freeHandle(lowerHandle);
    SharedLibrary::freeHandle(upperHandle);

    SharedLibrary::popDirectory(dirUpper.view());
    SharedLibrary::popDirectory(dirLower.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);
}

TEST_F(WindowsSharedLibraryTest, CommandLineUnattendedErrorModeRestoresThreadErrorModeCleanly)
{
    gp::CommandLine::initialize("gp_core_tests.exe -unattended");

    const DWORD initialMode = ::GetThreadErrorMode();
    EXPECT_EQ(SharedLibrary::getHandle("gp_ut_missing_library_for_error_mode_test.dll"), nullptr);
    EXPECT_EQ(::GetThreadErrorMode(), initialMode);

    // Also verify invalid/corrupt DLL bytes fail cleanly without hanging on an OS error box.
    const PathBuffer corruptDll = Paths::combine(m_sandboxRoot.view(), "corrupt_image.dll");
    ASSERT_TRUE(FileSystem::writeAllText(corruptDll.view(), "NOT_A_VALID_PE_IMAGE_HEADER").hasValue());
    EXPECT_EQ(SharedLibrary::getHandle(corruptDll.view()), nullptr);
    EXPECT_EQ(::GetThreadErrorMode(), initialMode);
}

TEST_F(WindowsSharedLibraryTest, MultiThreadedConcurrentLookupsAndDirectoryRegistrationsAreRaceFree)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 64u;

    const PathBuffer sharedDir = Paths::combine(m_sandboxRoot.view(), "ConcurrentDir");
    ASSERT_TRUE(FileSystem::createDirectoryTree(sharedDir.view()).hasValue());
    ASSERT_TRUE(FileSystem::writeAllText(Paths::combine(sharedDir.view(), "DummyA.dll").view(), "stub").hasValue());

    gp::thread::Atomic<bool> startLatch(false);
    gp::thread::Atomic<gp::UInt32> readyCount(0u);
    gp::thread::Atomic<gp::UInt32> successfulLookups(0u);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([t, &sharedDir, &startLatch, &readyCount, &successfulLookups]() noexcept
        {
            readyCount.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
            while (!startLatch.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                if ((t % 2u) == 0u)
                {
                    SharedLibrary::addDirectory(sharedDir.view());
                }

                void* const handle = SharedLibrary::getHandle("kernel32.dll");
                if (handle != nullptr)
                {
                    void* const proc = SharedLibrary::getExport(handle, "GetCurrentProcessId");
                    if (proc != nullptr)
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
    EXPECT_EQ(SharedLibrary::getRegisteredDirectoryCount(), 1u);
    EXPECT_EQ(SharedLibrary::getCachedLibraryCount(), 1u);
}

}   // namespace gp::platform::windows::tests

#endif   // GP_PLATFORM_WINDOWS
