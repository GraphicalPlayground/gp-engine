// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/ConsoleEnums.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::console::tests
{

namespace
{

static_assert(static_cast<gp::UInt8>(ConsoleVarType::Bool) == 0u);
static_assert(static_cast<gp::UInt8>(ConsoleVarType::String) == 5u);

static_assert(
    static_cast<gp::UInt8>(ConsoleVarSetBy::Constructor) < static_cast<gp::UInt8>(ConsoleVarSetBy::Scalability)
);
static_assert(
    static_cast<gp::UInt8>(ConsoleVarSetBy::Scalability) < static_cast<gp::UInt8>(ConsoleVarSetBy::GameSetting)
);
static_assert(
    static_cast<gp::UInt8>(ConsoleVarSetBy::GameSetting) < static_cast<gp::UInt8>(ConsoleVarSetBy::ProjectSetting)
);
static_assert(
    static_cast<gp::UInt8>(ConsoleVarSetBy::ProjectSetting) < static_cast<gp::UInt8>(ConsoleVarSetBy::SystemConfig)
);
static_assert(
    static_cast<gp::UInt8>(ConsoleVarSetBy::SystemConfig) < static_cast<gp::UInt8>(ConsoleVarSetBy::DeviceProfile)
);
static_assert(
    static_cast<gp::UInt8>(ConsoleVarSetBy::DeviceProfile) < static_cast<gp::UInt8>(ConsoleVarSetBy::ConsoleVariables)
);
static_assert(
    static_cast<gp::UInt8>(ConsoleVarSetBy::ConsoleVariables) < static_cast<gp::UInt8>(ConsoleVarSetBy::CommandLine)
);
static_assert(static_cast<gp::UInt8>(ConsoleVarSetBy::CommandLine) < static_cast<gp::UInt8>(ConsoleVarSetBy::Code));
static_assert(static_cast<gp::UInt8>(ConsoleVarSetBy::Code) < static_cast<gp::UInt8>(ConsoleVarSetBy::Console));

static_assert((ConsoleVarFlags::ReadOnly | ConsoleVarFlags::Cheat) != ConsoleVarFlags::None);
static_assert(
    ((ConsoleVarFlags::ReadOnly | ConsoleVarFlags::Cheat) & ConsoleVarFlags::Cheat) == ConsoleVarFlags::Cheat
);
static_assert(
    (~ConsoleVarFlags::ReadOnly & (ConsoleVarFlags::ReadOnly | ConsoleVarFlags::Latched)) == ConsoleVarFlags::Latched
);
static_assert(gp::StringView(varTypeToString(ConsoleVarType::Float64)) == gp::StringView("Float64"));
static_assert(gp::StringView(setByToString(ConsoleVarSetBy::CommandLine)) == gp::StringView("CommandLine"));
static_assert(gp::StringView(consoleResultToString(ConsoleResult::PriorityTooLow)) == gp::StringView("PriorityTooLow"));

}   // namespace

TEST(ConsoleEnumsTest, ConsoleVarFlagsBitwiseOperations)
{
    ConsoleVarFlags flags = ConsoleVarFlags::None;
    EXPECT_EQ(static_cast<gp::UInt32>(flags), 0u);

    flags |= ConsoleVarFlags::ReadOnly;
    flags |= ConsoleVarFlags::RenderThreadSafe;
    EXPECT_EQ(flags & ConsoleVarFlags::ReadOnly, ConsoleVarFlags::ReadOnly);
    EXPECT_EQ(flags & ConsoleVarFlags::RenderThreadSafe, ConsoleVarFlags::RenderThreadSafe);
    EXPECT_EQ(flags & ConsoleVarFlags::Cheat, ConsoleVarFlags::None);

    flags ^= ConsoleVarFlags::ReadOnly;
    EXPECT_EQ(flags, ConsoleVarFlags::RenderThreadSafe);

    flags |= ConsoleVarFlags::Latched | ConsoleVarFlags::Archive;
    flags &= ~ConsoleVarFlags::Latched;
    EXPECT_EQ(flags, ConsoleVarFlags::RenderThreadSafe | ConsoleVarFlags::Archive);
}

TEST(ConsoleEnumsTest, VarTypeToStringCoversAllValuesAndFallback)
{
    EXPECT_STREQ(varTypeToString(ConsoleVarType::Bool), "Bool");
    EXPECT_STREQ(varTypeToString(ConsoleVarType::Int32), "Int32");
    EXPECT_STREQ(varTypeToString(ConsoleVarType::UInt32), "UInt32");
    EXPECT_STREQ(varTypeToString(ConsoleVarType::Float32), "Float32");
    EXPECT_STREQ(varTypeToString(ConsoleVarType::Float64), "Float64");
    EXPECT_STREQ(varTypeToString(ConsoleVarType::String), "String");
    EXPECT_STREQ(varTypeToString(static_cast<ConsoleVarType>(250u)), "Unknown");
}

TEST(ConsoleEnumsTest, SetByToStringCoversAllValuesAndFallback)
{
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::Constructor), "Constructor");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::Scalability), "Scalability");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::GameSetting), "GameSetting");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::ProjectSetting), "ProjectSetting");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::SystemConfig), "SystemConfig");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::DeviceProfile), "DeviceProfile");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::ConsoleVariables), "ConsoleVariables");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::CommandLine), "CommandLine");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::Code), "Code");
    EXPECT_STREQ(setByToString(ConsoleVarSetBy::Console), "Console");
    EXPECT_STREQ(setByToString(static_cast<ConsoleVarSetBy>(250u)), "Unknown");
}

TEST(ConsoleEnumsTest, ConsoleResultToStringCoversAllValuesAndFallback)
{
    EXPECT_STREQ(consoleResultToString(ConsoleResult::Success), "Success");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::NotFound), "NotFound");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::AlreadyExists), "AlreadyExists");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::InvalidArguments), "InvalidArguments");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::TypeMismatch), "TypeMismatch");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::ParseFailed), "ParseFailed");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::ReadOnly), "ReadOnly");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::CheatBlocked), "CheatBlocked");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::PriorityTooLow), "PriorityTooLow");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::CapacityExceeded), "CapacityExceeded");
    EXPECT_STREQ(consoleResultToString(ConsoleResult::ConsoleDisabled), "ConsoleDisabled");
    EXPECT_STREQ(consoleResultToString(static_cast<ConsoleResult>(250u)), "Unknown");
}

}   // namespace gp::console::tests
