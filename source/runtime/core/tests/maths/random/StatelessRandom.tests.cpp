// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/random/StatelessRandom.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/base/FloatingPoint.hpp"
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"
#include "templates/Interval.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::math::tests
{

constexpr bool testConstexprStatelessRandom() noexcept
{
    const gp::UInt64 seedA = StatelessRandom::hashSeed("foliage_scatter");
    const gp::UInt64 seedB = StatelessRandom::hashSeed(gp::StringView("foliage_scatter"));
    if (seedA != seedB)
    {
        return false;
    }

    const gp::UInt64 childA = StatelessRandom::deriveSeed(seedA, "trees");
    const gp::UInt64 childB = StatelessRandom::deriveSeed(seedA, 42ULL);
    if (childA == childB)
    {
        return false;
    }

    const gp::UInt64 zeroSample = StatelessRandom::sampleUInt64(0ULL, 0ULL);
    const gp::UInt64 grid2A = StatelessRandom::sampleUInt64(0ULL, 0, 0);
    const gp::UInt64 grid2B = StatelessRandom::sampleUInt64(0ULL, 1, 0);
    const gp::UInt64 grid2Swap = StatelessRandom::sampleUInt64(0ULL, 0, 1);
    const gp::UInt64 grid3A = StatelessRandom::sampleUInt64(0ULL, 0, 0, 0);
    const gp::UInt64 grid3B = StatelessRandom::sampleUInt64(0ULL, 1, 2, 3);

    const gp::Float32 f0 = StatelessRandom::sampleFloat(seedA, 5ULL);
    const gp::Float64 d0 = StatelessRandom::sampleDouble(seedA, 5ULL);
    const gp::Int32 rInt = StatelessRandom::sampleRange(seedA, 5ULL, -4, 4);
    const gp::Float32 rFloat = StatelessRandom::sampleRange(seedA, 5ULL, 1.0f, 3.0f);

    return zeroSample != 0ULL && grid2A != 0ULL && grid2A != grid2B && grid2B != grid2Swap && grid3A != 0ULL &&
           grid3A != grid3B && f0 >= 0.0f && f0 < 1.0f && d0 >= 0.0 && d0 < 1.0 && rInt >= -4 && rInt <= 4 &&
           rFloat >= 1.0f && rFloat < 3.0f;
}

static_assert(testConstexprStatelessRandom(), "StatelessRandom constexpr check failed.");

TEST(StatelessRandomTest, EntropySeedProducesDistinctSeedsAcrossThreads)
{
    const gp::UInt64 s0 = makeEntropySeed();
    const gp::UInt64 s1 = makeEntropySeed();
    const gp::UInt64 s2 = makeEntropySeed();

    EXPECT_NE(s0, s1);
    EXPECT_NE(s1, s2);
    EXPECT_NE(s0, s2);

    constexpr gp::USize kThreads = 4u;
    constexpr gp::USize kPerThread = 200u;
    gp::Array<gp::Array<gp::UInt64, kPerThread>, kThreads> results{};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (gp::USize t = 0u; t < kThreads; ++t)
    {
        workers.emplace_back([t, &results]()
        {
            for (gp::USize i = 0u; i < kPerThread; ++i)
            {
                results[t][i] = makeEntropySeed();
            }
        });
    }

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    gp::UInt64 prev = results[0][0];
    for (gp::USize t = 0u; t < kThreads; ++t)
    {
        for (gp::USize i = 0u; i < kPerThread; ++i)
        {
            if (t != 0u || i != 0u)
            {
                EXPECT_NE(results[t][i], 0ULL);
                if (i > 0u)
                {
                    EXPECT_NE(results[t][i], results[t][i - 1u]);
                }
                prev ^= results[t][i];
            }
        }
    }
    EXPECT_NE(prev, 0ULL);
}

TEST(StatelessRandomTest, CoordinateAndIndexSamplingAreDeterministicAndDistinct)
{
    const gp::UInt64 worldSeed = StatelessRandom::hashSeed("world_seed_01");
    const gp::UInt64 lootSeed = StatelessRandom::deriveSeed(worldSeed, "loot");
    const gp::UInt64 weatherSeed = StatelessRandom::deriveSeed(worldSeed, gp::StringView("weather"));
    EXPECT_NE(lootSeed, weatherSeed);

    EXPECT_EQ(StatelessRandom::sampleUInt32(worldSeed, 12ULL), StatelessRandom::sampleUInt32(worldSeed, 12ULL));
    EXPECT_NE(StatelessRandom::sampleUInt32(worldSeed, 12ULL), StatelessRandom::sampleUInt32(worldSeed, 13ULL));

    EXPECT_EQ(StatelessRandom::sampleUInt32(worldSeed, -5, 19), StatelessRandom::sampleUInt32(worldSeed, -5, 19));
    EXPECT_NE(StatelessRandom::sampleUInt32(worldSeed, -5, 19), StatelessRandom::sampleUInt32(worldSeed, 19, -5));

    EXPECT_EQ(StatelessRandom::sampleUInt32(worldSeed, 1, 2, 3), StatelessRandom::sampleUInt32(worldSeed, 1, 2, 3));
    EXPECT_NE(StatelessRandom::sampleUInt32(worldSeed, 1, 2, 3), StatelessRandom::sampleUInt32(worldSeed, 3, 2, 1));

    for (gp::Int32 x = -4; x <= 4; ++x)
    {
        for (gp::Int32 y = -4; y <= 4; ++y)
        {
            const gp::Float32 f2 = StatelessRandom::sampleFloat(worldSeed, x, y);
            const gp::Float64 d2 = StatelessRandom::sampleDouble(worldSeed, x, y);
            EXPECT_GE(f2, 0.0f);
            EXPECT_LT(f2, 1.0f);
            EXPECT_GE(d2, 0.0);
            EXPECT_LT(d2, 1.0);

            const gp::Float32 f3 = StatelessRandom::sampleFloat(worldSeed, x, y, x - y);
            const gp::Float64 d3 = StatelessRandom::sampleDouble(worldSeed, x, y, x - y);
            EXPECT_GE(f3, 0.0f);
            EXPECT_LT(f3, 1.0f);
            EXPECT_GE(d3, 0.0);
            EXPECT_LT(d3, 1.0);
        }
    }
}

TEST(StatelessRandomTest, RangesBooleansPhiloxAndSpatialSamplers)
{
    const gp::UInt64 seed = 0xDEADBEEFCAFEBABEULL;

    EXPECT_FALSE(StatelessRandom::sampleBool(seed, 0ULL, 0.0f));
    EXPECT_TRUE(StatelessRandom::sampleBool(seed, 0ULL, 1.0f));

    gp::USize trueCount = 0u;
    for (gp::UInt64 i = 0ULL; i < 1000ULL; ++i)
    {
        if (StatelessRandom::sampleBool(seed, i, 0.5f))
        {
            ++trueCount;
        }
        const gp::Int32 r32 = StatelessRandom::sampleRange(seed, i, -100, 100);
        EXPECT_GE(r32, -100);
        EXPECT_LE(r32, 100);

        const gp::Int64 r64 = StatelessRandom::sampleRange(seed, i, gp::Int64{ -1000 }, gp::Int64{ 1000 });
        EXPECT_GE(r64, -1000);
        EXPECT_LE(r64, 1000);

        const gp::Float32 rf = StatelessRandom::sampleRange(seed, i, -3.0f, 7.0f);
        EXPECT_GE(rf, -3.0f);
        EXPECT_LT(rf, 7.0f);

        const gp::Float64 rd = StatelessRandom::sampleRange(seed, i, 2.0, 9.0);
        EXPECT_GE(rd, 2.0);
        EXPECT_LT(rd, 9.0);

        const gp::UInt32 u32 = StatelessRandom::sampleRange(seed, i, 10u, 20u);
        EXPECT_GE(u32, 10u);
        EXPECT_LE(u32, 20u);

        const gp::UInt64 u64 = StatelessRandom::sampleRange(seed, i, 100ULL, 200ULL);
        EXPECT_GE(u64, 100ULL);
        EXPECT_LE(u64, 200ULL);
    }
    EXPECT_GT(trueCount, 350u);
    EXPECT_LT(trueCount, 650u);

    EXPECT_EQ(StatelessRandom::sampleRange(seed, 0ULL, 5, 5), 5);
    EXPECT_FLOAT_EQ(StatelessRandom::sampleRange(seed, 0ULL, 2.5f, 2.5f), 2.5f);
    const gp::Float32 hugeRange = StatelessRandom::sampleRange(seed, 12ULL, -3.0e38f, 3.0e38f);
    EXPECT_TRUE(isFinite(hugeRange));
    EXPECT_GE(hugeRange, -3.0e38f);
    EXPECT_LT(hugeRange, 3.0e38f);

    const gp::Int32 ivInt = StatelessRandom::sampleInInterval(seed, 7ULL, gp::Interval<gp::Int32>::closed(-8, 8));
    EXPECT_GE(ivInt, -8);
    EXPECT_LE(ivInt, 8);
    const gp::Float32 ivFloat =
        StatelessRandom::sampleInInterval(seed, 7ULL, gp::Interval<gp::Float32>::halfOpen(1.0f, 4.0f));
    EXPECT_GE(ivFloat, 1.0f);
    EXPECT_LT(ivFloat, 4.0f);

    const gp::Array<gp::Float32, 4u> p4 = StatelessRandom::samplePhiloxFloat4(seed, 99ULL, 3ULL);
    for (gp::USize lane = 0u; lane < 4u; ++lane)
    {
        EXPECT_GE(p4[lane], 0.0f);
        EXPECT_LT(p4[lane], 1.0f);
    }

    const Vector3<gp::Float32> normal = Vector3<gp::Float32>(0.0f, 0.0f, 1.0f);
    for (gp::UInt64 i = 0ULL; i < 64ULL; ++i)
    {
        const Vector2<gp::Float32> c = StatelessRandom::sampleUnitCircle<gp::Float32>(seed, i);
        EXPECT_NEAR(c.length(), 1.0f, 1e-5f);

        const Vector3<gp::Float32> s = StatelessRandom::sampleUnitSphere<gp::Float32>(seed, i);
        EXPECT_NEAR(s.length(), 1.0f, 1e-5f);

        const Vector3<gp::Float32> h = StatelessRandom::sampleUnitHemisphere<gp::Float32>(seed, i, normal);
        EXPECT_NEAR(h.length(), 1.0f, 1e-5f);
        EXPECT_GE(h | normal, 0.0f);
    }
}

}   // namespace gp::math::tests
