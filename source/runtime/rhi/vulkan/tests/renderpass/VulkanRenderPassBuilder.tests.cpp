// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderPassBuilder.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "RHIPixelFormat.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

using RenderPass1Builder = VulkanRenderPassBuilder<VulkanRenderPass1Traits>;
using RenderPass2Builder = VulkanRenderPassBuilder<VulkanRenderPass2Traits>;

/// @brief Builds a pipeline initializer with RGBA8 colors and an optional D24S8 target.
/// @param[in] numColors Number of color targets.
/// @param[in] numSamples Sample count.
/// @param[in] hasDepth Adds a written depth-stencil target.
/// @param[in] hint Subpass hint.
/// @return Initializer.
[[nodiscard]] pipeline::GraphicsPipelineStateInitializer makeInitializer(
    const gp::UInt32 numColors,
    const gp::UInt16 numSamples,
    const bool hasDepth,
    const pipeline::SubpassHint hint = pipeline::SubpassHint::None
) noexcept
{
    pipeline::GraphicsPipelineStateInitializer initializer;
    initializer.renderTargetsEnabled = numColors;
    for (gp::UInt32 index = 0u; index < numColors; ++index)
    {
        initializer.renderTargetFormats[index] = PixelFormat::R8G8B8A8_UNorm;
    }
    if (hasDepth)
    {
        initializer.depthStencilTargetFormat = PixelFormat::D24_UNorm_S8_UInt;
        initializer.depthStencilAccess = resource::ExclusiveDepthStencil::kDepthWriteStencilWrite;
    }
    initializer.numSamples = numSamples;
    initializer.subpassHint = hint;
    return initializer;
}

/// @brief Returns the structure type at the head of a `pNext` chain.
/// @param[in] next Chain head, or `nullptr`.
/// @return Structure type, or `VK_STRUCTURE_TYPE_MAX_ENUM` for an empty chain.
[[nodiscard]] VkStructureType getChainType(const void* const next) noexcept
{
    return (next != nullptr) ? static_cast<const VkBaseInStructure*>(next)->sType : VK_STRUCTURE_TYPE_MAX_ENUM;
}

}   // namespace

TEST(VulkanRenderPassBuilderTest, SingleSubpassWithoutHint)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(caps, makeInitializer(2u, 1u, true));
    RenderPass1Builder builder(caps);
    builder.build(layout);

    const VkRenderPassCreateInfo& createInfo = builder.getCreateInfo();
    EXPECT_EQ(createInfo.sType, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    EXPECT_EQ(createInfo.subpassCount, 1u);
    EXPECT_EQ(createInfo.dependencyCount, 0u);
    EXPECT_EQ(createInfo.pDependencies, nullptr);
    EXPECT_EQ(createInfo.attachmentCount, 3u);

    const VkSubpassDescription& subpass = builder.getSubpasses()[0];
    EXPECT_EQ(subpass.pipelineBindPoint, VK_PIPELINE_BIND_POINT_GRAPHICS);
    EXPECT_EQ(subpass.colorAttachmentCount, 2u);
    EXPECT_EQ(subpass.inputAttachmentCount, 0u);
    ASSERT_NE(subpass.pDepthStencilAttachment, nullptr);
    EXPECT_EQ(subpass.pDepthStencilAttachment->attachment, 2u);
    EXPECT_EQ(subpass.pResolveAttachments, nullptr);
}

TEST(VulkanRenderPassBuilderTest, SecondApiFillsStructureTypes)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(caps, makeInitializer(1u, 1u, true));
    RenderPass2Builder builder(caps);
    builder.build(layout);

    const VkRenderPassCreateInfo2& createInfo = builder.getCreateInfo();
    EXPECT_EQ(createInfo.sType, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2);
    ASSERT_EQ(createInfo.subpassCount, 1u);
    EXPECT_EQ(createInfo.pAttachments[0].sType, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2);

    const VkSubpassDescription2& subpass = builder.getSubpasses()[0];
    EXPECT_EQ(subpass.sType, VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2);
    EXPECT_EQ(subpass.pColorAttachments[0].sType, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2);
    EXPECT_EQ(subpass.viewMask, 0u);
}

TEST(VulkanRenderPassBuilderTest, ResolvesSitOnTheLastSubpass)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(caps, makeInitializer(2u, 4u, false));
    RenderPass1Builder builder(caps);
    builder.build(layout);

    ASSERT_EQ(builder.getCreateInfo().subpassCount, 1u);
    const VkSubpassDescription& subpass = builder.getSubpasses()[0];
    ASSERT_NE(subpass.pResolveAttachments, nullptr);
    EXPECT_EQ(subpass.pResolveAttachments[0].attachment, layout.getResolveAttachmentReferences()[0].attachment);
    EXPECT_EQ(subpass.pResolveAttachments[1].attachment, layout.getResolveAttachmentReferences()[1].attachment);
}

TEST(VulkanRenderPassBuilderTest, DepthReadSubpassReadsDepthAsInput)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(caps, makeInitializer(2u, 1u, true, pipeline::SubpassHint::DepthReadSubpass));
    RenderPass1Builder builder(caps);
    builder.build(layout);

    const VkRenderPassCreateInfo& createInfo = builder.getCreateInfo();
    ASSERT_EQ(createInfo.subpassCount, 2u);
    ASSERT_EQ(createInfo.dependencyCount, 1u);

    const VkSubpassDescription& depthRead = builder.getSubpasses()[1];
    EXPECT_EQ(depthRead.colorAttachmentCount, 1u);
    ASSERT_EQ(depthRead.inputAttachmentCount, 1u);
    EXPECT_EQ(depthRead.pInputAttachments[0].attachment, layout.getDepthAttachmentReference()->attachment);
    EXPECT_EQ(depthRead.pInputAttachments[0].layout, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
    ASSERT_NE(depthRead.pDepthStencilAttachment, nullptr);
    EXPECT_EQ(depthRead.pDepthStencilAttachment->layout, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);

    const VkSubpassDependency& dependency = createInfo.pDependencies[0];
    EXPECT_EQ(dependency.srcSubpass, 0u);
    EXPECT_EQ(dependency.dstSubpass, 1u);
    EXPECT_EQ(dependency.dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT));
    EXPECT_EQ(dependency.dependencyFlags, static_cast<VkDependencyFlags>(VK_DEPENDENCY_BY_REGION_BIT));
}

TEST(VulkanRenderPassBuilderTest, DepthReadSwapsColorZeroResolveToLastSubpass)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(caps, makeInitializer(2u, 4u, true, pipeline::SubpassHint::DepthReadSubpass));
    RenderPass1Builder builder(caps);
    builder.build(layout);

    ASSERT_EQ(builder.getCreateInfo().subpassCount, 2u);
    const VkSubpassDescription& main = builder.getSubpasses()[0];
    const VkSubpassDescription& depthRead = builder.getSubpasses()[1];
    ASSERT_NE(main.pResolveAttachments, nullptr);
    ASSERT_NE(depthRead.pResolveAttachments, nullptr);

    EXPECT_EQ(main.pResolveAttachments[0].attachment, VK_ATTACHMENT_UNUSED);
    EXPECT_EQ(main.pResolveAttachments[1].attachment, layout.getResolveAttachmentReferences()[1].attachment);
    EXPECT_EQ(depthRead.pResolveAttachments[0].attachment, layout.getResolveAttachmentReferences()[0].attachment);
}

TEST(VulkanRenderPassBuilderTest, DeferredShadingBuildsThreeSubpasses)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(
        caps, makeInitializer(3u, 1u, true, pipeline::SubpassHint::DeferredShadingSubpass)
    );
    RenderPass1Builder builder(caps);
    builder.build(layout);

    const VkRenderPassCreateInfo& createInfo = builder.getCreateInfo();
    ASSERT_EQ(createInfo.subpassCount, 3u);
    ASSERT_EQ(createInfo.dependencyCount, 2u);

    const VkSubpassDescription& gbuffer = builder.getSubpasses()[1];
    EXPECT_EQ(gbuffer.colorAttachmentCount, 3u);
    EXPECT_EQ(gbuffer.inputAttachmentCount, 1u);

    // Lighting: depth, a hole for scene color, then colors 1 and 2.
    const VkSubpassDescription& lighting = builder.getSubpasses()[2];
    EXPECT_EQ(lighting.colorAttachmentCount, 1u);
    ASSERT_EQ(lighting.inputAttachmentCount, 4u);
    EXPECT_EQ(lighting.pInputAttachments[0].attachment, layout.getDepthAttachmentReference()->attachment);
    EXPECT_EQ(lighting.pInputAttachments[1].attachment, VK_ATTACHMENT_UNUSED);
    EXPECT_EQ(lighting.pInputAttachments[2].attachment, layout.getColorAttachmentReferences()[1].attachment);
    EXPECT_EQ(lighting.pInputAttachments[3].attachment, layout.getColorAttachmentReferences()[2].attachment);
    EXPECT_EQ(lighting.pInputAttachments[2].layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EXPECT_EQ(createInfo.pDependencies[1].srcSubpass, 1u);
    EXPECT_EQ(createInfo.pDependencies[1].dstSubpass, 2u);
    EXPECT_EQ(createInfo.pDependencies[1].dstAccessMask & VK_ACCESS_SHADER_READ_BIT, 0u);
}

TEST(VulkanRenderPassBuilderTest, InputAttachmentShaderReadQuirkWidensLightingDependency)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(
        caps, makeInitializer(2u, 1u, true, pipeline::SubpassHint::DeferredShadingSubpass)
    );
    RenderPass1Builder builder(caps, true);
    builder.build(layout);

    ASSERT_EQ(builder.getCreateInfo().dependencyCount, 2u);
    EXPECT_NE(builder.getCreateInfo().pDependencies[1].dstAccessMask & VK_ACCESS_SHADER_READ_BIT, 0u);
}

TEST(VulkanRenderPassBuilderTest, CustomResolveWritesLastColorReadingColorZero)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(
        caps, makeInitializer(2u, 4u, true, pipeline::SubpassHint::CustomResolveSubpass)
    );
    RenderPass1Builder builder(caps);
    builder.build(layout);

    const VkRenderPassCreateInfo& createInfo = builder.getCreateInfo();
    ASSERT_EQ(createInfo.subpassCount, 3u);
    EXPECT_EQ(createInfo.dependencyCount, 2u);

    // The main subpass leaves the single-sampled target alone.
    EXPECT_EQ(builder.getSubpasses()[0].colorAttachmentCount, 1u);

    const VkSubpassDescription& resolve = builder.getSubpasses()[2];
    ASSERT_EQ(resolve.colorAttachmentCount, 1u);
    EXPECT_EQ(resolve.pColorAttachments[0].attachment, layout.getColorAttachmentReferences()[1].attachment);
    ASSERT_EQ(resolve.inputAttachmentCount, 2u);
    EXPECT_EQ(resolve.pInputAttachments[0].attachment, VK_ATTACHMENT_UNUSED);
    EXPECT_EQ(resolve.pInputAttachments[1].attachment, layout.getColorAttachmentReferences()[0].attachment);
    EXPECT_EQ(resolve.pResolveAttachments, nullptr);
}

TEST(VulkanRenderPassBuilderTest, FirstApiMergesDepthStencilLayouts)
{
    VulkanRenderTargetCaps caps{};
    caps.supportsParallelRendering = true;
    const VulkanRenderTargetLayout layout(caps, makeInitializer(0u, 1u, true));
    RenderPass1Builder builder(caps);
    builder.build(layout);

    const VkSubpassDescription& subpass = builder.getSubpasses()[0];
    ASSERT_NE(subpass.pDepthStencilAttachment, nullptr);
    EXPECT_EQ(subpass.pDepthStencilAttachment->layout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(builder.getCreateInfo().pAttachments[0].finalLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

TEST(VulkanRenderPassBuilderTest, SecondApiMergesLayoutsWhenSeparateLayoutsAreUnavailable)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeInitializer(0u, 1u, true);
    initializer.depthStencilAccess = resource::ExclusiveDepthStencil::kDepthReadStencilWrite;
    const VulkanRenderTargetLayout layout(caps, initializer);
    RenderPass2Builder builder(caps);
    builder.build(layout);

    const VkSubpassDescription2& subpass = builder.getSubpasses()[0];
    ASSERT_NE(subpass.pDepthStencilAttachment, nullptr);
    EXPECT_EQ(subpass.pDepthStencilAttachment->layout, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(subpass.pDepthStencilAttachment->pNext, nullptr);
    EXPECT_EQ(builder.getCreateInfo().pAttachments[0].pNext, nullptr);
}

TEST(VulkanRenderPassBuilderTest, SecondApiChainsSeparateStencilLayouts)
{
    VulkanRenderTargetCaps caps{};
    caps.supportsParallelRendering = true;
    const VulkanRenderTargetLayout layout(caps, makeInitializer(0u, 1u, true));
    RenderPass2Builder builder(caps);
    builder.build(layout);

    const VkSubpassDescription2& subpass = builder.getSubpasses()[0];
    ASSERT_NE(subpass.pDepthStencilAttachment, nullptr);
    EXPECT_EQ(subpass.pDepthStencilAttachment->layout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(
        getChainType(subpass.pDepthStencilAttachment->pNext), VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT
    );
    EXPECT_EQ(
        getChainType(builder.getCreateInfo().pAttachments[0].pNext),
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT
    );
}

TEST(VulkanRenderPassBuilderTest, DepthResolveIsChainedOnlyThroughTheSecondApi)
{
    VulkanRenderTargetCaps caps{};
    caps.supportsDepthStencilResolve = true;
    pipeline::GraphicsPipelineStateInitializer initializer = makeInitializer(0u, 4u, true);
    initializer.depthTargetStoreAction = command::RenderTargetStoreAction::MultisampleResolve;
    const VulkanRenderTargetLayout layout(caps, initializer);
    ASSERT_TRUE(layout.getHasDepthStencilResolve());

    // The first API has no depth resolve: nothing is chained on the pass either.
    RenderPass1Builder builder1(caps);
    builder1.build(layout);
    EXPECT_EQ(builder1.getCreateInfo().pNext, nullptr);

    RenderPass2Builder builder2(caps);
    builder2.build(layout);
    const VkSubpassDescription2& subpass = builder2.getSubpasses()[0];
    ASSERT_EQ(getChainType(subpass.pNext), VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE);
    const VkSubpassDescriptionDepthStencilResolve& resolve =
        *static_cast<const VkSubpassDescriptionDepthStencilResolve*>(subpass.pNext);
    EXPECT_EQ(resolve.depthResolveMode, VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
    ASSERT_NE(resolve.pDepthStencilResolveAttachment, nullptr);
    EXPECT_EQ(
        resolve.pDepthStencilResolveAttachment->attachment,
        layout.getDepthStencilResolveAttachmentReference()->attachment
    );
}

TEST(VulkanRenderPassBuilderTest, SecondApiMultiViewSetsViewAndCorrelationMasks)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeInitializer(1u, 1u, false);
    initializer.multiViewCount = 2u;
    const VulkanRenderTargetLayout layout(caps, initializer);
    RenderPass2Builder builder(caps);
    builder.build(layout);

    const VkRenderPassCreateInfo2& createInfo = builder.getCreateInfo();
    EXPECT_EQ(builder.getSubpasses()[0].viewMask, 0b11u);
    ASSERT_EQ(createInfo.correlatedViewMaskCount, 1u);
    EXPECT_EQ(createInfo.pCorrelatedViewMasks[0], 0b11u);
    EXPECT_EQ(createInfo.pNext, nullptr);
}

TEST(VulkanRenderPassBuilderTest, FirstApiMultiViewChainsMultiviewInfo)
{
    VulkanRenderTargetCaps caps{};
    caps.hasKHRMultiview = true;
    pipeline::GraphicsPipelineStateInitializer initializer = makeInitializer(1u, 1u, true);
    initializer.multiViewCount = 4u;
    initializer.subpassHint = pipeline::SubpassHint::DepthReadSubpass;
    const VulkanRenderTargetLayout layout(caps, initializer);
    RenderPass1Builder builder(caps);
    builder.build(layout);

    const VkRenderPassCreateInfo& createInfo = builder.getCreateInfo();
    ASSERT_EQ(getChainType(createInfo.pNext), VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
    const VkRenderPassMultiviewCreateInfo& multiview =
        *static_cast<const VkRenderPassMultiviewCreateInfo*>(createInfo.pNext);
    ASSERT_EQ(multiview.subpassCount, 2u);
    EXPECT_EQ(multiview.pViewMasks[0], 0b1111u);
    EXPECT_EQ(multiview.pViewMasks[1], 0b1111u);
    ASSERT_EQ(multiview.correlationMaskCount, 1u);
    EXPECT_EQ(multiview.pCorrelationMasks[0], 0b1111u);
}

TEST(VulkanRenderPassBuilderTest, ShadingRateAttachmentUsesOneStructPerSubpass)
{
    VulkanRenderTargetCaps caps{};
    caps.supportsAttachmentVariableRateShading = true;
    caps.supportsFragmentShadingRateAttachment = true;
    caps.vrsImageDataType = pipeline::VRSImageDataType::Palette;
    caps.vrsImageFormat = PixelFormat::R8_UInt;
    caps.vrsTileWidth = 16u;
    caps.vrsTileHeight = 8u;
    pipeline::GraphicsPipelineStateInitializer initializer =
        makeInitializer(1u, 1u, true, pipeline::SubpassHint::DepthReadSubpass);
    initializer.hasFragmentDensityAttachment = true;
    const VulkanRenderTargetLayout layout(caps, initializer);
    RenderPass2Builder builder(caps);
    builder.build(layout);

    ASSERT_EQ(builder.getCreateInfo().subpassCount, 2u);
    const void* const first = builder.getSubpasses()[0].pNext;
    const void* const second = builder.getSubpasses()[1].pNext;
    ASSERT_EQ(getChainType(first), VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);
    ASSERT_EQ(getChainType(second), VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);
    EXPECT_NE(first, second);

    const VkFragmentShadingRateAttachmentInfoKHR& info =
        *static_cast<const VkFragmentShadingRateAttachmentInfoKHR*>(first);
    EXPECT_EQ(info.shadingRateAttachmentTexelSize.width, 16u);
    EXPECT_EQ(info.shadingRateAttachmentTexelSize.height, 8u);
    ASSERT_NE(info.pFragmentShadingRateAttachment, nullptr);
    EXPECT_EQ(
        info.pFragmentShadingRateAttachment->attachment, layout.getFragmentDensityAttachmentReference()->attachment
    );
    // The shading rate image also needs its own attachment description.
    EXPECT_EQ(builder.getCreateInfo().attachmentCount, 3u);
}

TEST(VulkanRenderPassBuilderTest, FirstApiIgnoresShadingRateAttachment)
{
    VulkanRenderTargetCaps caps{};
    caps.supportsAttachmentVariableRateShading = true;
    caps.supportsFragmentShadingRateAttachment = true;
    caps.vrsImageDataType = pipeline::VRSImageDataType::Palette;
    caps.vrsImageFormat = PixelFormat::R8_UInt;
    pipeline::GraphicsPipelineStateInitializer initializer = makeInitializer(1u, 1u, false);
    initializer.hasFragmentDensityAttachment = true;
    const VulkanRenderTargetLayout layout(caps, initializer);
    RenderPass1Builder builder(caps);
    builder.build(layout);

    EXPECT_EQ(builder.getCreateInfo().pNext, nullptr);
    EXPECT_EQ(builder.getCreateInfo().attachmentCount, 2u);
}

TEST(VulkanRenderPassBuilderTest, FractionalShadingRateChainsFragmentDensityMap)
{
    VulkanRenderTargetCaps caps{};
    caps.hasEXTFragmentDensityMap = true;
    caps.vrsImageDataType = pipeline::VRSImageDataType::Fractional;
    caps.vrsImageFormat = PixelFormat::R8G8_UNorm;
    pipeline::GraphicsPipelineStateInitializer initializer = makeInitializer(1u, 1u, false);
    initializer.hasFragmentDensityAttachment = true;
    const VulkanRenderTargetLayout layout(caps, initializer);
    RenderPass1Builder builder(caps);
    builder.build(layout);

    const VkRenderPassCreateInfo& createInfo = builder.getCreateInfo();
    ASSERT_EQ(getChainType(createInfo.pNext), VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);
    const VkRenderPassFragmentDensityMapCreateInfoEXT& density =
        *static_cast<const VkRenderPassFragmentDensityMapCreateInfoEXT*>(createInfo.pNext);
    EXPECT_EQ(density.fragmentDensityMapAttachment.attachment, 1u);
    EXPECT_EQ(density.fragmentDensityMapAttachment.layout, VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT);
}

}   // namespace gp::rhi::vulkan::tests
