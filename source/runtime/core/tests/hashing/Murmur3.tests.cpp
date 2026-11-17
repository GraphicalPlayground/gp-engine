// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/Murmur3.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

TEST(Murmur3Test, ConstexprReferenceVectors32And128)
{
    static_assert(Murmur3_32::hash(nullptr, 0u) == 0x00000000u);
    static_assert(Murmur3_32::hash("") == 0x00000000u);
    static_assert(Murmur3_32::hash("a") == 0x3C2569B2u);
    static_assert(Murmur3_32::hash("abc") == 0xB3DD93FAu);
    static_assert(Murmur3_32::hash("123456789") == 0xB4FEF382u);
    static_assert(Murmur3_32::hash("The quick brown fox jumps over the lazy dog") == 0x2E4FF723u);
    static_assert(Murmur3_32::hash("GraphicalPlayground") == 0x3E32B90Bu);

    static_assert(Murmur3_128::hash(nullptr, 0u) == Hash128(0ULL, 0ULL));
    static_assert(Murmur3_128::hash("") == Hash128(0ULL, 0ULL));
    static_assert(Murmur3_128::hash("a") == Hash128(0x85555565F6597889ULL, 0xE6B53A48510E895AULL));
    static_assert(Murmur3_128::hash("abc") == Hash128(0xB4963F3F3FAD7867ULL, 0x3BA2744126CA2D52ULL));
    static_assert(Murmur3_128::hash("123456789") == Hash128(0x3C84645EDB66CCA4ULL, 0x99F8FAC73A1EA105ULL));
    static_assert(
        Murmur3_128::hash("The quick brown fox jumps over the lazy dog") ==
        Hash128(0xE34BBC7BBC071B6CULL, 0x7A433CA9C49A9347ULL)
    );
    static_assert(Murmur3_128::hash("GraphicalPlayground") == Hash128(0xB387DC0D5660C3B1ULL, 0x5AB54BADFF4C90DEULL));

    EXPECT_EQ(Murmur3_32::hash("123456789", 9u), 0xB4FEF382u);
    EXPECT_EQ(Murmur3_128::hash("123456789", 9u), Hash128(0x3C84645EDB66CCA4ULL, 0x99F8FAC73A1EA105ULL));
}

TEST(Murmur3Test, StreamingChunkInvarianceAcrossAllBlockBoundaries)
{
    constexpr const char kSample[] = "The quick brown fox jumps over the lazy dog";
    constexpr gp::USize kLen = sizeof(kSample) - 1u;

    const gp::UInt32 expected32 = Murmur3_32::hash(kSample, kLen);
    const Hash128 expected128 = Murmur3_128::hash(kSample, kLen);

    // Every single split point exercises 4-byte (Murmur3_32) and 16-byte (Murmur3_128) tail buffering.
    for (gp::USize split = 0u; split <= kLen; ++split)
    {
        Murmur3_32 h32;
        h32.update(kSample, split);
        EXPECT_EQ(h32.finalize(), Murmur3_32::hash(kSample, split));
        h32.update(kSample + split, kLen - split);
        EXPECT_EQ(h32.finalize(), expected32);

        Murmur3_128 h128;
        h128.update(kSample, split);
        EXPECT_EQ(h128.finalize(), Murmur3_128::hash(kSample, split));
        h128.update(kSample + split, kLen - split);
        EXPECT_EQ(h128.finalize(), expected128);
    }

    // 1-byte streaming also matches one-shot.
    Murmur3_128 byteByByte(0x42ULL);
    for (gp::USize i = 0u; i < kLen; ++i)
    {
        byteByByte.updateByte(static_cast<gp::UInt8>(kSample[i]));
    }
    EXPECT_EQ(byteByByte.finalize(), Murmur3_128::hash(kSample, kLen, Hash128(0x42ULL, 0x42ULL)));

    byteByByte.reset(0x77ULL);
    byteByByte.update(gp::StringView("abc"));
    EXPECT_EQ(byteByByte.finalize(), Murmur3_128::hash(gp::StringView("abc"), Hash128(0x77ULL, 0x77ULL)));
}

TEST(Murmur3Test, SeededAndCaseInsensitiveVariants)
{
    const gp::UInt32 seeded32 = Murmur3_32::hash("abc", 3u, 0x9E3779B1u);
    EXPECT_NE(seeded32, Murmur3_32::hash("abc", 3u, 0u));

    EXPECT_EQ(Murmur3_32::hashIgnoreCase("SHADER_PASS"), Murmur3_32::hash("shader_pass"));
    EXPECT_EQ(Murmur3_128::hashIgnoreCase("SHADER_PASS"), Murmur3_128::hash("shader_pass"));
}

}   // namespace gp::hashing::tests
