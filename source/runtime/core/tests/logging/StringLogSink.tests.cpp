// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/StringLogSink.hpp"
#include "containers/strings/FixedString.hpp"
#include "logging/ILogSink.hpp"
#include "logging/LogCategory.hpp"
#include "logging/Logger.hpp"
#include "logging/LogMacros.hpp"
#include <gtest/gtest.h>

namespace gp::log::tests
{

namespace
{

GP_DEFINE_LOG_CATEGORY_INLINE(LogStringSinkTest, gp::log::LogLevel::Trace, gp::log::LogLevel::Trace);

}   // namespace

TEST(StringLogSinkTest, CapturesFormattedLinesThroughILogSinkAndTracksStats)
{
    StringLogSink sink(LogLevel::Info, LogSinkFlags::None);
    ILogSink* const outputDevice = &sink;

    EXPECT_TRUE(sink.isEmpty());
    EXPECT_FALSE(sink.isTruncated());
    EXPECT_EQ(sink.size(), 0u);
    EXPECT_EQ(sink.capacity(), StringLogSink::kCapacity);
    EXPECT_EQ(sink.remainingCapacity(), StringLogSink::kCapacity);
    EXPECT_EQ(sink.getTotalWriteCount(), 0u);
    EXPECT_EQ(sink.getFlushCount(), 0u);
    EXPECT_EQ(sink.getFlags(), LogSinkFlags::None);

    Logger logger{};
    LoggerDesc desc{};
    desc.mode = LogExecutionMode::Synchronous;
    desc.applyCommandLineOverrides = false;
    ASSERT_EQ(logger.initialize(desc), LogResult::Success);
    ASSERT_EQ(logger.addSink(outputDevice), LogResult::Success);

    GP_LOGGER_LOG(logger, LogStringSinkTest, Debug, "Filtered debug {}", 1);
    GP_LOGGER_LOG(logger, LogStringSinkTest, Info, "Boot step {} complete", 2);
    GP_LOGGER_LOG(logger, LogStringSinkTest, Warning, "Cache pressure {}%", 85);

    logger.flush();

    EXPECT_FALSE(sink.isEmpty());
    EXPECT_FALSE(sink.isTruncated());
    EXPECT_EQ(sink.getTotalWriteCount(), 2u);
    EXPECT_GE(sink.getFlushCount(), 1u);
    EXPECT_GT(sink.size(), 0u);
    EXPECT_EQ(sink.remainingCapacity(), StringLogSink::kCapacity - sink.size());
    EXPECT_STREQ(sink.cString(), sink.view().data());
    EXPECT_FALSE(sink.contains("Filtered debug"));
    EXPECT_TRUE(sink.contains("[INFO][LogStringSinkTest] Boot step 2 complete"));
    EXPECT_TRUE(sink.contains("[WARN][LogStringSinkTest] Cache pressure 85%"));

    sink.setFlags(LogSinkFlags::IncludeSourceLocation);
    EXPECT_EQ(sink.getFlags(), LogSinkFlags::IncludeSourceLocation);
    GP_LOGGER_LOG(logger, LogStringSinkTest, Error, "With location");
    EXPECT_TRUE(sink.contains("StringLogSink.tests.cpp:"));

    sink.clear();
    EXPECT_TRUE(sink.isEmpty());
    EXPECT_FALSE(sink.isTruncated());
    EXPECT_EQ(sink.size(), 0u);
    EXPECT_EQ(sink.remainingCapacity(), StringLogSink::kCapacity);
    EXPECT_EQ(sink.getTotalWriteCount(), 0u);
    EXPECT_EQ(sink.getFlushCount(), 0u);

    logger.shutdown();
}

TEST(StringLogSinkTest, ClipsGracefullyAtCapacityAndMarksTruncated)
{
    StringLogSink sink(LogLevel::Trace, LogSinkFlags::None);

    gp::FixedString<1024> chunk(1000u, 'A');
    LogRecordView record{};
    record.categoryName = "CapTest";
    record.level = LogLevel::Info;
    record.message = chunk.view();

    for (gp::UInt32 i = 0u; i < 20u; ++i)
    {
        sink.write(record);
    }

    EXPECT_TRUE(sink.isTruncated());
    EXPECT_EQ(sink.size(), StringLogSink::kCapacity);
    EXPECT_EQ(sink.remainingCapacity(), 0u);
    EXPECT_EQ(sink.getTotalWriteCount(), 20u);
    EXPECT_EQ(sink.cString()[StringLogSink::kCapacity], '\0');

    sink.clear();
    EXPECT_FALSE(sink.isTruncated());
    EXPECT_EQ(sink.size(), 0u);
}

}   // namespace gp::log::tests
