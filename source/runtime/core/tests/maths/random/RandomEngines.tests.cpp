// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashingTypes.hpp"
#include "hashing/HashOf.hpp"
#include "hashing/XxHash.hpp"   // IWYU pragma: keep
#include "maths/random/Pcg32.hpp"
#include "maths/random/Philox4x32.hpp"
#include "maths/random/Sfc64.hpp"
#include "maths/random/SplitMix64.hpp"
#include "maths/random/Xoshiro128.hpp"
#include "maths/random/Xoshiro256.hpp"
#include <gtest/gtest.h>

namespace gp::math::tests
{

template <typename InEngine>
constexpr bool testConstexprEngineContract() noexcept
{
    InEngine a(12345ULL, 7ULL);
    InEngine b(12345ULL, 7ULL);

    const gp::UInt64 first64 = a.nextUInt64();
    const gp::UInt64 second64 = b.nextUInt64();
    if (first64 != second64 || a != b)
    {
        return false;
    }

    const typename InEngine::StateType snapshot = a.state();
    const gp::UInt32 expected32 = a.nextUInt32();
    a.setState(snapshot);
    if (a.nextUInt32() != expected32)
    {
        return false;
    }

    a.reset();
    b.seed(12345ULL, 7ULL);
    if (a.nextWord() != b.nextWord())
    {
        return false;
    }

    InEngine c;
    c.seedFromString("biome_forest", 99ULL);
    const InEngine child = c.fork("tree_scatter");
    return child.initialSeed() != c.initialSeed();
}

static_assert(testConstexprEngineContract<SplitMix64>(), "SplitMix64 constexpr check failed.");
static_assert(testConstexprEngineContract<Pcg32>(), "Pcg32 constexpr check failed.");
static_assert(testConstexprEngineContract<Xoshiro128PlusPlus>(), "Xoshiro128PlusPlus constexpr check failed.");
static_assert(testConstexprEngineContract<Xoshiro256PlusPlus>(), "Xoshiro256PlusPlus constexpr check failed.");
static_assert(testConstexprEngineContract<Sfc64>(), "Sfc64 constexpr check failed.");

constexpr bool testConstexprPhilox() noexcept
{
    const Philox4x32::BlockType a = Philox4x32::evaluate(0x1234ULL, 5ULL, 2ULL);
    const Philox4x32::BlockType b = Philox4x32::evaluate(Philox4x32Key(0x1234ULL), Philox4x32Counter(5ULL, 2ULL));
    const gp::Array<gp::Float32, 4u> floats = Philox4x32::evaluateFloat4(0x1234ULL, 5ULL, 2ULL);
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3] && floats[0] >= 0.0f && floats[0] < 1.0f;
}

static_assert(testConstexprPhilox(), "Philox4x32 constexpr check failed.");

template <typename InEngine>
class RandomEngineTypedTest : public ::testing::Test
{};

using AllEngines = ::testing::Types<SplitMix64, Pcg32, Xoshiro128PlusPlus, Xoshiro256PlusPlus, Sfc64>;
TYPED_TEST_SUITE(RandomEngineTypedTest, AllEngines);

TYPED_TEST(RandomEngineTypedTest, SeedDeterminismStreamIsolationAndReset)
{
    TypeParam a(424242ULL);
    TypeParam b(424242ULL);
    TypeParam c(424242ULL, 1ULL);
    TypeParam zeroSeed(0ULL);

    EXPECT_EQ(a.initialSeed(), 424242ULL);
    EXPECT_EQ(a.streamId(), 0ULL);
    EXPECT_EQ(c.streamId(), 1ULL);
    EXPECT_EQ(a, b);

    gp::UInt64 zeroOrAcc = 0ULL;
    for (gp::USize i = 0u; i < 16u; ++i)
    {
        const gp::UInt64 va = a.nextUInt64();
        const gp::UInt64 vb = b.nextUInt64();
        const gp::UInt64 vc = c.nextUInt64();
        EXPECT_EQ(va, vb);
        EXPECT_NE(va, vc);
        zeroOrAcc |= zeroSeed.nextUInt64();
    }
    EXPECT_NE(zeroOrAcc, 0ULL);

    a.reset();
    b.seed(424242ULL);
    for (gp::USize i = 0u; i < 8u; ++i)
    {
        EXPECT_EQ(a.nextUInt32(), b.nextUInt32());
    }
}

TYPED_TEST(RandomEngineTypedTest, SeedFromStringAndHashes)
{
    TypeParam strA;
    TypeParam strB;
    TypeParam strSalted;

    strA.seedFromString(gp::StringView("weather_system"));
    strB.seedFromString("weather_system");
    strSalted.seedFromString(gp::String("weather_system"), 77ULL);

    EXPECT_EQ(strA.nextUInt64(), strB.nextUInt64());
    EXPECT_NE(strA.nextUInt64(), strSalted.nextUInt64());

    const gp::hashing::Hash128 h128(0x1111222233334444ULL, 0x5555666677778888ULL);
    const gp::hashing::Hash256 h256(0x1ULL, 0x2ULL, 0x3ULL, 0x4ULL);

    TypeParam fromH128;
    TypeParam fromH128Copy;
    fromH128.seedFromHash(h128);
    fromH128Copy.seed(h128.low, h128.high);
    EXPECT_EQ(fromH128, fromH128Copy);

    TypeParam fromH256;
    fromH256.seedFromHash(h256);
    EXPECT_NE(fromH256.nextUInt64(), fromH128.nextUInt64());

    TypeParam nullStr;
    TypeParam emptyStr;
    nullStr.seedFromString(static_cast<const char*>(nullptr));
    emptyStr.seedFromString("");
    EXPECT_EQ(nullStr, emptyStr);

    const TypeParam nullFork = nullStr.fork(static_cast<const char*>(nullptr));
    const TypeParam emptyFork = emptyStr.fork("");
    EXPECT_EQ(nullFork, emptyFork);
}

TYPED_TEST(RandomEngineTypedTest, DiscardStateSnapshotForkAndHashing)
{
    TypeParam stepped(98765ULL, 3ULL);
    TypeParam discarded(98765ULL, 3ULL);

    discarded.discard(0ULL);
    EXPECT_EQ(stepped, discarded);

    constexpr gp::USize kSkip = 37u;
    for (gp::USize i = 0u; i < kSkip; ++i)
    {
        (void)stepped.nextWord();
    }
    discarded.discard(kSkip);

    EXPECT_EQ(stepped, discarded);
    EXPECT_EQ(stepped.nextUInt64(), discarded.nextUInt64());

    const typename TypeParam::StateType snapshot = stepped.state();
    const gp::UInt64 w0 = stepped.nextUInt64();
    const gp::UInt64 w1 = stepped.nextUInt64();
    stepped.setState(snapshot);
    EXPECT_EQ(stepped.nextUInt64(), w0);
    EXPECT_EQ(stepped.nextUInt64(), w1);

    const typename TypeParam::StateType beforeFork = stepped.state();
    TypeParam childA = stepped.fork(1ULL);
    TypeParam childB = stepped.fork(2ULL);
    TypeParam childLabel = stepped.fork("loot_drop");
    TypeParam childLabelCStr = stepped.fork(gp::StringView("loot_drop"));

    EXPECT_EQ(stepped.state(), beforeFork);
    EXPECT_EQ(childLabel.nextUInt64(), childLabelCStr.nextUInt64());
    EXPECT_NE(childA.nextUInt64(), childB.nextUInt64());
    EXPECT_NE(childA.nextUInt64(), stepped.nextUInt64());

    const gp::UInt64 hashBefore = gp::hashing::hashOf(stepped);
    (void)stepped.nextWord();
    const gp::UInt64 hashAfter = gp::hashing::hashOf(stepped);
    EXPECT_NE(hashBefore, hashAfter);
}

TEST(SplitMix64SpecificTest, AdvanceRewindAndJumpAreExact)
{
    SplitMix64 forward(777ULL);
    SplitMix64 manual(777ULL);

    for (gp::USize i = 0u; i < 25u; ++i)
    {
        (void)manual.nextWord();
    }
    forward.advance(25);
    EXPECT_EQ(forward, manual);

    forward.advance(0);
    EXPECT_EQ(forward, manual);

    forward.advance(-25);
    EXPECT_EQ(forward, SplitMix64(777ULL));

    SplitMix64 jumped(777ULL);
    jumped.jump();
    EXPECT_NE(jumped, SplitMix64(777ULL));

    SplitMix64 longJumped(777ULL);
    longJumped.longJump();
    EXPECT_NE(longJumped, jumped);
    EXPECT_NE(longJumped, SplitMix64(777ULL));
}

TEST(Pcg32SpecificTest, AdvanceRewindJumpAndStreamIndependence)
{
    Pcg32 fast(1234ULL, 5678ULL);
    Pcg32 slow(1234ULL, 5678ULL);

    constexpr gp::UInt64 kSteps = 10000ULL;
    fast.advance(kSteps);
    for (gp::UInt64 i = 0ULL; i < kSteps; ++i)
    {
        (void)slow.nextWord();
    }
    fast.advance(0ULL);
    fast.rewind(0ULL);
    EXPECT_EQ(fast, slow);
    EXPECT_EQ(fast.nextWord(), slow.nextWord());

    fast.rewind(kSteps + 1ULL);
    EXPECT_EQ(fast, Pcg32(1234ULL, 5678ULL));

    const Pcg32 defaultStreamPcg(1234ULL, Pcg32::kDefaultStreamId);
    EXPECT_NE(defaultStreamPcg, Pcg32(1234ULL));

    Pcg32 jumped(1234ULL, 5678ULL);
    jumped.jump();
    EXPECT_NE(jumped, Pcg32(1234ULL, 5678ULL));

    Pcg32 longJumped(1234ULL, 5678ULL);
    longJumped.longJump();
    EXPECT_NE(longJumped, jumped);
}

TEST(XoshiroSpecificTest, JumpAndLongJumpProduceDistinctSubStreams)
{
    Xoshiro128PlusPlus x128a(555ULL);
    Xoshiro128PlusPlus x128b(555ULL);
    Xoshiro128PlusPlus x128c(555ULL);
    x128b.jump();
    x128c.longJump();
    EXPECT_NE(x128a.nextWord(), x128b.nextWord());
    EXPECT_NE(x128b.nextWord(), x128c.nextWord());

    Xoshiro256PlusPlus x256a(555ULL);
    Xoshiro256PlusPlus x256b(555ULL);
    Xoshiro256PlusPlus x256c(555ULL);
    x256b.jump();
    x256c.longJump();
    EXPECT_NE(x256a.nextWord(), x256b.nextWord());
    EXPECT_NE(x256b.nextWord(), x256c.nextWord());
}

TEST(Philox4x32SpecificTest, CounterEvaluationIsDeterministicAndOrderIndependent)
{
    const Philox4x32::BlockType b0 = Philox4x32::evaluate(0xCAFEBABEULL, 0ULL, 0ULL);
    const Philox4x32::BlockType b0Repeat = Philox4x32::evaluate(0xCAFEBABEULL, 0ULL, 0ULL);
    const Philox4x32::BlockType b1 = Philox4x32::evaluate(0xCAFEBABEULL, 1ULL, 0ULL);
    const Philox4x32::BlockType bPass = Philox4x32::evaluate(0xCAFEBABEULL, 0ULL, 1ULL);

    EXPECT_EQ(b0, b0Repeat);
    EXPECT_NE(b0, b1);
    EXPECT_NE(b0, bPass);

    const gp::UInt64 packed = Philox4x32::evaluate64(0xCAFEBABEULL, 0ULL, 0ULL);
    EXPECT_EQ(packed, (static_cast<gp::UInt64>(b0[0]) << 32u) | static_cast<gp::UInt64>(b0[1]));

    const gp::Array<gp::Float32, 4u> floats = Philox4x32::evaluateFloat4(0xCAFEBABEULL, 42ULL, 7ULL);
    for (gp::USize i = 0u; i < 4u; ++i)
    {
        EXPECT_GE(floats[i], 0.0f);
        EXPECT_LT(floats[i], 1.0f);
    }
}

}   // namespace gp::math::tests
