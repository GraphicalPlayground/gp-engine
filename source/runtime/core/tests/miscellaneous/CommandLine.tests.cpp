// File: source/runtime/core/tests/miscellaneous/CommandLine.tests.cpp
// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "miscellaneous/CommandLine.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

class CommandLineTest : public ::testing::Test
{
protected:
    virtual void TearDown() noexcept override
    {
        gp::CommandLine::shutdown();
    }
};

TEST_F(CommandLineTest, InitFromArgcArgvAndNonDanglingArena)
{
    {
        char mutableArg1[] = "gp-engine.exe";
        char mutableArg2[] = "--headless";
        char mutableArg3[] = "-rhi=vulkan";
        const char* argv[] = { mutableArg1, mutableArg2, mutableArg3 };

        gp::CommandLine::initialize(3, argv);

        // Mutate caller stack buffers to prove CommandLine copied tokens into its internal .bss arena
        mutableArg2[2] = 'X';
        mutableArg3[5] = 'Z';
    }

    EXPECT_TRUE(gp::CommandLine::isInitialized());
    EXPECT_EQ(gp::CommandLine::getArgumentCount(), 3u);
    EXPECT_EQ(gp::CommandLine::getExecutablePath(), gp::StringView("gp-engine.exe"));
    EXPECT_EQ(gp::CommandLine::getOriginalCommandLine(), gp::StringView("--headless -rhi=vulkan"));
    EXPECT_TRUE(gp::CommandLine::hasArgument("headless"));
    EXPECT_TRUE(gp::CommandLine::hasArgument("-headless"));
    EXPECT_TRUE(gp::CommandLine::hasArgument("--HEADLESS"));
    EXPECT_FALSE(gp::CommandLine::hasArgument("--HEADLESS", true));
    EXPECT_EQ(gp::CommandLine::getValue("-rhi"), gp::StringView("vulkan"));
}

TEST_F(CommandLineTest, InitFromRawStringWithQuotesAndAppend)
{
    gp::CommandLine::initialize(
        "\"C:\\Program Files\\GP\\gp-engine.exe\" -title=\"Graphical Playground\" -map:Level_01 -threads 8"
    );

    ASSERT_TRUE(gp::CommandLine::isInitialized());
    EXPECT_EQ(gp::CommandLine::getExecutablePath(), gp::StringView("C:\\Program Files\\GP\\gp-engine.exe"));
    EXPECT_EQ(gp::CommandLine::getValue("-title"), gp::StringView("Graphical Playground"));
    EXPECT_EQ(gp::CommandLine::getValue("--map"), gp::StringView("Level_01"));
    EXPECT_EQ(gp::CommandLine::getInt32("-threads"), 8);

    // Append developer overrides and verify last-wins behavior
    const gp::UInt32 appended = gp::CommandLine::append("-threads=16 -map 'Arena Map'");
    EXPECT_EQ(appended, 3u);
    EXPECT_EQ(gp::CommandLine::getInt32("threads"), 16);
    EXPECT_EQ(gp::CommandLine::getValue("map"), gp::StringView("Arena Map"));
}

TEST_F(CommandLineTest, BooleanParsingAndNegationPrefixes)
{
    const char* argv[] = { "gp-engine", "-fullscreen", "--no-vsync", "-hdr=on",   "-raytracing=0",
                           "-bloom",    "false",       "-audio",     "--no-audio" };
    gp::CommandLine::initialize(9, argv);

    EXPECT_TRUE(gp::CommandLine::getBool("-fullscreen", false));
    EXPECT_FALSE(gp::CommandLine::getBool("-vsync", true));
    EXPECT_TRUE(gp::CommandLine::getBool("hdr", false));
    EXPECT_FALSE(gp::CommandLine::getBool("-raytracing", true));
    EXPECT_FALSE(gp::CommandLine::getBool("-bloom", true));

    // Last-wins: "--no-audio" overrides earlier "-audio"
    EXPECT_FALSE(gp::CommandLine::getBool("-audio", true));

    // Unspecified flag returns empty Optional or fallback default
    EXPECT_FALSE(gp::CommandLine::tryGetBool("-vr").hasValue());
    EXPECT_TRUE(gp::CommandLine::getBool("-vr", true));
}

TEST_F(CommandLineTest, IntegerAndUnsignedParsingWithHexBinaryAndNegativeValues)
{
    const char* argv[] = { "gp-engine",        "-offset",        "-42",
                           "-seed=0xCAFEBABE", "-mask=0b101101", "-port=8080",
                           "-small=127",       "-overflow8=300", "-badInt=123abc" };
    gp::CommandLine::initialize(9, argv);

    // Negative space-separated value must not be mistaken for a flag
    EXPECT_EQ(gp::CommandLine::getInt32("-offset", 0), -42);
    EXPECT_FALSE(gp::CommandLine::tryGetUInt32("-offset").hasValue());

    // Hexadecimal and binary parsing
    EXPECT_EQ(gp::CommandLine::getUInt32("-seed", 0u), 0xCAFEBABEu);
    EXPECT_EQ(gp::CommandLine::getUInt32("-mask", 0u), 45u);
    EXPECT_EQ(gp::CommandLine::get<gp::UInt16>("-port", 0u), static_cast<gp::UInt16>(8080u));

    // Narrow integer bounds checking via template tryGet<T>
    const gp::Optional<gp::Int8> validInt8 = gp::CommandLine::tryGet<gp::Int8>("-small");
    ASSERT_TRUE(validInt8.hasValue());
    EXPECT_EQ(*validInt8, static_cast<gp::Int8>(127));

    EXPECT_FALSE(gp::CommandLine::tryGet<gp::UInt8>("-overflow8").hasValue());
    EXPECT_EQ(gp::CommandLine::get<gp::UInt8>("-overflow8", 10u), 10u);

    // Partial/trailing garbage characters must be rejected
    EXPECT_FALSE(gp::CommandLine::tryGetInt32("-badInt").hasValue());
}

TEST_F(CommandLineTest, FloatingPointParsing)
{
    const char* argv[] = { "gp-engine", "-fov=90.5", "-exposure", "-1.25", "-epsilon=1.5e-4", "-invalidFloat=3.14.15" };
    gp::CommandLine::initialize(6, argv);

    EXPECT_FLOAT_EQ(gp::CommandLine::getFloat32("-fov", 60.0f), 90.5f);
    EXPECT_FLOAT_EQ(gp::CommandLine::getFloat32("-exposure", 0.0f), -1.25f);
    EXPECT_DOUBLE_EQ(gp::CommandLine::getFloat64("-epsilon", 0.0), 1.5e-4);
    EXPECT_FALSE(gp::CommandLine::tryGetFloat32("-invalidFloat").hasValue());
}

TEST_F(CommandLineTest, MultiValueAndPositionalArgumentsWithDoubleDash)
{
    const char* argv[] = { "gp-engine", "project.gpproj",     "-plugin=RenderDoc", "-plugin", "Tracy",
                           "--",        "-rawPositionalFlag", "extra_file.pak" };
    gp::CommandLine::initialize(8, argv);

    gp::StringView plugins[4]{};
    const gp::UInt32 pluginCount = gp::CommandLine::getValues("-plugin", plugins, 4u);
    ASSERT_EQ(pluginCount, 2u);
    EXPECT_EQ(plugins[0], gp::StringView("RenderDoc"));
    EXPECT_EQ(plugins[1], gp::StringView("Tracy"));

    // "-rawPositionalFlag" after "--" must NOT be treated as a flag
    EXPECT_FALSE(gp::CommandLine::hasArgument("-rawPositionalFlag"));

    gp::StringView positional[4]{};
    const gp::UInt32 posCount = gp::CommandLine::getPositionalArguments(positional, 4u);
    ASSERT_EQ(posCount, 3u);
    EXPECT_EQ(positional[0], gp::StringView("project.gpproj"));
    EXPECT_EQ(positional[1], gp::StringView("-rawPositionalFlag"));
    EXPECT_EQ(positional[2], gp::StringView("extra_file.pak"));
}

}   // namespace gp::tests
