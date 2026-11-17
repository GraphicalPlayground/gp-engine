// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/HashingTypes.hpp"
#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/Murmur3.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/SipHash.hpp"
#include "hashing/Wyhash.hpp"
#include "hashing/XxHash.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

TEST(HashingEnumsAndConceptsTest, EnumMetadataAndConceptValidation)
{
    static_assert(algorithmDigestBits(HashAlgorithm::Fnv1a32) == 32u);
    static_assert(algorithmDigestBits(HashAlgorithm::Crc32) == 32u);
    static_assert(algorithmDigestBits(HashAlgorithm::Crc32C) == 32u);
    static_assert(algorithmDigestBits(HashAlgorithm::Murmur3_32) == 32u);
    static_assert(algorithmDigestBits(HashAlgorithm::XxHash32) == 32u);
    static_assert(algorithmDigestBits(HashAlgorithm::Fnv1a64) == 64u);
    static_assert(algorithmDigestBits(HashAlgorithm::XxHash64) == 64u);
    static_assert(algorithmDigestBits(HashAlgorithm::Wyhash64) == 64u);
    static_assert(algorithmDigestBits(HashAlgorithm::SipHash24) == 64u);
    static_assert(algorithmDigestBits(HashAlgorithm::Murmur3_128) == 128u);
    static_assert(algorithmDigestBits(HashAlgorithm::Sha256) == 256u);
    static_assert(algorithmDigestBits(HashAlgorithm::Count) == 0u);

    static_assert(algorithmWidth(HashAlgorithm::Crc32) == HashWidth::Bits32);
    static_assert(algorithmWidth(HashAlgorithm::Fnv1a64) == HashWidth::Bits64);
    static_assert(algorithmWidth(HashAlgorithm::Wyhash64) == HashWidth::Bits64);
    static_assert(algorithmWidth(HashAlgorithm::SipHash24) == HashWidth::Bits64);
    static_assert(algorithmWidth(HashAlgorithm::Murmur3_128) == HashWidth::Bits128);
    static_assert(algorithmWidth(HashAlgorithm::Sha256) == HashWidth::Bits256);

    static_assert(isChecksumAlgorithm(HashAlgorithm::Crc32));
    static_assert(isChecksumAlgorithm(HashAlgorithm::Crc32C));
    static_assert(!isChecksumAlgorithm(HashAlgorithm::Fnv1a64));
    static_assert(!isChecksumAlgorithm(HashAlgorithm::XxHash64));
    static_assert(isCryptographicAlgorithm(HashAlgorithm::Sha256));
    static_assert(!isCryptographicAlgorithm(HashAlgorithm::Wyhash64));
    static_assert(isKeyedAlgorithm(HashAlgorithm::SipHash24));
    static_assert(!isKeyedAlgorithm(HashAlgorithm::Murmur3_128));

    EXPECT_STREQ(algorithmToString(HashAlgorithm::Fnv1a32), "Fnv1a32");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::Fnv1a64), "Fnv1a64");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::Crc32), "Crc32");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::Crc32C), "Crc32C");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::Murmur3_32), "Murmur3_32");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::Murmur3_128), "Murmur3_128");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::XxHash32), "XxHash32");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::XxHash64), "XxHash64");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::Wyhash64), "Wyhash64");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::SipHash24), "SipHash24");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::Sha256), "Sha256");
    EXPECT_STREQ(algorithmToString(HashAlgorithm::Count), "Unknown");

    static_assert(concepts::IsHashDigest<gp::UInt32>);
    static_assert(concepts::IsHashDigest<gp::UInt64>);
    static_assert(concepts::IsHashDigest<Hash128>);
    static_assert(concepts::IsHashDigest<Hash256>);
    static_assert(!concepts::IsHashDigest<gp::Float32>);

    static_assert(concepts::IsStreamingHasher32<Fnv1a32>);
    static_assert(concepts::IsStreamingHasher64<Fnv1a64>);
    static_assert(concepts::IsStreamingHasher32<Crc32>);
    static_assert(concepts::IsStreamingHasher32<Crc32C>);
    static_assert(concepts::IsStreamingHasher32<Murmur3_32>);
    static_assert(concepts::IsStreamingHasher128<Murmur3_128>);
    static_assert(concepts::IsStreamingHasher32<XxHash32>);
    static_assert(concepts::IsStreamingHasher64<XxHash64>);
    static_assert(concepts::IsStreamingHasher64<Wyhash64>);
    static_assert(concepts::IsStreamingHasher64<SipHash24>);
    static_assert(concepts::IsStreamingHasher256<Sha256>);
    static_assert(!concepts::IsStreamingHasher<gp::Int32>);

    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::Fnv1a32>, Fnv1a32>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::Fnv1a64>, Fnv1a64>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::Crc32>, Crc32>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::Crc32C>, Crc32C>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::Murmur3_32>, Murmur3_32>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::Murmur3_128>, Murmur3_128>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::XxHash32>, XxHash32>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::XxHash64>, XxHash64>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::Wyhash64>, Wyhash64>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::SipHash24>, SipHash24>);
    static_assert(gp::concepts::IsSameAs<Hasher<HashAlgorithm::Sha256>, Sha256>);
}

TEST(Hash128Test, LayoutOrderingFoldingAndHexRoundTrip)
{
    static_assert(sizeof(Hash128) == 16u);
    static_assert(alignof(Hash128) == 8u);

    constexpr Hash128 kZero{};
    static_assert(kZero.isZero());
    static_assert(!kZero.isValid());
    static_assert(!static_cast<bool>(kZero));

    constexpr Hash128 kSmall(0x1111222233334444ULL, 0x0123456789ABCDEFULL);
    constexpr Hash128 kLargerHigh(0x0000000000000001ULL, 0xFEDCBA9876543210ULL);
    static_assert(!kSmall.isZero());
    static_assert(kSmall.isValid());
    static_assert(static_cast<bool>(kSmall));
    static_assert(kSmall < kLargerHigh);
    static_assert(kSmall == Hash128(0x1111222233334444ULL, 0x0123456789ABCDEFULL));
    static_assert(kSmall.foldTo64() != 0ULL);
    static_assert(kSmall.foldTo32() != 0u);

    char lowerHex[Hash128::kHexBufferSize]{};
    ASSERT_TRUE(kSmall.toHexString(lowerHex, sizeof(lowerHex), false));
    EXPECT_STREQ(lowerHex, "0123456789abcdef1111222233334444");

    char upperHex[Hash128::kHexBufferSize]{};
    ASSERT_TRUE(kSmall.toHexString(upperHex, sizeof(upperHex), true));
    EXPECT_STREQ(upperHex, "0123456789ABCDEF1111222233334444");

    EXPECT_FALSE(kSmall.toHexString(nullptr, Hash128::kHexBufferSize));
    EXPECT_FALSE(kSmall.toHexString(lowerHex, 16u));

    Hash128 parsed{};
    EXPECT_TRUE(Hash128::tryParseHex(lowerHex, 32u, parsed));
    EXPECT_EQ(parsed, kSmall);

    Hash128 parsedUpper{};
    EXPECT_TRUE(Hash128::tryParseHex(upperHex, 32u, parsedUpper));
    EXPECT_EQ(parsedUpper, kSmall);

    EXPECT_FALSE(Hash128::tryParseHex(nullptr, 32u, parsed));
    EXPECT_FALSE(Hash128::tryParseHex(lowerHex, 31u, parsed));
    EXPECT_FALSE(Hash128::tryParseHex("0123456789abcdef111122223333444z", 32u, parsed));
    EXPECT_FALSE(Hash128::tryParseHex("0123456789abcdzf1111222233334444", 32u, parsed));
}

TEST(HashDigestTest, WidthConversionsAndZeroFlags)
{
    constexpr HashDigest kDefault{};
    static_assert(kDefault.isZero());
    static_assert(!kDefault.isValid());
    static_assert(kDefault.toUInt32() == 0u);
    static_assert(kDefault.toUInt64() == 0ULL);

    constexpr HashDigest k32(0xCAFEBABEu, HashAlgorithm::Crc32);
    static_assert(k32.isValid());
    static_assert(k32.width == HashWidth::Bits32);
    static_assert(k32.toUInt32() == 0xCAFEBABEu);
    static_assert(k32.toUInt64() == 0x00000000CAFEBABEULL);

    constexpr HashDigest k64(0x1122334455667788ULL, HashAlgorithm::XxHash64);
    static_assert(k64.width == HashWidth::Bits64);
    static_assert(k64.toUInt64() == 0x1122334455667788ULL);
    EXPECT_NE(k64.toUInt32(), 0u);

    constexpr HashDigest kSizeDigest(gp::USize{ 42u }, HashAlgorithm::Fnv1a64);
    static_assert(kSizeDigest.width == HashWidth::Bits64);
    static_assert(kSizeDigest.toUInt64() == 42ULL);

    constexpr HashDigest k128(Hash128(0x1234ULL, 0x5678ULL), HashAlgorithm::Murmur3_128);
    static_assert(k128.width == HashWidth::Bits128);
    EXPECT_EQ(k128.toHash128(), Hash128(0x1234ULL, 0x5678ULL));
    EXPECT_EQ(k128.toUInt64(), Hash128(0x1234ULL, 0x5678ULL).foldTo64());
    EXPECT_EQ(k128.toUInt32(), Hash128(0x1234ULL, 0x5678ULL).foldTo32());

    constexpr Hash256 kRaw256(0x0123456789ABCDEFULL, 0x1111222233334444ULL, 0x5555666677778888ULL, 0x9999AAAABBBBCCCCULL);
    constexpr HashDigest k256(kRaw256, HashAlgorithm::Sha256);
    static_assert(k256.width == HashWidth::Bits256);
    static_assert(k256.toHash256() == kRaw256);
    EXPECT_EQ(k256.toHash128(), kRaw256.foldTo128());
    EXPECT_EQ(k256.toUInt64(), kRaw256.foldTo64());
    EXPECT_EQ(k256.toUInt32(), kRaw256.foldTo32());
}

TEST(Hash256Test, LayoutOrderingFoldingAndHexRoundTrip)
{
    static_assert(sizeof(Hash256) == 32u);
    static_assert(alignof(Hash256) == 8u);

    constexpr Hash256 kZero{};
    static_assert(kZero.isZero());
    static_assert(!kZero.isValid());
    static_assert(!static_cast<bool>(kZero));

    constexpr Hash256 kValA(0x0123456789ABCDEFULL, 0x1111222233334444ULL, 0x5555666677778888ULL, 0x9999AAAABBBBCCCCULL);
    constexpr Hash256 kValB(0xFEDCBA9876543210ULL, 0x0ULL, 0x0ULL, 0x0ULL);
    static_assert(!kValA.isZero());
    static_assert(kValA.isValid());
    static_assert(static_cast<bool>(kValA));
    static_assert(kValA < kValB);
    static_assert(kValA.foldTo128().isValid());
    static_assert(kValA.foldTo64() != 0ULL);
    static_assert(kValA.foldTo32() != 0u);

    constexpr Hash256 kFromHalves(Hash128(0x2ULL, 0x1ULL), Hash128(0x4ULL, 0x3ULL));
    static_assert(kFromHalves == Hash256(0x1ULL, 0x2ULL, 0x3ULL, 0x4ULL));

    char lowerHex[Hash256::kHexBufferSize]{};
    ASSERT_TRUE(kValA.toHexString(lowerHex, sizeof(lowerHex), false));
    EXPECT_STREQ(lowerHex, "0123456789abcdef111122223333444455556666777788889999aaaabbbbcccc");

    char upperHex[Hash256::kHexBufferSize]{};
    ASSERT_TRUE(kValA.toHexString(upperHex, sizeof(upperHex), true));
    EXPECT_STREQ(upperHex, "0123456789ABCDEF111122223333444455556666777788889999AAAABBBBCCCC");

    EXPECT_FALSE(kValA.toHexString(nullptr, Hash256::kHexBufferSize));
    EXPECT_FALSE(kValA.toHexString(lowerHex, 32u));

    Hash256 parsed{};
    EXPECT_TRUE(Hash256::tryParseHex(lowerHex, 64u, parsed));
    EXPECT_EQ(parsed, kValA);
    EXPECT_TRUE(Hash256::tryParseHex(upperHex, 64u, parsed));
    EXPECT_EQ(parsed, kValA);
    EXPECT_FALSE(Hash256::tryParseHex(nullptr, 64u, parsed));
    EXPECT_FALSE(Hash256::tryParseHex(lowerHex, 63u, parsed));
    EXPECT_FALSE(
        Hash256::tryParseHex("0123456789abcdef111122223333444455556666777788889999aaaabbbbcccz", 64u, parsed)
    );
}

}   // namespace gp::hashing::tests
