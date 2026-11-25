// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierConversion.hpp"
#include "barriers/VulkanBarrierContext.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierEnums.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanTransitionData.hpp"
#include "containers/arrays/Vector.hpp"
#include "CoreMinimal.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "VulkanBarrierTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

// Fake VkImage and VkBuffer handles used in resolver mappings. Never dereferenced.
const VkImage kFakeImage0 = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x1000u));
const VkImage kFakeImage1 = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x2000u));
const VkBuffer kFakeBuffer0 = reinterpret_cast<VkBuffer>(static_cast<gp::UIntPtr>(0x3000u));
const VkBuffer kFakeBuffer1 = reinterpret_cast<VkBuffer>(static_cast<gp::UIntPtr>(0x4000u));

// Fake resource pointers used in TransitionInfo entries. Never dereferenced: ResourceState only reads
// accessBefore/accessAfter when they are both explicit (non-Unknown), and the DummyComputeContext stubs never
// dereference them either.
resource::Buffer* const kFakeBufferPtr0 = reinterpret_cast<resource::Buffer*>(static_cast<gp::UIntPtr>(0x5000u));
resource::Buffer* const kFakeBufferPtr1 = reinterpret_cast<resource::Buffer*>(static_cast<gp::UIntPtr>(0x6000u));

// Builds a VulkanBarrierContext wired to the supplied fakes. The device desc is set up with separate graphics (0)
// and compute (1) queue families. All fields that are not explicitly set stay at their default values.
VulkanBarrierContext makeContext(
    DummyComputeContext& ctx,
    const FakeResolver& resolver,
    RecordingRecorder& recorder,
    const sync::PipelineStage executingPipeline,
    const VulkanBarrierSettings& settings = VulkanBarrierSettings{}
) noexcept
{
    VulkanBarrierDeviceDesc deviceDesc{};
    deviceDesc.supportsParallelRendering = true;
    deviceDesc.graphicsQueueFamilyIndex = 0u;
    deviceDesc.computeQueueFamilyIndex = 1u;
    deviceDesc.needsQueueFamilyOwnershipTransfers = true;
    deviceDesc.hasAsyncComputeQueue = true;
    deviceDesc.computeSupportedStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    deviceDesc.computeSupportedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                                        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

    return VulkanBarrierContext(
        ctx, resolver, recorder, executingPipeline, deviceDesc, VulkanBarrierTranslationDesc{}, settings
    );
}

// Builds a same-queue (Graphics→Graphics) VulkanTransitionData with one buffer transition.
VulkanTransitionData makeSingleQueueBufferData(
    resource::Buffer* const bufferPtr, const sync::ResourceAccess accessBefore, const sync::ResourceAccess accessAfter
) noexcept
{
    sync::TransitionInfo info{};
    info.buffer = bufferPtr;
    info.type = sync::TransitionResourceType::Buffer;
    info.accessBefore = accessBefore;
    info.accessAfter = accessAfter;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(info);
    return data;
}

}   // namespace

// convertTransitionToBarriers (sync2): empty transitionInfos produces no barriers in either array.
TEST(VulkanBarrierConversionTest, EmptyTransitionData_Sync2_ProducesNoBarriers)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_TRUE(out.memoryBarriers.isEmpty());
    EXPECT_TRUE(out.bufferBarriers.isEmpty());
    EXPECT_TRUE(out.imageBarriers.isEmpty());
}

// convertTransitionToBarriers: null resource in TransitionInfo is silently skipped.
TEST(VulkanBarrierConversionTest, NullResource_IsSkipped)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    sync::TransitionInfo info{};
    info.resource = nullptr;
    info.type = sync::TransitionResourceType::Buffer;
    info.accessBefore = sync::ResourceAccess::UAVCompute;
    info.accessAfter = sync::ResourceAccess::SRVCompute;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(info);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_TRUE(out.memoryBarriers.isEmpty());
    EXPECT_TRUE(out.bufferBarriers.isEmpty());
    EXPECT_TRUE(out.imageBarriers.isEmpty());
}

// convertTransitionToBarriers: Discard as accessAfter produces no barrier.
TEST(VulkanBarrierConversionTest, DiscardDestination_IsSkipped)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr0), makeBufferBarrierResource(kFakeBuffer0));
    RecordingRecorder recorder{};
    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    const VulkanTransitionData data =
        makeSingleQueueBufferData(kFakeBufferPtr0, sync::ResourceAccess::UAVCompute, sync::ResourceAccess::Discard);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_TRUE(out.memoryBarriers.isEmpty());
    EXPECT_TRUE(out.bufferBarriers.isEmpty());
    EXPECT_TRUE(out.imageBarriers.isEmpty());
}

// Buffer, same queue, useMemoryBarrierOpt=true: converter collapses the barrier into one memory barrier (sync2).
TEST(VulkanBarrierConversionTest, BufferSameQueue_MemOptOn_ProducesMemoryBarrier_Sync2)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr0), makeBufferBarrierResource(kFakeBuffer0));
    RecordingRecorder recorder{};

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = true;

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, settings);

    // UAVCompute -> SRVCompute: write-to-read, same queue.
    const VulkanTransitionData data =
        makeSingleQueueBufferData(kFakeBufferPtr0, sync::ResourceAccess::UAVCompute, sync::ResourceAccess::SRVCompute);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_EQ(out.memoryBarriers.size(), 1u);
    EXPECT_TRUE(out.bufferBarriers.isEmpty());
    EXPECT_TRUE(out.imageBarriers.isEmpty());
}

// Buffer, same queue, useMemoryBarrierOpt=false: converter emits a per-buffer barrier (sync2).
TEST(VulkanBarrierConversionTest, BufferSameQueue_MemOptOff_ProducesBufferBarrier_Sync2)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    const VulkanBarrierResource bufRes = makeBufferBarrierResource(kFakeBuffer0, 256u);
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr0), bufRes);
    RecordingRecorder recorder{};

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = false;

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, settings);

    const VulkanTransitionData data =
        makeSingleQueueBufferData(kFakeBufferPtr0, sync::ResourceAccess::UAVCompute, sync::ResourceAccess::SRVCompute);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_TRUE(out.memoryBarriers.isEmpty());
    ASSERT_EQ(out.bufferBarriers.size(), 1u);
    EXPECT_EQ(out.bufferBarriers[0u].buffer, kFakeBuffer0);
    EXPECT_EQ(out.bufferBarriers[0u].size, 256u);
    EXPECT_TRUE(out.imageBarriers.isEmpty());
}

// Buffer, cross queue (Graphics -> AsyncCompute): converter always emits a buffer barrier (sync2).
TEST(VulkanBarrierConversionTest, BufferCrossQueue_ProducesBufferBarrier_Sync2)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr0), makeBufferBarrierResource(kFakeBuffer0, 128u));
    RecordingRecorder recorder{};

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    sync::TransitionInfo info{};
    info.buffer = kFakeBufferPtr0;
    info.type = sync::TransitionResourceType::Buffer;
    info.accessBefore = sync::ResourceAccess::UAVCompute;
    info.accessAfter = sync::ResourceAccess::SRVCompute;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::AsyncCompute;
    data.transitionInfos.pushBack(info);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    // Cross-queue with memoryBarrierOpt enabled still emits a buffer barrier because srcPipelines != dstPipelines.
    EXPECT_TRUE(out.memoryBarriers.isEmpty());
    ASSERT_EQ(out.bufferBarriers.size(), 1u);
    EXPECT_EQ(out.bufferBarriers[0u].buffer, kFakeBuffer0);
}

// Buffer, same queue, read-only source access: memory barrier carries zero access masks.
TEST(VulkanBarrierConversionTest, BufferSameQueue_ReadOnlySource_MemoryBarrierHasZeroAccessMasks)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr0), makeBufferBarrierResource(kFakeBuffer0));
    RecordingRecorder recorder{};

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = true;

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, settings);

    // SRVCompute (read-only) -> CopyDest: source is read-only so no memory availability is needed.
    const VulkanTransitionData data =
        makeSingleQueueBufferData(kFakeBufferPtr0, sync::ResourceAccess::SRVCompute, sync::ResourceAccess::CopyDest);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    ASSERT_EQ(out.memoryBarriers.size(), 1u);
    EXPECT_EQ(out.memoryBarriers[0u].srcAccessMask, 0u);
    EXPECT_EQ(out.memoryBarriers[0u].dstAccessMask, 0u);
    // Stage masks are still set even when access masks are zero.
    EXPECT_NE(out.memoryBarriers[0u].srcStageMask, 0u);
    EXPECT_NE(out.memoryBarriers[0u].dstStageMask, 0u);
}

// BVH acceleration structure: converter routes it through the memory barrier path.
TEST(VulkanBarrierConversionTest, BVHTransition_ProducesMemoryBarrier_Sync2)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = true;

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, settings);

    // BVH: getViewableResource returns nullptr; ResourceState works without a resource pointer here.
    resource::RayTracingAccelerationStructure* const fakeBVH =
        reinterpret_cast<resource::RayTracingAccelerationStructure*>(static_cast<gp::UIntPtr>(0x7000u));

    sync::TransitionInfo info{};
    info.bvh = fakeBVH;
    info.type = sync::TransitionResourceType::BVH;
    info.accessBefore = sync::ResourceAccess::BVHWrite;
    info.accessAfter = sync::ResourceAccess::BVHRead;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(info);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_EQ(out.memoryBarriers.size(), 1u);
    EXPECT_TRUE(out.bufferBarriers.isEmpty());
    EXPECT_TRUE(out.imageBarriers.isEmpty());
}

// Color texture, same queue, same layout: folded into a memory barrier when useMemoryBarrierOpt is on (sync2).
TEST(VulkanBarrierConversionTest, ColorTexture_SameQueue_SameLayout_ProducesMemoryBarrier_Sync2)
{
    ScopedBypassDeleteCheck bypass{};
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};

    resource::TextureCreateDesc texDesc{};
    DummyTexture tex(texDesc);

    resolver.addMapping(static_cast<const void*>(&tex), makeColorBarrierResource(kFakeImage0));

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = true;

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, settings);

    // UAVCompute -> UAVCompute: both map to VK_IMAGE_LAYOUT_GENERAL; same layout.
    sync::TransitionInfo info{};
    info.texture = &tex;
    info.type = sync::TransitionResourceType::Texture;
    info.accessBefore = sync::ResourceAccess::UAVCompute;
    info.accessAfter = sync::ResourceAccess::UAVCompute;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(info);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_EQ(out.memoryBarriers.size(), 1u);
    EXPECT_TRUE(out.bufferBarriers.isEmpty());
    EXPECT_TRUE(out.imageBarriers.isEmpty());
}

// Color texture, same queue, different layouts: converter emits an image barrier (sync2).
TEST(VulkanBarrierConversionTest, ColorTexture_SameQueue_DifferentLayout_ProducesImageBarrier_Sync2)
{
    ScopedBypassDeleteCheck bypass{};
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};

    resource::TextureCreateDesc texDesc{};
    DummyTexture tex(texDesc);

    resolver.addMapping(static_cast<const void*>(&tex), makeColorBarrierResource(kFakeImage0));

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    // CopyDest -> SRVGraphicsPixel: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL; different layouts.
    sync::TransitionInfo info{};
    info.texture = &tex;
    info.type = sync::TransitionResourceType::Texture;
    info.accessBefore = sync::ResourceAccess::CopyDest;
    info.accessAfter = sync::ResourceAccess::SRVGraphicsPixel;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(info);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_TRUE(out.memoryBarriers.isEmpty());
    EXPECT_TRUE(out.bufferBarriers.isEmpty());
    ASSERT_EQ(out.imageBarriers.size(), 1u);
    EXPECT_EQ(out.imageBarriers[0u].image, kFakeImage0);
    EXPECT_NE(out.imageBarriers[0u].oldLayout, out.imageBarriers[0u].newLayout);
}

// Image barrier: all-subresources range produces VK_REMAINING_MIP_LEVELS and VK_REMAINING_ARRAY_LAYERS.
TEST(VulkanBarrierConversionTest, ImageBarrier_AllSubresources_UsesRemainingCounts)
{
    ScopedBypassDeleteCheck bypass{};
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};

    resource::TextureCreateDesc texDesc{};
    DummyTexture tex(texDesc);

    resolver.addMapping(static_cast<const void*>(&tex), makeColorBarrierResource(kFakeImage0));

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    // All mips and all array layers (kAllSubresources sentinel).
    sync::TransitionInfo info{};
    info.texture = &tex;
    info.type = sync::TransitionResourceType::Texture;
    info.accessBefore = sync::ResourceAccess::CopyDest;
    info.accessAfter = sync::ResourceAccess::SRVGraphicsPixel;
    // mipIndex, arraySlice default to kAllSubresources.

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(info);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    ASSERT_EQ(out.imageBarriers.size(), 1u);
    EXPECT_EQ(out.imageBarriers[0u].subresourceRange.baseMipLevel, 0u);
    EXPECT_EQ(out.imageBarriers[0u].subresourceRange.levelCount, VK_REMAINING_MIP_LEVELS);
    EXPECT_EQ(out.imageBarriers[0u].subresourceRange.baseArrayLayer, 0u);
    EXPECT_EQ(out.imageBarriers[0u].subresourceRange.layerCount, VK_REMAINING_ARRAY_LAYERS);
}

// Image barrier: specific mip and array slice produces base + count of 1.
TEST(VulkanBarrierConversionTest, ImageBarrier_SpecificMipAndSlice_UsesOneCounts)
{
    ScopedBypassDeleteCheck bypass{};
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};

    resource::TextureCreateDesc texDesc{};
    DummyTexture tex(texDesc);

    resolver.addMapping(static_cast<const void*>(&tex), makeColorBarrierResource(kFakeImage0));

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    sync::TransitionInfo info(
        &tex,
        sync::ResourceAccess::CopyDest,
        sync::ResourceAccess::SRVGraphicsPixel,
        sync::ResourceTransitionFlags::None,
        2u,                                        // mipIndex
        3u,                                        // arraySlice
        sync::SubresourceRange::kAllSubresources   // planeSlice = all
    );

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(info);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    ASSERT_EQ(out.imageBarriers.size(), 1u);
    EXPECT_EQ(out.imageBarriers[0u].subresourceRange.baseMipLevel, 2u);
    EXPECT_EQ(out.imageBarriers[0u].subresourceRange.levelCount, 1u);
    EXPECT_EQ(out.imageBarriers[0u].subresourceRange.baseArrayLayer, 3u);
    EXPECT_EQ(out.imageBarriers[0u].subresourceRange.layerCount, 1u);
}

// Sync2 image barrier carries per-barrier stage masks.
TEST(VulkanBarrierConversionTest, Sync2ImageBarrier_SetsPerBarrierStageMasks)
{
    ScopedBypassDeleteCheck bypass{};
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};

    resource::TextureCreateDesc texDesc{};
    DummyTexture tex(texDesc);

    resolver.addMapping(static_cast<const void*>(&tex), makeColorBarrierResource(kFakeImage0));

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    sync::TransitionInfo info{};
    info.texture = &tex;
    info.type = sync::TransitionResourceType::Texture;
    info.accessBefore = sync::ResourceAccess::CopyDest;
    info.accessAfter = sync::ResourceAccess::SRVGraphicsPixel;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(info);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    ASSERT_EQ(out.imageBarriers.size(), 1u);
    EXPECT_NE(out.imageBarriers[0u].srcStageMask, 0u);
    EXPECT_NE(out.imageBarriers[0u].dstStageMask, 0u);
}

// Legacy overload: stage masks are merged into the shared srcStageMask/dstStageMask fields, not per-barrier.
TEST(VulkanBarrierConversionTest, LegacyBuffer_MergesStageMasksIntoSharedFields)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr0), makeBufferBarrierResource(kFakeBuffer0));
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr1), makeBufferBarrierResource(kFakeBuffer1));
    RecordingRecorder recorder{};

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = false;

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, settings);

    sync::TransitionInfo infoA{};
    infoA.buffer = kFakeBufferPtr0;
    infoA.type = sync::TransitionResourceType::Buffer;
    infoA.accessBefore = sync::ResourceAccess::UAVCompute;
    infoA.accessAfter = sync::ResourceAccess::SRVCompute;

    sync::TransitionInfo infoB{};
    infoB.buffer = kFakeBufferPtr1;
    infoB.type = sync::TransitionResourceType::Buffer;
    infoB.accessBefore = sync::ResourceAccess::CopyDest;
    infoB.accessAfter = sync::ResourceAccess::SRVGraphicsPixel;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(infoA);
    data.transitionInfos.pushBack(infoB);

    VulkanLegacyBarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    ASSERT_EQ(out.bufferBarriers.size(), 2u);
    // Both barriers' stage bits must be unioned into the shared stage mask fields.
    EXPECT_NE(out.srcStageMask, 0u);
    EXPECT_NE(out.dstStageMask, 0u);
}

// Two same-queue buffers with memoryBarrierOpt=true are merged into a single memory barrier.
TEST(VulkanBarrierConversionTest, TwoBuffers_SameQueue_MemOptOn_MergesIntoOneMemoryBarrier)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr0), makeBufferBarrierResource(kFakeBuffer0));
    resolver.addMapping(static_cast<const void*>(kFakeBufferPtr1), makeBufferBarrierResource(kFakeBuffer1));
    RecordingRecorder recorder{};

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = true;

    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, settings);

    sync::TransitionInfo infoA{};
    infoA.buffer = kFakeBufferPtr0;
    infoA.type = sync::TransitionResourceType::Buffer;
    infoA.accessBefore = sync::ResourceAccess::UAVCompute;
    infoA.accessAfter = sync::ResourceAccess::SRVCompute;

    sync::TransitionInfo infoB{};
    infoB.buffer = kFakeBufferPtr1;
    infoB.type = sync::TransitionResourceType::Buffer;
    infoB.accessBefore = sync::ResourceAccess::CopyDest;
    infoB.accessAfter = sync::ResourceAccess::SRVGraphicsPixel;

    VulkanTransitionData data{};
    data.srcPipelines = sync::PipelineStage::Graphics;
    data.dstPipelines = sync::PipelineStage::Graphics;
    data.transitionInfos.pushBack(infoA);
    data.transitionInfos.pushBack(infoB);

    VulkanSync2BarrierArrays out{};
    convertTransitionToBarriers(context, data, out);

    EXPECT_EQ(out.memoryBarriers.size(), 1u);
    EXPECT_TRUE(out.bufferBarriers.isEmpty());
}

// mergePlanes: a full-aspect depth-stencil barrier stays unchanged (no partner lookup needed).
TEST(VulkanBarrierConversionTest, MergePlanes_FullAspect_NoPatchNeeded)
{
    const VkImage image = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x8000u));

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    barrier.subresourceRange.aspectMask =
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    VulkanImageBarrierExtraData extra{};
    extra.baseTexture = makeDepthStencilBarrierResource(image);

    // Both planes already in one barrier; mergePlanes should leave the aspect mask intact.
    mergePlanes(&barrier, &extra, 1);

    EXPECT_EQ(
        barrier.subresourceRange.aspectMask,
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
    );
}

// mergePlanes: a depth-only barrier finds its stencil partner and merges both into one.
TEST(VulkanBarrierConversionTest, MergePlanes_DepthOnly_PartnerFound_MergesLayouts)
{
    const VkImage image = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x9000u));

    VulkanBarrierResource depthStencilResource = makeDepthStencilBarrierResource(image);

    // Barrier[0]: depth only.
    VkImageMemoryBarrier barriers[2]{};
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].image = image;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0u;
    barriers[0].subresourceRange.baseArrayLayer = 0u;

    // Barrier[1]: stencil only (partner).
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].image = image;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0u;
    barriers[1].subresourceRange.baseArrayLayer = 0u;

    VulkanImageBarrierExtraData extras[2]{};
    extras[0].baseTexture = depthStencilResource;
    extras[0].planeSlice = 0u;
    extras[1].baseTexture = depthStencilResource;
    extras[1].planeSlice = 1u;

    mergePlanes(barriers, extras, 2);

    // The depth barrier must now carry both aspects.
    EXPECT_EQ(
        barriers[0].subresourceRange.aspectMask,
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
    );
    // The partner barrier aspect mask must be cleared (it is later dropped).
    EXPECT_EQ(barriers[1].subresourceRange.aspectMask, 0u);
    EXPECT_EQ(barriers[0].oldLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(barriers[0].newLayout, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
}

// maskSupportedAsyncFlags: removes stage and access bits the compute queue does not support.
TEST(VulkanBarrierConversionTest, MaskSupportedAsyncFlags_Buffer_RemovesUnsupportedBits)
{
    VulkanBarrierDeviceDesc device{};
    device.computeSupportedStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    device.computeSupportedAccess = VK_ACCESS_SHADER_READ_BIT;

    gp::Vector<VkBufferMemoryBarrier2> barriers{};
    VkBufferMemoryBarrier2 barrier{};
    // Set some stages that the compute queue does not support.
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.srcAccessMask =
        static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriers.pushBack(barrier);

    // Mask the source side only.
    maskSupportedAsyncFlags(device, barriers, true, false, false);

    EXPECT_EQ(barriers[0u].srcStageMask, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    EXPECT_EQ(barriers[0u].srcAccessMask, static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_READ_BIT));
    // Destination side must be untouched.
    EXPECT_EQ(barriers[0u].dstStageMask, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
}

// maskSupportedAsyncFlags with removeEmpty=true: a barrier whose stage mask becomes zero is removed.
TEST(VulkanBarrierConversionTest, MaskSupportedAsyncFlags_Buffer_RemoveEmpty_DropsZeroStageBarrier)
{
    VulkanBarrierDeviceDesc device{};
    device.computeSupportedStages = 0u;
    device.computeSupportedAccess = 0u;

    gp::Vector<VkBufferMemoryBarrier2> barriers{};
    VkBufferMemoryBarrier2 barrier{};
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barriers.pushBack(barrier);

    maskSupportedAsyncFlags(device, barriers, true, true, true);

    EXPECT_TRUE(barriers.isEmpty());
}

// patchCrossPipeTransitions: single->single release clears dstAccessMask and sets queue family indices.
TEST(VulkanBarrierConversionTest, PatchCrossPipe_SingleToSingle_Release_ClearsDstAccessAndSetsQueueFamilies)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    gp::Vector<VkImageMemoryBarrier2> barriers{};
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers.pushBack(barrier);

    // Release: executingPipeline == srcPipelines == Graphics.
    patchCrossPipeTransitions(
        barriers,
        context,
        sync::PipelineStage::Graphics,
        sync::PipelineStage::AsyncCompute,
        true   // isBeginTransition (release)
    );

    ASSERT_EQ(barriers.size(), 1u);
    EXPECT_EQ(barriers[0u].dstAccessMask, 0u);
    EXPECT_EQ(barriers[0u].srcQueueFamilyIndex, 0u);   // graphicsQueueFamilyIndex
    EXPECT_EQ(barriers[0u].dstQueueFamilyIndex, 1u);   // computeQueueFamilyIndex
    // Source access and layouts must be preserved.
    EXPECT_NE(barriers[0u].srcAccessMask, 0u);
    EXPECT_EQ(barriers[0u].oldLayout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(barriers[0u].newLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// patchCrossPipeTransitions: single->single acquire clears srcAccessMask.
TEST(VulkanBarrierConversionTest, PatchCrossPipe_SingleToSingle_Acquire_ClearsSrcAccess)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::AsyncCompute);

    gp::Vector<VkImageMemoryBarrier2> barriers{};
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barriers.pushBack(barrier);

    // Acquire: executingPipeline == dstPipelines == AsyncCompute.
    patchCrossPipeTransitions(
        barriers,
        context,
        sync::PipelineStage::Graphics,
        sync::PipelineStage::AsyncCompute,
        false   // isBeginTransition=false (acquire)
    );

    ASSERT_EQ(barriers.size(), 1u);
    EXPECT_EQ(barriers[0u].srcAccessMask, 0u);
    EXPECT_NE(barriers[0u].dstAccessMask, 0u);
    EXPECT_EQ(barriers[0u].srcQueueFamilyIndex, 0u);
    EXPECT_EQ(barriers[0u].dstQueueFamilyIndex, 1u);
}

// patchCrossPipeTransitions: All -> single (N..1) release removes the layout change from the image barrier.
TEST(VulkanBarrierConversionTest, PatchCrossPipe_AllToSingle_Release_RemovesLayoutChange)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    // Graphics executes this release half.
    const VulkanBarrierContext context = makeContext(ctx, resolver, recorder, sync::PipelineStage::Graphics);

    gp::Vector<VkImageMemoryBarrier2> barriers{};
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers.pushBack(barrier);

    // All -> AsyncCompute: srcPipelines is not single (All), dstPipelines is single (AsyncCompute).
    patchCrossPipeTransitions(
        barriers,
        context,
        sync::PipelineStage::All,
        sync::PipelineStage::AsyncCompute,
        true   // isBeginTransition (release)
    );

    ASSERT_EQ(barriers.size(), 1u);
    // For 1..N or N..1, layout change is removed from the ownership transfer barrier.
    EXPECT_EQ(barriers[0u].oldLayout, barriers[0u].newLayout);
    EXPECT_EQ(barriers[0u].dstAccessMask, 0u);
}

}   // namespace gp::rhi::vulkan::tests
