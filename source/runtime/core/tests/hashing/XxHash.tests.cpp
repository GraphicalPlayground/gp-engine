// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/XxHash.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

TEST(XxHashTest, ConstexprReferenceVectors32And64)
{
    static_assert(XxHash32::hash(nullptr, 0u) == 0x02CC5D05u);
    static_assert(XxHash32::hash("") == 0x02CC5D05u);
    static_assert(XxHash32::hash(nullptr, 0u, 0x9E3779B1u) == 0x36B78AE7u);
    static_assert(XxHash32::hash("a") == 0x550D7456u);
    static_assert(XxHash32::hash("abc") == 0x32D153FFu);
    static_assert(XxHash32::hash("123456789") == 0x937BAD67u);
    static_assert(XxHash32::hash("The quick brown fox jumps over the lazy dog") == 0xE85EA4DEu);
    static_assert(XxHash32::hash("GraphicalPlayground") == 0x59484481u);

    static_assert(XxHash64::hash(nullptr, 0u) == 0xEF46DB3751D8E999ULL);
    static_assert(XxHash64::hash("") == 0xEF46DB3751D8E999ULL);
    static_assert(XxHash64::hash(nullptr, 0u, 0x9E3779B1ULL) == 0xAC75FDA2929B17EFULL);
    static_assert(XxHash64::hash("a") == 0xD24EC4F1A98C6E5BULL);
    static_assert(XxHash64::hash("abc") == 0x44BC2CF5AD770999ULL);
    static_assert(XxHash64::hash("123456789") == 0x8CB841DB40E6AE83ULL);
    static_assert(XxHash64::hash("The quick brown fox jumps over the lazy dog") == 0x0B242D361FDA71BCULL);
    static_assert(XxHash64::hash("GraphicalPlayground") == 0x4C662AF23097B492ULL);

    EXPECT_EQ(XxHash32::hash("The quick brown fox jumps over the lazy dog", 43u), 0xE85EA4DEu);
    EXPECT_EQ(XxHash64::hash("The quick brown fox jumps over the lazy dog", 43u), 0x0B242D361FDA71BCULL);
}

TEST(XxHashTest, StreamingStripeAndBoundaryInvariance)
{
    gp::UInt8 pattern[137]{};
    for (gp::USize i = 0u; i < sizeof(pattern); ++i)
    {
        pattern[i] = static_cast<gp::UInt8>((i * 31u + 7u) & 0xFFu);
    }

    const gp::UInt32 full32 = XxHash32::hash(pattern, sizeof(pattern), 0x12345678u);
    const gp::UInt64 full64 = XxHash64::hash(pattern, sizeof(pattern), 0x123456789ABCDEF0ULL);

    // Exercise every split boundary from 0..137 across 16-byte (XXH32) and 32-byte (XXH64) stripes.
    for (gp::USize split = 0u; split <= sizeof(pattern); ++split)
    {
        XxHash32 h32(0x12345678u);
        h32.update(pattern, split);
        EXPECT_EQ(h32.finalize(), XxHash32::hash(pattern, split, 0x12345678u));
        h32.update(pattern + split, sizeof(pattern) - split);
        EXPECT_EQ(h32.finalize(), full32);

        XxHash64 h64(0x123456789ABCDEF0ULL);
        h64.update(pattern, split);
        EXPECT_EQ(h64.finalize(), XxHash64::hash(pattern, split, 0x123456789ABCDEF0ULL));
        h64.update(pattern + split, sizeof(pattern) - split);
        EXPECT_EQ(h64.finalize(), full64);
    }

    // Small fixed chunking (3-byte increments) matches one-shot.
    XxHash64 chunked(0x123456789ABCDEF0ULL);
    for (gp::USize offset = 0u; offset < sizeof(pattern); offset += 3u)
    {
        const gp::USize rem = sizeof(pattern) - offset;
        chunked.update(pattern + offset, (rem < 3u) ? rem : 3u);
    }
    EXPECT_EQ(chunked.finalize(), full64);

    chunked.reset();
    EXPECT_EQ(chunked.finalize(), 0xEF46DB3751D8E999ULL);
}

TEST(XxHashTest, CaseInsensitiveAndVoidOverloads)
{
    const gp::StringView upper("The Quick Brown Fox Jumps Over The Lazy Dog");
    const gp::StringView lower("the quick brown fox jumps over the lazy dog");

    EXPECT_EQ(XxHash32::hashIgnoreCase(upper), XxHash32::hash(lower));
    EXPECT_EQ(XxHash64::hashIgnoreCase(upper), XxHash64::hash(lower));

    const void* const rawPtr = upper.data();
    EXPECT_EQ(XxHash64::hash(rawPtr, upper.size()), XxHash64::hash(upper));
}

}   // namespace gp::hashing::tests
