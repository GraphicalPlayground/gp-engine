// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/SipHash.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashingConcepts.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

static_assert(concepts::IsStreamingHasher64<SipHash24>);
static_assert(SipHash24::kAlgorithm == HashAlgorithm::SipHash24);
static_assert(SipHash24::kDigestBits == 64u);

constexpr bool testOfficialSipHashVectors() noexcept
{
    // Official SipHash-2-4 test key: 00 01 02 ... 0f (little-endian words).
    constexpr gp::UInt64 kK0 = 0x0706050403020100ULL;
    constexpr gp::UInt64 kK1 = 0x0F0E0D0C0B0A0908ULL;

    constexpr gp::UInt8 kMsg15[15] = { 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
                                       0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu };

    const gp::UInt64 emptyDigest = SipHash24::hashKeyed(kK0, kK1, nullptr, 0u);
    const gp::UInt64 msg15Digest = SipHash24::hashKeyed(kK0, kK1, kMsg15, 15u);

    SipHash24 streamed(Hash128(kK0, kK1));
    streamed.update(kMsg15, 5u);
    streamed.update(kMsg15 + 5u, 4u);
    streamed.update(kMsg15 + 9u, 6u);

    return emptyDigest == 0x726FDB47DD0E0E31ULL && msg15Digest == 0xA129CA6149BE45E5ULL &&
           streamed.finalize() == msg15Digest;
}

static_assert(testOfficialSipHashVectors(), "SipHash24 official specification vectors failed.");

TEST(SipHashTest, KeyedVectorsStreamingAndReset)
{
    const Hash128 keyA(0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL);
    const Hash128 keyB(0x1111111111111111ULL, 0x2222222222222222ULL);

    const gp::StringView packet("SessionToken=42");
    const gp::UInt64 digestA = SipHash24::hashKeyed(keyA, packet);
    const gp::UInt64 digestB = SipHash24::hashKeyed(keyB, packet);
    EXPECT_NE(digestA, digestB);

    SipHash24 hasher(keyA);
    for (gp::USize i = 0u; i < packet.size(); ++i)
    {
        hasher.update(packet.data() + i, 1u);
    }
    EXPECT_EQ(hasher.finalize(), digestA);

    hasher.resetKey(keyB);
    hasher.update(packet);
    EXPECT_EQ(hasher.finalize(), digestB);

    hasher.reset(0x55ULL);
    hasher.update(packet);
    EXPECT_EQ(hasher.finalize(), SipHash24::hash(packet, 0x55ULL));
    EXPECT_EQ(SipHash24::hashIgnoreCase("Packet"), SipHash24::hash("packet"));
}

}   // namespace gp::hashing::tests
