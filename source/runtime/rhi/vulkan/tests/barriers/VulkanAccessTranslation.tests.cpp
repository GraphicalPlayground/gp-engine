// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanAccessTranslation.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

VulkanBarrierTranslationDesc makeDefaultDesc() noexcept
{
    VulkanBarrierTranslationDesc desc{};
    desc.graphicsPipelineStageBits = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    desc.supportsRayTracingShaders = false;
    desc.shadingRateImageType = VulkanShadingRateImageType::NotSupported;
    desc.supportsDedicatedSRVGraphicsNonPixelAccess = false;
    return desc;
}

}   // namespace

// validateShadingRateImageType

TEST(VulkanAccessTranslationTest, ValidateShadingRateImageTypeReturnsTrueForSupportedModes)
{
    EXPECT_TRUE(validateShadingRateImageType(VulkanShadingRateImageType::Palette));
    EXPECT_TRUE(validateShadingRateImageType(VulkanShadingRateImageType::Fractional));
}

// getVkStageAndAccessFlags - exclusive switch-case states

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsUnknownCPUReadAndCopyDest)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    const TransitionResourceType kTexture = TransitionResourceType::Texture;

    // Unknown - all commands, full memory access, undefined layout
    const VulkanStageAccessLayout unknown =
        getVkStageAndAccessFlags(ResourceAccess::Unknown, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(unknown.stageFlags, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    EXPECT_EQ(unknown.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT));
    EXPECT_EQ(unknown.layout, VK_IMAGE_LAYOUT_UNDEFINED);

    // CPURead
    const VulkanStageAccessLayout cpuRead =
        getVkStageAndAccessFlags(ResourceAccess::CPURead, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(cpuRead.stageFlags, VK_PIPELINE_STAGE_HOST_BIT);
    EXPECT_EQ(cpuRead.accessFlags, VK_ACCESS_HOST_READ_BIT);
    EXPECT_EQ(cpuRead.layout, VK_IMAGE_LAYOUT_GENERAL);

    // CopyDest
    const VulkanStageAccessLayout copyDest =
        getVkStageAndAccessFlags(ResourceAccess::CopyDest, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(copyDest.stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(copyDest.accessFlags, VK_ACCESS_TRANSFER_WRITE_BIT);
    EXPECT_EQ(copyDest.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // ResolveDst falls through to same code path as RTV
    const VulkanStageAccessLayout resolveDst =
        getVkStageAndAccessFlags(ResourceAccess::ResolveDst, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(resolveDst.stageFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(
        resolveDst.accessFlags,
        static_cast<VkAccessFlags>(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
    );
    EXPECT_EQ(resolveDst.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsRTVWithAndWithoutLocalRead)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    const TransitionResourceType kTexture = TransitionResourceType::Texture;

    // RTV without local read
    const VulkanStageAccessLayout rtvStandard =
        getVkStageAndAccessFlags(ResourceAccess::RTV, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(rtvStandard.stageFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(
        rtvStandard.accessFlags,
        static_cast<VkAccessFlags>(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
    );
    EXPECT_EQ(rtvStandard.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // RTV with local read enabled
    const VulkanStageAccessLayout rtvLocalRead =
        getVkStageAndAccessFlags(ResourceAccess::RTV, kTexture, 0u, false, true, true, false, desc);
    EXPECT_EQ(
        rtvLocalRead.stageFlags,
        static_cast<VkPipelineStageFlags>(
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        )
    );
    EXPECT_EQ(
        rtvLocalRead.accessFlags,
        static_cast<VkAccessFlags>(
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
        )
    );
    EXPECT_EQ(rtvLocalRead.layout, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ);

    // RTV | ResolveDst is reduced to RTV before translation
    const VulkanStageAccessLayout rtvResolveWithLocal = getVkStageAndAccessFlags(
        ResourceAccess::RTV | ResourceAccess::ResolveDst, kTexture, 0u, false, true, true, false, desc
    );
    EXPECT_EQ(rtvResolveWithLocal.layout, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ);

    // RTV | ResolveDst without local read also reduces to plain RTV
    const VulkanStageAccessLayout rtvResolveStandard = getVkStageAndAccessFlags(
        ResourceAccess::RTV | ResourceAccess::ResolveDst, kTexture, 0u, false, true, false, false, desc
    );
    EXPECT_EQ(rtvResolveStandard.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsPresentSrcAndDst)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    const TransitionResourceType kTexture = TransitionResourceType::Texture;

    const VulkanStageAccessLayout presentSrc =
        getVkStageAndAccessFlags(ResourceAccess::Present, kTexture, 0u, false, true, false, true, desc);
    EXPECT_EQ(presentSrc.stageFlags, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(presentSrc.accessFlags, 0u);
    EXPECT_EQ(presentSrc.layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    const VulkanStageAccessLayout presentDst =
        getVkStageAndAccessFlags(ResourceAccess::Present, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(presentDst.stageFlags, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    EXPECT_EQ(presentDst.accessFlags, 0u);
    EXPECT_EQ(presentDst.layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsDiscardSrcDstAndSrvLayoutSelection)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    const TransitionResourceType kTexture = TransitionResourceType::Texture;

    // Discard as source: layout = UNDEFINED
    const VulkanStageAccessLayout discardSrc =
        getVkStageAndAccessFlags(ResourceAccess::Discard, kTexture, 0u, false, true, false, true, desc);
    EXPECT_EQ(discardSrc.layout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(discardSrc.stageFlags, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    // Discard as dst, supportsReadOnlyOptimal=true: srvLayout = SHADER_READ_ONLY_OPTIMAL
    const VulkanStageAccessLayout discardDstROpt =
        getVkStageAndAccessFlags(ResourceAccess::Discard, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(discardDstROpt.layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Discard as dst, supportsReadOnlyOptimal=false: srvLayout = GENERAL
    const VulkanStageAccessLayout discardDstGeneral =
        getVkStageAndAccessFlags(ResourceAccess::Discard, kTexture, 0u, false, false, false, false, desc);
    EXPECT_EQ(discardDstGeneral.layout, VK_IMAGE_LAYOUT_GENERAL);

    // Discard as dst on depth stencil: srvLayout = READ_ONLY_OPTIMAL
    const VulkanStageAccessLayout discardDstDepth =
        getVkStageAndAccessFlags(ResourceAccess::Discard, kTexture, 0u, true, true, false, false, desc);
    EXPECT_EQ(discardDstDepth.layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    // SRVCompute dst, supportsReadOnlyOptimal=false: also falls back to GENERAL
    const VulkanStageAccessLayout srvComputeGeneral =
        getVkStageAndAccessFlags(ResourceAccess::SRVCompute, kTexture, 0u, false, false, false, false, desc);
    EXPECT_EQ(srvComputeGeneral.layout, VK_IMAGE_LAYOUT_GENERAL);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsBVHReadWriteWithAndWithoutRayTracing)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const TransitionResourceType kBVH = TransitionResourceType::BVH;
    VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    desc.supportsRayTracingShaders = false;

    const VkPipelineStageFlags kBvhBaseStages = static_cast<VkPipelineStageFlags>(
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );

    const VulkanStageAccessLayout bvhReadNoRT =
        getVkStageAndAccessFlags(ResourceAccess::BVHRead, kBVH, 0u, false, true, false, false, desc);
    EXPECT_EQ(bvhReadNoRT.stageFlags, kBvhBaseStages);
    EXPECT_EQ(bvhReadNoRT.accessFlags, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);

    const VulkanStageAccessLayout bvhWriteNoRT =
        getVkStageAndAccessFlags(ResourceAccess::BVHWrite, kBVH, 0u, false, true, false, false, desc);
    EXPECT_EQ(bvhWriteNoRT.stageFlags, kBvhBaseStages);
    EXPECT_EQ(bvhWriteNoRT.accessFlags, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);

    desc.supportsRayTracingShaders = true;
    const VkPipelineStageFlags kBvhRTStages =
        static_cast<VkPipelineStageFlags>(kBvhBaseStages | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

    const VulkanStageAccessLayout bvhReadRT =
        getVkStageAndAccessFlags(ResourceAccess::BVHRead, kBVH, 0u, false, true, false, false, desc);
    EXPECT_EQ(bvhReadRT.stageFlags, kBvhRTStages);
    EXPECT_EQ(bvhReadRT.accessFlags, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);

    const VulkanStageAccessLayout bvhWriteRT =
        getVkStageAndAccessFlags(ResourceAccess::BVHWrite, kBVH, 0u, false, true, false, false, desc);
    EXPECT_EQ(bvhWriteRT.stageFlags, kBvhRTStages);
    EXPECT_EQ(bvhWriteRT.accessFlags, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);

    // BVHRead | SRVCompute: SRV bits are stripped, result equals pure BVHRead
    desc.supportsRayTracingShaders = false;
    const VulkanStageAccessLayout bvhReadStrippedSRV = getVkStageAndAccessFlags(
        ResourceAccess::BVHRead | ResourceAccess::SRVCompute, kBVH, 0u, false, true, false, false, desc
    );
    EXPECT_EQ(bvhReadStrippedSRV.stageFlags, kBvhBaseStages);
    EXPECT_EQ(bvhReadStrippedSRV.accessFlags, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);

    // BVHRead | SRVGraphicsPixel: same stripping
    const VulkanStageAccessLayout bvhReadStrippedSRVPixel = getVkStageAndAccessFlags(
        ResourceAccess::BVHRead | ResourceAccess::SRVGraphicsPixel, kBVH, 0u, false, true, false, false, desc
    );
    EXPECT_EQ(bvhReadStrippedSRVPixel.stageFlags, kBvhBaseStages);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsDSVWriteAndDSVRead)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    const TransitionResourceType kTexture = TransitionResourceType::Texture;

    const VkPipelineStageFlags kDSStages = static_cast<VkPipelineStageFlags>(
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
    );

    // DSVWrite on depth stencil: ATTACHMENT_OPTIMAL, full DS read-write access
    const VulkanStageAccessLayout dsvWrite =
        getVkStageAndAccessFlags(ResourceAccess::DSVWrite, kTexture, 0u, true, true, false, false, desc);
    EXPECT_EQ(dsvWrite.stageFlags, kDSStages);
    EXPECT_EQ(
        dsvWrite.accessFlags,
        static_cast<VkAccessFlags>(
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        )
    );
    EXPECT_EQ(dsvWrite.layout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);

    // DSVRead on depth stencil: READ_ONLY_OPTIMAL
    const VulkanStageAccessLayout dsvRead =
        getVkStageAndAccessFlags(ResourceAccess::DSVRead, kTexture, 0u, true, true, false, false, desc);
    EXPECT_EQ(dsvRead.stageFlags, kDSStages);
    EXPECT_EQ(dsvRead.accessFlags, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    EXPECT_EQ(dsvRead.layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    // DSVRead | SRVGraphicsPixel on depth: layout stays READ_ONLY_OPTIMAL
    const VulkanStageAccessLayout dsvReadShader = getVkStageAndAccessFlags(
        ResourceAccess::DSVRead | ResourceAccess::SRVGraphicsPixel, kTexture, 0u, true, true, false, false, desc
    );
    EXPECT_EQ(dsvReadShader.layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    EXPECT_NE(dsvReadShader.stageFlags & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u);
    EXPECT_NE(dsvReadShader.accessFlags & VK_ACCESS_SHADER_READ_BIT, 0u);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsCombinableBufferFlags)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    const TransitionResourceType kBuffer = TransitionResourceType::Buffer;

    // IndirectArgs on buffer
    const VulkanStageAccessLayout indirect =
        getVkStageAndAccessFlags(ResourceAccess::IndirectArgs, kBuffer, 0u, false, true, false, false, desc);
    EXPECT_EQ(indirect.stageFlags, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
    EXPECT_EQ(indirect.accessFlags, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

    // VertexOrIndexBuffer: index bit only
    const VulkanStageAccessLayout indexOnly = getVkStageAndAccessFlags(
        ResourceAccess::VertexOrIndexBuffer, kBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, false, true, false, false, desc
    );
    EXPECT_EQ(indexOnly.stageFlags, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
    EXPECT_NE(indexOnly.accessFlags & VK_ACCESS_INDEX_READ_BIT, 0u);
    EXPECT_EQ(indexOnly.accessFlags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 0u);

    // VertexOrIndexBuffer: vertex bit only
    const VulkanStageAccessLayout vertexOnly = getVkStageAndAccessFlags(
        ResourceAccess::VertexOrIndexBuffer, kBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, false, true, false, false, desc
    );
    EXPECT_NE(vertexOnly.accessFlags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 0u);
    EXPECT_EQ(vertexOnly.accessFlags & VK_ACCESS_INDEX_READ_BIT, 0u);

    // SRVGraphicsNonPixel with uniform, index, and vertex usage on buffer
    const VkBufferUsageFlags kAllUsage = static_cast<VkBufferUsageFlags>(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );
    const VulkanStageAccessLayout srvNonPixel = getVkStageAndAccessFlags(
        ResourceAccess::SRVGraphicsNonPixel, kBuffer, kAllUsage, false, true, false, false, desc
    );
    EXPECT_NE(srvNonPixel.stageFlags & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0u);
    EXPECT_EQ(srvNonPixel.stageFlags & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u);
    EXPECT_NE(srvNonPixel.accessFlags & VK_ACCESS_SHADER_READ_BIT, 0u);
    EXPECT_NE(srvNonPixel.accessFlags & VK_ACCESS_UNIFORM_READ_BIT, 0u);
    EXPECT_NE(srvNonPixel.accessFlags & VK_ACCESS_INDEX_READ_BIT, 0u);
    EXPECT_NE(srvNonPixel.accessFlags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 0u);

    // SRVGraphicsPixel with uniform usage on buffer
    const VulkanStageAccessLayout srvPixelUniform = getVkStageAndAccessFlags(
        ResourceAccess::SRVGraphicsPixel, kBuffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false, true, false, false, desc
    );
    EXPECT_EQ(srvPixelUniform.stageFlags, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    EXPECT_NE(srvPixelUniform.accessFlags & VK_ACCESS_SHADER_READ_BIT, 0u);
    EXPECT_NE(srvPixelUniform.accessFlags & VK_ACCESS_UNIFORM_READ_BIT, 0u);

    // SRVCompute on buffer
    const VulkanStageAccessLayout srvCompute =
        getVkStageAndAccessFlags(ResourceAccess::SRVCompute, kBuffer, 0u, false, true, false, false, desc);
    EXPECT_EQ(srvCompute.stageFlags, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    EXPECT_EQ(srvCompute.accessFlags, VK_ACCESS_SHADER_READ_BIT);

    // CopySrc on buffer: no layout concept, just TRANSFER stage/access
    const VulkanStageAccessLayout copySrcBuf =
        getVkStageAndAccessFlags(ResourceAccess::CopySrc, kBuffer, 0u, false, true, false, false, desc);
    EXPECT_EQ(copySrcBuf.stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(copySrcBuf.accessFlags, VK_ACCESS_TRANSFER_READ_BIT);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsTextureUAVAndSRV)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    const TransitionResourceType kTexture = TransitionResourceType::Texture;

    // SRVGraphicsPixel on texture: SHADER_READ_ONLY_OPTIMAL
    const VulkanStageAccessLayout srvPixel =
        getVkStageAndAccessFlags(ResourceAccess::SRVGraphicsPixel, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(srvPixel.stageFlags, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(srvPixel.accessFlags, VK_ACCESS_SHADER_READ_BIT);
    EXPECT_EQ(srvPixel.layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // SRVCompute on depth texture: srvLayout = READ_ONLY_OPTIMAL
    const VulkanStageAccessLayout srvComputeDepth =
        getVkStageAndAccessFlags(ResourceAccess::SRVCompute, kTexture, 0u, true, true, false, false, desc);
    EXPECT_EQ(srvComputeDepth.layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    // UAVGraphics: GENERAL layout, all graphics stages, shader read-write
    const VulkanStageAccessLayout uavGraphics =
        getVkStageAndAccessFlags(ResourceAccess::UAVGraphics, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(uavGraphics.stageFlags, desc.graphicsPipelineStageBits);
    EXPECT_EQ(
        uavGraphics.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
    );
    EXPECT_EQ(uavGraphics.layout, VK_IMAGE_LAYOUT_GENERAL);

    // UAVCompute: GENERAL layout, compute stage, shader read-write
    const VulkanStageAccessLayout uavCompute =
        getVkStageAndAccessFlags(ResourceAccess::UAVCompute, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(uavCompute.stageFlags, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    EXPECT_EQ(
        uavCompute.accessFlags, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
    );
    EXPECT_EQ(uavCompute.layout, VK_IMAGE_LAYOUT_GENERAL);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsCopySrcAloneVsMixedOnTexture)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const VulkanBarrierTranslationDesc desc = makeDefaultDesc();
    const TransitionResourceType kTexture = TransitionResourceType::Texture;

    // CopySrc alone on texture: dedicated TRANSFER_SRC_OPTIMAL
    const VulkanStageAccessLayout copySrcAlone =
        getVkStageAndAccessFlags(ResourceAccess::CopySrc, kTexture, 0u, false, true, false, false, desc);
    EXPECT_EQ(copySrcAlone.stageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(copySrcAlone.accessFlags, VK_ACCESS_TRANSFER_READ_BIT);
    EXPECT_EQ(copySrcAlone.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // CopySrc mixed with SRVCompute on texture: falls back to GENERAL
    const VulkanStageAccessLayout copySrcMixed = getVkStageAndAccessFlags(
        ResourceAccess::CopySrc | ResourceAccess::SRVCompute, kTexture, 0u, false, true, false, false, desc
    );
    EXPECT_EQ(copySrcMixed.layout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_NE(copySrcMixed.stageFlags & VK_PIPELINE_STAGE_TRANSFER_BIT, 0u);
    EXPECT_NE(copySrcMixed.stageFlags & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u);
    EXPECT_NE(copySrcMixed.accessFlags & VK_ACCESS_TRANSFER_READ_BIT, 0u);
    EXPECT_NE(copySrcMixed.accessFlags & VK_ACCESS_SHADER_READ_BIT, 0u);
}

TEST(VulkanAccessTranslationTest, StageAndAccessFlagsShadingRateSourcePaletteAndFractional)
{
    using sync::ResourceAccess;
    using sync::TransitionResourceType;

    const TransitionResourceType kTexture = TransitionResourceType::Texture;

    VulkanBarrierTranslationDesc paletteDesc = makeDefaultDesc();
    paletteDesc.shadingRateImageType = VulkanShadingRateImageType::Palette;

    const VulkanStageAccessLayout palette = getVkStageAndAccessFlags(
        ResourceAccess::ShadingRateSource, kTexture, 0u, false, true, false, false, paletteDesc
    );
    EXPECT_EQ(palette.stageFlags, VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);
    EXPECT_EQ(palette.accessFlags, VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR);
    EXPECT_EQ(palette.layout, VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR);

    VulkanBarrierTranslationDesc fractionalDesc = makeDefaultDesc();
    fractionalDesc.shadingRateImageType = VulkanShadingRateImageType::Fractional;

    const VulkanStageAccessLayout fractional = getVkStageAndAccessFlags(
        ResourceAccess::ShadingRateSource, kTexture, 0u, false, true, false, false, fractionalDesc
    );
    EXPECT_EQ(fractional.stageFlags, VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT);
    EXPECT_EQ(fractional.accessFlags, VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT);
    EXPECT_EQ(fractional.layout, VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT);
}

}   // namespace gp::rhi::vulkan::tests
