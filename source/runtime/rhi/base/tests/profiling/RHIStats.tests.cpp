// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "profiling/RHIStats.hpp"
#include "core/GPUMask.hpp"
#include "memory/MemoryOrder.hpp"
#include "profiling/RHICoreStats.hpp"
#include "resources/BufferDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureMemoryStats.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::profiling::tests
{

TEST(RHIStatsTests, MemoryStatsOverBudgetAndDrawCategoriesAccumulateAndLatch)
{
    resetDrawCallStats();

    RHIMemoryStats memStats{};
    EXPECT_FALSE(memStats.isValid());
    EXPECT_FALSE(memStats.isOverBudget());

    memStats.dedicatedVideoMemory = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    memStats.budgetVideoMemory = 6ULL * 1024ULL * 1024ULL * 1024ULL;
    memStats.usedVideoMemory = 7ULL * 1024ULL * 1024ULL * 1024ULL;
    EXPECT_TRUE(memStats.isValid());
    EXPECT_TRUE(memStats.isOverBudget());

    DrawStatsCategory basePassCategory(gp::Name("TestBasePass"));
    DrawStatsCategory shadowCategory(gp::Name("TestShadows"));
    EXPECT_NE(basePassCategory.getIndex(), shadowCategory.getIndex());

    GP_RHI_DRAW_CALL_STATS(GPUMask::gpu0(), &basePassCategory, 120u);
    GP_RHI_DRAW_CALL_STATS(GPUMask::gpu0(), &shadowCategory, 80u);
    GP_RHI_DRAW_CALL_INC(GPUMask::gpu0(), &basePassCategory);
    GP_RHI_DISPATCH_CALL_INC(GPUMask::gpu0());

    EXPECT_EQ(g_numDrawCallsRHI[0].load(gp::memory::MemoryOrder::Relaxed), 3);
    EXPECT_EQ(g_numPrimitivesDrawnRHI[0].load(gp::memory::MemoryOrder::Relaxed), 200);
    EXPECT_EQ(g_numDispatchCallsRHI[0].load(gp::memory::MemoryOrder::Relaxed), 1);

    DrawStatsCategory::Manager& mgr = DrawStatsCategory::getManager();
    mgr.latchFrameCounters();
    EXPECT_EQ(mgr.getTotalDisplayDraws(0u), 3u);
    EXPECT_EQ(mgr.getTotalDisplayPrimitives(0u), 200u);

    resetDrawCallStats();
}

TEST(RHIStatsTests, RHICoreResourceMemoryCountersTrackTexturesBuffersUniformsAndCommits)
{
    resetGlobalRHIResourceMemoryCounters();

    resource::TextureDesc streamingTex{};
    streamingTex.extent = resource::IntPoint(512, 512);
    streamingTex.format = PixelFormat::BC7_UNorm;
    streamingTex.flags = resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::Streamable;
    updateGlobalTextureStats(streamingTex, 262144u, true);

    resource::TextureDesc rtTex{};
    rtTex.extent = resource::IntPoint(1024, 1024);
    rtTex.format = PixelFormat::R16G16B16A16_Float;
    rtTex.flags = resource::TextureCreateFlags::RenderTargetable;
    updateGlobalTextureStats(rtTex, 8388608u, true);

    resource::BufferDesc vbDesc(65536u, 32u, resource::BufferUsageFlags::VertexBuffer);
    updateGlobalBufferStats(vbDesc, 65536, true);

    resource::BufferDesc reservedBuf(1048576u, 4u, resource::BufferUsageFlags::ReservedResource);
    updateGlobalBufferStats(reservedBuf, 1048576, true);
    updateReservedResourceStatsOnCommit(262144, true, true);

    updateGlobalUniformBufferStats(4096, true);

    const RHIResourceMemoryCountersSnapshot snap = getGlobalRHIResourceMemoryCounters();
    EXPECT_EQ(snap.streamingTextureBytes, 262144);
    EXPECT_EQ(snap.texture2DBytes, 262144);
    EXPECT_EQ(snap.renderTarget2DBytes, 8388608);
    EXPECT_EQ(snap.vertexBufferBytes, 65536);
    EXPECT_EQ(snap.reservedUncommittedBufferBytes, 1048576 - 262144);
    EXPECT_EQ(snap.reservedCommittedBufferBytes, 262144);
    EXPECT_EQ(snap.uniformBufferBytes, 4096);

    resource::TextureMemoryStats baseline{};
    fillBaselineTextureMemoryStats(baseline);
    EXPECT_EQ(baseline.streamingMemorySize, 262144);

    // Deallocate and verify counters return to zero.
    updateReservedResourceStatsOnCommit(262144, true, false);
    updateGlobalBufferStats(reservedBuf, 1048576, false);
    updateGlobalBufferStats(vbDesc, 65536, false);
    updateGlobalTextureStats(rtTex, 8388608u, false);
    updateGlobalTextureStats(streamingTex, 262144u, false);
    updateGlobalUniformBufferStats(4096, false);

    const RHIResourceMemoryCountersSnapshot afterFree = getGlobalRHIResourceMemoryCounters();
    EXPECT_EQ(afterFree, RHIResourceMemoryCountersSnapshot{});
}

}   // namespace gp::rhi::profiling::tests
