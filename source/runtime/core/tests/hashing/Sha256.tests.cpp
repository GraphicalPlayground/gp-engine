// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/Sha256.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashingConcepts.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

static_assert(concepts::IsStreamingHasher256<Sha256>);
static_assert(Sha256::kAlgorithm == HashAlgorithm::Sha256);
static_assert(Sha256::kDigestBits == 256u);

// NIST FIPS 180-4 reference test vectors evaluated at compile time.
static_assert(
    Sha256::hash("") ==
    Hash256(0xE3B0C44298FC1C14ULL, 0x9AFBF4C8996FB924ULL, 0x27AE41E4649B934CULL, 0xA495991B7852B855ULL)
);

static_assert(
    Sha256::hash("abc") ==
    Hash256(0xBA7816BF8F01CFEAULL, 0x414140DE5DAE2223ULL, 0xB00361A396177A9CULL, 0xB410FF61F20015ADULL)
);

static_assert(
    Sha256::hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
    Hash256(0x248D6A61D20638B8ULL, 0xE5C026930C3E6039ULL, 0xA33CE45964FF2167ULL, 0xF6ECEDD419DB06C1ULL)
);

TEST(Sha256Test, NistVectorsHexFormattingAndStreamingBoundaries)
{
    const Hash256 abcDigest = Sha256::hash(gp::StringView("abc"));
    char hexBuf[Hash256::kHexBufferSize]{};
    ASSERT_TRUE(abcDigest.toHexString(hexBuf, sizeof(hexBuf), false));
    EXPECT_STREQ(hexBuf, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // Test multi-chunk streaming across the 56-byte padding boundary and 64-byte compression block boundary.
    gp::UInt8 payload[200]{};
    for (gp::USize i = 0u; i < sizeof(payload); ++i)
    {
        payload[i] = static_cast<gp::UInt8>('a' + (i % 26u));
    }

    for (gp::USize len = 0u; len <= sizeof(payload); len += 7u)
    {
        const Hash256 expected = Sha256::hash(payload, len);
        Sha256 streamed;
        for (gp::USize offset = 0u; offset < len; offset += 13u)
        {
            const gp::USize chunk = (offset + 13u <= len) ? 13u : (len - offset);
            streamed.update(payload + offset, chunk);
        }
        EXPECT_EQ(streamed.finalize(), expected) << "SHA-256 streaming mismatch at len=" << len;
    }

    // Domain-seeded SHA-256 differs from unseeded standard SHA-256.
    const Hash256 domainSeed(1ULL, 2ULL, 3ULL, 4ULL);
    EXPECT_NE(Sha256::hash(gp::StringView("abc"), domainSeed), abcDigest);

    // Case-insensitive helper.
    EXPECT_EQ(Sha256::hashIgnoreCase("ABC"), abcDigest);
}

}   // namespace gp::hashing::tests
