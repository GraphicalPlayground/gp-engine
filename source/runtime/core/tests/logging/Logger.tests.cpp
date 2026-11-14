// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/Logger.hpp"
#include "logging/LogMacros.hpp"
#include "logging/sinks/DebugOutputSink.hpp"
#include "logging/sinks/FileSink.hpp"
#include "logging/sinks/MemoryRingSink.hpp"
#include "logging/sinks/TracySink.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platforms/base/system/Paths.hpp"
#include <cstdio>
#include <gtest/gtest.h>

namespace gp::log::tests
{

namespace
{

GP_DEFINE_LOG_CATEGORY_INLINE(LogTestSubsystem, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

struct AsyncProducerContext final
{
    Logger* logger = nullptr;
    gp::thread::Atomic<bool>* startGate = nullptr;
    gp::UInt32 threadIndex = 0u;
    gp::UInt32 logsPerThread = 0u;
};

gp::UInt32 asyncProducerEntry(void* inUserData)
{
    const AsyncProducerContext* const ctx = static_cast<const AsyncProducerContext*>(inUserData);
    if (ctx == nullptr || ctx->logger == nullptr || ctx->startGate == nullptr)
    {
        return 1u;
    }

    while (!ctx->startGate->load(gp::MemoryOrder::Acquire))
    {
        gp::platform::Thread::yieldExecution();
    }

    for (gp::UInt32 i = 0u; i < ctx->logsPerThread; ++i)
    {
        GP_LOGGER_LOG(*(ctx->logger), LogTestSubsystem, Info, "Thread {} Iter {}", ctx->threadIndex, i);
    }

    return 0u;
}

class RecursiveTestSink final : public ILogSink
{
public:
    explicit RecursiveTestSink(Logger& inOwnerLogger) noexcept
        : ILogSink(LogLevel::Trace)
        , m_ownerLogger(inOwnerLogger)
    {}

    void write(const LogRecordView& inRecord) noexcept override
    {
        (void)inRecord;
        ++m_writeCalls;

        // Attempt a recursive synchronous log inside write() on the same thread.
        // Logger must detect same-thread dispatch lock ownership and drop the recursive record instead of deadlocking.
        GP_LOGGER_LOG(m_ownerLogger, LogTestSubsystem, Warning, "Recursive log attempt {}", m_writeCalls);
    }

    void flush() noexcept override
    {
        ++m_flushCalls;
    }

    [[nodiscard]] gp::UInt32 getWriteCalls() const noexcept
    {
        return m_writeCalls;
    }

    [[nodiscard]] gp::UInt32 getFlushCalls() const noexcept
    {
        return m_flushCalls;
    }

private:
    Logger& m_ownerLogger;
    gp::UInt32 m_writeCalls = 0u;
    gp::UInt32 m_flushCalls = 0u;
};

}   // namespace

TEST(LoggerTest, SynchronousMultiSinkRoutingAndFiltering)
{
    Logger logger{};
    LoggerDesc desc{};
    desc.mode = LogExecutionMode::Synchronous;
    desc.applyCommandLineOverrides = false;
    ASSERT_EQ(logger.initialize(desc), LogResult::Success);

    MemoryRingSink allSink(LogLevel::Trace);
    MemoryRingSink errorOnlySink(LogLevel::Error);

    ASSERT_EQ(logger.addSink(&allSink), LogResult::Success);
    ASSERT_EQ(logger.addSink(&errorOnlySink), LogResult::Success);
    EXPECT_EQ(logger.getSinkCount(), 2u);

    LogTestSubsystem.resetToDefaultLevel();
    GP_LOGGER_LOG(logger, LogTestSubsystem, Debug, "Ignored debug message {}", 1);
    GP_LOGGER_LOG(logger, LogTestSubsystem, Info, "Hello from {} (frame {})", "JobSystem", 60);
    GP_LOGGER_LOG(logger, LogTestSubsystem, Error, "Device lost error code {:#x}", 0x887A0005u);

    EXPECT_EQ(allSink.getSize(), 2u);
    EXPECT_EQ(errorOnlySink.getSize(), 1u);

    const LogRecordView infoRec = allSink.getEntry(0u).toView();
    EXPECT_EQ(infoRec.level, LogLevel::Info);
    EXPECT_EQ(infoRec.categoryName, "LogTestSubsystem");
    EXPECT_EQ(infoRec.message, "Hello from JobSystem (frame 60)");
    EXPECT_GT(infoRec.location.line, 0u);

    const LogRecordView errRec = errorOnlySink.getLastRecord();
    EXPECT_EQ(errRec.level, LogLevel::Error);
    EXPECT_EQ(errRec.message, "Device lost error code 0x887a0005");

    EXPECT_EQ(logger.removeSink(&errorOnlySink), LogResult::Success);
    EXPECT_EQ(logger.getSinkCount(), 1u);
    logger.shutdown();
}

TEST(LoggerTest, MacroShortCircuitsDisabledArgumentEvaluation)
{
    Logger logger{};
    LoggerDesc desc{};
    desc.mode = LogExecutionMode::Synchronous;
    desc.applyCommandLineOverrides = false;
    ASSERT_EQ(logger.initialize(desc), LogResult::Success);

    MemoryRingSink ringSink(LogLevel::Trace);
    ASSERT_EQ(logger.addSink(&ringSink), LogResult::Success);

    LogTestSubsystem.setActiveLevel(LogLevel::Warning);

    gp::UInt32 sideEffectCount = 0u;
    const auto expensiveEvaluation = [&sideEffectCount]() noexcept -> gp::UInt32
    {
        ++sideEffectCount;
        return 999u;
    };

    // Because Info < Warning, GP_LOGGER_LOG must short-circuit before evaluating expensiveEvaluation()!
    GP_LOGGER_LOG(logger, LogTestSubsystem, Info, "Value={}", expensiveEvaluation());
    EXPECT_EQ(sideEffectCount, 0u);
    EXPECT_EQ(ringSink.getSize(), 0u);

    // Warning >= Warning, so expensiveEvaluation() is evaluated exactly once.
    GP_LOGGER_LOG(logger, LogTestSubsystem, Warning, "Value={}", expensiveEvaluation());
    EXPECT_EQ(sideEffectCount, 1u);
    EXPECT_EQ(ringSink.getSize(), 1u);
    EXPECT_EQ(ringSink.getLastRecord().message, "Value=999");

    LogTestSubsystem.resetToDefaultLevel();
    logger.shutdown();
}

TEST(LoggerTest, GlobalInstanceAndCpp23Functions)
{
    Logger& globalLogger = Logger::instance();
    globalLogger.shutdown();
    globalLogger.clearSinks();

    LoggerDesc desc{};
    desc.mode = LogExecutionMode::Synchronous;
    desc.applyCommandLineOverrides = false;
    ASSERT_EQ(globalLogger.initialize(desc), LogResult::Success);

    MemoryRingSink ringSink(LogLevel::Trace);
    DebugOutputSink debugSink(LogLevel::Warning);
    TracySink tracySink(LogLevel::Info);

    ASSERT_EQ(globalLogger.addSink(&ringSink), LogResult::Success);
    ASSERT_EQ(globalLogger.addSink(&debugSink), LogResult::Success);
    ASSERT_EQ(globalLogger.addSink(&tracySink), LogResult::Success);

    LogCore.setActiveLevel(LogLevel::Trace);

    gp::log::info(LogCore, "Direct C++23 info call {}", 42);
    GP_LOG_WARNING(LogCore, "Macro warning call {}", "ok");
    GP_LOG_ERROR(LogCore, "Zero-arg macro error call");

    ASSERT_EQ(ringSink.getSize(), 3u);
    EXPECT_EQ(ringSink.getEntry(0u).toView().message, "Direct C++23 info call 42");
    EXPECT_EQ(ringSink.getEntry(1u).toView().message, "Macro warning call ok");
    EXPECT_EQ(ringSink.getEntry(2u).toView().message, "Zero-arg macro error call");

    LogCore.resetToDefaultLevel();
    globalLogger.clearSinks();
    globalLogger.shutdown();
}

TEST(LoggerTest, FileSinkWritesAndFlushesToDisk)
{
    const gp::platform::PathBuffer logFilePath =
        gp::platform::Paths::combine(gp::platform::Paths::tempDir(), "gp_logger_unit_test.log");

    FileSinkDesc fileDesc{};
    fileDesc.filePath = logFilePath.view();
    fileDesc.minLevel = LogLevel::Info;
    fileDesc.flags = LogSinkFlags::None;
    fileDesc.truncateOnOpen = true;

    FileSink fileSink{};
    ASSERT_EQ(fileSink.open(fileDesc), LogResult::Success);
    EXPECT_TRUE(fileSink.isOpen());

    Logger logger{};
    LoggerDesc desc{};
    desc.mode = LogExecutionMode::Synchronous;
    desc.applyCommandLineOverrides = false;
    ASSERT_EQ(logger.initialize(desc), LogResult::Success);
    ASSERT_EQ(logger.addSink(&fileSink), LogResult::Success);

    LogTestSubsystem.setActiveLevel(LogLevel::Info);
    GP_LOGGER_LOG(logger, LogTestSubsystem, Info, "First disk line {}", 100);
    GP_LOGGER_LOG(logger, LogTestSubsystem, Error, "Second disk line {}", 200);

    logger.flush();
    EXPECT_GT(fileSink.getTotalBytesWritten(), 0ULL);
    fileSink.close();
    logger.shutdown();

    // Verify file contents on disk and clean up temporary test file.
#if GP_PLATFORM_WINDOWS
    std::FILE* readHandle = nullptr;
    ASSERT_EQ(::fopen_s(&readHandle, logFilePath.cString(), "rb"), 0);
#else
    std::FILE* const readHandle = std::fopen(logFilePath.cString(), "rb");
#endif
    ASSERT_NE(readHandle, nullptr);

    char readBuf[512]{};
    const gp::USize bytesRead = std::fread(readBuf, sizeof(char), sizeof(readBuf) - 1u, readHandle);
    (void)std::fclose(readHandle);
    (void)std::remove(logFilePath.cString());

    const gp::StringView fileContents(readBuf, bytesRead);
    EXPECT_TRUE(fileContents.contains("[INFO][LogTestSubsystem] First disk line 100"));
    EXPECT_TRUE(fileContents.contains("[ERROR][LogTestSubsystem] Second disk line 200"));
}

TEST(LoggerTest, AsyncRingMultiThreadedConcurrencyStress)
{
    constexpr gp::UInt32 kThreadCount = 4u;
    constexpr gp::UInt32 kLogsPerThread = 200u;

    Logger logger{};
    LoggerDesc desc{};
    desc.mode = LogExecutionMode::AsyncRing;
    desc.overflowPolicy = LogOverflowPolicy::FlushSynchronously;
    desc.asyncFlushIntervalMicroseconds = 200u;
    desc.applyCommandLineOverrides = false;
    ASSERT_EQ(logger.initialize(desc), LogResult::Success);
    EXPECT_EQ(logger.getExecutionMode(), LogExecutionMode::AsyncRing);

    MemoryRingSink ringSink(LogLevel::Trace);
    ASSERT_EQ(logger.addSink(&ringSink), LogResult::Success);

    LogTestSubsystem.setActiveLevel(LogLevel::Trace);
    gp::thread::Atomic<bool> startGate(false);

    AsyncProducerContext contexts[kThreadCount]{};
    gp::platform::Thread producers[kThreadCount]{};

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        contexts[t].logger = &logger;
        contexts[t].startGate = &startGate;
        contexts[t].threadIndex = t;
        contexts[t].logsPerThread = kLogsPerThread;

        gp::platform::ThreadDesc threadDesc{};
        threadDesc.name = "gp.log.test.producer";
        threadDesc.entry = &asyncProducerEntry;
        threadDesc.userData = &contexts[t];
        threadDesc.stackSize = 64u * 1024u;
        threadDesc.priority = gp::platform::ThreadPriority::Normal;

        ASSERT_EQ(producers[t].start(threadDesc), gp::platform::ThreadResult::Success);
    }

    startGate.store(true, gp::MemoryOrder::Release);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        EXPECT_EQ(producers[t].join(), gp::platform::ThreadResult::Success);
    }

    logger.flush();
    logger.shutdown();

    const gp::UInt64 expectedTotal = static_cast<gp::UInt64>(kThreadCount * kLogsPerThread);
    EXPECT_EQ(logger.getTotalSubmittedCount(), expectedTotal);
    EXPECT_EQ(logger.getTotalDispatchedCount(), expectedTotal);
    EXPECT_EQ(ringSink.getTotalWriteCount(), static_cast<gp::USize>(expectedTotal));
    EXPECT_EQ(ringSink.getSize(), MemoryRingSink::kCapacity);
}

TEST(LoggerTest, GuardsAgainstRecursiveReEntrancyInCustomSink)
{
    Logger logger{};
    LoggerDesc desc{};
    desc.mode = LogExecutionMode::Synchronous;
    desc.applyCommandLineOverrides = false;
    ASSERT_EQ(logger.initialize(desc), LogResult::Success);

    RecursiveTestSink recursiveSink(logger);
    ASSERT_EQ(logger.addSink(&recursiveSink), LogResult::Success);

    LogTestSubsystem.setActiveLevel(LogLevel::Info);
    GP_LOGGER_LOG(logger, LogTestSubsystem, Info, "Outer log message {}", 1);

    // Outer message was dispatched once; the inner recursive log inside write() was safely dropped without deadlocking.
    EXPECT_EQ(recursiveSink.getWriteCalls(), 1u);
    EXPECT_EQ(logger.getTotalDispatchedCount(), 1ULL);
    EXPECT_EQ(logger.getDroppedRecordCount(), 1ULL);

    logger.shutdown();
}

TEST(LoggerTest, FatalRecordForcesSynchronousDrainAndSinkFlush)
{
    Logger logger{};
    LoggerDesc desc{};
    desc.mode = LogExecutionMode::AsyncRing;
    desc.asyncFlushIntervalMicroseconds = 50000u;
    desc.applyCommandLineOverrides = false;
    ASSERT_EQ(logger.initialize(desc), LogResult::Success);

    MemoryRingSink ringSink(LogLevel::Trace);
    ASSERT_EQ(logger.addSink(&ringSink), LogResult::Success);

    LogTestSubsystem.setActiveLevel(LogLevel::Info);
    GP_LOGGER_LOG(logger, LogTestSubsystem, Info, "Queued info before fatal");
    GP_LOGGER_LOG(logger, LogTestSubsystem, Fatal, "Unrecoverable GPU page fault at {:#010x}", 0xBAD000ULL);

    // Fatal submission must synchronously drain the prior queued record, dispatch the Fatal record, and flush sinks
    // immediately without waiting for shutdown().
    EXPECT_EQ(ringSink.getSize(), 2u);
    EXPECT_GE(ringSink.getFlushCount(), 1u);
    EXPECT_EQ(ringSink.getEntry(0u).toView().message, "Queued info before fatal");
    EXPECT_EQ(ringSink.getEntry(1u).toView().message, "Unrecoverable GPU page fault at 0x00bad000");

    logger.shutdown();
}

TEST(LoggerTest, CommandLineOverridesVerbosityAndSyncMode)
{
    gp::CommandLine::initialize("gp_tests.exe -log-sync -log-level=warning");

    Logger logger{};
    LoggerDesc desc{};
    desc.mode = LogExecutionMode::AsyncRing;
    desc.globalMinLevel = LogLevel::Trace;
    desc.applyCommandLineOverrides = true;

    ASSERT_EQ(logger.initialize(desc), LogResult::Success);
    EXPECT_EQ(logger.getExecutionMode(), LogExecutionMode::Synchronous);
    EXPECT_EQ(logger.getGlobalMinLevel(), LogLevel::Warning);

    logger.shutdown();
    gp::CommandLine::shutdown();
}

}   // namespace gp::log::tests
