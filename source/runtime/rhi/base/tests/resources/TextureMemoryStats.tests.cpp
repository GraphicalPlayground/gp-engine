// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/TextureMemoryStats.hpp"
#include "device/RHIGlobals.hpp"
#include "resources/TextureUtils.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::resource::tests
{

namespace
{

[[nodiscard]] consteval bool verifyConstexprSubresourceDecomposition() noexcept
{
    gp::UInt32 face = 0u;
    gp::UInt32 slice = 0u;
    gp::UInt32 mip = 0u;
    // Linear index for face=3, slice=2, mip=4 with numMips=5, numSlices=4 -> 4 + 5 * (2 + 4 * 3) = 74.
    subresourceIndexToIndices(74u, 5u, 4u, face, slice, mip);
    return (face == 3u) && (slice == 2u) && (mip == 4u);
}

static_assert(verifyConstexprSubresourceDecomposition());

}   // namespace

TEST(TextureMemoryStatsTest, ValidityPoolBudgetAndWorkingMemoryCalculations)
{
    struct ScopedGlobalsReset final
    {
        ~ScopedGlobalsReset() noexcept
        {
            getRHIGlobals().reset();
        }
    } const resetGuard{};

    TextureMemoryStats stats{};
    EXPECT_FALSE(stats.areHardwareStatsValid());
    EXPECT_FALSE(stats.isUsingLimitedPoolSize());
    EXPECT_EQ(stats.computeAvailableMemorySize(), 0);
    EXPECT_EQ(stats.getTotalDeviceWorkingMemory(false), 0);
    EXPECT_EQ(stats.getTotalDeviceWorkingMemory(true), 0);

    stats.dedicatedVideoMemory = 8ll * 1024ll * 1024ll * 1024ll;
    stats.totalGraphicsMemory = 16ll * 1024ll * 1024ll * 1024ll;
    stats.texturePoolSize = 2ll * 1024ll * 1024ll * 1024ll;
    stats.streamingMemorySize = 512ll * 1024ll * 1024ll;

    EXPECT_TRUE(stats.areHardwareStatsValid());
    EXPECT_TRUE(stats.isUsingLimitedPoolSize());
    EXPECT_EQ(stats.computeAvailableMemorySize(), (2048ll - 512ll) * 1024ll * 1024ll);

    // Discrete devices report dedicatedVideoMemory; integrated devices take max(totalGraphicsMemory,
    // dedicatedVideoMemory).
    EXPECT_EQ(stats.getTotalDeviceWorkingMemory(false), stats.dedicatedVideoMemory);
    EXPECT_EQ(stats.getTotalDeviceWorkingMemory(true), stats.totalGraphicsMemory);

    getRHIGlobals().deviceIsIntegrated = false;
    EXPECT_EQ(stats.getTotalDeviceWorkingMemory(), stats.dedicatedVideoMemory);
    getRHIGlobals().deviceIsIntegrated = true;
    EXPECT_EQ(stats.getTotalDeviceWorkingMemory(), stats.totalGraphicsMemory);

    // Over-budget streaming usage clamps available pool memory to zero.
    stats.streamingMemorySize = 3ll * 1024ll * 1024ll * 1024ll;
    EXPECT_EQ(stats.computeAvailableMemorySize(), 0);

    const TextureMemoryStats copy = stats;
    EXPECT_EQ(copy, stats);
}

TEST(TextureMemoryStatsTest, SubresourceIndexToIndicesRoundTrip)
{
    constexpr gp::UInt32 kNumFaces = 6u;
    constexpr gp::UInt32 kNumSlices = 3u;
    constexpr gp::UInt32 kNumMips = 4u;

    for (gp::UInt32 face = 0u; face < kNumFaces; ++face)
    {
        for (gp::UInt32 slice = 0u; slice < kNumSlices; ++slice)
        {
            for (gp::UInt32 mip = 0u; mip < kNumMips; ++mip)
            {
                const gp::UInt32 linear = mip + (kNumMips * (slice + (kNumSlices * face)));
                gp::UInt32 outFace = 99u;
                gp::UInt32 outSlice = 99u;
                gp::UInt32 outMip = 99u;
                subresourceIndexToIndices(linear, kNumMips, kNumSlices, outFace, outSlice, outMip);
                EXPECT_EQ(outFace, face);
                EXPECT_EQ(outSlice, slice);
                EXPECT_EQ(outMip, mip);
            }
        }
    }
}

}   // namespace gp::rhi::resource::tests
