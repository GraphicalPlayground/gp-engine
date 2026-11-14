// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/LogFormatter.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/Name.hpp"
#include <gtest/gtest.h>

namespace gp::log::tests
{

TEST(LogFormatterTest, FormatsFundamentalTypesAndStrings)
{
    gp::FixedString<256> buffer{};

    const gp::Int32 intVal = -42;
    const gp::UInt64 uintVal = 1024ULL;
    const bool boolVal = true;
    const char charVal = 'X';
    const gp::StringView strVal = "Swapchain";

    const gp::StringView result =
        formatTo(buffer, "Int={} UInt={} Bool={} Char={} Str={}", intVal, uintVal, boolVal, charVal, strVal);

    EXPECT_EQ(result, "Int=-42 UInt=1024 Bool=true Char=X Str=Swapchain");
}

TEST(LogFormatterTest, FormatsHexadecimalBinaryAndPadding)
{
    gp::FixedString<256> buffer{};

    const gp::UInt32 hexVal = 0xDEADBEEFu;
    const gp::UInt32 shortHex = 0x4Au;
    const gp::UInt32 binVal = 0b10110u;
    const gp::Int32 negVal = -42;
    const void* const nullPtrVal = nullptr;
    const void* const nonNullPtr = reinterpret_cast<const void*>(static_cast<gp::UIntPtr>(0x1234ULL));

    const gp::StringView result = formatTo(
        buffer,
        "lower={:x} prefixed={:#x} upper={:#X} padHex={:#06x} bin={:#08b} negPad={:05} null={} ptr={}",
        hexVal,
        hexVal,
        hexVal,
        shortHex,
        binVal,
        negVal,
        nullPtrVal,
        nonNullPtr
    );

    EXPECT_EQ(
        result,
        "lower=deadbeef prefixed=0xdeadbeef upper=0XDEADBEEF padHex=0x004a bin=0b010110 negPad=-0042 null=nullptr ptr=0x1234"
    );
}

TEST(LogFormatterTest, FormatsFloatingPointMultiDigitPrecisionAndScientific)
{
    gp::FixedString<256> buffer{};

    const gp::Float64 pi = 3.1415926535;
    const gp::Float64 large = 1250.0;
    const gp::StringView result = formatTo(buffer, "Fixed2={:.2f} Fixed6={:.6f} Sci={:.2e}", pi, pi, large);

    EXPECT_EQ(result, "Fixed2=3.14 Fixed6=3.141593 Sci=1.25e+03");
}

TEST(LogFormatterTest, EvaluatesAtCompileTimeAndPreservesCStringAndEnums)
{
    constexpr auto buildCompileTimeString = []() consteval -> gp::FixedString<64>
    {
        gp::FixedString<64> out{};
        (void)formatTo(out, "Code={:#06x} Level={}", 0x2Au, LogLevel::Error);
        return out;
    };

    constexpr gp::FixedString<64> kCompiled = buildCompileTimeString();
    static_assert(kCompiled.view() == "Code=0x002a Level=ERROR");

    gp::FixedString<128> runtimeBuf{};
    const char* const nullCStr = nullptr;
    const char* const validCStr = "VulkanRHI";
    const gp::StringView runtimeView =
        formatTo(runtimeBuf, "A={} B={} Mode={}", nullCStr, validCStr, LogExecutionMode::AsyncRing);
    EXPECT_EQ(runtimeView, "A=(null) B=VulkanRHI Mode=1");
}

TEST(LogFormatterTest, FormatsEngineNameAndEscapedBraces)
{
    gp::FixedString<256> buffer{};
    const gp::Name entityName("Hero_7");

    const gp::StringView result = formatTo(buffer, "{{Entity}}: {} {{Done}}", entityName);
    EXPECT_EQ(result, "{Entity}: Hero_7 {Done}");
}

TEST(LogFormatterTest, TruncatesSafelyWhenBufferCapacityExceeded)
{
    gp::FixedString<10> tinyBuffer{};
    const gp::StringView result = formatTo(tinyBuffer, "0123456789ABCDEF", 42);

    // Must clamp cleanly to 10 characters without overflowing.
    EXPECT_EQ(result.size(), 10u);
    EXPECT_EQ(result, "0123456789");
    EXPECT_EQ(tinyBuffer.cString()[10], '\0');
}

TEST(LogFormatterTest, FormatsCompleteLogRecordLine)
{
    LogRecordView record{};
    record.dateTime.hour = 14u;
    record.dateTime.minute = 5u;
    record.dateTime.second = 9u;
    record.dateTime.millisecond = 42u;
    record.threadId = 77u;
    record.fiberId = 3u;
    record.level = LogLevel::Warning;
    record.categoryName = "LogRHI";
    record.message = "Descriptor heap near capacity";
    record.location = LogSourceLocation("C:/Engine/source/rhi/VulkanDevice.cpp", "allocateDescriptor", 128u);

    gp::FixedString<512> line{};
    const LogSinkFlags flags =
        LogSinkFlags::IncludeTimestamp | LogSinkFlags::IncludeThreadAndFiber | LogSinkFlags::IncludeSourceLocation;
    const gp::StringView formatted = formatLogRecordLine(line, record, flags);

    EXPECT_EQ(
        formatted, "[14:05:09.042][T:77 F:3][WARN][LogRHI][VulkanDevice.cpp:128] Descriptor heap near capacity\n"
    );
}

}   // namespace gp::log::tests
