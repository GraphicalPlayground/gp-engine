// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/Crc32.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

TEST(Crc32Test, ConstexprIeeeAndCastagnoliReferenceVectors)
{
    static_assert(Crc32::hash(nullptr, 0u) == 0x00000000u);
    static_assert(Crc32::hash("") == 0x00000000u);
    static_assert(Crc32::hash("a") == 0xE8B7BE43u);
    static_assert(Crc32::hash("abc") == 0x352441C2u);
    static_assert(Crc32::hash("123456789") == 0xCBF43926u);
    static_assert(Crc32::hash("The quick brown fox jumps over the lazy dog") == 0x414FA339u);
    static_assert(Crc32::hash("GraphicalPlayground") == 0xBF54F678u);

    static_assert(Crc32C::hash(nullptr, 0u) == 0x00000000u);
    static_assert(Crc32C::hash("") == 0x00000000u);
    static_assert(Crc32C::hash("a") == 0xC1D04330u);
    static_assert(Crc32C::hash("abc") == 0x364B3FB7u);
    static_assert(Crc32C::hash("123456789") == 0xE3069283u);
    static_assert(Crc32C::hash("The quick brown fox jumps over the lazy dog") == 0x22620404u);
    static_assert(Crc32C::hash("GraphicalPlayground") == 0x169E955Cu);

    EXPECT_EQ(CRC32::hash("123456789", 9u), 0xCBF43926u);
    EXPECT_EQ(CRC32C::hash("123456789", 9u), 0xE3069283u);
}

TEST(Crc32Test, StreamingChainedSeedsAndSplitInvariance)
{
    constexpr const char kPayload[] = "The quick brown fox jumps over the lazy dog";
    constexpr gp::USize kLen = sizeof(kPayload) - 1u;

    const gp::UInt32 fullIeee = Crc32::hash(kPayload, kLen);
    const gp::UInt32 fullCastagnoli = Crc32C::hash(kPayload, kLen);
    EXPECT_EQ(fullIeee, 0x414FA339u);
    EXPECT_EQ(fullCastagnoli, 0x22620404u);

    // Every split point from 0..kLen must yield identical digests across streaming and chained seeds.
    for (gp::USize split = 0u; split <= kLen; ++split)
    {
        Crc32 streamIeee;
        streamIeee.update(kPayload, split);
        const gp::UInt32 partialIeee = streamIeee.finalize();
        streamIeee.update(kPayload + split, kLen - split);
        EXPECT_EQ(streamIeee.finalize(), fullIeee);

        const gp::UInt32 chainedIeee = Crc32::hash(kPayload + split, kLen - split, partialIeee);
        EXPECT_EQ(chainedIeee, fullIeee);

        Crc32C streamC;
        streamC.update(kPayload, split);
        const gp::UInt32 partialC = streamC.finalize();
        streamC.update(kPayload + split, kLen - split);
        EXPECT_EQ(streamC.finalize(), fullCastagnoli);

        const gp::UInt32 chainedC = Crc32C::hash(kPayload + split, kLen - split, partialC);
        EXPECT_EQ(chainedC, fullCastagnoli);
    }
}

TEST(Crc32Test, CaseFoldingByteArrayAndReset)
{
    const gp::StringView upperView("123456789_ASSET");
    const gp::String lowerStr("123456789_asset");

    EXPECT_EQ(Crc32::hashIgnoreCase(upperView), Crc32::hash(lowerStr));
    EXPECT_EQ(Crc32C::hashIgnoreCase(upperView), Crc32C::hash(lowerStr));

    Crc32 hasher;
    hasher.update("Temporary", 9u);
    EXPECT_NE(hasher.finalize(), 0u);
    hasher.reset();
    EXPECT_EQ(hasher.finalize(), 0u);

    const gp::Byte stdBytes[9] = { gp::Byte{ '1' }, gp::Byte{ '2' }, gp::Byte{ '3' }, gp::Byte{ '4' }, gp::Byte{ '5' },
                                   gp::Byte{ '6' }, gp::Byte{ '7' }, gp::Byte{ '8' }, gp::Byte{ '9' } };
    EXPECT_EQ(Crc32::hash(stdBytes, 9u), 0xCBF43926u);
    EXPECT_EQ(Crc32C::hash(static_cast<const void*>(stdBytes), 9u), 0xE3069283u);
}

}   // namespace gp::hashing::tests
