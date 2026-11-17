// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/RuntimeHash.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Hashing.hpp"               // IWYU pragma: keep
#include "hashing/HashMix.hpp"               // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

constexpr bool testConstexprDynamicHasherReset() noexcept
{
    DynamicHasher hasher(HashAlgorithm::Fnv1a32);
    hasher.update("ignored", 7u);
    hasher.reset(HashAlgorithm::Crc32);
    hasher.update("123456789", 9u);
    const gp::UInt32 crc = hasher.finalize32();
    hasher.reset(HashAlgorithm::XxHash64);
    hasher.update("abc", 3u);
    const gp::UInt64 xxh = hasher.finalize64();
    return crc == 0xCBF43926u && xxh == 0x44BC2CF5AD770999ULL;
}

static_assert(testConstexprDynamicHasherReset(), "DynamicHasher constexpr reset failed.");

TEST(RuntimeHashTest, DynamicHasherMatchesEveryStaticAlgorithm)
{
    constexpr const char kInput[] = "The quick brown fox jumps over the lazy dog";
    constexpr gp::USize kLen = sizeof(kInput) - 1u;

    static_assert(computeHash32(HashAlgorithm::Crc32, gp::StringView("123456789")) == 0xCBF43926u);
    static_assert(computeHash32(HashAlgorithm::Crc32C, gp::StringView("123456789")) == 0xE3069283u);
    static_assert(computeHash64(HashAlgorithm::Fnv1a64, gp::StringView("abc")) == 0xE71FA2190541574BULL);
    static_assert(
        computeHash128(HashAlgorithm::Murmur3_128, gp::StringView("abc")) ==
        Hash128(0xB4963F3F3FAD7867ULL, 0x3BA2744126CA2D52ULL)
    );
    static_assert(computeHash256(HashAlgorithm::Sha256, gp::StringView("abc")) == Sha256::hash(gp::StringView("abc")));

    EXPECT_EQ(computeHash32(HashAlgorithm::Fnv1a32, kInput, kLen), Fnv1a32::hash(kInput, kLen));
    EXPECT_EQ(computeHash64(HashAlgorithm::Fnv1a64, kInput, kLen), Fnv1a64::hash(kInput, kLen));
    EXPECT_EQ(computeHash32(HashAlgorithm::Crc32, kInput, kLen), Crc32::hash(kInput, kLen));
    EXPECT_EQ(computeHash32(HashAlgorithm::Crc32C, kInput, kLen), Crc32C::hash(kInput, kLen));
    EXPECT_EQ(computeHash32(HashAlgorithm::Murmur3_32, kInput, kLen), Murmur3_32::hash(kInput, kLen));
    EXPECT_EQ(computeHash128(HashAlgorithm::Murmur3_128, kInput, kLen), Murmur3_128::hash(kInput, kLen));
    EXPECT_EQ(computeHash32(HashAlgorithm::XxHash32, kInput, kLen), XxHash32::hash(kInput, kLen));
    EXPECT_EQ(computeHash64(HashAlgorithm::XxHash64, kInput, kLen), XxHash64::hash(kInput, kLen));
    EXPECT_EQ(computeHash64(HashAlgorithm::Wyhash64, kInput, kLen), Wyhash64::hash(kInput, kLen));
    EXPECT_EQ(computeHash64(HashAlgorithm::SipHash24, kInput, kLen), SipHash24::hash(kInput, kLen));
    EXPECT_EQ(computeHash256(HashAlgorithm::Sha256, kInput, kLen), Sha256::hash(kInput, kLen));

    const gp::UInt8* const bytePtr = reinterpret_cast<const gp::UInt8*>(kInput);
    EXPECT_EQ(computeHash64(HashAlgorithm::XxHash64, bytePtr, kLen), XxHash64::hash(kInput, kLen));
    EXPECT_EQ(computeHash64(HashAlgorithm::Wyhash64, bytePtr, kLen), Wyhash64::hash(kInput, kLen));
    EXPECT_EQ(computeHash32(HashAlgorithm::Crc32, bytePtr, kLen), Crc32::hash(kInput, kLen));
    EXPECT_EQ(computeHash128(HashAlgorithm::Murmur3_128, bytePtr, kLen), Murmur3_128::hash(kInput, kLen));
    EXPECT_EQ(
        computeHash256(HashAlgorithm::Sha256, static_cast<const void*>(kInput), kLen), Sha256::hash(kInput, kLen)
    );

    const HashDigest dBytes = computeHash(HashAlgorithm::XxHash32, bytePtr, kLen);
    const HashDigest dChars = computeHash(HashAlgorithm::XxHash32, kInput, kLen);
    const HashDigest dVoid = computeHash(HashAlgorithm::XxHash32, static_cast<const void*>(kInput), kLen);
    const HashDigest dView = computeHash(HashAlgorithm::XxHash32, gp::StringView(kInput, kLen));
    EXPECT_EQ(dBytes, dChars);
    EXPECT_EQ(dBytes, dVoid);
    EXPECT_EQ(dBytes, dView);
    EXPECT_EQ(dBytes.toUInt32(), XxHash32::hash(kInput, kLen));
}

TEST(RuntimeHashTest, DynamicHasherStreamingResetSeedAndCaseFolding)
{
    DynamicHasher hasher(HashAlgorithm::XxHash64);
    EXPECT_EQ(hasher.algorithm(), HashAlgorithm::XxHash64);
    EXPECT_EQ(hasher.digestBits(), 64u);

    hasher.update(gp::StringView("The quick brown "));
    hasher.update("fox jumps over the lazy dog", 27u);
    EXPECT_EQ(hasher.finalize64(), XxHash64::hash("The quick brown fox jumps over the lazy dog"));

    // Switch algorithm in place with zero allocations.
    hasher.reset(HashAlgorithm::Crc32);
    EXPECT_EQ(hasher.algorithm(), HashAlgorithm::Crc32);
    EXPECT_EQ(hasher.digestBits(), 32u);
    hasher.updateIgnoreCase(gp::StringView("123456789"));
    EXPECT_EQ(hasher.finalize32(), 0xCBF43926u);

    hasher.reset(HashAlgorithm::Murmur3_128);
    hasher.resetWithSeed(0x42ULL);
    hasher.update(gp::StringView("abc"));
    EXPECT_EQ(hasher.finalize128(), Murmur3_128::hash(gp::StringView("abc"), Hash128(0x42ULL, 0x42ULL)));

    hasher.resetWithSeed(Hash128(0x1111ULL, 0x2222ULL));
    hasher.update(gp::StringView("abc"));
    EXPECT_EQ(hasher.finalize128(), Murmur3_128::hash(gp::StringView("abc"), Hash128(0x1111ULL, 0x2222ULL)));

    hasher.reset(HashAlgorithm::Fnv1a64);
    hasher.append(gp::UInt32{ 42u });
    hasher.append(gp::StringView("Node"));
    const HashDigest streamed = hasher.finalize();
    EXPECT_EQ(streamed.width, HashWidth::Bits64);
    EXPECT_TRUE(streamed.isValid());

    DynamicHasher expected(HashAlgorithm::Fnv1a64);
    expected.append(gp::UInt32{ 42u });
    expected.append(gp::StringView("Node"));
    EXPECT_EQ(streamed, expected.finalize());

    // Null pointer and gp::Byte overloads are safe.
    hasher.reset(HashAlgorithm::Crc32C);
    hasher.update(nullptr, 10u);
    const gp::Byte payload[3] = { gp::Byte{ 'a' }, gp::Byte{ 'b' }, gp::Byte{ 'c' } };
    hasher.update(payload, 3u);
    EXPECT_EQ(hasher.finalize32(), 0x364B3FB7u);

    // Sha256 and SipHash24 via DynamicHasher.
    hasher.reset(HashAlgorithm::Sha256);
    EXPECT_EQ(hasher.digestBits(), 256u);
    hasher.update(gp::StringView("abc"));
    EXPECT_EQ(hasher.finalize256(), Sha256::hash(gp::StringView("abc")));

    const Hash256 domainSeed(1ULL, 2ULL, 3ULL, 4ULL);
    hasher.resetWithSeed(domainSeed);
    hasher.update(gp::StringView("abc"));
    EXPECT_EQ(hasher.finalize256(), Sha256::hash(gp::StringView("abc"), domainSeed));

    hasher.reset(HashAlgorithm::SipHash24);
    const Hash128 sipKey(0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL);
    hasher.resetWithSeed(sipKey);
    EXPECT_EQ(hasher.finalize64(), 0x726FDB47DD0E0E31ULL);
}

}   // namespace gp::hashing::tests
