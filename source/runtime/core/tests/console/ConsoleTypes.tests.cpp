// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/ConsoleTypes.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "templates/Optional.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace gp::console::tests
{

namespace
{

static_assert(kMaxConsoleNameLength == 127u);
static_assert(kMaxConsoleHelpLength == 255u);
static_assert(kMaxConsoleCommandArgs == 32u);
static_assert(kMaxConsoleCommandLineLength == 511u);
static_assert(kMaxConsoleStringLength == 255u);
static_assert(kDefaultConsoleHashCapacity == 2048u);
static_assert(kMaxDynamicConsoleVariables == 256u);
static_assert(kMaxDynamicConsoleCommands == 128u);
static_assert(kMaxConsoleSinks == 128u);
static_assert(kMaxConsoleCallbacksPerVariable == 8u);

static_assert(IsConsoleStringType<gp::StringView>);
static_assert(IsConsoleStringType<const gp::StringView&>);
static_assert(IsConsoleStringType<ConsoleString>);
static_assert(IsConsoleStringType<gp::String>);
static_assert(IsConsoleStringType<const char*>);
static_assert(!IsConsoleStringType<gp::Int32>);

static_assert(IsConsoleValueType<bool>);
static_assert(IsConsoleValueType<gp::Int32>);
static_assert(IsConsoleValueType<gp::UInt32>);
static_assert(IsConsoleValueType<gp::Float32>);
static_assert(IsConsoleValueType<gp::Float64>);
static_assert(IsConsoleValueType<gp::StringView>);
static_assert(!IsConsoleValueType<gp::Int16>);
static_assert(!IsConsoleValueType<gp::UInt64>);
static_assert(!IsConsoleValueType<void*>);

}   // namespace

TEST(ConsoleTypesTest, TrimConsoleWhitespace)
{
    EXPECT_TRUE(trimConsoleWhitespace("").isEmpty());
    EXPECT_TRUE(trimConsoleWhitespace("   \t\r\n  ").isEmpty());
    EXPECT_EQ(trimConsoleWhitespace("r.Shadows"), gp::StringView("r.Shadows"));
    EXPECT_EQ(trimConsoleWhitespace(" \t r.Shadow Quality \r\n "), gp::StringView("r.Shadow Quality"));
}

TEST(ConsoleTypesTest, HashConsoleNameIsCaseInsensitiveAndDistinct)
{
    const gp::UInt64 lowerHash = hashConsoleName("r.shadowquality");
    const gp::UInt64 upperHash = hashConsoleName("R.SHADOWQUALITY");
    const gp::UInt64 mixedHash = hashConsoleName("r.ShadowQuality");
    const gp::UInt64 otherHash = hashConsoleName("r.Bloom");

    EXPECT_EQ(lowerHash, upperHash);
    EXPECT_EQ(lowerHash, mixedHash);
    EXPECT_NE(lowerHash, otherHash);
}

TEST(ConsoleTypesTest, ParseConsoleBoolHandlesValidAndInvalidInputs)
{
    EXPECT_EQ(parseConsoleBool("true").value(), true);
    EXPECT_EQ(parseConsoleBool("  TRUE \t").value(), true);
    EXPECT_EQ(parseConsoleBool("1").value(), true);
    EXPECT_EQ(parseConsoleBool("on").value(), true);
    EXPECT_EQ(parseConsoleBool("On").value(), true);
    EXPECT_EQ(parseConsoleBool("yes").value(), true);
    EXPECT_EQ(parseConsoleBool("YES").value(), true);

    EXPECT_EQ(parseConsoleBool("false").value(), false);
    EXPECT_EQ(parseConsoleBool(" False ").value(), false);
    EXPECT_EQ(parseConsoleBool("0").value(), false);
    EXPECT_EQ(parseConsoleBool("off").value(), false);
    EXPECT_EQ(parseConsoleBool("OFF").value(), false);
    EXPECT_EQ(parseConsoleBool("no").value(), false);
    EXPECT_EQ(parseConsoleBool("NO").value(), false);

    EXPECT_FALSE(parseConsoleBool("").hasValue());
    EXPECT_FALSE(parseConsoleBool("2").hasValue());
    EXPECT_FALSE(parseConsoleBool("-1").hasValue());
    EXPECT_FALSE(parseConsoleBool("maybe").hasValue());
}

TEST(ConsoleTypesTest, ParseConsoleInt32HandlesDecimalHexBooleansAndRejectsInvalid)
{
    EXPECT_EQ(parseConsoleInt32("0").value(), 0);
    EXPECT_EQ(parseConsoleInt32("+42").value(), 42);
    EXPECT_EQ(parseConsoleInt32(" -123 ").value(), -123);
    EXPECT_EQ(parseConsoleInt32("2147483647").value(), std::numeric_limits<gp::Int32>::max());
    EXPECT_EQ(parseConsoleInt32("-2147483648").value(), std::numeric_limits<gp::Int32>::min());
    EXPECT_EQ(parseConsoleInt32("0x2A").value(), 42);
    EXPECT_EQ(parseConsoleInt32("0X7FFFFFFF").value(), std::numeric_limits<gp::Int32>::max());
    EXPECT_EQ(parseConsoleInt32("-0x10").value(), -16);
    EXPECT_EQ(parseConsoleInt32("-0x80000000").value(), std::numeric_limits<gp::Int32>::min());
    EXPECT_EQ(parseConsoleInt32("+0x10").value(), 16);
    EXPECT_EQ(parseConsoleInt32("true").value(), 1);
    EXPECT_EQ(parseConsoleInt32("false").value(), 0);

    EXPECT_FALSE(parseConsoleInt32("").hasValue());
    EXPECT_FALSE(parseConsoleInt32("+").hasValue());
    EXPECT_FALSE(parseConsoleInt32("+-5").hasValue());
    EXPECT_FALSE(parseConsoleInt32("-+5").hasValue());
    EXPECT_FALSE(parseConsoleInt32("0x").hasValue());
    EXPECT_FALSE(parseConsoleInt32("-0x80000001").hasValue());
    EXPECT_FALSE(parseConsoleInt32("0x80000000").hasValue());
    EXPECT_FALSE(parseConsoleInt32("2147483648").hasValue());
    EXPECT_FALSE(parseConsoleInt32("12abc").hasValue());
}

TEST(ConsoleTypesTest, ParseConsoleUInt32HandlesDecimalHexBooleansAndRejectsInvalid)
{
    EXPECT_EQ(parseConsoleUInt32("0").value(), 0u);
    EXPECT_EQ(parseConsoleUInt32("+99").value(), 99u);
    EXPECT_EQ(parseConsoleUInt32("4294967295").value(), std::numeric_limits<gp::UInt32>::max());
    EXPECT_EQ(parseConsoleUInt32("0xDEADBEEF").value(), 0xDEADBEEFu);
    EXPECT_EQ(parseConsoleUInt32("true").value(), 1u);
    EXPECT_EQ(parseConsoleUInt32("false").value(), 0u);

    EXPECT_FALSE(parseConsoleUInt32("").hasValue());
    EXPECT_FALSE(parseConsoleUInt32("+").hasValue());
    EXPECT_FALSE(parseConsoleUInt32("0x").hasValue());
    EXPECT_FALSE(parseConsoleUInt32("-1").hasValue());
    EXPECT_FALSE(parseConsoleUInt32("4294967296").hasValue());
    EXPECT_FALSE(parseConsoleUInt32("0x100000000").hasValue());
    EXPECT_FALSE(parseConsoleUInt32("0xZZ").hasValue());
}

TEST(ConsoleTypesTest, ParseConsoleFloat32AndFloat64HandleValidAndNonFiniteInputs)
{
    EXPECT_FLOAT_EQ(parseConsoleFloat32("0").value(), 0.0f);
    EXPECT_FLOAT_EQ(parseConsoleFloat32("+3.5").value(), 3.5f);
    EXPECT_FLOAT_EQ(parseConsoleFloat32(" -2.25e2 ").value(), -225.0f);
    EXPECT_FALSE(parseConsoleFloat32("").hasValue());
    EXPECT_FALSE(parseConsoleFloat32("+").hasValue());
    EXPECT_FALSE(parseConsoleFloat32("nan").hasValue());
    EXPECT_FALSE(parseConsoleFloat32("inf").hasValue());
    EXPECT_FLOAT_EQ(parseConsoleFloat32("1.5f").value(), 1.5f);
    EXPECT_FALSE(parseConsoleFloat32("1.5abc").hasValue());
    EXPECT_FALSE(parseConsoleFloat32("1e100").hasValue());

    EXPECT_DOUBLE_EQ(parseConsoleFloat64("0").value(), 0.0);
    EXPECT_DOUBLE_EQ(parseConsoleFloat64("+1.75").value(), 1.75);
    EXPECT_DOUBLE_EQ(parseConsoleFloat64("-3.14159265358979").value(), -3.14159265358979);
    EXPECT_FALSE(parseConsoleFloat64("").hasValue());
    EXPECT_FALSE(parseConsoleFloat64("+").hasValue());
    EXPECT_FALSE(parseConsoleFloat64("nan").hasValue());
    EXPECT_FALSE(parseConsoleFloat64("-inf").hasValue());
    EXPECT_FALSE(parseConsoleFloat64("abc").hasValue());
}

TEST(ConsoleTypesTest, FormatConsoleValueFunctionsRoundTripCleanly)
{
    EXPECT_EQ(formatConsoleBool(true).view(), gp::StringView("true"));
    EXPECT_EQ(formatConsoleBool(false).view(), gp::StringView("false"));

    EXPECT_EQ(formatConsoleInt32(0).view(), gp::StringView("0"));
    EXPECT_EQ(formatConsoleInt32(-2147483648).view(), gp::StringView("-2147483648"));
    EXPECT_EQ(formatConsoleUInt32(4294967295u).view(), gp::StringView("4294967295"));

    EXPECT_EQ(parseConsoleFloat32(formatConsoleFloat32(12.5f).view()).value(), 12.5f);
    EXPECT_EQ(formatConsoleFloat32(std::numeric_limits<gp::Float32>::quiet_NaN()).view(), gp::StringView("0"));
    EXPECT_EQ(formatConsoleFloat32(std::numeric_limits<gp::Float32>::infinity()).view(), gp::StringView("0"));

    EXPECT_DOUBLE_EQ(parseConsoleFloat64(formatConsoleFloat64(-99.125).view()).value(), -99.125);
    EXPECT_EQ(formatConsoleFloat64(std::numeric_limits<gp::Float64>::quiet_NaN()).view(), gp::StringView("0"));
    EXPECT_EQ(formatConsoleFloat64(-std::numeric_limits<gp::Float64>::infinity()).view(), gp::StringView("0"));
}

TEST(ConsoleTypesTest, ConsoleCommandArgsDefaultAndParsing)
{
    const ConsoleCommandArgs emptyArgs{};
    EXPECT_TRUE(emptyArgs.getCommandName().isEmpty());
    EXPECT_TRUE(emptyArgs.getRawArgs().isEmpty());
    EXPECT_EQ(emptyArgs.getArgCount(), 0u);
    EXPECT_TRUE(emptyArgs.isEmpty());
    EXPECT_TRUE(emptyArgs.getArg(0u).isEmpty());
    EXPECT_EQ(emptyArgs.getOutputDelegate(), nullptr);

    gp::container::BasicFixedString<char, 255u> capturedOutput{};
    ConsoleOutputDelegate outputSink = ConsoleOutputDelegate::createLambda(
        [&capturedOutput](const gp::StringView line) noexcept -> void
    {
        if (!capturedOutput.isEmpty())
        {
            capturedOutput.pushBack('\n');
        }
        capturedOutput.append(line);
    }
    );

    const ConsoleCommandArgs parsed(
        "  r.Spawn   \"Ultra \\\"High\\\"\"  'Shadow Map'  true  -42  0x10  3.5  6.25  ", &outputSink
    );
    EXPECT_EQ(parsed.getCommandName(), gp::StringView("r.Spawn"));
    EXPECT_EQ(parsed.getRawArgs(), gp::StringView("\"Ultra \\\"High\\\"\"  'Shadow Map'  true  -42  0x10  3.5  6.25"));
    EXPECT_FALSE(parsed.isEmpty());
    ASSERT_EQ(parsed.getArgCount(), 7u);
    EXPECT_EQ(parsed.getArg(0u), gp::StringView("Ultra \"High\""));
    EXPECT_EQ(parsed.getArg(1u), gp::StringView("Shadow Map"));
    EXPECT_EQ(parsed.getArg(2u), gp::StringView("true"));
    EXPECT_EQ(parsed.getArg(3u), gp::StringView("-42"));
    EXPECT_EQ(parsed.getArg(4u), gp::StringView("0x10"));
    EXPECT_EQ(parsed.getArg(5u), gp::StringView("3.5"));
    EXPECT_EQ(parsed.getArg(6u), gp::StringView("6.25"));
    EXPECT_TRUE(parsed.getArg(7u).isEmpty());

    EXPECT_EQ(parsed.tryGetBool(2u).value(), true);
    EXPECT_FALSE(parsed.tryGetBool(3u).hasValue());
    EXPECT_FALSE(parsed.tryGetBool(99u).hasValue());

    EXPECT_EQ(parsed.tryGetInt32(3u).value(), -42);
    EXPECT_EQ(parsed.tryGetInt32(4u).value(), 16);
    EXPECT_FALSE(parsed.tryGetInt32(0u).hasValue());

    EXPECT_EQ(parsed.tryGetUInt32(4u).value(), 16u);
    EXPECT_FALSE(parsed.tryGetUInt32(3u).hasValue());

    EXPECT_FLOAT_EQ(parsed.tryGetFloat32(5u).value(), 3.5f);
    EXPECT_FALSE(parsed.tryGetFloat32(0u).hasValue());

    EXPECT_DOUBLE_EQ(parsed.tryGetFloat64(6u).value(), 6.25);
    EXPECT_FALSE(parsed.tryGetFloat64(0u).hasValue());

    EXPECT_EQ(parsed.getOutputDelegate(), &outputSink);
    parsed.writeLine("line-one");
    parsed.writeLine("line-two");
    EXPECT_EQ(capturedOutput.view(), gp::StringView("line-one\nline-two"));
}

TEST(ConsoleTypesTest, ConsoleCommandArgsEdgeCasesAndLineLimits)
{
    ConsoleCommandArgs args("seed a b", nullptr);
    args.writeLine("ignored-without-sink");

    ConsoleOutputDelegate unboundSink{};
    const ConsoleCommandArgs unboundArgs("cmd", &unboundSink);
    unboundArgs.writeLine("ignored-unbound");

    EXPECT_FALSE(ConsoleCommandArgs::parse("   \t  ", args, nullptr));
    EXPECT_TRUE(args.getCommandName().isEmpty());
    EXPECT_EQ(args.getArgCount(), 0u);

    // Build an input line containing more than kMaxConsoleCommandLineLength bytes.
    char oversized[kMaxConsoleCommandLineLength + 16u]{};
    for (gp::USize i = 0u; i < kMaxConsoleCommandLineLength + 8u; ++i)
    {
        oversized[i] = 'a';
    }
    EXPECT_FALSE(
        ConsoleCommandArgs::parse(gp::StringView(oversized, kMaxConsoleCommandLineLength + 8u), args, nullptr)
    );

    // Build a valid line with exactly kMaxConsoleCommandArgs positional arguments (must succeed) and +1 (must fail).
    gp::container::BasicFixedString<char, kMaxConsoleCommandLineLength> exactArgs{};
    exactArgs.append("cmd");
    for (gp::USize i = 0u; i < kMaxConsoleCommandArgs; ++i)
    {
        exactArgs.append(" x");
    }
    ASSERT_TRUE(ConsoleCommandArgs::parse(exactArgs.view(), args, nullptr));
    EXPECT_EQ(args.getCommandName(), gp::StringView("cmd"));
    EXPECT_EQ(args.getArgCount(), kMaxConsoleCommandArgs);

    exactArgs.append(" x");
    EXPECT_FALSE(ConsoleCommandArgs::parse(exactArgs.view(), args, nullptr));

    // Unclosed double or single quotes must fail parsing cleanly.
    EXPECT_FALSE(ConsoleCommandArgs::parse("cmd \"unterminated", args, nullptr));
    EXPECT_FALSE(ConsoleCommandArgs::parse("cmd 'unterminated", args, nullptr));

    // Backslash escape inside single quotes preserves literal quote and backslash.
    ASSERT_TRUE(ConsoleCommandArgs::parse("cmd 'it\\'s \\\\ok'", args, nullptr));
    EXPECT_EQ(args.getArg(0u), gp::StringView("it's \\ok"));
}

}   // namespace gp::console::tests
