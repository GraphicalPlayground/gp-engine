// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/String.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/allocators/StackAllocator.hpp"
#include "memory/allocators/TaggedAllocator.hpp"   // IWYU pragma: keep
#include "memory/allocators/ThreadScratchAllocator.hpp"
#include <format>
#include <gtest/gtest.h>
#include <sstream>

namespace gp::tests
{

TEST(StringTests, FootprintAndConcepts)
{
    static_assert(sizeof(gp::String) == 24u);
    static_assert(sizeof(gp::WString) == 24u);
    static_assert(sizeof(gp::U8String) == 24u);
    static_assert(sizeof(gp::U16String) == 24u);
    static_assert(sizeof(gp::U32String) == 24u);
    static_assert(sizeof(gp::ScratchString) == 24u);
    static_assert(sizeof(gp::PolymorphicString) == 32u);

    static_assert(gp::concepts::IsString<gp::String>);
    static_assert(gp::concepts::IsString<gp::WString>);
    static_assert(gp::concepts::IsString<gp::ScratchString>);
    static_assert(gp::concepts::IsString<gp::PolymorphicString>);
    static_assert(gp::concepts::IsString<gp::InlineString<128u>>);
    static_assert(!gp::concepts::IsString<gp::StringView>);
}

TEST(StringTests, SsoBoundaryAndHeapTransition)
{
    gp::String str;
    EXPECT_TRUE(str.isEmpty());
    EXPECT_TRUE(str.isSso());
    EXPECT_FALSE(str.isLong());
    EXPECT_EQ(str.size(), 0u);
    EXPECT_EQ(str.capacity(), 22u);
    EXPECT_STREQ(str.cString(), "");

    // Fill right up to the exact 22-character SSO boundary ("0123456789012345678901")
    const char* const exact22 = "0123456789012345678901";
    str.assign(exact22);
    EXPECT_EQ(str.size(), 22u);
    EXPECT_EQ(str.capacity(), 22u);
    EXPECT_TRUE(str.isSso());
    EXPECT_FALSE(str.isLong());
    EXPECT_EQ(str.cString()[22], '\0');
    EXPECT_STREQ(*str, exact22);

    // Pushing the 23rd character must transition from SSO to Long (Heap) mode
    str.pushBack('2');
    EXPECT_EQ(str.size(), 23u);
    EXPECT_GT(str.capacity(), 22u);
    EXPECT_FALSE(str.isSso());
    EXPECT_TRUE(str.isLong());
    EXPECT_STREQ(*str, "01234567890123456789012");

    // Popping back to 22 keeps it in Long mode until shrinkToFit() or reset() is called
    str.popBack();
    EXPECT_EQ(str.size(), 22u);
    EXPECT_TRUE(str.isLong());

    // shrinkToFit() at size <= 22 must demote back into the 24-byte inline SSO buffer and free the heap block!
    str.shrinkToFit();
    EXPECT_TRUE(str.isSso());
    EXPECT_FALSE(str.isLong());
    EXPECT_EQ(str.size(), 22u);
    EXPECT_EQ(str.capacity(), 22u);
    EXPECT_STREQ(*str, exact22);
}

TEST(StringTests, ConstructorsAndAssignments)
{
    gp::String s1("Hello");
    EXPECT_EQ(s1, "Hello");

    gp::String s2(5u, 'A');
    EXPECT_EQ(s2, "AAAAA");

    gp::String s3("Hello World", 5u);
    EXPECT_EQ(s3, "Hello");

    gp::FixedString<16> fixed("FixedPayload");
    gp::String s4(fixed);
    EXPECT_EQ(s4, "FixedPayload");

    gp::String s5({ 'G', 'P', 'E' });
    EXPECT_EQ(s5, "GPE");

    const char arr[] = { 'D', 'O', 'D' };
    gp::String s6(arr, arr + 3);
    EXPECT_EQ(s6, "DOD");

    // Copy and move for both SSO and Long strings
    gp::String longStr("This string is definitely longer than twenty-two characters!");
    EXPECT_TRUE(longStr.isLong());

    gp::String longCopy(longStr);
    EXPECT_TRUE(longCopy.isLong());
    EXPECT_EQ(longCopy, longStr);
    EXPECT_NE(longCopy.data(), longStr.data());

    const char* const originalHeapPtr = longCopy.data();
    gp::String longMoved(std::move(longCopy));
    EXPECT_TRUE(longMoved.isLong());
    EXPECT_EQ(longMoved.data(), originalHeapPtr);
    EXPECT_TRUE(longCopy.isEmpty());
    EXPECT_TRUE(longCopy.isSso());
}

TEST(StringTests, LinearAllocatorTopOfBumperInPlaceGrowth)
{
    alignas(16) gp::UInt8 arenaBuffer[1024]{};
    gp::memory::LinearAllocator linearArena(arenaBuffer, sizeof(arenaBuffer), "TestStringLinearArena");

    {
        gp::PolymorphicString polyStr{ gp::memory::PolymorphicAllocator(linearArena) };
        EXPECT_EQ(linearArena.getUsedBytes(), 0u);

        // Fit in SSO first -> 0 bytes allocated from arena
        polyStr.assign("ShortSSO");
        EXPECT_TRUE(polyStr.isSso());
        EXPECT_EQ(linearArena.getUsedBytes(), 0u);

        // Reserve 31 chars (32 bytes with null terminator) -> allocates from LinearAllocator
        polyStr.reserve(31u);
        EXPECT_TRUE(polyStr.isLong());
        const char* const firstArenaPtr = polyStr.data();
        const gp::USize usedAfterFirstAlloc = linearArena.getUsedBytes();
        EXPECT_GE(usedAfterFirstAlloc, 32u);

        // Reserve 63 chars (64 bytes with null terminator) while at the top of the bump cursor:
        // LinearAllocator::reallocate must expand the block IN-PLACE without moving the pointer!
        polyStr.reserve(63u);
        EXPECT_EQ(polyStr.data(), firstArenaPtr);
        EXPECT_EQ(polyStr.view(), "ShortSSO");
        EXPECT_GE(linearArena.getUsedBytes(), 64u);
    }

    // Upon destruction at the top of the bump cursor, LinearAllocator rewinds back to 0!
    EXPECT_EQ(linearArena.getUsedBytes(), 0u);
}

TEST(StringTests, StackAllocatorAndScratchStringIntegration)
{
    alignas(16) gp::UInt8 stackBuffer[1024]{};
    gp::memory::StackAllocator stackArena(stackBuffer, sizeof(stackBuffer), "TestStringStackArena");

    {
        gp::PolymorphicString stackStr{ gp::memory::PolymorphicAllocator(stackArena) };
        stackStr.assign("Stack-allocated string exceeding 22 characters for LIFO test");
        EXPECT_TRUE(stackStr.isLong());
        EXPECT_GT(stackArena.getUsedBytes(), 0u);
    }
    EXPECT_EQ(stackArena.getUsedBytes(), 0u);

    // ThreadScratchAllocator integration
    gp::memory::LinearAllocator& tlsArena = gp::memory::ThreadScratchArena::get();
    const gp::USize baselineUsed = tlsArena.getUsedBytes();
    {
        const gp::memory::ScopedScratchMark mark;
        gp::ScratchString scratchStr("Frame-temporary scratch string that exceeds 22 characters in length!");
        EXPECT_TRUE(scratchStr.isLong());
        EXPECT_GT(tlsArena.getUsedBytes(), baselineUsed);
        EXPECT_TRUE(scratchStr.startsWith("Frame-temporary"));
    }
    EXPECT_EQ(tlsArena.getUsedBytes(), baselineUsed);
}

TEST(StringTests, InlineStringThreeTierStorageAndCopyMove)
{
    gp::InlineString<128u> inlineStr;
    // Tier 1: 24-byte SSO (<= 22 chars)
    inlineStr.assign("Tier1_SSO_String");
    EXPECT_TRUE(inlineStr.isSso());
    EXPECT_EQ(inlineStr.getAllocator().getInlineUsedBytes(), 0u);

    // Tier 2: InlineAllocator embedded buffer (23..127 chars)
    inlineStr.assign("Tier2_InlineAllocator_String_That_Exceeds_22_Chars_Without_Touching_The_Heap");
    EXPECT_TRUE(inlineStr.isLong());
    EXPECT_TRUE(inlineStr.getAllocator().isInlinePointer(inlineStr.data()));
    EXPECT_GT(inlineStr.getAllocator().getInlineUsedBytes(), 0u);

    // Copy and Move of InlineString must not alias the source's embedded inline buffer!
    gp::InlineString<128u> copiedInline(inlineStr);
    EXPECT_EQ(copiedInline, inlineStr);
    EXPECT_NE(copiedInline.data(), inlineStr.data());
    EXPECT_TRUE(copiedInline.getAllocator().isInlinePointer(copiedInline.data()));

    gp::InlineString<128u> movedInline(std::move(copiedInline));
    EXPECT_EQ(movedInline, inlineStr);
    EXPECT_TRUE(movedInline.getAllocator().isInlinePointer(movedInline.data()));
    EXPECT_TRUE(copiedInline.isEmpty());

    // Tier 3: Spill over past 128 bytes to fallback HeapAllocator
    inlineStr.append(100u, 'X');
    EXPECT_TRUE(inlineStr.isLong());
    EXPECT_FALSE(inlineStr.getAllocator().isInlinePointer(inlineStr.data()));
}

TEST(StringTests, SelfAliasingAppendInsertAssignAndReplace)
{
    // 1. Self-append across SSO -> Long reallocation boundary
    gp::String str("012345678901234");   // 15 chars (SSO)
    EXPECT_TRUE(str.isSso());
    str.append(str.view());              // 30 chars (transitions to Long!)
    EXPECT_TRUE(str.isLong());
    EXPECT_EQ(str, "012345678901234012345678901234");

    // 2. Self-subview assign
    str.assign(str.subview(5u, 10u));
    EXPECT_EQ(str, "5678901234");

    // 3. Self-aliased insert (before, after, and straddling the insertion index)
    gp::String ins("ABCDEF");
    ins.insert(
        3u, ins.subview(1u, 4u)
    );   // insert "BCDE" at index 3 of "ABCDEF" -> "ABCBCFEDEF"? Wait: "ABC" + "BCDE" + "DEF"
    EXPECT_EQ(ins, "ABCBCDE" "DEF");

    // 4. Self-aliased replaceInPlace
    gp::String rep("foo_bar_foo");
    const gp::USize count = rep.replaceInPlace(rep.subview(0u, 3u), rep.subview(4u, 3u));
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(rep, "bar_bar_bar");
}

TEST(StringTests, MutatingOperationsAndInPlaceReplacement)
{
    gp::String s("World");
    s.prepend("Hello ");
    s.pushBack('!');
    EXPECT_EQ(s, "Hello World!");

    s.insert(5u, ",");
    EXPECT_EQ(s, "Hello, World!");

    s.erase(5u, 1u);
    EXPECT_EQ(s, "Hello World!");

    s.removePrefix(6u);
    s.removeSuffix(1u);
    EXPECT_EQ(s, "World");

    // Path separator normalization via replaceCharInPlace
    gp::String winPath("C:\\Game\\Assets\\Meshes\\Hero.gpasset");
    EXPECT_EQ(winPath.replaceCharInPlace('\\', '/'), 4u);
    EXPECT_EQ(winPath, "C:/Game/Assets/Meshes/Hero.gpasset");

    // Shrinking in-place replacement (zero allocations)
    gp::String shaderCode("float4 color = SAMPLE_TEX(albedo) + SAMPLE_TEX(emissive);");
    EXPECT_EQ(shaderCode.replaceInPlace("SAMPLE_TEX", "tex2D"), 2u);
    EXPECT_EQ(shaderCode, "float4 color = tex2D(albedo) + tex2D(emissive);");

    // Expanding replacement
    EXPECT_EQ(shaderCode.replaceInPlace("tex2D", "SampleTexture2D"), 2u);
    EXPECT_EQ(shaderCode, "float4 color = SampleTexture2D(albedo) + SampleTexture2D(emissive);");

    // Case-insensitive replacement
    gp::String tag("lod0_mesh_LOD0_skin");
    EXPECT_EQ(tag.replaceInPlace("lod0", "LOD1", SearchCase::IgnoreCase), 2u);
    EXPECT_EQ(tag, "LOD1_mesh_LOD1_skin");
}

TEST(StringTests, CaseConversionTrimmingPaddingAndReversal)
{
    gp::String s("  \t  Decima Engine  \r\n ");
    EXPECT_EQ(s.trimStart(), "Decima Engine  \r\n ");
    EXPECT_EQ(s.trimEnd(), "  \t  Decima Engine");
    EXPECT_EQ(s.trim(), "Decima Engine");

    s.trimInPlace();
    EXPECT_EQ(s, "Decima Engine");
    EXPECT_EQ(s.toLower(), "decima engine");
    EXPECT_EQ(s.toUpper(), "DECIMA ENGINE");

    gp::String num("42");
    EXPECT_EQ(num.padLeft(5u, '0'), "00042");
    EXPECT_EQ(num.padRight(5u, '_'), "42___");

    gp::String rev("ABCD");
    EXPECT_EQ(rev.reverse(), "DCBA");
    rev.reverseInPlace();
    EXPECT_EQ(rev, "DCBA");
}

TEST(StringTests, FormattingNumericConversionsAndJoining)
{
    const gp::String formatted = gp::String::format("Entity[{}] Pos=({:.1f}, {:.1f})_LOD{}", "Hero", 12.5, -3.0, 2);
    EXPECT_EQ(formatted, "Entity[Hero] Pos=(12.5, -3.0)_LOD2");

    gp::String builder("Header: ");
    builder.appendFormat("Count={}", 108);
    EXPECT_EQ(builder, "Header: Count=108");

    // std::formatter specialization for gp::String
    const std::string stdFormatted = std::format("Wrapped={}", builder);
    EXPECT_EQ(stdFormatted, "Wrapped=Header: Count=108");

    EXPECT_EQ(gp::String::fromInt(-123456789LL), "-123456789");
    EXPECT_EQ(gp::String::fromUInt(255ULL, 16), "ff");
    EXPECT_EQ(gp::String::fromFloat(3.14159, 2), "3.14");
    EXPECT_EQ(gp::String::fromBool(true), "true");
    EXPECT_EQ(gp::String::fromBool(false), "false");

    gp::Int32 parsedInt = 0;
    EXPECT_TRUE(gp::String::fromInt(-9001).tryParseInt32(parsedInt));
    EXPECT_EQ(parsedInt, -9001);

    const gp::String joined = gp::String::join({ "Engine", "Runtime", "Core", "String" }, "/");
    EXPECT_EQ(joined, "Engine/Runtime/Core/String");
}

TEST(StringTests, UnicodeTranscodingAndCodepointIteration)
{
    // UTF-8 string containing ASCII, Latin-1 ('é'), CJK ('日'), and Emoji ('🎮')
    const gp::String utf8Str("A\xC3\xA9\xE6\x97\xA5\xF0\x9F\x8E\xAE");
    EXPECT_TRUE(utf8Str.isValidUtf());
    EXPECT_EQ(utf8Str.size(), 10u);
    EXPECT_EQ(utf8Str.codepointCount(), 4u);

    const gp::U16String u16 = utf8Str.toUtf16();
    EXPECT_TRUE(u16.isValidUtf());
    EXPECT_EQ(u16.size(), 5u);   // '🎮' is a surrogate pair (2 char16_t units)
    EXPECT_EQ(u16.codepointCount(), 4u);

    const gp::U32String u32 = utf8Str.toUtf32();
    EXPECT_TRUE(u32.isValidUtf());
    EXPECT_EQ(u32.size(), 4u);
    EXPECT_EQ(u32[0], U'A');
    EXPECT_EQ(u32[1], 0x00E9u);
    EXPECT_EQ(u32[2], 0x65E5u);
    EXPECT_EQ(u32[3], 0x1F3AEu);

    const gp::WString wide = utf8Str.toWide();
    EXPECT_TRUE(wide.isValidUtf());
    EXPECT_EQ(wide.codepointCount(), 4u);

    EXPECT_EQ(gp::String::fromUtf16(u16.view()), utf8Str);
    EXPECT_EQ(gp::String::fromUtf32(u32.view()), utf8Str);
    EXPECT_EQ(gp::String::fromWide(wide.view()), utf8Str);
}

TEST(StringTests, OperatorsAndStreamOutput)
{
    const gp::String a("Graphical");
    const gp::String b("Playground");
    const gp::String combined = a + ' ' + b + " Engine";
    EXPECT_EQ(combined, "Graphical Playground Engine");

    std::ostringstream oss;
    oss << combined;
    EXPECT_EQ(oss.str(), "Graphical Playground Engine");
}

TEST(StringTests, ConstexprSsoEvaluation)
{
    constexpr auto evaluateConstexprSso = []() constexpr -> bool
    {
        gp::String s("ConstexprEngine");
        s.append("_v1");
        return s.isSso() && (s.size() == 18u) && s.startsWith("constexpr", SearchCase::IgnoreCase) &&
               (s == "ConstexprEngine_v1");
    };

    static_assert(evaluateConstexprSso());
}

TEST(StringTests, InPlaceReplaceExpansionOnInlineStringAndIndexOf)
{
    gp::InlineString<64> inlineStr("shader.hlsl + shader.hlsl");
    inlineStr.replaceInPlace("hlsl", "spv_bytecode", SearchCase::CaseSensitive);
    EXPECT_EQ(inlineStr, "shader.spv_bytecode + shader.spv_bytecode");
    EXPECT_EQ(inlineStr.indexOf("spv", SearchCase::CaseSensitive, SearchDir::FromStart), 7u);
    EXPECT_EQ(inlineStr.indexOf("spv", SearchCase::CaseSensitive, SearchDir::FromEnd), 29u);
    EXPECT_EQ(inlineStr.indexOf('.', SearchCase::CaseSensitive, SearchDir::FromEnd), 28u);
}

TEST(StringTests, StatefulPolymorphicAllocatorPropagationAcrossReturningMethods)
{
    alignas(16) gp::UInt8 arenaBuffer[4096]{};
    gp::memory::LinearAllocator arena(arenaBuffer, sizeof(arenaBuffer));
    gp::memory::PolymorphicAllocator polyAlloc(&arena);

    const gp::PolymorphicString base("  Graphical_Playground_Engine_Architecture_Masterclass  ", polyAlloc);
    const gp::PolymorphicString trimmed = base.trim();
    EXPECT_EQ(trimmed.getAllocator().getResource(), &arena);
    EXPECT_EQ(trimmed, "Graphical_Playground_Engine_Architecture_Masterclass");

    const gp::PolymorphicString sub = trimmed.substr(0u, 9u);
    EXPECT_EQ(sub.getAllocator().getResource(), &arena);
    EXPECT_EQ(sub, "Graphical");

    const gp::PolymorphicString replaced = trimmed.replace("_", " ");
    EXPECT_EQ(replaced.getAllocator().getResource(), &arena);
    EXPECT_EQ(replaced, "Graphical Playground Engine Architecture Masterclass");

    const gp::PolymorphicString padded = sub.padRight(12u, '!');
    EXPECT_EQ(padded.getAllocator().getResource(), &arena);
    EXPECT_EQ(padded, "Graphical!!!");

    const gp::PolymorphicString concat = sub + "_Engine";
    EXPECT_EQ(concat.getAllocator().getResource(), &arena);
    EXPECT_EQ(concat, "Graphical_Engine");
}

}   // namespace gp::tests
