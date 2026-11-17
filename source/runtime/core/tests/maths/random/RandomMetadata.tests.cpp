// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"   // IWYU pragma: keep
#include "hashing/HashOf.hpp"
#include "maths/random/Pcg32.hpp"
#include "maths/random/Philox4x32.hpp"
#include "maths/random/Random.hpp"
#include "maths/random/RandomConcepts.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/random/RandomStream.hpp"
#include "maths/random/RandomTypes.hpp"
#include "maths/random/Sfc64.hpp"
#include "maths/random/SplitMix64.hpp"
#include "maths/random/Xoshiro128.hpp"
#include "maths/random/Xoshiro256.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::math::tests
{

namespace
{

struct InvalidEngine
{
    using WordType = gp::UInt16;
    using StateType = gp::UInt16;
};

}   // namespace

static_assert(IsRandomWord<gp::UInt32>, "UInt32 must satisfy IsRandomWord.");
static_assert(IsRandomWord<gp::UInt64>, "UInt64 must satisfy IsRandomWord.");
static_assert(!IsRandomWord<gp::Int32>, "Signed Int32 must not satisfy IsRandomWord.");
static_assert(!IsRandomWord<gp::UInt16>, "UInt16 must not satisfy IsRandomWord.");

static_assert(IsRandomEngine<SplitMix64>, "SplitMix64 must satisfy IsRandomEngine.");
static_assert(IsRandomEngine<Pcg32>, "Pcg32 must satisfy IsRandomEngine.");
static_assert(IsRandomEngine<Xoshiro128PlusPlus>, "Xoshiro128PlusPlus must satisfy IsRandomEngine.");
static_assert(IsRandomEngine<Xoshiro256PlusPlus>, "Xoshiro256PlusPlus must satisfy IsRandomEngine.");
static_assert(IsRandomEngine<Sfc64>, "Sfc64 must satisfy IsRandomEngine.");
static_assert(!IsRandomEngine<InvalidEngine>, "InvalidEngine must be rejected by IsRandomEngine.");

static_assert(IsJumpableRandomEngine<SplitMix64>, "SplitMix64 must satisfy IsJumpableRandomEngine.");
static_assert(IsJumpableRandomEngine<Pcg32>, "Pcg32 must satisfy IsJumpableRandomEngine.");
static_assert(IsJumpableRandomEngine<Xoshiro128PlusPlus>, "Xoshiro128PlusPlus must satisfy IsJumpableRandomEngine.");
static_assert(IsJumpableRandomEngine<Xoshiro256PlusPlus>, "Xoshiro256PlusPlus must satisfy IsJumpableRandomEngine.");
static_assert(!IsJumpableRandomEngine<Sfc64>, "Sfc64 must not satisfy IsJumpableRandomEngine.");

static_assert(IsCounterRandomEngine<Philox4x32>, "Philox4x32 must satisfy IsCounterRandomEngine.");

static_assert(
    gp::concepts::IsSameAs<RandomEngine<RandomAlgorithm::SplitMix64>, SplitMix64>,
    "RandomEngine trait mismatch for SplitMix64."
);
static_assert(
    gp::concepts::IsSameAs<RandomEngine<RandomAlgorithm::Pcg32>, Pcg32>, "RandomEngine trait mismatch for Pcg32."
);
static_assert(
    gp::concepts::IsSameAs<RandomEngine<RandomAlgorithm::Xoshiro128PlusPlus>, Xoshiro128PlusPlus>,
    "RandomEngine trait mismatch for Xoshiro128PlusPlus."
);
static_assert(
    gp::concepts::IsSameAs<RandomEngine<RandomAlgorithm::Xoshiro256PlusPlus>, Xoshiro256PlusPlus>,
    "RandomEngine trait mismatch for Xoshiro256PlusPlus."
);
static_assert(
    gp::concepts::IsSameAs<RandomEngine<RandomAlgorithm::Sfc64>, Sfc64>, "RandomEngine trait mismatch for Sfc64."
);

static_assert(bitsToFloat01(0u) == 0.0f, "bitsToFloat01(0) must equal 0.0f.");
static_assert(bitsToFloat01(0xFFFFFFFFu) < 1.0f, "bitsToFloat01(UINT32_MAX) must stay strictly below 1.0f.");
static_assert(bitsToFloat01Inclusive(0u) == 0.0f, "bitsToFloat01Inclusive(0) must equal 0.0f.");
static_assert(bitsToFloat01Inclusive(0xFFFFFFFFu) == 1.0f, "bitsToFloat01Inclusive(UINT32_MAX) must equal 1.0f.");

static_assert(bitsToDouble01(0ULL) == 0.0, "bitsToDouble01(0) must equal 0.0.");
static_assert(bitsToDouble01(0xFFFFFFFFFFFFFFFFULL) < 1.0, "bitsToDouble01(UINT64_MAX) must stay strictly below 1.0.");
static_assert(bitsToDouble01Inclusive(0ULL) == 0.0, "bitsToDouble01Inclusive(0) must equal 0.0.");
static_assert(
    bitsToDouble01Inclusive(0xFFFFFFFFFFFFFFFFULL) == 1.0, "bitsToDouble01Inclusive(UINT64_MAX) must equal 1.0."
);

static_assert(bitsToSignedFloat(0u) == -1.0f, "bitsToSignedFloat(0) must equal -1.0f.");
static_assert(bitsToSignedFloat(0xFFFFFFFFu) == 1.0f, "bitsToSignedFloat(UINT32_MAX) must equal 1.0f.");
static_assert(bitsToSignedFloatHalfOpen(0u) == -1.0f, "bitsToSignedFloatHalfOpen(0) must equal -1.0f.");
static_assert(bitsToSignedFloatHalfOpen(0xFFFFFFFFu) < 1.0f, "bitsToSignedFloatHalfOpen(max) must stay below 1.0f.");

static_assert(bitsToSignedDouble(0ULL) == -1.0, "bitsToSignedDouble(0) must equal -1.0.");
static_assert(bitsToSignedDouble(0xFFFFFFFFFFFFFFFFULL) == 1.0, "bitsToSignedDouble(UINT64_MAX) must equal 1.0.");
static_assert(bitsToSignedDoubleHalfOpen(0ULL) == -1.0, "bitsToSignedDoubleHalfOpen(0) must equal -1.0.");
static_assert(
    bitsToSignedDoubleHalfOpen(0xFFFFFFFFFFFFFFFFULL) < 1.0, "bitsToSignedDoubleHalfOpen(max) must stay below 1.0."
);

static_assert(std::is_trivially_copyable_v<SplitMix64>, "SplitMix64 must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<Pcg32>, "Pcg32 must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<Xoshiro128PlusPlus>, "Xoshiro128PlusPlus must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<Xoshiro256PlusPlus>, "Xoshiro256PlusPlus must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<Sfc64>, "Sfc64 must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<RandomStream>, "RandomStream must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<RandomStream32>, "RandomStream32 must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<CompactRandomStream>, "CompactRandomStream must be trivially copyable.");
static_assert(std::is_trivially_copyable_v<FastRandomStream>, "FastRandomStream must be trivially copyable.");

TEST(RandomMetadataTest, AlgorithmEnumsAndDescriptors)
{
    const RandomAlgorithm invalidAlg = static_cast<RandomAlgorithm>(255u);

    EXPECT_STREQ(algorithmToString(RandomAlgorithm::SplitMix64), "SplitMix64");
    EXPECT_STREQ(algorithmToString(RandomAlgorithm::Pcg32), "Pcg32");
    EXPECT_STREQ(algorithmToString(RandomAlgorithm::Xoshiro128PlusPlus), "Xoshiro128PlusPlus");
    EXPECT_STREQ(algorithmToString(RandomAlgorithm::Xoshiro256PlusPlus), "Xoshiro256PlusPlus");
    EXPECT_STREQ(algorithmToString(RandomAlgorithm::Sfc64), "Sfc64");
    EXPECT_STREQ(algorithmToString(RandomAlgorithm::Count), "Unknown");
    EXPECT_STREQ(algorithmToString(invalidAlg), "Unknown");

    EXPECT_EQ(algorithmOutputBits(RandomAlgorithm::SplitMix64), 64u);
    EXPECT_EQ(algorithmOutputBits(RandomAlgorithm::Pcg32), 32u);
    EXPECT_EQ(algorithmOutputBits(RandomAlgorithm::Xoshiro128PlusPlus), 32u);
    EXPECT_EQ(algorithmOutputBits(RandomAlgorithm::Xoshiro256PlusPlus), 64u);
    EXPECT_EQ(algorithmOutputBits(RandomAlgorithm::Sfc64), 64u);
    EXPECT_EQ(algorithmOutputBits(RandomAlgorithm::Count), 0u);
    EXPECT_EQ(algorithmOutputBits(invalidAlg), 0u);

    EXPECT_EQ(algorithmStateBytes(RandomAlgorithm::SplitMix64), 8u);
    EXPECT_EQ(algorithmStateBytes(RandomAlgorithm::Pcg32), 16u);
    EXPECT_EQ(algorithmStateBytes(RandomAlgorithm::Xoshiro128PlusPlus), 16u);
    EXPECT_EQ(algorithmStateBytes(RandomAlgorithm::Xoshiro256PlusPlus), 32u);
    EXPECT_EQ(algorithmStateBytes(RandomAlgorithm::Sfc64), 32u);
    EXPECT_EQ(algorithmStateBytes(RandomAlgorithm::Count), 0u);
    EXPECT_EQ(algorithmStateBytes(invalidAlg), 0u);

    EXPECT_TRUE(supportsJumpAhead(RandomAlgorithm::SplitMix64));
    EXPECT_TRUE(supportsJumpAhead(RandomAlgorithm::Pcg32));
    EXPECT_TRUE(supportsJumpAhead(RandomAlgorithm::Xoshiro128PlusPlus));
    EXPECT_TRUE(supportsJumpAhead(RandomAlgorithm::Xoshiro256PlusPlus));
    EXPECT_FALSE(supportsJumpAhead(RandomAlgorithm::Sfc64));
    EXPECT_FALSE(supportsJumpAhead(RandomAlgorithm::Count));
    EXPECT_FALSE(supportsJumpAhead(invalidAlg));

    EXPECT_TRUE(supportsNativeStreamId(RandomAlgorithm::Pcg32));
    EXPECT_FALSE(supportsNativeStreamId(RandomAlgorithm::SplitMix64));
    EXPECT_FALSE(supportsNativeStreamId(RandomAlgorithm::Xoshiro128PlusPlus));
    EXPECT_FALSE(supportsNativeStreamId(RandomAlgorithm::Xoshiro256PlusPlus));
    EXPECT_FALSE(supportsNativeStreamId(RandomAlgorithm::Sfc64));
    EXPECT_FALSE(supportsNativeStreamId(invalidAlg));
}

TEST(RandomMetadataTest, StateSnapshotsAndHashingSupport)
{
    const SplitMix64State smA{ 42ULL };
    const SplitMix64State smB{ 43ULL };
    EXPECT_EQ(smA, SplitMix64State{ 42ULL });
    EXPECT_NE(smA, smB);
    EXPECT_NE(gp::hashing::hashOf(smA), gp::hashing::hashOf(smB));

    const Pcg32State pcgA{ 1ULL, 3ULL };
    const Pcg32State pcgB{ 1ULL, 5ULL };
    EXPECT_NE(pcgA, pcgB);
    EXPECT_NE(gp::hashing::hashOf(pcgA), gp::hashing::hashOf(pcgB));

    const Xoshiro128State x128A{
        { 1u, 2u, 3u, 4u }
    };
    const Xoshiro128State x128B{
        { 1u, 2u, 3u, 5u }
    };
    EXPECT_NE(x128A, x128B);
    EXPECT_NE(gp::hashing::hashOf(x128A), gp::hashing::hashOf(x128B));

    const Xoshiro256State x256A{
        { 1ULL, 2ULL, 3ULL, 4ULL }
    };
    const Xoshiro256State x256B{
        { 1ULL, 2ULL, 3ULL, 5ULL }
    };
    EXPECT_NE(x256A, x256B);
    EXPECT_NE(gp::hashing::hashOf(x256A), gp::hashing::hashOf(x256B));

    const Sfc64State sfcA{ 1ULL, 2ULL, 3ULL, 4ULL };
    const Sfc64State sfcB{ 1ULL, 2ULL, 3ULL, 5ULL };
    EXPECT_NE(sfcA, sfcB);
    EXPECT_NE(gp::hashing::hashOf(sfcA), gp::hashing::hashOf(sfcB));

    const Philox4x32Key key64(0x1122334455667788ULL);
    const Philox4x32Key keyPair(0x55667788u, 0x11223344u);
    EXPECT_EQ(key64, keyPair);
    EXPECT_EQ(gp::hashing::hashOf(key64), gp::hashing::hashOf(keyPair));

    const Philox4x32Counter ctrPacked(0x0102030405060708ULL, 0x1112131415161718ULL);
    const Philox4x32Counter ctrExplicit(0x05060708u, 0x01020304u, 0x15161718u, 0x11121314u);
    EXPECT_EQ(ctrPacked, ctrExplicit);
    EXPECT_EQ(gp::hashing::hashOf(ctrPacked), gp::hashing::hashOf(ctrExplicit));
}

TEST(RandomMetadataTest, BitConversionsCoverBoundaries)
{
    EXPECT_FLOAT_EQ(bitsToFloat01(0u), 0.0f);
    EXPECT_LT(bitsToFloat01(0xFFFFFFFFu), 1.0f);
    EXPECT_GE(bitsToFloat01(0x80000000u), 0.5f);

    EXPECT_FLOAT_EQ(bitsToFloat01Inclusive(0u), 0.0f);
    EXPECT_FLOAT_EQ(bitsToFloat01Inclusive(0xFFFFFFFFu), 1.0f);

    EXPECT_DOUBLE_EQ(bitsToDouble01(0ULL), 0.0);
    EXPECT_LT(bitsToDouble01(0xFFFFFFFFFFFFFFFFULL), 1.0);
    EXPECT_GE(bitsToDouble01(0x8000000000000000ULL), 0.5);

    EXPECT_DOUBLE_EQ(bitsToDouble01Inclusive(0ULL), 0.0);
    EXPECT_DOUBLE_EQ(bitsToDouble01Inclusive(0xFFFFFFFFFFFFFFFFULL), 1.0);

    EXPECT_FLOAT_EQ(bitsToSignedFloat(0u), -1.0f);
    EXPECT_FLOAT_EQ(bitsToSignedFloat(0xFFFFFFFFu), 1.0f);
    EXPECT_FLOAT_EQ(bitsToSignedFloatHalfOpen(0u), -1.0f);
    EXPECT_LT(bitsToSignedFloatHalfOpen(0xFFFFFFFFu), 1.0f);

    EXPECT_DOUBLE_EQ(bitsToSignedDouble(0ULL), -1.0);
    EXPECT_DOUBLE_EQ(bitsToSignedDouble(0xFFFFFFFFFFFFFFFFULL), 1.0);
    EXPECT_DOUBLE_EQ(bitsToSignedDoubleHalfOpen(0ULL), -1.0);
    EXPECT_LT(bitsToSignedDoubleHalfOpen(0xFFFFFFFFFFFFFFFFULL), 1.0);
}

}   // namespace gp::math::tests
