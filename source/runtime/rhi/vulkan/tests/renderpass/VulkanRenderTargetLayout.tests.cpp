// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "commands/CommandEnums.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "RHIPixelFormat.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Builds a pipeline initializer with `count` RGBA8 color targets.
/// @param[in] count Number of color targets.
/// @param[in] numSamples Sample count.
/// @return Initializer.
[[nodiscard]] pipeline::GraphicsPipelineStateInitializer
    makeColorInitializer(const gp::UInt32 count, const gp::UInt16 numSamples = 1u) noexcept
{
    pipeline::GraphicsPipelineStateInitializer initializer;
    initializer.renderTargetsEnabled = count;
    for (gp::UInt32 index = 0u; index < count; ++index)
    {
        initializer.renderTargetFormats[index] = PixelFormat::R8G8B8A8_UNorm;
    }
    initializer.numSamples = numSamples;
    return initializer;
}

/// @brief Adds a D24S8 depth-stencil target to an initializer.
/// @param[in,out] initializer Initializer.
/// @param[in] access Depth-stencil access.
void addDepthStencil(
    pipeline::GraphicsPipelineStateInitializer& initializer, const resource::ExclusiveDepthStencil access
) noexcept
{
    initializer.depthStencilTargetFormat = PixelFormat::D24_UNorm_S8_UInt;
    initializer.depthStencilAccess = access;
}

}   // namespace

TEST(VulkanRenderTargetLayoutTest, DefaultLayoutIsEmpty)
{
    const VulkanRenderTargetLayout layout;

    EXPECT_EQ(layout.getNumAttachmentDescriptions(), 0u);
    EXPECT_EQ(layout.getNumColorAttachments(), 0u);
    EXPECT_FALSE(layout.getHasDepthStencil());
    EXPECT_FALSE(layout.getHasResolveAttachments());
    EXPECT_FALSE(layout.getIsMultiView());
    EXPECT_EQ(layout.getColorAttachmentReferences(), nullptr);
    EXPECT_EQ(layout.getDepthAttachmentReference(), nullptr);
    EXPECT_EQ(layout.getResolveAttachmentReferences(), nullptr);
}

TEST(VulkanRenderTargetLayoutTest, PipelineColorTargetsUseDontCareOpsAndColorLayout)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(caps, makeColorInitializer(2u));

    ASSERT_EQ(layout.getNumColorAttachments(), 2u);
    ASSERT_EQ(layout.getNumAttachmentDescriptions(), 2u);
    for (gp::UInt32 index = 0u; index < 2u; ++index)
    {
        const VkAttachmentDescription& description = layout.getAttachmentDescriptions()[index];
        EXPECT_EQ(description.format, VK_FORMAT_R8G8B8A8_UNORM);
        EXPECT_EQ(description.samples, VK_SAMPLE_COUNT_1_BIT);
        EXPECT_EQ(description.loadOp, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
        EXPECT_EQ(description.storeOp, VK_ATTACHMENT_STORE_OP_DONT_CARE);
        EXPECT_EQ(description.initialLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        EXPECT_EQ(description.finalLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        EXPECT_EQ(layout.getColorAttachmentReferences()[index].attachment, index);
        EXPECT_EQ(layout.getColorAttachmentReferences()[index].layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    EXPECT_FALSE(layout.getHasResolveAttachments());
    EXPECT_FALSE(layout.getHasDepthStencil());
    EXPECT_EQ(layout.getNumUsedClearValues(), 0u);
}

TEST(VulkanRenderTargetLayoutTest, UnknownFormatsAreSkipped)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(3u);
    initializer.renderTargetFormats[1] = PixelFormat::Unknown;

    const VulkanRenderTargetLayout layout(caps, initializer);

    EXPECT_EQ(layout.getNumColorAttachments(), 2u);
    EXPECT_EQ(layout.getNumAttachmentDescriptions(), 2u);
}

TEST(VulkanRenderTargetLayoutTest, MultisampledColorInterleavesItsResolve)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(caps, makeColorInitializer(2u, 4u));

    ASSERT_EQ(layout.getNumColorAttachments(), 2u);
    ASSERT_EQ(layout.getNumAttachmentDescriptions(), 4u);
    ASSERT_TRUE(layout.getHasResolveAttachments());
    ASSERT_NE(layout.getResolveAttachmentReferences(), nullptr);

    // Color 0, resolve 0, color 1, resolve 1.
    EXPECT_EQ(layout.getColorAttachmentReferences()[0].attachment, 0u);
    EXPECT_EQ(layout.getResolveAttachmentReferences()[0].attachment, 1u);
    EXPECT_EQ(layout.getColorAttachmentReferences()[1].attachment, 2u);
    EXPECT_EQ(layout.getResolveAttachmentReferences()[1].attachment, 3u);

    EXPECT_EQ(layout.getAttachmentDescriptions()[0].samples, VK_SAMPLE_COUNT_4_BIT);
    EXPECT_EQ(layout.getAttachmentDescriptions()[1].samples, VK_SAMPLE_COUNT_1_BIT);
    EXPECT_EQ(layout.getAttachmentDescriptions()[1].storeOp, VK_ATTACHMENT_STORE_OP_STORE);
    EXPECT_EQ(layout.getNumSamples(), 4u);
}

TEST(VulkanRenderTargetLayoutTest, UnusedResolveSlotsReferenceNoAttachment)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout layout(caps, makeColorInitializer(1u, 4u));

    ASSERT_NE(layout.getResolveAttachmentReferences(), nullptr);
    for (gp::UInt32 index = 1u; index < kMaxSimultaneousRenderTargets; ++index)
    {
        EXPECT_EQ(layout.getResolveAttachmentReferences()[index].attachment, VK_ATTACHMENT_UNUSED);
    }
}

TEST(VulkanRenderTargetLayoutTest, DepthWriteUsesAttachmentOptimalLayouts)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(1u);
    addDepthStencil(initializer, resource::ExclusiveDepthStencil::kDepthWriteStencilWrite);

    const VulkanRenderTargetLayout layout(caps, initializer);

    ASSERT_TRUE(layout.getHasDepthStencil());
    ASSERT_NE(layout.getDepthAttachmentReference(), nullptr);
    ASSERT_NE(layout.getStencilAttachmentReference(), nullptr);
    EXPECT_EQ(layout.getNumAttachmentDescriptions(), 2u);
    EXPECT_EQ(layout.getDepthAttachmentReference()->attachment, 1u);
    EXPECT_EQ(layout.getDepthAttachmentReference()->layout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(layout.getStencilAttachmentReference()->stencilLayout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(layout.getAttachmentDescriptions()[1].format, VK_FORMAT_D24_UNORM_S8_UINT);
}

TEST(VulkanRenderTargetLayoutTest, DepthReadUsesReadOnlyLayoutPerAspect)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(0u);
    addDepthStencil(initializer, resource::ExclusiveDepthStencil::kDepthReadStencilWrite);

    const VulkanRenderTargetLayout layout(caps, initializer);

    ASSERT_TRUE(layout.getHasDepthStencil());
    EXPECT_EQ(layout.getDepthAttachmentReference()->attachment, 0u);
    EXPECT_EQ(layout.getDepthAttachmentReference()->layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    EXPECT_EQ(layout.getStencilAttachmentReference()->stencilLayout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(layout.getStencilDescription()->stencilFinalLayout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
}

TEST(VulkanRenderTargetLayoutTest, DepthClearReportsClearValues)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(1u);
    addDepthStencil(initializer, resource::ExclusiveDepthStencil::kDepthWriteStencilWrite);
    initializer.depthTargetLoadAction = command::RenderTargetLoadAction::Clear;

    const VulkanRenderTargetLayout layout(caps, initializer);

    EXPECT_EQ(layout.getAttachmentDescriptions()[1].loadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
    EXPECT_EQ(layout.getNumUsedClearValues(), layout.getNumAttachmentDescriptions());
}

TEST(VulkanRenderTargetLayoutTest, DepthResolveNeedsCapsAndMultisampling)
{
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(0u, 4u);
    addDepthStencil(initializer, resource::ExclusiveDepthStencil::kDepthWriteStencilWrite);
    initializer.depthTargetStoreAction = command::RenderTargetStoreAction::MultisampleResolve;

    VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout unsupported(caps, initializer);
    EXPECT_FALSE(unsupported.getHasDepthStencilResolve());

    caps.supportsDepthStencilResolve = true;
    const VulkanRenderTargetLayout supported(caps, initializer);
    ASSERT_TRUE(supported.getHasDepthStencilResolve());
    ASSERT_NE(supported.getDepthStencilResolveAttachmentReference(), nullptr);
    EXPECT_EQ(supported.getDepthStencilResolveAttachmentReference()->attachment, 1u);
    EXPECT_EQ(supported.getNumAttachmentDescriptions(), 2u);
}

TEST(VulkanRenderTargetLayoutTest, CustomResolveWritesLastColorSingleSampled)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(2u, 4u);
    addDepthStencil(initializer, resource::ExclusiveDepthStencil::kDepthWriteStencilWrite);
    initializer.subpassHint = pipeline::SubpassHint::CustomResolveSubpass;

    const VulkanRenderTargetLayout layout(caps, initializer);

    ASSERT_EQ(layout.getNumColorAttachments(), 2u);
    const gp::UInt32 lastColor = layout.getColorAttachmentReferences()[1].attachment;
    EXPECT_EQ(layout.getAttachmentDescriptions()[lastColor].samples, VK_SAMPLE_COUNT_1_BIT);
    EXPECT_EQ(layout.getAttachmentDescriptions()[0].samples, VK_SAMPLE_COUNT_4_BIT);
    EXPECT_EQ(layout.getSubpassHint(), pipeline::SubpassHint::CustomResolveSubpass);
}

TEST(VulkanRenderTargetLayoutTest, ShadingRateAttachmentComesLast)
{
    VulkanRenderTargetCaps caps{};
    caps.vrsImageDataType = pipeline::VRSImageDataType::Palette;
    caps.vrsImageFormat = PixelFormat::R8_UInt;
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(1u);
    addDepthStencil(initializer, resource::ExclusiveDepthStencil::kDepthWriteStencilWrite);
    initializer.hasFragmentDensityAttachment = true;

    const VulkanRenderTargetLayout layout(caps, initializer);

    ASSERT_TRUE(layout.getHasFragmentDensityAttachment());
    ASSERT_NE(layout.getFragmentDensityAttachmentReference(), nullptr);
    EXPECT_EQ(layout.getNumAttachmentDescriptions(), 3u);
    EXPECT_EQ(layout.getFragmentDensityAttachmentReference()->attachment, 2u);
    EXPECT_EQ(
        layout.getFragmentDensityAttachmentReference()->layout,
        VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR
    );
    EXPECT_EQ(layout.getAttachmentDescriptions()[2].format, VK_FORMAT_R8_UINT);
    EXPECT_EQ(layout.getAttachmentDescriptions()[2].loadOp, VK_ATTACHMENT_LOAD_OP_LOAD);
}

TEST(VulkanRenderTargetLayoutTest, MultiViewCountIsKept)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(1u);
    initializer.multiViewCount = 2u;

    const VulkanRenderTargetLayout layout(caps, initializer);

    EXPECT_TRUE(layout.getIsMultiView());
    EXPECT_EQ(layout.getMultiViewCount(), 2u);
}

TEST(VulkanRenderTargetLayoutTest, EqualInitializersProduceEqualHashes)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer initializer = makeColorInitializer(2u);
    addDepthStencil(initializer, resource::ExclusiveDepthStencil::kDepthWriteStencilWrite);

    const VulkanRenderTargetLayout first(caps, initializer);
    const VulkanRenderTargetLayout second(caps, initializer);

    EXPECT_EQ(first.getRenderPassCompatibleHash(), second.getRenderPassCompatibleHash());
    EXPECT_EQ(first.getRenderPassFullHash(), second.getRenderPassFullHash());
    EXPECT_NE(first.getRenderPassCompatibleHash(), first.getRenderPassFullHash());
}

TEST(VulkanRenderTargetLayoutTest, FormatAndSampleCountChangeTheCompatibleHash)
{
    const VulkanRenderTargetCaps caps{};
    const VulkanRenderTargetLayout reference(caps, makeColorInitializer(1u));

    pipeline::GraphicsPipelineStateInitializer otherFormat = makeColorInitializer(1u);
    otherFormat.renderTargetFormats[0] = PixelFormat::R16G16B16A16_Float;
    const VulkanRenderTargetLayout formatLayout(caps, otherFormat);

    const VulkanRenderTargetLayout samplesLayout(caps, makeColorInitializer(1u, 4u));

    EXPECT_NE(reference.getRenderPassCompatibleHash(), formatLayout.getRenderPassCompatibleHash());
    EXPECT_NE(reference.getRenderPassCompatibleHash(), samplesLayout.getRenderPassCompatibleHash());
}

TEST(VulkanRenderTargetLayoutTest, LoadActionChangesOnlyTheFullHash)
{
    const VulkanRenderTargetCaps caps{};
    pipeline::GraphicsPipelineStateInitializer loadInitializer = makeColorInitializer(1u);
    addDepthStencil(loadInitializer, resource::ExclusiveDepthStencil::kDepthWriteStencilWrite);
    loadInitializer.depthTargetLoadAction = command::RenderTargetLoadAction::Load;

    pipeline::GraphicsPipelineStateInitializer clearInitializer = loadInitializer;
    clearInitializer.depthTargetLoadAction = command::RenderTargetLoadAction::Clear;

    const VulkanRenderTargetLayout loadLayout(caps, loadInitializer);
    const VulkanRenderTargetLayout clearLayout(caps, clearInitializer);

    EXPECT_EQ(loadLayout.getRenderPassCompatibleHash(), clearLayout.getRenderPassCompatibleHash());
    EXPECT_NE(loadLayout.getRenderPassFullHash(), clearLayout.getRenderPassFullHash());
}

}   // namespace gp::rhi::vulkan::tests
