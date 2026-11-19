// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/system/FileSystem.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "platforms/base/system/Paths.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::platform::tests
{

class FileSystemTest : public ::testing::Test
{
protected:
    PathBuffer m_sandboxRoot{};

protected:
    void SetUp() noexcept override
    {
        static gp::thread::Atomic<gp::UInt32> s_sandboxCounter(1u);
        const gp::UInt32 id = s_sandboxCounter.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);

        gp::FixedString<64> folderName("gp_fs_test_sandbox_");
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
        if (!m_sandboxRoot.isEmpty() && FileSystem::exists(m_sandboxRoot.view()))
        {
            (void)FileSystem::removeDirectoryTree(m_sandboxRoot.view());
        }
    }
};

TEST_F(FileSystemTest, DefaultFileHandleIsInvalidAndRejectsOperations)
{
    FileHandle handle{};
    EXPECT_FALSE(handle.isValid());
    EXPECT_FALSE(static_cast<bool>(handle));
    EXPECT_EQ(handle.nativeDescriptor(), kInvalidNativeFileDescriptor);

    gp::Byte dummy{ 0 };
    EXPECT_EQ(handle.read(&dummy, 1u).error(), FileError::InvalidHandle);
    EXPECT_EQ(handle.write(&dummy, 1u).error(), FileError::InvalidHandle);
    EXPECT_EQ(handle.readAt(0ULL, &dummy, 1u).error(), FileError::InvalidHandle);
    EXPECT_EQ(handle.writeAt(0ULL, &dummy, 1u).error(), FileError::InvalidHandle);
    EXPECT_EQ(handle.seek(0, FileSeekOrigin::Begin).error(), FileError::InvalidHandle);
    EXPECT_EQ(handle.tell().error(), FileError::InvalidHandle);
    EXPECT_EQ(handle.size().error(), FileError::InvalidHandle);
    EXPECT_EQ(handle.flush().error(), FileError::InvalidHandle);
    EXPECT_EQ(handle.truncate(0ULL).error(), FileError::InvalidHandle);
    EXPECT_TRUE(handle.close().hasValue());
}

TEST_F(FileSystemTest, CreateDirectoryAndCreateDirectoryTreeHierarchy)
{
    const PathBuffer singleDir = Paths::combine(m_sandboxRoot.view(), "SingleFolder");
    EXPECT_FALSE(FileSystem::exists(singleDir.view()));

    const gp::Expected<bool, FileError> mkdirRes = FileSystem::createDirectory(singleDir.view());
    ASSERT_TRUE(mkdirRes.hasValue());
    EXPECT_TRUE(FileSystem::exists(singleDir.view()));
    EXPECT_TRUE(FileSystem::isDirectory(singleDir.view()));
    EXPECT_FALSE(FileSystem::isFile(singleDir.view()));

    // Idempotent second call on existing directory must succeed
    EXPECT_TRUE(FileSystem::createDirectory(singleDir.view()).hasValue());

    // Deep nested tree creation
    const PathBuffer deepDir = Paths::combine(m_sandboxRoot.view(), "LevelA/LevelB/LevelC/LevelD");
    EXPECT_FALSE(FileSystem::exists(deepDir.view()));
    ASSERT_TRUE(FileSystem::createDirectoryTree(deepDir.view()).hasValue());
    EXPECT_TRUE(FileSystem::isDirectory(deepDir.view()));

    // Removing non-empty parent with single removeDirectory must fail with DirectoryNotEmpty
    const PathBuffer levelA = Paths::combine(m_sandboxRoot.view(), "LevelA");
    const gp::Expected<bool, FileError> failRm = FileSystem::removeDirectory(levelA.view());
    EXPECT_FALSE(failRm.hasValue());
    EXPECT_EQ(failRm.error(), FileError::DirectoryNotEmpty);

    // Recursive tree removal must clean up everything
    ASSERT_TRUE(FileSystem::removeDirectoryTree(levelA.view()).hasValue());
    EXPECT_FALSE(FileSystem::exists(levelA.view()));
}

TEST_F(FileSystemTest, FileHandleSequentialReadWriteSeekTellTruncateAndMoveSemantics)
{
    const PathBuffer filePath = Paths::combine(m_sandboxRoot.view(), "sequential.bin");

    gp::Expected<FileHandle, FileError> openRes =
        FileSystem::open(filePath.view(), FileAccessMode::ReadWrite, FileOpenMode::CreateNew, FileShareMode::Read);
    ASSERT_TRUE(openRes.hasValue());

    // Move construction transfers ownership cleanly
    FileHandle handle(std::move(openRes.value()));
    EXPECT_FALSE(openRes.value().isValid());
    ASSERT_TRUE(handle.isValid());

    const gp::StringView payload = "GraphicalPlaygroundEngine_Layer0_TestPayload";
    const gp::Expected<gp::USize, FileError> writeRes = handle.write(payload.data(), payload.size());
    ASSERT_TRUE(writeRes.hasValue());
    EXPECT_EQ(writeRes.value(), payload.size());

    const gp::Expected<gp::UInt64, FileError> tellRes = handle.tell();
    ASSERT_TRUE(tellRes.hasValue());
    EXPECT_EQ(tellRes.value(), static_cast<gp::UInt64>(payload.size()));

    const gp::Expected<gp::UInt64, FileError> sizeRes = handle.size();
    ASSERT_TRUE(sizeRes.hasValue());
    EXPECT_EQ(sizeRes.value(), static_cast<gp::UInt64>(payload.size()));

    // Seek back to offset 9 ("Playground...")
    const gp::Expected<gp::UInt64, FileError> seekRes = handle.seek(9, FileSeekOrigin::Begin);
    ASSERT_TRUE(seekRes.hasValue());
    EXPECT_EQ(seekRes.value(), 9ULL);

    char readBuf[16]{};
    const gp::Expected<gp::USize, FileError> readRes = handle.read(readBuf, 10u);
    ASSERT_TRUE(readRes.hasValue());
    EXPECT_EQ(readRes.value(), 10u);
    EXPECT_EQ(gp::StringView(readBuf, 10u), gp::StringView("Playground"));

    // Truncate to 9 bytes ("Graphical")
    ASSERT_TRUE(handle.truncate(9ULL).hasValue());
    EXPECT_EQ(handle.size().value(), 9ULL);
    ASSERT_TRUE(handle.flush().hasValue());
    ASSERT_TRUE(handle.close().hasValue());
    EXPECT_FALSE(handle.isValid());

    // Verify truncated content via readAllText
    gp::String truncatedText{};
    ASSERT_TRUE(FileSystem::readAllText(filePath.view(), truncatedText).hasValue());
    EXPECT_EQ(truncatedText.view(), gp::StringView("Graphical"));
}

TEST_F(FileSystemTest, PositionalReadAtWriteAtAndMultiThreadedConcurrentReads)
{
    const PathBuffer filePath = Paths::combine(m_sandboxRoot.view(), "positional_blocks.bin");
    constexpr gp::USize kBlockCount = 64u;
    constexpr gp::USize kBlockSize = 128u;

    {
        gp::Expected<FileHandle, FileError> writeHandleRes = FileSystem::open(
            filePath.view(), FileAccessMode::ReadWrite, FileOpenMode::CreateAlways, FileShareMode::Read
        );
        ASSERT_TRUE(writeHandleRes.hasValue());
        FileHandle& wh = writeHandleRes.value();

        // Write blocks out-of-order using positional writeAt
        for (gp::USize idx = kBlockCount; idx > 0u; --idx)
        {
            const gp::USize blockIndex = idx - 1u;
            gp::Byte blockData[kBlockSize]{};
            for (gp::USize b = 0u; b < kBlockSize; ++b)
            {
                blockData[b] = static_cast<gp::Byte>((blockIndex * 31u + b) & 0xFFu);
            }

            const gp::UInt64 byteOffset = static_cast<gp::UInt64>(blockIndex * kBlockSize);
            const gp::Expected<gp::USize, FileError> wRes = wh.writeAt(byteOffset, blockData, kBlockSize);
            ASSERT_TRUE(wRes.hasValue());
            EXPECT_EQ(wRes.value(), kBlockSize);
        }
    }

    // Open a SINGLE read-only FileHandle and share it across 8 concurrent reader threads using readAt()
    gp::Expected<FileHandle, FileError> sharedReadRes = FileSystem::openRead(filePath.view());
    ASSERT_TRUE(sharedReadRes.hasValue());
    const FileHandle& sharedHandle = sharedReadRes.value();

    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kReadsPerThread = 200u;
    gp::thread::Atomic<gp::UInt32> readyCount(0u);
    gp::thread::Atomic<gp::UInt32> mismatchCount(0u);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back(
            [&sharedHandle, &readyCount, &mismatchCount, t]() -> void
        {
            readyCount.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
            while (readyCount.load(gp::memory::MemoryOrder::Acquire) < kThreadCount)
            {
                std::this_thread::yield();
            }

            gp::Byte localBuf[kBlockSize]{};
            for (gp::UInt32 iter = 0u; iter < kReadsPerThread; ++iter)
            {
                const gp::USize blockIndex = static_cast<gp::USize>((t * 17u + iter * 13u) % kBlockCount);
                const gp::UInt64 offset = static_cast<gp::UInt64>(blockIndex * kBlockSize);

                const gp::Expected<gp::USize, FileError> rRes = sharedHandle.readAt(offset, localBuf, kBlockSize);
                if (!rRes.hasValue() || rRes.value() != kBlockSize)
                {
                    mismatchCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
                    continue;
                }

                for (gp::USize b = 0u; b < kBlockSize; ++b)
                {
                    const gp::Byte expectedByte = static_cast<gp::Byte>((blockIndex * 31u + b) & 0xFFu);
                    if (localBuf[b] != expectedByte)
                    {
                        mismatchCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
                        break;
                    }
                }
            }
        }
        );
    }

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(mismatchCount.load(gp::memory::MemoryOrder::Acquire), 0u);
}

TEST_F(FileSystemTest, BulkReadWriteAppendAtomicWriteAndArenaStringIntegration)
{
    const PathBuffer textPath = Paths::combine(m_sandboxRoot.view(), "config.ini");

    ASSERT_TRUE(FileSystem::writeAllText(textPath.view(), "[Renderer]\nVsync=true\n").hasValue());
    ASSERT_TRUE(FileSystem::appendAllText(textPath.view(), "RayTracing=true\n").hasValue());

    // Read into a LinearAllocator-backed PolymorphicString to verify zero-heap arena interoperability (ADR-0042)
    alignas(16) gp::Byte arenaSlab[2048]{};
    gp::memory::LinearAllocator linearArena(arenaSlab, sizeof(arenaSlab));
    gp::PolymorphicString arenaStr{ gp::memory::PolymorphicAllocator(linearArena) };

    const gp::Expected<gp::USize, FileError> readRes = FileSystem::readAllText(textPath.view(), arenaStr);
    ASSERT_TRUE(readRes.hasValue());
    EXPECT_EQ(arenaStr.view(), gp::StringView("[Renderer]\nVsync=true\nRayTracing=true\n"));

    // Crash-safe writeAtomic replacement over existing file
    const gp::StringView atomicPayload = "[Renderer]\nVsync=false\nHDR=true\n";
    ASSERT_TRUE(FileSystem::writeAtomicText(textPath.view(), atomicPayload).hasValue());

    gp::String verifiedText{};
    ASSERT_TRUE(FileSystem::readAllText(textPath.view(), verifiedText).hasValue());
    EXPECT_EQ(verifiedText.view(), atomicPayload);

    gp::String verifiedBytes{};
    ASSERT_TRUE(FileSystem::readAllBytes(textPath.view(), verifiedBytes).hasValue());
    EXPECT_EQ(verifiedBytes.view(), atomicPayload);

    // Ensure the temporary staging file (.tmp) was cleaned up by the atomic rename
    PathBuffer stagingPath(textPath);
    stagingPath.append(".tmp");
    EXPECT_FALSE(FileSystem::exists(stagingPath.view()));
}

TEST_F(FileSystemTest, MoveCopyStatsAndReadOnlyAttributes)
{
    const PathBuffer srcPath = Paths::combine(m_sandboxRoot.view(), "original.txt");
    const PathBuffer copyPath = Paths::combine(m_sandboxRoot.view(), "copied.txt");
    const PathBuffer movedPath = Paths::combine(m_sandboxRoot.view(), "moved.txt");

    ASSERT_TRUE(FileSystem::writeAllText(srcPath.view(), "HelloFileStats12345").hasValue());

    const gp::Expected<FileStats, FileError> statsRes = FileSystem::getStats(srcPath.view());
    ASSERT_TRUE(statsRes.hasValue());
    EXPECT_TRUE(statsRes.value().isFile());
    EXPECT_FALSE(statsRes.value().isDirectory());
    EXPECT_EQ(statsRes.value().size, 19ULL);
    EXPECT_GT(statsRes.value().lastWriteTime.nanoseconds, 0ULL);
    EXPECT_EQ(FileSystem::fileSize(srcPath.view()).value(), 19ULL);

    // Copy file
    ASSERT_TRUE(FileSystem::copyFile(srcPath.view(), copyPath.view(), false).hasValue());
    EXPECT_TRUE(FileSystem::exists(copyPath.view()));
    EXPECT_EQ(FileSystem::fileSize(copyPath.view()).value(), 19ULL);

    // Copy without overwrite must fail with AlreadyExists
    EXPECT_EQ(FileSystem::copyFile(srcPath.view(), copyPath.view(), false).error(), FileError::AlreadyExists);
    // Copy with overwrite=true must succeed
    EXPECT_TRUE(FileSystem::copyFile(srcPath.view(), copyPath.view(), true).hasValue());

    // Move file
    ASSERT_TRUE(FileSystem::move(copyPath.view(), movedPath.view(), true).hasValue());
    EXPECT_FALSE(FileSystem::exists(copyPath.view()));
    EXPECT_TRUE(FileSystem::exists(movedPath.view()));

    // Read-only attribute toggling
    EXPECT_FALSE(FileSystem::isReadOnly(movedPath.view()));
    ASSERT_TRUE(FileSystem::setReadOnly(movedPath.view(), true).hasValue());
    EXPECT_TRUE(FileSystem::isReadOnly(movedPath.view()));
    ASSERT_TRUE(FileSystem::setReadOnly(movedPath.view(), false).hasValue());
    EXPECT_FALSE(FileSystem::isReadOnly(movedPath.view()));

    ASSERT_TRUE(FileSystem::removeFile(srcPath.view()).hasValue());
    ASSERT_TRUE(FileSystem::removeFile(movedPath.view()).hasValue());
    EXPECT_FALSE(FileSystem::exists(srcPath.view()));
    EXPECT_FALSE(FileSystem::exists(movedPath.view()));
}

TEST_F(FileSystemTest, ZeroAllocationDirectoryIterationAndRecursiveSkipAndStopControl)
{
    const PathBuffer subA = Paths::combine(m_sandboxRoot.view(), "SubA");
    const PathBuffer subSkipped = Paths::combine(m_sandboxRoot.view(), "SubSkipped");
    ASSERT_TRUE(FileSystem::createDirectoryTree(subA.view()).hasValue());
    ASSERT_TRUE(FileSystem::createDirectoryTree(subSkipped.view()).hasValue());

    ASSERT_TRUE(
        FileSystem::writeAllText(Paths::combine(m_sandboxRoot.view(), "root_file.txt").view(), "root").hasValue()
    );
    ASSERT_TRUE(FileSystem::writeAllText(Paths::combine(subA.view(), "child_a.txt").view(), "childA").hasValue());
    ASSERT_TRUE(
        FileSystem::writeAllText(Paths::combine(subSkipped.view(), "hidden_child.txt").view(), "skipMe").hasValue()
    );

    struct WalkSummary
    {
        gp::UInt32 fileCount{ 0u };
        gp::UInt32 dirCount{ 0u };
        bool sawHiddenChild{ false };
    };

    WalkSummary topSummary{};
    const DirectoryVisitorFn topVisitor = [](const DirectoryEntry& entry, void* const rawCtx) -> DirectoryVisitControl
    {
        WalkSummary* const sum = static_cast<WalkSummary*>(rawCtx);
        if (entry.isDirectory())
        {
            ++sum->dirCount;
        }
        else if (entry.isFile())
        {
            ++sum->fileCount;
        }
        return DirectoryVisitControl::Continue;
    };

    ASSERT_TRUE(FileSystem::iterateDirectory(m_sandboxRoot.view(), topVisitor, &topSummary).hasValue());
    EXPECT_EQ(topSummary.dirCount, 2u);
    EXPECT_EQ(topSummary.fileCount, 1u);

    // Recursive walk that skips "SubSkipped"
    WalkSummary recSummary{};
    const DirectoryVisitorFn recVisitor = [](const DirectoryEntry& entry, void* const rawCtx) -> DirectoryVisitControl
    {
        WalkSummary* const sum = static_cast<WalkSummary*>(rawCtx);
        if (entry.name == "hidden_child.txt")
        {
            sum->sawHiddenChild = true;
        }
        if (entry.isDirectory())
        {
            ++sum->dirCount;
            if (entry.name == "SubSkipped")
            {
                return DirectoryVisitControl::SkipSubdirectory;
            }
        }
        else if (entry.isFile())
        {
            ++sum->fileCount;
        }
        return DirectoryVisitControl::Continue;
    };

    ASSERT_TRUE(FileSystem::iterateDirectoryRecursive(m_sandboxRoot.view(), recVisitor, &recSummary).hasValue());
    EXPECT_EQ(recSummary.dirCount, 2u);
    EXPECT_EQ(recSummary.fileCount, 2u);   // root_file.txt + child_a.txt (hidden_child.txt was skipped!)
    EXPECT_FALSE(recSummary.sawHiddenChild);

    // Ergonomic C++23 capturing lambda overload + early Stop verification
    gp::UInt32 lambdaVisited = 0u;
    const gp::Expected<bool, FileError> stopRes = FileSystem::iterateDirectoryRecursive(
        m_sandboxRoot.view(),
        [&lambdaVisited](const DirectoryEntry& entry) -> DirectoryVisitControl
    {
        (void)entry;
        ++lambdaVisited;
        return (lambdaVisited == 2u) ? DirectoryVisitControl::Stop : DirectoryVisitControl::Continue;
    }
    );
    ASSERT_TRUE(stopRes.hasValue());
    EXPECT_EQ(lambdaVisited, 2u);
}

TEST_F(FileSystemTest, OpenModesErrorPathsAndSeekOriginsEdgeCases)
{
    const PathBuffer missingPath = Paths::combine(m_sandboxRoot.view(), "does_not_exist.bin");
    const PathBuffer testFile = Paths::combine(m_sandboxRoot.view(), "modes_test.bin");
    const PathBuffer testDir = Paths::combine(m_sandboxRoot.view(), "a_directory");
    ASSERT_TRUE(FileSystem::createDirectory(testDir.view()).hasValue());

    // OpenExisting on missing file -> NotFound
    EXPECT_EQ(FileSystem::openRead(missingPath.view()).error(), FileError::NotFound);
    EXPECT_EQ(FileSystem::fileSize(missingPath.view()).error(), FileError::NotFound);
    EXPECT_EQ(FileSystem::getStats(missingPath.view()).error(), FileError::NotFound);
    EXPECT_EQ(FileSystem::removeFile(missingPath.view()).error(), FileError::NotFound);
    EXPECT_EQ(FileSystem::removeDirectory(missingPath.view()).error(), FileError::NotFound);

    // Operations on directory vs regular file type mismatches
    EXPECT_EQ(FileSystem::openRead(testDir.view()).error(), FileError::IsDirectory);
    EXPECT_EQ(FileSystem::fileSize(testDir.view()).error(), FileError::IsDirectory);
    EXPECT_EQ(FileSystem::removeFile(testDir.view()).error(), FileError::IsDirectory);
    EXPECT_EQ(FileSystem::copyFile(testDir.view(), testFile.view(), true).error(), FileError::IsDirectory);

    // CreateNew succeeds once, then fails with AlreadyExists
    {
        gp::Expected<FileHandle, FileError> created =
            FileSystem::open(testFile.view(), FileAccessMode::ReadWrite, FileOpenMode::CreateNew, FileShareMode::Read);
        ASSERT_TRUE(created.hasValue());
        ASSERT_TRUE(created.value().write("0123456789", 10u).hasValue());

        // Seek relative to Current and End
        EXPECT_EQ(created.value().seek(-4, FileSeekOrigin::Current).value(), 6ULL);
        EXPECT_EQ(created.value().seek(-2, FileSeekOrigin::End).value(), 8ULL);
        char tail[2]{};
        EXPECT_EQ(created.value().read(tail, 2u).value(), 2u);
        EXPECT_EQ(gp::StringView(tail, 2u), gp::StringView("89"));

        // Zero-length read/write and null buffer validation
        EXPECT_EQ(created.value().read(nullptr, 0u).value(), 0u);
        EXPECT_EQ(created.value().write(nullptr, 0u).value(), 0u);
        EXPECT_EQ(created.value().read(nullptr, 4u).error(), FileError::InvalidArgument);
        EXPECT_EQ(created.value().write(nullptr, 4u).error(), FileError::InvalidArgument);
        EXPECT_EQ(created.value().readAt(0ULL, nullptr, 4u).error(), FileError::InvalidArgument);
        EXPECT_EQ(created.value().writeAt(0ULL, nullptr, 4u).error(), FileError::InvalidArgument);

        // Read past EOF returns 0 bytes (not an error)
        EXPECT_EQ(created.value().readAt(500ULL, tail, 2u).value(), 0u);
    }

    EXPECT_EQ(
        FileSystem::open(testFile.view(), FileAccessMode::Write, FileOpenMode::CreateNew, FileShareMode::None).error(),
        FileError::AlreadyExists
    );

    // OpenAlways preserves existing content without truncating
    {
        gp::Expected<FileHandle, FileError> existing =
            FileSystem::open(testFile.view(), FileAccessMode::ReadWrite, FileOpenMode::OpenAlways, FileShareMode::Read);
        ASSERT_TRUE(existing.hasValue());
        EXPECT_EQ(existing.value().size().value(), 10ULL);
    }

    // Regular file passed to directory APIs -> NotDirectory / AlreadyExists
    EXPECT_EQ(FileSystem::createDirectory(testFile.view()).error(), FileError::AlreadyExists);
    EXPECT_EQ(FileSystem::removeDirectory(testFile.view()).error(), FileError::NotDirectory);
    EXPECT_EQ(FileSystem::iterateDirectory(testFile.view(), nullptr, nullptr).error(), FileError::InvalidArgument);
    EXPECT_EQ(
        FileSystem::iterateDirectory(
            testFile.view(),
            [](const DirectoryEntry&, void*) -> DirectoryVisitControl
    {
        return DirectoryVisitControl::Continue;
    }
        ).error(),
        FileError::NotDirectory
    );

    // Empty / invalid paths and self-copy rejection
    EXPECT_EQ(FileSystem::openRead("").error(), FileError::InvalidPath);
    EXPECT_EQ(FileSystem::createDirectory("").error(), FileError::InvalidPath);
    EXPECT_EQ(FileSystem::createDirectoryTree("").error(), FileError::InvalidPath);
    EXPECT_EQ(FileSystem::copyFile(testFile.view(), testFile.view(), true).error(), FileError::InvalidArgument);
    EXPECT_EQ(FileSystem::move(testFile.view(), testDir.view(), false).error(), FileError::AlreadyExists);

    // FileError::toString constexpr verification
    static_assert(toString(FileError::None) == "None");
    static_assert(toString(FileError::NotFound) == "NotFound");
    static_assert(toString(FileError::DiskFull) == "DiskFull");
    EXPECT_EQ(toString(FileError::SharingViolation), gp::StringView("SharingViolation"));
}

TEST_F(FileSystemTest, ConcurrentWriteAtomicAndEmptyFileEdgeCases)
{
    const PathBuffer emptyPath = Paths::combine(m_sandboxRoot.view(), "empty.dat");
    ASSERT_TRUE(FileSystem::writeAllBytes(emptyPath.view(), nullptr, 0u).hasValue());
    EXPECT_TRUE(FileSystem::isFile(emptyPath.view()));
    EXPECT_EQ(FileSystem::fileSize(emptyPath.view()).value(), 0ULL);

    gp::String emptyRead("non_empty_before_read");
    ASSERT_TRUE(FileSystem::readAllText(emptyPath.view(), emptyRead).hasValue());
    EXPECT_TRUE(emptyRead.isEmpty());

    // Multi-threaded concurrent writeAtomic to the SAME target path must never collide on staging files or corrupt
    // the file (every reader must see either a complete payload A or complete payload B, never torn/empty).
    const PathBuffer sharedSave = Paths::combine(m_sandboxRoot.view(), "concurrent_save.json");
    const gp::StringView payloadA = "{\"slot\":1,\"checksum\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
    const gp::StringView payloadB = "{\"slot\":2,\"checksum\":\"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\"}";
    ASSERT_TRUE(FileSystem::writeAtomicText(sharedSave.view(), payloadA).hasValue());

    constexpr gp::UInt32 kWriterThreads = 4u;
    constexpr gp::UInt32 kWritesPerThread = 25u;
    gp::thread::Atomic<gp::UInt32> startLatch(0u);
    gp::thread::Atomic<gp::UInt32> tornReads(0u);

    std::vector<std::thread> threads;
    threads.reserve(kWriterThreads);
    for (gp::UInt32 t = 0u; t < kWriterThreads; ++t)
    {
        threads.emplace_back(
            [&sharedSave, &startLatch, &tornReads, payloadA, payloadB, t]() -> void
        {
            startLatch.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
            while (startLatch.load(gp::memory::MemoryOrder::Acquire) < kWriterThreads)
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kWritesPerThread; ++i)
            {
                const gp::StringView chosen = ((t + i) % 2u == 0u) ? payloadA : payloadB;
                // On Windows, simultaneous MoveFileExW replace over the same file can transiently return
                // AccessDenied/SharingViolation under extreme contention; what matters is zero corruption and
                // zero leaked .tmp.* files.
                (void)FileSystem::writeAtomicText(sharedSave.view(), chosen);

                gp::String snapshot{};
                const gp::Expected<gp::USize, FileError> r = FileSystem::readAllText(sharedSave.view(), snapshot);
                if (r.hasValue() && snapshot.view() != payloadA && snapshot.view() != payloadB)
                {
                    tornReads.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
                }
            }
        }
        );
    }

    for (std::thread& th: threads)
    {
        th.join();
    }

    EXPECT_EQ(tornReads.load(gp::memory::MemoryOrder::Acquire), 0u);

    // Verify no leaked .tmp.* staging files remain in the sandbox directory
    gp::UInt32 leftoverTempFiles = 0u;
    ASSERT_TRUE(
        FileSystem::iterateDirectory(
            m_sandboxRoot.view(),
            [&leftoverTempFiles](const DirectoryEntry& entry) -> DirectoryVisitControl
    {
        if (entry.name.contains(".tmp."))
        {
            ++leftoverTempFiles;
        }
        return DirectoryVisitControl::Continue;
    }
        ).hasValue()
    );
    EXPECT_EQ(leftoverTempFiles, 0u);
}

TEST_F(FileSystemTest, UnicodeUtf8FilenamesAndReadOnlyTreeDeletion)
{
    const PathBuffer utf8Dir = Paths::combine(m_sandboxRoot.view(), "Assets_日本語_éàü");
    ASSERT_TRUE(FileSystem::createDirectoryTree(utf8Dir.view()).hasValue());
    EXPECT_TRUE(FileSystem::isDirectory(utf8Dir.view()));

    const PathBuffer utf8File = Paths::combine(utf8Dir.view(), "héros_テクスチャ.txt");
    ASSERT_TRUE(FileSystem::writeAllText(utf8File.view(), "UTF-8 payload ✓").hasValue());
    EXPECT_TRUE(FileSystem::isFile(utf8File.view()));

    gp::String readBack{};
    ASSERT_TRUE(FileSystem::readAllText(utf8File.view(), readBack).hasValue());
    EXPECT_EQ(readBack.view(), gp::StringView("UTF-8 payload ✓"));

    // Mark file read-only and verify removeDirectoryTree clears the attribute and succeeds cleanly
    ASSERT_TRUE(FileSystem::setReadOnly(utf8File.view(), true).hasValue());
    EXPECT_TRUE(FileSystem::isReadOnly(utf8File.view()));
    ASSERT_TRUE(FileSystem::removeDirectoryTree(utf8Dir.view()).hasValue());
    EXPECT_FALSE(FileSystem::exists(utf8Dir.view()));
}

}   // namespace gp::platform::tests
