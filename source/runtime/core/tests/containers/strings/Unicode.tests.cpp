// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/Unicode.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::tests
{

TEST(UnicodeTests, CodepointValidationAndEncodedLengths)
{
    static_assert(unicode::isValidCodepoint(U'A'));
    static_assert(unicode::isValidCodepoint(0x0010FFFFu));
    static_assert(!unicode::isValidCodepoint(0x00110000u));
    static_assert(!unicode::isValidCodepoint(0x0000D800u));
    static_assert(!unicode::isValidCodepoint(0x0000DFFFu));

    static_assert(unicode::utf8EncodedLength(U'A') == 1u);
    static_assert(unicode::utf8EncodedLength(0x00E9u) == 2u);     // 'é'
    static_assert(unicode::utf8EncodedLength(0x65E5u) == 3u);     // '日'
    static_assert(unicode::utf8EncodedLength(0x1F3AEu) == 4u);    // '🎮'
    static_assert(unicode::utf8EncodedLength(0x00D800u) == 3u);   // invalid -> U+FFFD (3 bytes)

    static_assert(unicode::utf16EncodedLength(U'A') == 1u);
    static_assert(unicode::utf16EncodedLength(0x65E5u) == 1u);
    static_assert(unicode::utf16EncodedLength(0x1F3AEu) == 2u);
}

TEST(UnicodeTests, DecodeAndEncodeUtf8ValidSequences)
{
    // "Aé日🎮" -> U+0041 (1B), U+00E9 (2B), U+65E5 (3B), U+1F3AE (4B) = 10 UTF-8 bytes
    const char utf8Sample[] = "A\xC3\xA9\xE6\x97\xA5\xF0\x9F\x8E\xAE";
    const gp::StringView view(utf8Sample, 10u);

    EXPECT_TRUE(unicode::isValidUtf8(view));
    EXPECT_EQ(unicode::countCodepoints(view), 4u);

    char32_t decoded[4]{};
    gp::USize idx = 0u;
    for (const char32_t cp: unicode::CodepointView<char>(view))
    {
        ASSERT_LT(idx, 4u);
        decoded[idx++] = cp;
    }

    EXPECT_EQ(idx, 4u);
    EXPECT_EQ(decoded[0], U'A');
    EXPECT_EQ(decoded[1], 0x00E9u);
    EXPECT_EQ(decoded[2], 0x65E5u);
    EXPECT_EQ(decoded[3], 0x1F3AEu);

    // Re-encode back to UTF-8 and verify exact round-trip
    char reencoded[16]{};
    gp::USize offset = 0u;
    for (gp::USize i = 0u; i < 4u; ++i)
    {
        offset += unicode::encodeUtf8(decoded[i], reencoded + offset);
    }
    EXPECT_EQ(offset, 10u);
    EXPECT_EQ(gp::StringView(reencoded, offset), view);
}

TEST(UnicodeTests, IllFormedUtf8ReplacedWithFFFD)
{
    // 1. Overlong 2-byte encoding of '/' (0xC0 0xAF) -> 2 replacement chars
    const char overlong2[] = "\xC0\xAF";
    const gp::StringView vOverlong2(overlong2, 2u);
    EXPECT_FALSE(unicode::isValidUtf8(vOverlong2));
    EXPECT_EQ(unicode::countCodepoints(vOverlong2), 2u);
    for (const char32_t cp: unicode::CodepointView<char>(vOverlong2))
    {
        EXPECT_EQ(cp, unicode::kReplacementChar);
    }

    // 2. UTF-8 encoded surrogate U+D800 (0xED 0xA0 0x80) -> invalid
    const char surrogateUtf8[] = "\xED\xA0\x80";
    const gp::StringView vSurrogate(surrogateUtf8, 3u);
    EXPECT_FALSE(unicode::isValidUtf8(vSurrogate));

    // 3. Truncated 3-byte sequence followed by ASCII 'Z' (\xE6\x97Z)
    const char truncated[] = "\xE6\x97Z";
    const gp::StringView vTruncated(truncated, 3u);
    EXPECT_FALSE(unicode::isValidUtf8(vTruncated));

    char32_t cps[2]{};
    gp::USize i = 0u;
    for (const char32_t cp: unicode::CodepointView<char>(vTruncated))
    {
        ASSERT_LT(i, 2u);
        cps[i++] = cp;
    }
    EXPECT_EQ(i, 2u);
    EXPECT_EQ(cps[0], unicode::kReplacementChar);
    EXPECT_EQ(cps[1], U'Z');
}

TEST(UnicodeTests, Utf16SurrogateValidationAndReplacement)
{
    const char16_t validU16[] = { u'A', 0x65E5u, 0xD83Cu, 0xDFAE0u & 0xFFFFu };
    // Construct proper surrogate pair for U+1F3AE (0xD83C, 0xDFAE)
    const char16_t properU16[] = { u'A', 0x65E5u, 0xD83Cu, 0xDFAEu };
    const gp::U16StringView vValid(properU16, 4u);
    (void)validU16;

    EXPECT_TRUE(unicode::isValidUtf16(vValid));
    EXPECT_EQ(unicode::countCodepoints(vValid), 3u);

    // Unpaired high surrogate followed by BMP char
    const char16_t unpairedHigh[] = { 0xD83Cu, u'X' };
    const gp::U16StringView vUnpaired(unpairedHigh, 2u);
    EXPECT_FALSE(unicode::isValidUtf16(vUnpaired));

    char32_t out[2]{};
    gp::USize idx = 0u;
    for (const char32_t cp: unicode::CodepointView<char16_t>(vUnpaired))
    {
        out[idx++] = cp;
    }
    EXPECT_EQ(idx, 2u);
    EXPECT_EQ(out[0], unicode::kReplacementChar);
    EXPECT_EQ(out[1], U'X');
}

TEST(UnicodeTests, CrossWidthContainerTranscoding)
{
    const gp::StringView utf8Src("A\xC3\xA9\xE6\x97\xA5\xF0\x9F\x8E\xAE", 10u);

    gp::U16String u16Str;
    unicode::convert(utf8Src, u16Str);
    EXPECT_EQ(u16Str.size(), 5u);
    EXPECT_TRUE(u16Str.isValidUtf());

    gp::U32String u32Str;
    unicode::convert(u16Str.view(), u32Str);
    EXPECT_EQ(u32Str.size(), 4u);
    EXPECT_EQ(u32Str[0], U'A');
    EXPECT_EQ(u32Str[1], 0x00E9u);
    EXPECT_EQ(u32Str[2], 0x65E5u);
    EXPECT_EQ(u32Str[3], 0x1F3AEu);

    gp::FixedString<32> fixedRoundTrip;
    unicode::convert(u32Str.view(), fixedRoundTrip);
    EXPECT_EQ(fixedRoundTrip.view(), utf8Src);
}

}   // namespace gp::tests
