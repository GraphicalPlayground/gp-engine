// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/LogCategory.hpp"
#include "logging/LogEnums.hpp"
#include <gtest/gtest.h>

namespace gp::log::tests
{

constexpr bool testConstexprCategoryEvaluation() noexcept
{
    const LogCategory cat("ConstexprCat", LogLevel::Warning, LogLevel::Debug);
    if (cat.getName() != "ConstexprCat")
    {
        return false;
    }
    if (cat.getDefaultLevel() != LogLevel::Warning)
    {
        return false;
    }
    if (cat.getCompileTimeMinLevel() != LogLevel::Debug)
    {
        return false;
    }
    if (cat.isEnabled(LogLevel::Info))
    {
        return false;
    }
    if (!cat.isEnabled(LogLevel::Warning) || !cat.isEnabled(LogLevel::Error))
    {
        return false;
    }
    return true;
}

static_assert(testConstexprCategoryEvaluation(), "Constexpr LogCategory evaluation failed.");

TEST(LogCategoryTest, DefaultAndCompileTimeLevelClamping)
{
    // Default level (Trace) is below compile-time min (Warning), so it must clamp to Warning.
    LogCategory category("LogPhysics", LogLevel::Trace, LogLevel::Warning);

    EXPECT_EQ(category.getName(), "LogPhysics");
    EXPECT_EQ(category.getNameLength(), 10u);
    EXPECT_EQ(category.getCompileTimeMinLevel(), LogLevel::Warning);
    EXPECT_EQ(category.getDefaultLevel(), LogLevel::Warning);
    EXPECT_EQ(category.getActiveLevel(), LogLevel::Warning);

    EXPECT_FALSE(category.isEnabled(LogLevel::Trace));
    EXPECT_FALSE(category.isEnabled(LogLevel::Debug));
    EXPECT_FALSE(category.isEnabled(LogLevel::Info));
    EXPECT_TRUE(category.isEnabled(LogLevel::Warning));
    EXPECT_TRUE(category.isEnabled(LogLevel::Error));
    EXPECT_TRUE(category.isEnabled(LogLevel::Fatal));
    EXPECT_FALSE(category.isEnabled(LogLevel::Off));
}

TEST(LogCategoryTest, RuntimeLevelMutationRespectsCompileTimeFloor)
{
    LogCategory category("LogAudio", LogLevel::Info, LogLevel::Debug);

    EXPECT_FALSE(category.isEnabled(LogLevel::Debug));
    EXPECT_TRUE(category.isEnabled(LogLevel::Info));

    // Lowering runtime level to Debug (allowed by compile-time floor) enables Debug.
    category.setActiveLevel(LogLevel::Debug);
    EXPECT_EQ(category.getActiveLevel(), LogLevel::Debug);
    EXPECT_TRUE(category.isEnabled(LogLevel::Debug));

    // Attempting to lower runtime level to Trace (below compile-time floor Debug) clamps to Debug.
    category.setActiveLevel(LogLevel::Trace);
    EXPECT_EQ(category.getActiveLevel(), LogLevel::Debug);
    EXPECT_FALSE(category.isEnabled(LogLevel::Trace));

    // Setting runtime level to Off disables all levels.
    category.setActiveLevel(LogLevel::Off);
    EXPECT_FALSE(category.isEnabled(LogLevel::Fatal));

    // Resetting restores initial default level (Info).
    category.resetToDefaultLevel();
    EXPECT_EQ(category.getActiveLevel(), LogLevel::Info);
}

TEST(LogCategoryTest, NullOrEmptyCategoryNameFallsBackToDefault)
{
    const LogCategory nullCat(nullptr, LogLevel::Info, LogLevel::Trace);
    EXPECT_EQ(nullCat.getName(), "Default");

    const LogCategory emptyCat("", LogLevel::Info, LogLevel::Trace);
    EXPECT_EQ(emptyCat.getName(), "Default");
}

TEST(LogEnumsTest, LevelStringConversionAndParsing)
{
    EXPECT_EQ(logLevelToString(LogLevel::Trace), "TRACE");
    EXPECT_EQ(logLevelToString(LogLevel::Debug), "DEBUG");
    EXPECT_EQ(logLevelToString(LogLevel::Info), "INFO");
    EXPECT_EQ(logLevelToString(LogLevel::Warning), "WARN");
    EXPECT_EQ(logLevelToString(LogLevel::Error), "ERROR");
    EXPECT_EQ(logLevelToString(LogLevel::Fatal), "FATAL");
    EXPECT_EQ(logLevelToString(LogLevel::Off), "OFF");

    LogLevel parsed = LogLevel::Info;
    EXPECT_TRUE(tryParseLogLevel("trace", parsed));
    EXPECT_EQ(parsed, LogLevel::Trace);

    EXPECT_TRUE(tryParseLogLevel("DEBUG", parsed));
    EXPECT_EQ(parsed, LogLevel::Debug);

    EXPECT_TRUE(tryParseLogLevel("Warn", parsed));
    EXPECT_EQ(parsed, LogLevel::Warning);

    EXPECT_TRUE(tryParseLogLevel("Warning", parsed));
    EXPECT_EQ(parsed, LogLevel::Warning);

    EXPECT_TRUE(tryParseLogLevel("Error", parsed));
    EXPECT_EQ(parsed, LogLevel::Error);

    EXPECT_TRUE(tryParseLogLevel("Fatal", parsed));
    EXPECT_EQ(parsed, LogLevel::Fatal);

    EXPECT_TRUE(tryParseLogLevel("off", parsed));
    EXPECT_EQ(parsed, LogLevel::Off);

    EXPECT_FALSE(tryParseLogLevel("invalid_level", parsed));
}

}   // namespace gp::log::tests
