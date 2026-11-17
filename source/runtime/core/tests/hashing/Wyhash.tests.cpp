// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/Wyhash.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashingConcepts.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

static_assert(concepts::IsStreamingHasher64<Wyhash64>);
static_assert(Wyhash64::kAlgorithm == HashAlgorithm::Wyhash64);
static_assert(Wyhash64::kDigestBits == 64u);

constexpr bool testConstexprWyhashStreaming() noexcept
{
    constexpr const char kMsg[] = "The quick brown fox jumps over the lazy dog and runs past the 48 byte boundary!!";
    constexpr gp::USize kLen = sizeof(kMsg) - 1u;

    const gp::UInt64 oneShot = Wyhash64::hash(kMsg, kLen);

    Wyhash64 chunked;
    chunked.update(kMsg, 11u);
    chunked.update(kMsg + 11u, 37u);
    chunked.update(kMsg + 48u, kLen - 48u);

    Wyhash64 byteByByte;
    for (gp::USize i = 0u; i < kLen; ++i)
    {
        byteByByte.updateByte(static_cast<gp::UInt8>(kMsg[i]));
    }

    return oneShot != 0ULL && oneShot == chunked.finalize() && oneShot == byteByByte.finalize();
}

static_assert(testConstexprWyhashStreaming(), "Wyhash64 constexpr streaming failed.");

TEST(WyhashTest, StreamingMatchesOneShotAcrossAllLengthRegimes)
{
    // Covers 0, 1..3, 4..8, 9..16, 17..48, 49..96, and >96 byte boundaries.
    gp::UInt8 payload[140]{};
    for (gp::USize i = 0u; i < sizeof(payload); ++i)
    {
        payload[i] = static_cast<gp::UInt8>((i * 37u + 13u) & 0xFFu);
    }

    for (gp::USize len = 0u; len <= sizeof(payload); ++len)
    {
        const gp::UInt64 expected = Wyhash64::hash(payload, len);

        Wyhash64 oneByteAtATime;
        for (gp::USize i = 0u; i < len; ++i)
        {
            oneByteAtATime.update(payload + i, 1u);
        }
        EXPECT_EQ(oneByteAtATime.finalize(), expected) << "Mismatch at 1-byte streaming len=" << len;

        Wyhash64 sevenByteChunks;
        for (gp::USize offset = 0u; offset < len; offset += 7u)
        {
            const gp::USize chunk = (offset + 7u <= len) ? 7u : (len - offset);
            sevenByteChunks.update(payload + offset, chunk);
        }
        EXPECT_EQ(sevenByteChunks.finalize(), expected) << "Mismatch at 7-byte streaming len=" << len;
    }
}

TEST(WyhashTest, SeedsCaseFoldingAndBufferOverloads)
{
    const gp::UInt64 unseeded = Wyhash64::hash("DecimaAsset");
    const gp::UInt64 seeded = Wyhash64::hash(gp::StringView("DecimaAsset"), 0x123456789ABCDEF0ULL);
    EXPECT_NE(unseeded, seeded);

    EXPECT_EQ(Wyhash64::hashIgnoreCase("Albedo_Map"), Wyhash64::hash("albedo_map"));
    EXPECT_EQ(Wyhash64::hashCStringIgnoreCase("ALBEDO_MAP"), Wyhash64::hash("albedo_map"));

    const gp::Byte bytes[5] = { gp::Byte{ 'h' }, gp::Byte{ 'e' }, gp::Byte{ 'l' }, gp::Byte{ 'l' }, gp::Byte{ 'o' } };
    EXPECT_EQ(Wyhash64::hash(bytes, 5u), Wyhash64::hash(static_cast<const void*>(bytes), 5u));
    EXPECT_EQ(Wyhash64::hash(bytes, 5u), Wyhash64::hash("hello"));

    Wyhash64 reusable(99ULL);
    reusable.update("junk", 4u);
    reusable.reset();
    reusable.update("hello", 5u);
    EXPECT_EQ(reusable.finalize(), Wyhash64::hash("hello"));
}

}   // namespace gp::hashing::tests
