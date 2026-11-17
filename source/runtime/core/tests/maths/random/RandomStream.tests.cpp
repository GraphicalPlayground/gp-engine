// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/random/RandomStream.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashingTypes.hpp"
#include "hashing/HashOf.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/FloatingPoint.hpp"
#include "maths/random/DynamicRandomStream.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"
#include "maths/vector/Vector4.hpp"
#include "templates/Interval.hpp"
#include "templates/Pair.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

namespace gp::math::tests
{

constexpr bool testConstexprRandomStream() noexcept
{
    RandomStream stream(1337ULL, 9ULL);
    const gp::Int32 rInt = stream.nextRange(-10, 10);
    const gp::Float32 rFloat = stream.nextRange(2.0f, 5.0f);
    const gp::Float64 rDouble = stream.nextRange(-1.0, 1.0);
    const gp::USize idx = stream.nextIndex(17u);
    const Vector3<gp::Float32> bary = stream.nextBarycentric<gp::Float32>();

    gp::Array<gp::Int32, 6u> items{ 1, 2, 3, 4, 5, 6 };
    stream.shuffle(items);

    gp::UInt8 bytes[11]{};
    stream.fillBytes(bytes, 11u);

    const gp::Float32 barySum = bary.x + bary.y + bary.z;
    return rInt >= -10 && rInt <= 10 && rFloat >= 2.0f && rFloat < 5.0f && rDouble >= -1.0 && rDouble < 1.0 &&
           idx < 17u && isNearlyEqual(barySum, 1.0f, 1e-5f);
}

static_assert(testConstexprRandomStream(), "RandomStream constexpr evaluation failed.");

constexpr bool testConstexprDynamicStream() noexcept
{
    DynamicRandomStream dyn(RandomAlgorithm::Pcg32, 999ULL, 2ULL);
    const gp::UInt32 v0 = dyn.nextUInt32();
    dyn.reset();
    const gp::UInt32 v1 = dyn.nextUInt32();
    dyn.switchAlgorithm(RandomAlgorithm::SplitMix64, 42ULL);
    const gp::Int32 bounded = dyn.nextRange(1, 6);
    return v0 == v1 && dyn.algorithm() == RandomAlgorithm::SplitMix64 && bounded >= 1 && bounded <= 6;
}

static_assert(testConstexprDynamicStream(), "DynamicRandomStream constexpr evaluation failed.");

template <typename InStream>
class RandomStreamTypedTest : public ::testing::Test
{};

using StreamTypes = ::testing::Types<RandomStream, RandomStream32, CompactRandomStream, FastRandomStream>;
TYPED_TEST_SUITE(RandomStreamTypedTest, StreamTypes);

TYPED_TEST(RandomStreamTypedTest, ConstructorsSeedingResetAndFork)
{
    TypeParam defaultStream;
    TypeParam seedStream(kDefaultRandomSeed);
    EXPECT_EQ(defaultStream, seedStream);

    TypeParam strViewStream(gp::StringView("terrain_erosion"), 4u);
    TypeParam cstrStream("terrain_erosion", 4u);
    EXPECT_EQ(strViewStream, cstrStream);

    TypeParam mutStream;
    mutStream.seedFromString(gp::String("terrain_erosion"), 4u);
    EXPECT_EQ(mutStream, strViewStream);

    const gp::hashing::Hash128 h128(0xAAAABBBBULL, 0xCCCCDDDDULL);
    const gp::hashing::Hash256 h256(1ULL, 2ULL, 3ULL, 4ULL);
    TypeParam h128Stream(h128);
    TypeParam h256Stream(h256);
    TypeParam hMut;
    hMut.seedFromHash(h128);
    EXPECT_EQ(hMut, h128Stream);
    hMut.seedFromHash(h256);
    EXPECT_EQ(hMut, h256Stream);

    TypeParam wrapped(typename TypeParam::EngineType(55ULL, 66ULL));
    EXPECT_EQ(wrapped.initialSeed(), 55ULL);
    EXPECT_EQ(wrapped.streamId(), 66ULL);

    const gp::UInt64 firstDraw = wrapped.nextUInt64();
    wrapped.reset();
    EXPECT_EQ(wrapped.nextUInt64(), firstDraw);

    const typename TypeParam::StateType saved = wrapped.state();
    const gp::UInt32 afterSave = wrapped.nextUInt32();
    wrapped.setState(saved);
    EXPECT_EQ(wrapped.nextUInt32(), afterSave);

    TypeParam forkedNum = wrapped.fork(10ULL);
    TypeParam forkedStr = wrapped.fork(gp::StringView("npc_bark"));
    TypeParam forkedCStr = wrapped.fork("npc_bark");
    EXPECT_EQ(forkedStr, forkedCStr);
    EXPECT_NE(forkedNum, forkedStr);
    EXPECT_NE(gp::hashing::hashOf(forkedNum), gp::hashing::hashOf(forkedStr));
}

TYPED_TEST(RandomStreamTypedTest, UniformScalarAndRangeMethods)
{
    TypeParam stream(2026ULL);

    EXPECT_FALSE(stream.nextBool(0.0f));
    EXPECT_FALSE(stream.nextBool(-0.5f));
    EXPECT_TRUE(stream.nextBool(1.0f));
    EXPECT_TRUE(stream.nextBool(1.5f));
    EXPECT_FALSE(stream.nextBool(0.0));
    EXPECT_TRUE(stream.nextBool(1.0));

    gp::USize trueCount = 0u;
    gp::USize doubleTrueCount = 0u;
    constexpr gp::USize kBoolTrials = 1000u;
    for (gp::USize i = 0u; i < kBoolTrials; ++i)
    {
        if (stream.nextBool())
        {
            ++trueCount;
        }
        if (stream.nextBool(0.5))
        {
            ++doubleTrueCount;
        }
    }
    EXPECT_GT(trueCount, 350u);
    EXPECT_LT(trueCount, 650u);
    EXPECT_GT(doubleTrueCount, 350u);
    EXPECT_LT(doubleTrueCount, 650u);

    for (gp::USize i = 0u; i < 200u; ++i)
    {
        const gp::Float32 f01 = stream.nextFloat();
        EXPECT_GE(f01, 0.0f);
        EXPECT_LT(f01, 1.0f);

        const gp::Float32 fInc = stream.nextFloatInclusive();
        EXPECT_GE(fInc, 0.0f);
        EXPECT_LE(fInc, 1.0f);

        const gp::Float32 fSigned = stream.nextSignedFloat();
        EXPECT_GE(fSigned, -1.0f);
        EXPECT_LE(fSigned, 1.0f);

        const gp::Float32 fSignedHalf = stream.nextSignedFloatHalfOpen();
        EXPECT_GE(fSignedHalf, -1.0f);
        EXPECT_LT(fSignedHalf, 1.0f);

        const gp::Float64 d01 = stream.nextDouble();
        EXPECT_GE(d01, 0.0);
        EXPECT_LT(d01, 1.0);

        const gp::Float64 dInc = stream.nextDoubleInclusive();
        EXPECT_GE(dInc, 0.0);
        EXPECT_LE(dInc, 1.0);

        const gp::Float64 dSigned = stream.nextSignedDouble();
        EXPECT_GE(dSigned, -1.0);
        EXPECT_LE(dSigned, 1.0);

        const gp::Float64 dSignedHalf = stream.nextSignedDoubleHalfOpen();
        EXPECT_GE(dSignedHalf, -1.0);
        EXPECT_LT(dSignedHalf, 1.0);

        const gp::Int32 iRange = stream.nextRange(-15, 25);
        EXPECT_GE(iRange, -15);
        EXPECT_LE(iRange, 25);

        const gp::UInt32 uRange = stream.nextRange(10u, 19u);
        EXPECT_GE(uRange, 10u);
        EXPECT_LE(uRange, 19u);

        const gp::Int64 i64Range = stream.nextRange(gp::Int64{ -10000000000LL }, gp::Int64{ 10000000000LL });
        EXPECT_GE(i64Range, -10000000000LL);
        EXPECT_LE(i64Range, 10000000000LL);

        const gp::UInt64 u64Range = stream.nextRange(100ULL, 500ULL);
        EXPECT_GE(u64Range, 100ULL);
        EXPECT_LE(u64Range, 500ULL);

        const gp::Float32 fRange = stream.nextRange(-5.0f, 12.5f);
        EXPECT_GE(fRange, -5.0f);
        EXPECT_LT(fRange, 12.5f);

        const gp::Float64 dRange = stream.nextRange(-2.5, 3.5);
        EXPECT_GE(dRange, -2.5);
        EXPECT_LT(dRange, 3.5);

        const gp::Float32 u32Scalar = stream.template nextUnitScalar<gp::Float32>();
        EXPECT_GE(u32Scalar, 0.0f);
        EXPECT_LT(u32Scalar, 1.0f);

        const gp::Float64 u64Scalar = stream.template nextUnitScalar<gp::Float64>();
        EXPECT_GE(u64Scalar, 0.0);
        EXPECT_LT(u64Scalar, 1.0);

        const gp::Int32 signInt = stream.template nextSign<gp::Int32>();
        EXPECT_TRUE(signInt == 1 || signInt == -1);
        const gp::Float32 signFloat = stream.template nextSign<gp::Float32>();
        EXPECT_TRUE(signFloat == 1.0f || signFloat == -1.0f);
        const gp::Float64 signDouble = stream.template nextSign<gp::Float64>();
        EXPECT_TRUE(signDouble == 1.0 || signDouble == -1.0);
    }

    // Huge symmetric floating-point bounds must not overflow (max - min) to NaN.
    const gp::Float32 hugeFloat = stream.nextRange(-3.0e38f, 3.0e38f);
    EXPECT_TRUE(isFinite(hugeFloat));
    EXPECT_GE(hugeFloat, -3.0e38f);
    EXPECT_LT(hugeFloat, 3.0e38f);

    const gp::Float64 hugeDouble = stream.nextRange(-1.0e307, 1.0e307);
    EXPECT_TRUE(isFinite(hugeDouble));
    EXPECT_GE(hugeDouble, -1.0e307);
    EXPECT_LT(hugeDouble, 1.0e307);

    // Degenerate and full-width bounds.
    EXPECT_EQ(stream.nextBoundedUInt32(0u), 0u);
    EXPECT_EQ(stream.nextBoundedUInt32(1u), 0u);
    EXPECT_EQ(stream.nextBoundedUInt64(0ULL), 0ULL);
    EXPECT_EQ(stream.nextBoundedUInt64(1ULL), 0ULL);
    EXPECT_EQ(stream.nextIndex(0u), 0u);
    EXPECT_EQ(stream.nextIndex(1u), 0u);
    EXPECT_EQ(stream.nextRange(7, 7), 7);
    EXPECT_FLOAT_EQ(stream.nextRange(3.5f, 3.5f), 3.5f);

    const gp::Int32 fullSigned32 =
        stream.nextRange(static_cast<gp::Int32>(-2147483647 - 1), static_cast<gp::Int32>(2147483647));
    EXPECT_TRUE(fullSigned32 <= 2147483647);

    const gp::UInt64 fullUnsigned64 = stream.nextRange(0ULL, 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_TRUE(fullUnsigned64 <= 0xFFFFFFFFFFFFFFFFULL);

    const gp::Int8 fullSigned8 = stream.nextRange(static_cast<gp::Int8>(-128), static_cast<gp::Int8>(127));
    EXPECT_GE(fullSigned8, static_cast<gp::Int8>(-128));
    EXPECT_LE(fullSigned8, static_cast<gp::Int8>(127));

    const gp::UInt16 fullUnsigned16 = stream.nextRange(gp::UInt16{ 0u }, gp::UInt16{ 65535u });
    EXPECT_LE(fullUnsigned16, gp::UInt16{ 65535u });

    const gp::Interval<gp::Int32> intInterval = gp::Interval<gp::Int32>::closed(-5, 5);
    const gp::Interval<gp::Float32> floatInterval = gp::Interval<gp::Float32>::halfOpen(10.0f, 20.0f);
    const gp::Int32 ivInt = stream.nextInInterval(intInterval);
    const gp::Float32 ivFloat = stream.nextInInterval(floatInterval);
    EXPECT_GE(ivInt, -5);
    EXPECT_LE(ivInt, 5);
    EXPECT_GE(ivFloat, 10.0f);
    EXPECT_LT(ivFloat, 20.0f);
}

TEST(RandomStreamSpecificTest, EntropySplitJumpAndDiscard)
{
    const RandomStream entropyA = RandomStream::fromEntropy();
    const RandomStream entropyB = RandomStream::fromEntropy();
    EXPECT_NE(entropyA.initialSeed(), entropyB.initialSeed());

    RandomStream parent(404ULL);
    RandomStream worker0 = parent.split();
    RandomStream worker1 = parent.split();

    RandomStream jumped(404ULL);
    jumped.jump();
    EXPECT_EQ(jumped, worker1);
    EXPECT_EQ(jumped.nextUInt64(), worker1.nextUInt64());
    EXPECT_NE(worker0.nextUInt64(), worker1.nextUInt64());
    EXPECT_NE(worker1.nextUInt64(), parent.nextUInt64());

    RandomStream ljA(404ULL);
    RandomStream ljB(404ULL);
    ljB.longJump();
    EXPECT_NE(ljA.nextUInt64(), ljB.nextUInt64());

    RandomStream dA(404ULL);
    RandomStream dB(404ULL);
    dA.discard(12u);
    for (gp::USize i = 0u; i < 12u; ++i)
    {
        (void)dB.nextUInt64();
    }
    EXPECT_EQ(dA, dB);
}

TEST(RandomStreamSpecificTest, StatisticalDistributions)
{
    RandomStream rng(8888ULL);

    constexpr gp::USize kSamples = 4000u;
    gp::Float64 gaussianSum = 0.0;
    gp::Float64 gaussianSqSum = 0.0;
    constexpr gp::Float64 kMean = 10.0;
    constexpr gp::Float64 kStdDev = 2.5;

    for (gp::USize i = 0u; i < kSamples; ++i)
    {
        const gp::Pair<gp::Float64, gp::Float64> pair = rng.nextGaussianPair<gp::Float64>(kMean, kStdDev);
        gaussianSum += pair.first + pair.second;
        const gp::Float64 d0 = pair.first - kMean;
        const gp::Float64 d1 = pair.second - kMean;
        gaussianSqSum += d0 * d0 + d1 * d1;
    }

    const gp::Float64 empiricalMean = gaussianSum / static_cast<gp::Float64>(kSamples * 2u);
    const gp::Float64 empiricalStdDev = gp::math::sqrt(gaussianSqSum / static_cast<gp::Float64>(kSamples * 2u));
    EXPECT_NEAR(empiricalMean, kMean, 0.15);
    EXPECT_NEAR(empiricalStdDev, kStdDev, 0.15);

    const gp::Float32 g32 = rng.nextGaussian<gp::Float32>(1.0f, 0.5f);
    EXPECT_TRUE(isFinite(g32));
    const gp::Float64 g64 = rng.nextGaussian<gp::Float64>(-3.0, 0.0);
    EXPECT_DOUBLE_EQ(g64, -3.0);

    gp::Float64 expSum = 0.0;
    for (gp::USize i = 0u; i < 2000u; ++i)
    {
        const gp::Float64 e = rng.nextExponential<gp::Float64>(2.0);
        EXPECT_GE(e, 0.0);
        expSum += e;
    }
    EXPECT_NEAR(expSum / 2000.0, 0.5, 0.05);
    EXPECT_GE(rng.nextExponential<gp::Float32>(1.5f), 0.0f);

    for (gp::USize i = 0u; i < 200u; ++i)
    {
        const gp::Float32 lnVal = rng.nextLogNormal<gp::Float32>(0.0f, 0.5f);
        EXPECT_GT(lnVal, 0.0f);
        EXPECT_GT(rng.nextLogNormal<gp::Float64>(0.0, 0.25), 0.0);

        const gp::Float32 triVal = rng.nextTriangular<gp::Float32>(-2.0f, 8.0f, 5.0f);
        EXPECT_GE(triVal, -2.0f);
        EXPECT_LE(triVal, 8.0f);
        const gp::Float64 triVal64 = rng.nextTriangular<gp::Float64>(-1.0, 4.0, 1.5);
        EXPECT_GE(triVal64, -1.0);
        EXPECT_LE(triVal64, 4.0);

        const gp::Float32 biasVal = rng.nextBiased<gp::Float32>(1.0f, 10.0f, 2.0f);
        EXPECT_GE(biasVal, 1.0f);
        EXPECT_LT(biasVal, 10.0f);
        const gp::Float64 biasVal64 = rng.nextBiased<gp::Float64>(0.0, 5.0, 0.5);
        EXPECT_GE(biasVal64, 0.0);
        EXPECT_LT(biasVal64, 5.0);

        const gp::Float32 rad = rng.nextAngleRadians<gp::Float32>();
        EXPECT_GE(rad, 0.0f);
        EXPECT_LT(rad, Constants<gp::Float32>::twoPi);
        EXPECT_LT(rng.nextAngleRadians<gp::Float64>(), Constants<gp::Float64>::twoPi);

        const gp::Float32 deg = rng.nextAngleDegrees<gp::Float32>();
        EXPECT_GE(deg, 0.0f);
        EXPECT_LT(deg, 360.0f);
        EXPECT_LT(rng.nextAngleDegrees<gp::Float64>(), 360.0);
    }

    // Check modulo-bias freedom across a non-power-of-two bucket count.
    gp::Array<gp::USize, 7u> bucketCounts{};
    for (gp::USize i = 0u; i < 7000u; ++i)
    {
        const gp::UInt32 bucket = rng.nextBoundedUInt32(7u);
        ASSERT_LT(bucket, 7u);
        ++bucketCounts[bucket];
    }
    for (gp::USize b = 0u; b < 7u; ++b)
    {
        EXPECT_GT(bucketCounts[b], 800u);
        EXPECT_LT(bucketCounts[b], 1200u);
    }

    EXPECT_FLOAT_EQ(rng.nextTriangular<gp::Float32>(4.0f, 4.0f, 4.0f), 4.0f);
    EXPECT_FLOAT_EQ(rng.nextBiased<gp::Float32>(4.0f, 4.0f, 2.0f), 4.0f);
}

TEST(RandomStreamSpecificTest, GeometricAndSpatialSampling)
{
    RandomStream rng(9001ULL);
    const Vector3<gp::Float32> upNormal(0.0f, 1.0f, 0.0f);
    const Vector3<gp::Float32> coneAxis(1.0f, 1.0f, 1.0f);
    const gp::Float32 halfAngle = degreesToRadians(25.0f);

    for (gp::USize i = 0u; i < 200u; ++i)
    {
        const Vector2<gp::Float32> uc = rng.nextUnitCircle<gp::Float32>();
        EXPECT_NEAR(uc.length(), 1.0f, 1e-5f);

        const Vector2<gp::Float32> disk = rng.nextPointInUnitCircle<gp::Float32>();
        EXPECT_LE(disk.lengthSquared(), 1.0f + 1e-5f);

        const Vector2<gp::Float32> circleCenter(10.0f, -5.0f);
        const Vector2<gp::Float32> inCircle = rng.nextPointInCircle<gp::Float32>(circleCenter, 3.0f);
        EXPECT_LE((inCircle - circleCenter).length(), 3.0f + 1e-4f);

        const Vector2<gp::Float32> annulus = rng.nextPointInAnnulus<gp::Float32>(2.0f, 5.0f);
        const gp::Float32 annLen = annulus.length();
        EXPECT_GE(annLen, 2.0f - 1e-4f);
        EXPECT_LE(annLen, 5.0f + 1e-4f);

        const Vector3<gp::Float32> us = rng.nextUnitSphere<gp::Float32>();
        EXPECT_NEAR(us.length(), 1.0f, 1e-5f);

        const Vector3<gp::Float32> ball = rng.nextPointInUnitSphere<gp::Float32>();
        EXPECT_LE(ball.lengthSquared(), 1.0f + 1e-5f);

        const Vector3<gp::Float32> sphereCenter(-4.0f, 2.0f, 8.0f);
        const Vector3<gp::Float32> inSphere = rng.nextPointInSphere<gp::Float32>(sphereCenter, 2.5f);
        EXPECT_LE((inSphere - sphereCenter).length(), 2.5f + 1e-4f);

        const Vector3<gp::Float32> hemi = rng.nextUnitHemisphere<gp::Float32>(upNormal);
        EXPECT_NEAR(hemi.length(), 1.0f, 1e-5f);
        EXPECT_GE(hemi | upNormal, -1e-5f);

        const Vector3<gp::Float32> cosHemi = rng.nextCosineHemisphere<gp::Float32>(upNormal);
        EXPECT_NEAR(cosHemi.length(), 1.0f, 1e-5f);
        EXPECT_GE(cosHemi | upNormal, -1e-5f);

        const Vector3<gp::Float32> coneDir = rng.nextConeDirection<gp::Float32>(coneAxis, halfAngle);
        EXPECT_NEAR(coneDir.length(), 1.0f, 1e-5f);
        EXPECT_GE(coneDir | coneAxis.getSafeNormal(), std::cos(halfAngle) - 1e-4f);

        const Vector2<gp::Float32> box2 = rng.nextPointInBox<gp::Float32>({ -1.0f, 2.0f }, { 3.0f, 6.0f });
        EXPECT_GE(box2.x, -1.0f);
        EXPECT_LT(box2.x, 3.0f);
        EXPECT_GE(box2.y, 2.0f);
        EXPECT_LT(box2.y, 6.0f);

        const Vector3<gp::Float32> box3 = rng.nextPointInBox<gp::Float32>({ -1.0f, 0.0f, 5.0f }, { 1.0f, 2.0f, 9.0f });
        EXPECT_GE(box3.x, -1.0f);
        EXPECT_LT(box3.x, 1.0f);
        EXPECT_GE(box3.y, 0.0f);
        EXPECT_LT(box3.y, 2.0f);
        EXPECT_GE(box3.z, 5.0f);
        EXPECT_LT(box3.z, 9.0f);

        const Vector3<gp::Float32> bary = rng.nextBarycentric<gp::Float32>();
        EXPECT_GE(bary.x, 0.0f);
        EXPECT_GE(bary.y, 0.0f);
        EXPECT_GE(bary.z, 0.0f);
        EXPECT_NEAR(bary.x + bary.y + bary.z, 1.0f, 1e-5f);

        const Vector4<gp::Float32> opaque = rng.nextColorRgba<gp::Float32>(false);
        EXPECT_FLOAT_EQ(opaque.w, 1.0f);
        const Vector4<gp::Float32> alphaColor = rng.nextColorRgba<gp::Float32>(true);
        EXPECT_GE(alphaColor.w, 0.0f);
        EXPECT_LT(alphaColor.w, 1.0f);
    }

    // Degenerate zero-normal, zero-width annulus, and cone angle boundary checks.
    const Vector2<gp::Float32> zeroAnnulus = rng.nextPointInAnnulus<gp::Float32>(0.0f, 0.0f);
    EXPECT_FLOAT_EQ(zeroAnnulus.lengthSquared(), 0.0f);

    const Vector2<gp::Float32> thinRing = rng.nextPointInAnnulus<gp::Float32>(3.0f, 3.0f);
    EXPECT_NEAR(thinRing.length(), 3.0f, 1e-4f);

    const Vector3<gp::Float32> zeroAngleCone = rng.nextConeDirection<gp::Float32>(coneAxis, 0.0f);
    EXPECT_NEAR((zeroAngleCone - coneAxis.getSafeNormal()).length(), 0.0f, 1e-5f);

    const Vector3<gp::Float32> fullSphereCone =
        rng.nextConeDirection<gp::Float32>(coneAxis, Constants<gp::Float32>::pi);
    EXPECT_NEAR(fullSphereCone.length(), 1.0f, 1e-5f);

    const Vector3<gp::Float32> zeroNormHemi = rng.nextUnitHemisphere<gp::Float32>(Vector3<gp::Float32>::zero());
    EXPECT_NEAR(zeroNormHemi.length(), 1.0f, 1e-5f);

    const Vector3<gp::Float32> zeroNormCos = rng.nextCosineHemisphere<gp::Float32>(Vector3<gp::Float32>::zero());
    EXPECT_NEAR(zeroNormCos.length(), 1.0f, 1e-5f);

    const Vector3<gp::Float32> zeroNormCone = rng.nextConeDirection<gp::Float32>(Vector3<gp::Float32>::zero(), 0.2f);
    EXPECT_NEAR(zeroNormCone.length(), 1.0f, 1e-5f);
}

TEST(RandomStreamSpecificTest, BytesShufflePickAndSamplingWithoutReplacement)
{
    RandomStream a(314159ULL);
    RandomStream b(314159ULL);

    gp::UInt8 u8Buf[13]{};
    gp::Byte byteBuf[13]{};
    gp::UInt8 voidBuf[13]{};
    a.fillBytes(u8Buf, sizeof(u8Buf));
    b.fillBytes(byteBuf, sizeof(byteBuf));
    for (gp::USize i = 0u; i < 13u; ++i)
    {
        EXPECT_EQ(u8Buf[i], std::to_integer<gp::UInt8>(byteBuf[i]));
    }

    a.reset();
    a.fillBytes(static_cast<void*>(voidBuf), sizeof(voidBuf));
    for (gp::USize i = 0u; i < 13u; ++i)
    {
        EXPECT_EQ(voidBuf[i], u8Buf[i]);
    }
    a.fillBytes(static_cast<gp::UInt8*>(nullptr), 16u);
    a.fillBytes(u8Buf, 0u);

    gp::Array<gp::Int32, 10u> deck{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    a.shuffle(deck);
    gp::Int32 sum = 0;
    for (gp::USize i = 0u; i < deck.size(); ++i)
    {
        sum += deck[i];
    }
    EXPECT_EQ(sum, 45);

    gp::USize perm[8]{};
    a.nextPermutation(perm, 8u);
    gp::USize permMask = 0u;
    for (gp::USize i = 0u; i < 8u; ++i)
    {
        ASSERT_LT(perm[i], 8u);
        permMask |= (gp::USize{ 1 } << perm[i]);
    }
    EXPECT_EQ(permMask, 0xFFu);

    // Weighted selection ignores non-positive weights and tracks positive probabilities.
    const gp::Array<gp::Float32, 4u> weights{ 0.0f, 1.0f, 3.0f, 0.0f };
    gp::Array<gp::USize, 4u> counts{};
    for (gp::USize i = 0u; i < 2000u; ++i)
    {
        const gp::USize picked = a.pickIndex(weights);
        ASSERT_LT(picked, 4u);
        ++counts[picked];
    }
    EXPECT_EQ(counts[0], 0u);
    EXPECT_EQ(counts[3], 0u);
    EXPECT_GT(counts[2], counts[1] * 2u);

    const gp::Float32 allZeroWeights[3]{ 0.0f, -1.0f, 0.0f };
    EXPECT_EQ(a.pickIndex(allZeroWeights, 3u), 0u);
    EXPECT_EQ(a.pickIndex(static_cast<const gp::Float32*>(nullptr), 3u), 0u);

    const gp::Float32 nonFiniteWeights[3]{ Constants<gp::Float32>::infinity,
                                           2.0f,
                                           std::numeric_limits<gp::Float32>::quiet_NaN() };
    EXPECT_EQ(a.pickIndex(nonFiniteWeights, 3u), 1u);

    const gp::Int32& constChoice = a.pickElement(deck);
    EXPECT_GE(constChoice, 0);
    EXPECT_LE(constChoice, 9);
    gp::Int32& mutChoice = a.pickElement(deck);
    mutChoice += 0;

    const gp::Int32 population[8]{ 10, 20, 30, 40, 50, 60, 70, 80 };
    gp::Int32 sample[4]{};
    const gp::USize written = a.sampleWithoutReplacement(population, 8u, sample, 4u);
    ASSERT_EQ(written, 4u);
    for (gp::USize i = 1u; i < written; ++i)
    {
        EXPECT_LT(sample[i - 1u], sample[i]);
    }

    gp::Int32 clampedSample[12]{};
    EXPECT_EQ(a.sampleWithoutReplacement(population, 8u, clampedSample, 12u), 8u);
    EXPECT_EQ(a.sampleWithoutReplacement(population, 8u, clampedSample, 0u), 0u);
    EXPECT_EQ(a.sampleWithoutReplacement(static_cast<const gp::Int32*>(nullptr), 8u, sample, 4u), 0u);

    // Null and single-element edge cases for shuffle, permutation, and pickElement.
    a.shuffle(static_cast<gp::Int32*>(nullptr), 4u);
    gp::Int32 singleItem = 99;
    a.shuffle(&singleItem, 1u);
    EXPECT_EQ(singleItem, 99);
    EXPECT_EQ(a.pickElement(&singleItem, 1u), 99);

    gp::USize singlePerm = 77u;
    a.nextPermutation(&singlePerm, 1u);
    EXPECT_EQ(singlePerm, 0u);
    a.nextPermutation(static_cast<gp::USize*>(nullptr), 4u);
    a.nextPermutation(&singlePerm, 0u);
}

TEST(DynamicRandomStreamTest, MatchesEveryStaticAlgorithmAndSwitchesInPlace)
{
    const RandomAlgorithm algorithms[5] = {
        RandomAlgorithm::SplitMix64,         RandomAlgorithm::Pcg32, RandomAlgorithm::Xoshiro128PlusPlus,
        RandomAlgorithm::Xoshiro256PlusPlus, RandomAlgorithm::Sfc64,
    };

    for (const RandomAlgorithm alg: algorithms)
    {
        DynamicRandomStream dyn(alg, 777ULL, 3ULL);
        EXPECT_EQ(dyn.algorithm(), alg);
        EXPECT_EQ(dyn.outputBits(), algorithmOutputBits(alg));
        EXPECT_EQ(dyn.stateBytes(), algorithmStateBytes(alg));
        EXPECT_EQ(dyn.supportsJump(), supportsJumpAhead(alg));
        EXPECT_EQ(dyn.initialSeed(), 777ULL);
        EXPECT_EQ(dyn.streamId(), 3ULL);

        switch (alg)
        {
        case RandomAlgorithm::SplitMix64:
        {
            BasicRandomStream<SplitMix64> ref(777ULL, 3ULL);
            EXPECT_EQ(dyn.nextUInt64(), ref.nextUInt64());
            EXPECT_EQ(dyn.nextUInt32(), ref.nextUInt32());
            break;
        }
        case RandomAlgorithm::Pcg32:
        {
            BasicRandomStream<Pcg32> ref(777ULL, 3ULL);
            EXPECT_EQ(dyn.nextUInt64(), ref.nextUInt64());
            EXPECT_EQ(dyn.nextUInt32(), ref.nextUInt32());
            break;
        }
        case RandomAlgorithm::Xoshiro128PlusPlus:
        {
            BasicRandomStream<Xoshiro128PlusPlus> ref(777ULL, 3ULL);
            EXPECT_EQ(dyn.nextUInt64(), ref.nextUInt64());
            EXPECT_EQ(dyn.nextUInt32(), ref.nextUInt32());
            break;
        }
        case RandomAlgorithm::Xoshiro256PlusPlus:
        {
            BasicRandomStream<Xoshiro256PlusPlus> ref(777ULL, 3ULL);
            EXPECT_EQ(dyn.nextUInt64(), ref.nextUInt64());
            EXPECT_EQ(dyn.nextUInt32(), ref.nextUInt32());
            break;
        }
        case RandomAlgorithm::Sfc64:
        {
            BasicRandomStream<Sfc64> ref(777ULL, 3ULL);
            EXPECT_EQ(dyn.nextUInt64(), ref.nextUInt64());
            EXPECT_EQ(dyn.nextUInt32(), ref.nextUInt32());
            break;
        }
        case RandomAlgorithm::Count:
        default:
            break;
        }
    }

    DynamicRandomStream stream(RandomAlgorithm::Xoshiro256PlusPlus, 101ULL);
    EXPECT_TRUE(stream.tryJump());
    EXPECT_TRUE(stream.tryLongJump());

    stream.switchAlgorithm(RandomAlgorithm::Sfc64, 101ULL);
    EXPECT_FALSE(stream.tryJump());
    EXPECT_FALSE(stream.tryLongJump());

    stream.switchAlgorithm(RandomAlgorithm::Pcg32, 101ULL);
    const gp::UInt64 beforeReset = stream.nextUInt64();
    stream.reset();
    EXPECT_EQ(stream.nextUInt64(), beforeReset);

    stream.discard(5u);
    const DynamicRandomStream forkedA = stream.fork(1u);
    const DynamicRandomStream forkedB = stream.fork("fx_sparks");
    const DynamicRandomStream forkedC = stream.fork(gp::StringView("fx_sparks"));
    EXPECT_EQ(forkedB, forkedC);
    EXPECT_NE(forkedA, forkedB);
    EXPECT_NE(gp::hashing::hashOf(forkedA), gp::hashing::hashOf(forkedB));

    stream.seedFromString("audio_ambience", 11ULL);
    DynamicRandomStream copyByStr(RandomAlgorithm::Pcg32);
    copyByStr.seedFromString(gp::StringView("audio_ambience"), 11ULL);
    EXPECT_EQ(stream, copyByStr);

    const gp::hashing::Hash128 h128(10ULL, 20ULL);
    const gp::hashing::Hash256 h256(10ULL, 20ULL, 30ULL, 40ULL);
    stream.seedFromHash(h128);
    copyByStr.seed(10ULL, 20ULL);
    EXPECT_EQ(stream, copyByStr);
    stream.seedFromHash(h256);
    EXPECT_NE(stream, copyByStr);

    stream.switchAlgorithm(static_cast<RandomAlgorithm>(255u), 101ULL);
    EXPECT_EQ(stream.algorithm(), RandomAlgorithm::Xoshiro256PlusPlus);
    EXPECT_NE(stream, copyByStr);

    const DynamicRandomStream ent = DynamicRandomStream::fromEntropy();
    EXPECT_EQ(ent.algorithm(), RandomAlgorithm::Xoshiro256PlusPlus);
    const DynamicRandomStream entPcg = DynamicRandomStream::fromEntropy(RandomAlgorithm::Pcg32);
    EXPECT_EQ(entPcg.algorithm(), RandomAlgorithm::Pcg32);
}

}   // namespace gp::math::tests
