// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "translation/VulkanResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

static_assert(textureDimensionToVkImageType(resource::TextureDimension::Texture2D) == VK_IMAGE_TYPE_2D);
static_assert(textureDimensionToVkImageType(resource::TextureDimension::Texture3D) == VK_IMAGE_TYPE_3D);
static_assert(
    textureDimensionToVkImageViewType(resource::TextureDimension::TextureCubeArray) == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
);
static_assert(sampleCountToVkSampleCountFlagBits(4u) == VK_SAMPLE_COUNT_4_BIT);
static_assert(indexStrideToVkIndexType(4u) == VK_INDEX_TYPE_UINT32);
static_assert(renderQueryTypeToVkQueryType(sync::RenderQueryType::Occlusion) == VK_QUERY_TYPE_OCCLUSION);
static_assert(
    getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) ==
    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
);
static_assert(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false) == VK_ACCESS_SHADER_READ_BIT);
static_assert(
    getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false) == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
);
static_assert(
    getDepthStencilAspectMask(sync::SubresourceRange::kAllSubresources) ==
    (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
);
static_assert(getDepthOrStencilLayout(sync::ResourceAccess::DSVWrite) == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
static_assert(
    translateDepthOrStencilAccessToVulkan(sync::ResourceAccess::DSVRead).accessFlags ==
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
);
static_assert(
    translateResourceAccessToVulkan(sync::ResourceAccess::CopyDest, VulkanAccessTranslationContext{}).layout ==
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
);

}   // namespace

TEST(VulkanResourceEnumsTest, TextureDimensionSampleCountIndexAndQueryConversions)
{
    EXPECT_EQ(textureDimensionToVkImageType(resource::TextureDimension::Texture2D), VK_IMAGE_TYPE_2D);
    EXPECT_EQ(textureDimensionToVkImageType(resource::TextureDimension::Texture2DArray), VK_IMAGE_TYPE_2D);
    EXPECT_EQ(textureDimensionToVkImageType(resource::TextureDimension::TextureCube), VK_IMAGE_TYPE_2D);
    EXPECT_EQ(textureDimensionToVkImageType(resource::TextureDimension::TextureCubeArray), VK_IMAGE_TYPE_2D);
    EXPECT_EQ(textureDimensionToVkImageType(resource::TextureDimension::Texture3D), VK_IMAGE_TYPE_3D);
    EXPECT_EQ(textureDimensionToVkImageType(static_cast<resource::TextureDimension>(255u)), VK_IMAGE_TYPE_MAX_ENUM);

    EXPECT_EQ(textureDimensionToVkImageViewType(resource::TextureDimension::Texture2D), VK_IMAGE_VIEW_TYPE_2D);
    EXPECT_EQ(
        textureDimensionToVkImageViewType(resource::TextureDimension::Texture2DArray), VK_IMAGE_VIEW_TYPE_2D_ARRAY
    );
    EXPECT_EQ(textureDimensionToVkImageViewType(resource::TextureDimension::Texture3D), VK_IMAGE_VIEW_TYPE_3D);
    EXPECT_EQ(textureDimensionToVkImageViewType(resource::TextureDimension::TextureCube), VK_IMAGE_VIEW_TYPE_CUBE);
    EXPECT_EQ(
        textureDimensionToVkImageViewType(resource::TextureDimension::TextureCubeArray), VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
    );
    EXPECT_EQ(
        textureDimensionToVkImageViewType(static_cast<resource::TextureDimension>(255u)), VK_IMAGE_VIEW_TYPE_MAX_ENUM
    );

    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(1u), VK_SAMPLE_COUNT_1_BIT);
    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(2u), VK_SAMPLE_COUNT_2_BIT);
    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(4u), VK_SAMPLE_COUNT_4_BIT);
    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(8u), VK_SAMPLE_COUNT_8_BIT);
    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(16u), VK_SAMPLE_COUNT_16_BIT);
    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(32u), VK_SAMPLE_COUNT_32_BIT);
    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(64u), VK_SAMPLE_COUNT_64_BIT);
    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(0u), VK_SAMPLE_COUNT_1_BIT);
    EXPECT_EQ(sampleCountToVkSampleCountFlagBits(3u), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_EQ(indexStrideToVkIndexType(1u), VK_INDEX_TYPE_UINT8_KHR);
    EXPECT_EQ(indexStrideToVkIndexType(2u), VK_INDEX_TYPE_UINT16);
    EXPECT_EQ(indexStrideToVkIndexType(4u), VK_INDEX_TYPE_UINT32);
    EXPECT_EQ(indexStrideToVkIndexType(8u), VK_INDEX_TYPE_MAX_ENUM);

    EXPECT_EQ(renderQueryTypeToVkQueryType(sync::RenderQueryType::Occlusion), VK_QUERY_TYPE_OCCLUSION);
    EXPECT_EQ(renderQueryTypeToVkQueryType(sync::RenderQueryType::AbsoluteTime), VK_QUERY_TYPE_TIMESTAMP);
    EXPECT_EQ(renderQueryTypeToVkQueryType(sync::RenderQueryType::Undefined), VK_QUERY_TYPE_MAX_ENUM);
}

TEST(VulkanResourceEnumsTest, MergedDepthStencilLayoutAllBranches)
{
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED), VK_IMAGE_LAYOUT_UNDEFINED
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // Already-merged depth or stencil layouts pass through unchanged.
    EXPECT_EQ(
        getMergedDepthStencilLayout(
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED
        ),
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
        ),
        VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
    );

    // Separate attachment / read-only / undefined pairs
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL),
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL),
        VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    );
    EXPECT_EQ(
        getMergedDepthStencilLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    );
}

TEST(VulkanResourceEnumsTest, InternalLayoutAccessAndStageMasksAllCases)
{
    const VkAccessFlags dsReadWrite =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    const VkPipelineStageFlags dsStages =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    const VkPipelineStageFlags dsReadStages = dsStages | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    EXPECT_EQ(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, false), VK_ACCESS_TRANSFER_READ_BIT);
    EXPECT_EQ(getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, false), VK_PIPELINE_STAGE_TRANSFER_BIT);

    EXPECT_EQ(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false), VK_ACCESS_TRANSFER_WRITE_BIT);
    EXPECT_EQ(getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false), VK_PIPELINE_STAGE_TRANSFER_BIT);

    EXPECT_EQ(
        getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    );
    EXPECT_EQ(
        getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false),
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );

    EXPECT_EQ(
        getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ, false),
        static_cast<VkAccessFlags>(
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
        )
    );
    EXPECT_EQ(
        getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ, true),
        static_cast<VkAccessFlags>(dsReadWrite | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
    );
    EXPECT_EQ(
        getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ, false),
        static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        )
    );
    EXPECT_EQ(getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ, true), dsReadStages);

    const VkImageLayout dsAttachLayouts[] = { VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                              VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                              VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                                              VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL };
    for (const VkImageLayout layout: dsAttachLayouts)
    {
        EXPECT_EQ(getVkAccessMaskForLayout(layout, true), dsReadWrite);
        EXPECT_EQ(getVkStageFlagsForLayout(layout, true), dsStages);
    }

    const VkImageLayout dsMixedLayouts[] = { VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
                                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL };
    for (const VkImageLayout layout: dsMixedLayouts)
    {
        EXPECT_EQ(
            getVkAccessMaskForLayout(layout, true), static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | dsReadWrite)
        );
        EXPECT_EQ(getVkStageFlagsForLayout(layout, true), dsReadStages);
    }

    EXPECT_EQ(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false), VK_ACCESS_SHADER_READ_BIT);
    EXPECT_EQ(
        getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );

    const VkImageLayout dsReadOnlyLayouts[] = { VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                                                VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                                                VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL };
    for (const VkImageLayout layout: dsReadOnlyLayouts)
    {
        EXPECT_EQ(
            getVkAccessMaskForLayout(layout, true),
            static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
        );
        EXPECT_EQ(getVkStageFlagsForLayout(layout, true), dsReadStages);
    }

    EXPECT_EQ(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, true), VK_ACCESS_SHADER_READ_BIT);
    EXPECT_EQ(getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, true), dsReadStages);

    EXPECT_EQ(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, false), 0u);
    EXPECT_EQ(getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, false), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    EXPECT_EQ(
        getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, false),
        VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT
    );
    EXPECT_EQ(
        getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, false),
        VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT
    );

    EXPECT_EQ(
        getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, false),
        VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR
    );
    EXPECT_EQ(
        getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR, false),
        VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR
    );

    EXPECT_EQ(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_GENERAL, false), 0u);
    EXPECT_EQ(getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_GENERAL, false), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    EXPECT_EQ(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_UNDEFINED, false), 0u);
    EXPECT_EQ(getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_UNDEFINED, false), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    EXPECT_EQ(getVkAccessMaskForLayout(VK_IMAGE_LAYOUT_MAX_ENUM, false), 0u);
    EXPECT_EQ(getVkStageFlagsForLayout(VK_IMAGE_LAYOUT_MAX_ENUM, false), 0u);
}

TEST(VulkanResourceEnumsTest, ResourceAccessTranslationExclusiveAndCombinedStates)
{
    VulkanAccessTranslationContext ctx{};
    ctx.resourceType = sync::TransitionResourceType::Texture;
    ctx.deviceGraphicsPipelineStageBits = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    // Unknown, CPURead, CopyDest, standalone ResolveDst
    const VulkanBarrierState unknownState = translateResourceAccessToVulkan(sync::ResourceAccess::Unknown, ctx);
    EXPECT_EQ(unknownState.stageFlags, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    EXPECT_EQ(
        unknownState.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)
    );
    EXPECT_EQ(unknownState.layout, VK_IMAGE_LAYOUT_UNDEFINED);

    const VulkanBarrierState cpuRead = translateResourceAccessToVulkan(sync::ResourceAccess::CPURead, ctx);
    EXPECT_EQ(cpuRead.stageFlags, VK_PIPELINE_STAGE_HOST_BIT);
    EXPECT_EQ(cpuRead.accessFlags, VK_ACCESS_HOST_READ_BIT);
    EXPECT_EQ(cpuRead.layout, VK_IMAGE_LAYOUT_GENERAL);

    const VulkanBarrierState copyDest = translateResourceAccessToVulkan(sync::ResourceAccess::CopyDest, ctx);
    EXPECT_EQ(copyDest.stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(copyDest.accessFlags, VK_ACCESS_TRANSFER_WRITE_BIT);
    EXPECT_EQ(copyDest.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    const VulkanBarrierState resolveDst = translateResourceAccessToVulkan(sync::ResourceAccess::ResolveDst, ctx);
    EXPECT_EQ(resolveDst.stageFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(resolveDst.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // RTV with and without local read
    const VulkanBarrierState rtvStandard = translateResourceAccessToVulkan(sync::ResourceAccess::RTV, ctx);
    EXPECT_EQ(rtvStandard.stageFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(
        rtvStandard.accessFlags,
        static_cast<VkAccessFlags>(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
    );
    EXPECT_EQ(rtvStandard.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    ctx.supportsLocalRead = true;
    const VulkanBarrierState rtvLocalRead =
        translateResourceAccessToVulkan(sync::ResourceAccess::RTV | sync::ResourceAccess::ResolveDst, ctx);
    EXPECT_EQ(
        rtvLocalRead.stageFlags,
        static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        )
    );
    EXPECT_EQ(rtvLocalRead.layout, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ);
    ctx.supportsLocalRead = false;

    // Present source vs destination
    ctx.isSourceState = true;
    EXPECT_EQ(
        translateResourceAccessToVulkan(sync::ResourceAccess::Present, ctx).stageFlags,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );
    ctx.isSourceState = false;
    EXPECT_EQ(
        translateResourceAccessToVulkan(sync::ResourceAccess::Present, ctx).stageFlags,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    );

    // Discard with ReadOnlyOptimal, General fallback, and depth-stencil
    ctx.isSourceState = true;
    EXPECT_EQ(translateResourceAccessToVulkan(sync::ResourceAccess::Discard, ctx).layout, VK_IMAGE_LAYOUT_UNDEFINED);
    ctx.isSourceState = false;
    EXPECT_EQ(
        translateResourceAccessToVulkan(sync::ResourceAccess::Discard, ctx).layout,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    ctx.supportsReadOnlyOptimal = false;
    EXPECT_EQ(translateResourceAccessToVulkan(sync::ResourceAccess::Discard, ctx).layout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(translateResourceAccessToVulkan(sync::ResourceAccess::SRVCompute, ctx).layout, VK_IMAGE_LAYOUT_GENERAL);
    ctx.supportsReadOnlyOptimal = true;

    // Depth-stencil read/write and Discard
    ctx.isDepthStencil = true;
    EXPECT_EQ(
        translateResourceAccessToVulkan(sync::ResourceAccess::Discard, ctx).layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(
        translateResourceAccessToVulkan(sync::ResourceAccess::DSVWrite | sync::ResourceAccess::DSVRead, ctx).layout,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
    );
    const VulkanBarrierState dsvRead =
        translateResourceAccessToVulkan(sync::ResourceAccess::DSVRead | sync::ResourceAccess::SRVGraphicsPixel, ctx);
    EXPECT_EQ(dsvRead.layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    EXPECT_EQ(dsvRead.unprocessedFlags, 0u);
    ctx.isDepthStencil = false;

    // UAVGraphics and UAVCompute
    const VulkanBarrierState uavGfx = translateResourceAccessToVulkan(sync::ResourceAccess::UAVGraphics, ctx);
    EXPECT_EQ(uavGfx.stageFlags, ctx.deviceGraphicsPipelineStageBits);
    EXPECT_EQ(uavGfx.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));
    EXPECT_EQ(uavGfx.layout, VK_IMAGE_LAYOUT_GENERAL);

    const VulkanBarrierState uavCompute = translateResourceAccessToVulkan(sync::ResourceAccess::UAVCompute, ctx);
    EXPECT_EQ(uavCompute.stageFlags, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    EXPECT_EQ(
        uavCompute.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
    );
    EXPECT_EQ(uavCompute.layout, VK_IMAGE_LAYOUT_GENERAL);

    // Texture SRV + CopySrc / ResolveSrc
    const VulkanBarrierState srvAndCopy =
        translateResourceAccessToVulkan(sync::ResourceAccess::SRVCompute | sync::ResourceAccess::CopySrc, ctx);
    EXPECT_EQ(srvAndCopy.layout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(
        srvAndCopy.stageFlags,
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT)
    );
    EXPECT_EQ(
        srvAndCopy.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT)
    );

    const VulkanBarrierState pureResolveSrc = translateResourceAccessToVulkan(sync::ResourceAccess::ResolveSrc, ctx);
    EXPECT_EQ(pureResolveSrc.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(pureResolveSrc.stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(pureResolveSrc.accessFlags, VK_ACCESS_TRANSFER_READ_BIT);

    // VRS Palette, Fractional, and NotSupported (unprocessed bit reported)
    ctx.vrsImageDataType = pipeline::VRSImageDataType::Palette;
    EXPECT_EQ(
        translateResourceAccessToVulkan(sync::ResourceAccess::ShadingRateSource, ctx).layout,
        VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR
    );
    ctx.vrsImageDataType = pipeline::VRSImageDataType::Fractional;
    EXPECT_EQ(
        translateResourceAccessToVulkan(sync::ResourceAccess::ShadingRateSource, ctx).layout,
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
    );
    ctx.vrsImageDataType = pipeline::VRSImageDataType::NotSupported;
    EXPECT_EQ(
        translateResourceAccessToVulkan(sync::ResourceAccess::ShadingRateSource, ctx).unprocessedFlags,
        static_cast<gp::UInt32>(sync::ResourceAccess::ShadingRateSource)
    );

    // Buffer usage flags: isolated SRVGraphicsNonPixel, SRVGraphicsPixel uniform, and combined SRVCompute | CopySrc
    VulkanAccessTranslationContext bufCtx{};
    bufCtx.resourceType = sync::TransitionResourceType::Buffer;
    bufCtx.bufferUsageFlags =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    const VulkanBarrierState srvNonPixelBuf =
        translateResourceAccessToVulkan(sync::ResourceAccess::SRVGraphicsNonPixel, bufCtx);
    EXPECT_EQ(
        srvNonPixelBuf.stageFlags,
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT)
    );
    EXPECT_EQ(
        srvNonPixelBuf.accessFlags,
        static_cast<VkAccessFlags>(
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INDEX_READ_BIT |
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
        )
    );

    const VulkanBarrierState srvPixelUniform =
        translateResourceAccessToVulkan(sync::ResourceAccess::SRVGraphicsPixel, bufCtx);
    EXPECT_EQ(
        srvPixelUniform.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT)
    );

    const VulkanBarrierState bufCombinedCopy = translateResourceAccessToVulkan(
        sync::ResourceAccess::IndirectArgs | sync::ResourceAccess::VertexOrIndexBuffer |
            sync::ResourceAccess::SRVCompute | sync::ResourceAccess::CopySrc,
        bufCtx
    );
    EXPECT_EQ(
        bufCombinedCopy.stageFlags,
        static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT
        )
    );
    EXPECT_EQ(
        bufCombinedCopy.accessFlags,
        static_cast<VkAccessFlags>(
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT
        )
    );
    EXPECT_EQ(bufCombinedCopy.unprocessedFlags, 0u);

    // BVHRead and BVHWrite with and without ray tracing shaders
    VulkanAccessTranslationContext rtCtx{};
    rtCtx.resourceType = sync::TransitionResourceType::BVH;
    rtCtx.supportsRayTracingShaders = false;
    const VulkanBarrierState bvhReadNoRT = translateResourceAccessToVulkan(sync::ResourceAccess::BVHRead, rtCtx);
    EXPECT_EQ(
        bvhReadNoRT.stageFlags,
        static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        )
    );
    const VulkanBarrierState bvhWriteNoRT = translateResourceAccessToVulkan(sync::ResourceAccess::BVHWrite, rtCtx);
    EXPECT_EQ(bvhWriteNoRT.accessFlags, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
    EXPECT_EQ(bvhWriteNoRT.stageFlags, bvhReadNoRT.stageFlags);

    rtCtx.supportsRayTracingShaders = true;
    const VulkanBarrierState bvhWriteRT = translateResourceAccessToVulkan(sync::ResourceAccess::BVHWrite, rtCtx);
    EXPECT_EQ(
        bvhWriteRT.stageFlags,
        static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
        )
    );

    // Unrecognized bit outside ResourceAccess::Mask is reported in unprocessedFlags.
    const sync::ResourceAccess invalidBit = static_cast<sync::ResourceAccess>(1u << 30u);
    const VulkanBarrierState invalidState = translateResourceAccessToVulkan(invalidBit, ctx);
    EXPECT_EQ(invalidState.unprocessedFlags, 1u << 30u);
}

TEST(VulkanResourceEnumsTest, DepthOrStencilPlaneAspectLayoutAndAccessTranslation)
{
    EXPECT_EQ(
        getDepthStencilAspectMask(sync::SubresourceRange::kAllSubresources),
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
    );
    EXPECT_EQ(getDepthStencilAspectMask(sync::SubresourceRange::kDepthPlaneSlice), VK_IMAGE_ASPECT_DEPTH_BIT);
    EXPECT_EQ(getDepthStencilAspectMask(sync::SubresourceRange::kStencilPlaneSlice), VK_IMAGE_ASPECT_STENCIL_BIT);
    EXPECT_EQ(getDepthStencilAspectMask(42u), 0u);

    EXPECT_EQ(getDepthOrStencilLayout(sync::ResourceAccess::Unknown), VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(getDepthOrStencilLayout(sync::ResourceAccess::Discard), VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(getDepthOrStencilLayout(sync::ResourceAccess::CopySrc), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(getDepthOrStencilLayout(sync::ResourceAccess::CopyDest), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(getDepthOrStencilLayout(sync::ResourceAccess::DSVWrite), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(getDepthOrStencilLayout(sync::ResourceAccess::DSVRead), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    EXPECT_EQ(getDepthOrStencilLayout(sync::ResourceAccess::SRVGraphicsPixel), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    const VulkanBarrierState unknownPlane = translateDepthOrStencilAccessToVulkan(sync::ResourceAccess::Unknown);
    EXPECT_EQ(unknownPlane.stageFlags, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    EXPECT_EQ(
        unknownPlane.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)
    );
    EXPECT_EQ(unknownPlane.layout, VK_IMAGE_LAYOUT_UNDEFINED);

    const VulkanBarrierState discardPlane = translateDepthOrStencilAccessToVulkan(sync::ResourceAccess::Discard);
    EXPECT_EQ(discardPlane.stageFlags, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    EXPECT_EQ(discardPlane.layout, VK_IMAGE_LAYOUT_UNDEFINED);

    const VulkanBarrierState resolveAndWrite =
        translateDepthOrStencilAccessToVulkan(sync::ResourceAccess::ResolveDst | sync::ResourceAccess::DSVWrite);
    EXPECT_EQ(
        resolveAndWrite.stageFlags,
        static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
        )
    );
    EXPECT_EQ(
        resolveAndWrite.accessFlags,
        static_cast<VkAccessFlags>(
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        )
    );
    EXPECT_EQ(resolveAndWrite.layout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(resolveAndWrite.unprocessedFlags, 0u);

    const VulkanBarrierState readAndShader = translateDepthOrStencilAccessToVulkan(
        sync::ResourceAccess::DSVRead | sync::ResourceAccess::SRVGraphics | sync::ResourceAccess::SRVCompute
    );
    EXPECT_EQ(
        readAndShader.stageFlags,
        static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        )
    );
    EXPECT_EQ(
        readAndShader.accessFlags,
        static_cast<VkAccessFlags>(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT)
    );
    EXPECT_EQ(readAndShader.layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    EXPECT_EQ(readAndShader.unprocessedFlags, 0u);

    const VulkanBarrierState uavPlane =
        translateDepthOrStencilAccessToVulkan(sync::ResourceAccess::UAVGraphics | sync::ResourceAccess::UAVCompute);
    EXPECT_EQ(
        uavPlane.stageFlags,
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
    );
    EXPECT_EQ(uavPlane.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));
    EXPECT_EQ(uavPlane.unprocessedFlags, 0u);

    const VulkanBarrierState copySrcPlane = translateDepthOrStencilAccessToVulkan(sync::ResourceAccess::CopySrc);
    EXPECT_EQ(copySrcPlane.stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(copySrcPlane.accessFlags, VK_ACCESS_TRANSFER_READ_BIT);
    EXPECT_EQ(copySrcPlane.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    const VulkanBarrierState copyDstPlane = translateDepthOrStencilAccessToVulkan(sync::ResourceAccess::CopyDest);
    EXPECT_EQ(copyDstPlane.stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(copyDstPlane.accessFlags, VK_ACCESS_TRANSFER_WRITE_BIT);
    EXPECT_EQ(copyDstPlane.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Unsupported plane flag reports unprocessedFlags.
    const VulkanBarrierState badPlane = translateDepthOrStencilAccessToVulkan(sync::ResourceAccess::Present);
    EXPECT_EQ(badPlane.unprocessedFlags, static_cast<gp::UInt32>(sync::ResourceAccess::Present));
}

}   // namespace gp::rhi::vulkan::tests
