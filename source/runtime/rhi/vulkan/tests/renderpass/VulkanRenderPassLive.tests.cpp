// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "../views/VulkanViewTestFixture.hpp"
#include "commands/CommandEnums.hpp"
#include "commands/RenderPassInfo.hpp"
#include "core/ClearValueBinding.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "renderpass/VulkanBeginRenderPassInfo.hpp"
#include "renderpass/VulkanDynamicRendering.hpp"
#include "renderpass/VulkanFramebuffer.hpp"
#include "renderpass/VulkanRenderPass.hpp"
#include "renderpass/VulkanRenderPassManager.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "renderpass/VulkanRenderTargetViews.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

static_assert(toRenderTargetSliceIndex(0xFFFFFFFFu) == kAllRenderTargetSlices);
static_assert(toRenderTargetSliceIndex(0u) == 0);
static_assert(toRenderTargetSliceIndex(3u) == 3);

/// @brief Live fixture rendering into a cleared RGBA8 color target and a D32 depth target.
class VulkanRenderPassLiveTest : public VulkanViewLiveTest
{
protected:
    /// @brief Creates a color render target cleared to green.
    /// @return Owning reference.
    [[nodiscard]] VulkanViewTestTextureRef makeColorTarget() noexcept
    {
        resource::TextureCreateDesc desc = resource::TextureCreateDesc::create2D(
            "RenderPassColor", kTextureSize, kTextureSize, PixelFormat::R8G8B8A8_UNorm
        );
        desc.setFlags(resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ShaderResource);
        desc.setClearValue(resource::ClearValueBinding::kGreen);
        return createTexture(desc);
    }

    /// @brief Creates a depth target cleared to one.
    /// @return Owning reference.
    [[nodiscard]] VulkanViewTestTextureRef makeDepthTarget() noexcept
    {
        resource::TextureCreateDesc desc = resource::TextureCreateDesc::create2D(
            "RenderPassDepth", kTextureSize, kTextureSize, PixelFormat::D32_Float
        );
        desc.setFlags(resource::TextureCreateFlags::DepthStencilTargetable);
        desc.setClearValue(resource::ClearValueBinding::kDepthOne);
        return createTexture(desc);
    }

    /// @brief Describes a pass clearing and storing both targets.
    /// @param[in] color Color target.
    /// @param[in] depth Depth target.
    /// @return Pass description.
    [[nodiscard]] static command::RenderPassInfo makePassInfo(VulkanTexture& color, VulkanTexture& depth) noexcept
    {
        return command::RenderPassInfo(
            &color,
            command::RenderTargetActions::ClearStore,
            &depth,
            command::DepthStencilTargetActions::ClearDepthStencilStoreDepthStencil
        );
    }

    /// @brief Builds the layout of a pass, starting from undefined depth and stencil layouts.
    /// @param[in] caps Device capabilities.
    /// @param[in] rpInfo Pass description.
    /// @return Layout.
    [[nodiscard]] static VulkanRenderTargetLayout
        makePassLayout(const VulkanRenderTargetCaps& caps, const command::RenderPassInfo& rpInfo) noexcept
    {
        return VulkanRenderTargetLayout(caps, rpInfo, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED);
    }
};

}   // namespace

TEST(VulkanRenderPassManagerTest, FramebufferHashDependsOnMipAndSlice)
{
    const VulkanRenderTargetLayout layout;
    command::SetRenderTargetsInfo rtInfo;
    const gp::UInt32 baseHash = VulkanRenderPassManager::computeFramebufferHash(rtInfo, layout);

    EXPECT_EQ(baseHash, VulkanRenderPassManager::computeFramebufferHash(rtInfo, layout));

    rtInfo.colorRenderTarget[0].mipIndex = 1u;
    const gp::UInt32 mipHash = VulkanRenderPassManager::computeFramebufferHash(rtInfo, layout);
    EXPECT_NE(baseHash, mipHash);

    rtInfo.colorRenderTarget[0].mipIndex = 0u;
    rtInfo.colorRenderTarget[0].arraySliceIndex = 2u;
    EXPECT_NE(baseHash, VulkanRenderPassManager::computeFramebufferHash(rtInfo, layout));
}

TEST_F(VulkanRenderPassLiveTest, PassLayoutDescribesClearedTargets)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    ASSERT_NE(color->getImage(), static_cast<VkImage>(VK_NULL_HANDLE));
    ASSERT_NE(depth->getImage(), static_cast<VkImage>(VK_NULL_HANDLE));

    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    const VulkanRenderTargetLayout layout = makePassLayout(caps, makePassInfo(*color, *depth));

    ASSERT_EQ(layout.getNumColorAttachments(), 1u);
    ASSERT_TRUE(layout.getHasDepthStencil());
    ASSERT_EQ(layout.getNumAttachmentDescriptions(), 2u);
    EXPECT_EQ(layout.getAttachmentDescriptions()[0].loadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
    EXPECT_EQ(layout.getAttachmentDescriptions()[0].storeOp, VK_ATTACHMENT_STORE_OP_STORE);
    EXPECT_EQ(layout.getAttachmentDescriptions()[1].format, VK_FORMAT_D32_SFLOAT);
    EXPECT_EQ(layout.getAttachmentDescriptions()[1].loadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
    EXPECT_EQ(layout.getNumUsedClearValues(), 2u);
    EXPECT_EQ(layout.getExtent2D().width, static_cast<gp::UInt32>(kTextureSize));
    EXPECT_EQ(layout.getExtent2D().height, static_cast<gp::UInt32>(kTextureSize));
}

TEST_F(VulkanRenderPassLiveTest, PassAndPipelineLayoutsAreCompatible)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    const VulkanRenderTargetLayout passLayout = makePassLayout(caps, makePassInfo(*color, *depth));

    pipeline::GraphicsPipelineStateInitializer initializer;
    initializer.renderTargetsEnabled = 1u;
    initializer.renderTargetFormats[0] = PixelFormat::R8G8B8A8_UNorm;
    initializer.renderTargetFlags[0] = color->getFlags();
    initializer.depthStencilTargetFormat = PixelFormat::D32_Float;
    initializer.depthStencilTargetFlag = depth->getFlags();
    initializer.depthStencilAccess = resource::ExclusiveDepthStencil::kDepthWriteStencilWrite;
    const VulkanRenderTargetLayout pipelineLayout(caps, initializer);

    EXPECT_EQ(passLayout.getRenderPassCompatibleHash(), pipelineLayout.getRenderPassCompatibleHash());
    // Load and store ops differ: the pipeline only knows the formats.
    EXPECT_NE(passLayout.getRenderPassFullHash(), pipelineLayout.getRenderPassFullHash());
}

TEST_F(VulkanRenderPassLiveTest, SetRenderTargetsLayoutMatchesPassLayout)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    const command::RenderPassInfo rpInfo = makePassInfo(*color, *depth);
    command::SetRenderTargetsInfo rtInfo;
    rpInfo.convertToRenderTargetsInfo(rtInfo);

    const VulkanRenderTargetLayout passLayout = makePassLayout(caps, rpInfo);
    const VulkanRenderTargetLayout rtLayout(caps, rtInfo);

    EXPECT_EQ(rtLayout.getNumColorAttachments(), passLayout.getNumColorAttachments());
    EXPECT_EQ(rtLayout.getNumAttachmentDescriptions(), passLayout.getNumAttachmentDescriptions());
    EXPECT_EQ(rtLayout.getRenderPassCompatibleHash(), passLayout.getRenderPassCompatibleHash());
}

TEST_F(VulkanRenderPassLiveTest, CreatesRenderPassAndFramebuffer)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    const command::RenderPassInfo rpInfo = makePassInfo(*color, *depth);
    command::SetRenderTargetsInfo rtInfo;
    rpInfo.convertToRenderTargetsInfo(rtInfo);
    const VulkanRenderTargetLayout layout = makePassLayout(caps, rpInfo);

    const VulkanRenderPass renderPass(*m_device, nullptr, caps, layout);
    ASSERT_NE(renderPass.getHandle(), static_cast<VkRenderPass>(VK_NULL_HANDLE));
    EXPECT_EQ(renderPass.getNumUsedClearValues(), 2u);
    EXPECT_EQ(renderPass.getLayout().getRenderPassFullHash(), layout.getRenderPassFullHash());

    VulkanFramebuffer framebuffer(*m_device, nullptr, rtInfo, layout, renderPass);
    ASSERT_NE(framebuffer.getHandle(), static_cast<VkFramebuffer>(VK_NULL_HANDLE));
    EXPECT_EQ(framebuffer.getNumAttachmentViews(), 2u);
    EXPECT_EQ(framebuffer.getNumColorAttachments(), 1u);
    EXPECT_EQ(framebuffer.getNumLayers(), 1u);
    EXPECT_EQ(framebuffer.getRenderArea().extent.width, static_cast<gp::UInt32>(kTextureSize));
    EXPECT_TRUE(framebuffer.containsImage(color->getImage()));
    EXPECT_TRUE(framebuffer.containsImage(depth->getImage()));
    EXPECT_FALSE(framebuffer.containsImage(VK_NULL_HANDLE));
    EXPECT_TRUE(framebuffer.matches(rtInfo));

    framebuffer.destroy(true);
    EXPECT_EQ(framebuffer.getHandle(), static_cast<VkFramebuffer>(VK_NULL_HANDLE));
}

TEST_F(VulkanRenderPassLiveTest, FramebufferDoesNotMatchOtherTargets)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef otherColor = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    command::SetRenderTargetsInfo rtInfo;
    makePassInfo(*color, *depth).convertToRenderTargetsInfo(rtInfo);
    const VulkanRenderTargetLayout layout(caps, rtInfo);
    const VulkanRenderPass renderPass(*m_device, nullptr, caps, layout);
    const VulkanFramebuffer framebuffer(*m_device, nullptr, rtInfo, layout, renderPass);

    command::SetRenderTargetsInfo otherInfo;
    makePassInfo(*otherColor, *depth).convertToRenderTargetsInfo(otherInfo);

    EXPECT_FALSE(framebuffer.matches(otherInfo));
    EXPECT_FALSE(framebuffer.containsImage(otherColor->getImage()));
}

TEST_F(VulkanRenderPassLiveTest, ManagerCachesRenderPassesAndFramebuffers)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    const command::RenderPassInfo rpInfo = makePassInfo(*color, *depth);
    command::SetRenderTargetsInfo rtInfo;
    rpInfo.convertToRenderTargetsInfo(rtInfo);
    const VulkanRenderTargetLayout layout = makePassLayout(caps, rpInfo);

    VulkanRenderPassManager manager(*m_device, nullptr, caps);
    EXPECT_FALSE(manager.usesDynamicRendering());

    VulkanRenderPass* const renderPass = manager.getOrCreateRenderPass(layout);
    ASSERT_NE(renderPass, nullptr);
    EXPECT_EQ(manager.getOrCreateRenderPass(layout), renderPass);
    EXPECT_EQ(manager.getNumRenderPasses(), 1u);

    VulkanFramebuffer* const framebuffer = manager.getOrCreateFramebuffer(rtInfo, layout, *renderPass);
    ASSERT_NE(framebuffer, nullptr);
    EXPECT_NE(framebuffer->getHandle(), static_cast<VkFramebuffer>(VK_NULL_HANDLE));
    EXPECT_EQ(manager.getOrCreateFramebuffer(rtInfo, layout, *renderPass), framebuffer);
    EXPECT_EQ(manager.getNumFramebuffers(), 1u);
}

TEST_F(VulkanRenderPassLiveTest, ManagerCreatesOneRenderPassPerLoadAction)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    command::RenderPassInfo loadInfo = makePassInfo(*color, *depth);
    loadInfo.colorRenderTargets[0].action = command::RenderTargetActions::LoadStore;

    VulkanRenderPassManager manager(*m_device, nullptr, caps);
    VulkanRenderPass* const clearPass =
        manager.getOrCreateRenderPass(makePassLayout(caps, makePassInfo(*color, *depth)));
    VulkanRenderPass* const loadPass = manager.getOrCreateRenderPass(makePassLayout(caps, loadInfo));

    EXPECT_NE(clearPass, loadPass);
    EXPECT_EQ(manager.getNumRenderPasses(), 2u);
}

TEST_F(VulkanRenderPassLiveTest, DeletedAttachmentImagesDropTheirFramebuffers)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    command::SetRenderTargetsInfo rtInfo;
    makePassInfo(*color, *depth).convertToRenderTargetsInfo(rtInfo);
    const VulkanRenderTargetLayout layout(caps, rtInfo);

    VulkanRenderPassManager manager(*m_device, nullptr, caps);
    VulkanRenderPass* const renderPass = manager.getOrCreateRenderPass(layout);
    ASSERT_NE(renderPass, nullptr);
    static_cast<void>(manager.getOrCreateFramebuffer(rtInfo, layout, *renderPass));
    ASSERT_EQ(manager.getNumFramebuffers(), 1u);

    // Images that cannot be attachments never touch the cache.
    manager.notifyDeletedImage(color->getImage(), resource::TextureCreateFlags::ShaderResource);
    EXPECT_EQ(manager.getNumFramebuffers(), 1u);
    manager.notifyDeletedImage(VK_NULL_HANDLE, resource::TextureCreateFlags::RenderTargetable);
    EXPECT_EQ(manager.getNumFramebuffers(), 1u);

    manager.notifyDeletedImage(color->getImage(), color->getFlags());
    EXPECT_EQ(manager.getNumFramebuffers(), 0u);
    EXPECT_EQ(manager.getNumRenderPasses(), 1u);
}

TEST_F(VulkanRenderPassLiveTest, BeginInfoPlacesClearValuesAtAttachmentIndices)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    const command::RenderPassInfo rpInfo = makePassInfo(*color, *depth);
    command::SetRenderTargetsInfo rtInfo;
    rpInfo.convertToRenderTargetsInfo(rtInfo);
    const VulkanRenderTargetLayout layout = makePassLayout(caps, rpInfo);
    const VulkanRenderPass renderPass(*m_device, nullptr, caps, layout);
    const VulkanFramebuffer framebuffer(*m_device, nullptr, rtInfo, layout, renderPass);

    VulkanBeginRenderPassInfo beginInfo;
    beginInfo.build(rpInfo, renderPass, framebuffer);

    const VkRenderPassBeginInfo& info = beginInfo.beginInfo;
    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    EXPECT_EQ(info.renderPass, renderPass.getHandle());
    EXPECT_EQ(info.framebuffer, framebuffer.getHandle());
    EXPECT_EQ(info.renderArea.extent.width, static_cast<gp::UInt32>(kTextureSize));
    ASSERT_EQ(info.clearValueCount, 2u);
    ASSERT_NE(info.pClearValues, nullptr);

    const gp::UInt32 colorIndex = layout.getColorAttachmentReferences()[0].attachment;
    const gp::UInt32 depthIndex = layout.getDepthAttachmentReference()->attachment;
    for (gp::UInt32 channel = 0u; channel < 4u; ++channel)
    {
        EXPECT_FLOAT_EQ(
            info.pClearValues[colorIndex].color.float32[channel],
            resource::ClearValueBinding::kGreen.value.color[channel]
        );
    }
    EXPECT_FLOAT_EQ(
        info.pClearValues[depthIndex].depthStencil.depth,
        resource::ClearValueBinding::kDepthOne.value.depthStencil.depth
    );
    EXPECT_EQ(beginInfo.getSubpassContents(), VK_SUBPASS_CONTENTS_INLINE);

    beginInfo.isParallelRenderPass = true;
    EXPECT_EQ(beginInfo.getSubpassContents(), VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}

TEST_F(VulkanRenderPassLiveTest, DepthStencilLayoutsFollowAccess)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    caps.supportsParallelRendering = true;
    caps.requiresMergedDepthStencilLayout = false;

    command::RenderPassInfo rpInfo = makePassInfo(*color, *depth);
    VkImageLayout depthLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
    VkImageLayout stencilLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
    extractVulkanDepthStencilLayouts(caps, rpInfo, depthLayout, stencilLayout);
    EXPECT_EQ(depthLayout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    // D32 has no stencil aspect.
    EXPECT_EQ(stencilLayout, VK_IMAGE_LAYOUT_UNDEFINED);

    rpInfo.depthStencilRenderTarget.exclusiveDepthStencil = resource::ExclusiveDepthStencil::kDepthReadStencilRead;
    extractVulkanDepthStencilLayouts(caps, rpInfo, depthLayout, stencilLayout);
    EXPECT_EQ(depthLayout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    rpInfo.depthStencilRenderTarget.exclusiveDepthStencil = resource::ExclusiveDepthStencil::kDepthNopStencilNop;
    extractVulkanDepthStencilLayouts(caps, rpInfo, depthLayout, stencilLayout);
    EXPECT_EQ(depthLayout, VK_IMAGE_LAYOUT_UNDEFINED);

    const command::RenderPassInfo colorOnly(color.get(), command::RenderTargetActions::LoadStore);
    extractVulkanDepthStencilLayouts(caps, colorOnly, depthLayout, stencilLayout);
    EXPECT_EQ(depthLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(stencilLayout, VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_F(VulkanRenderPassLiveTest, DynamicRenderingInfoMirrorsThePass)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const VulkanViewTestTextureRef depth = makeDepthTarget();
    const VulkanRenderTargetCaps caps = makeVulkanRenderTargetCaps(*m_device);
    const command::RenderPassInfo rpInfo = makePassInfo(*color, *depth);

    VulkanDynamicRenderingInfo info;
    fillVulkanDynamicRenderingInfo(caps, rpInfo, info);

    const VkRenderingInfo& rendering = info.renderingInfo;
    EXPECT_EQ(rendering.sType, VK_STRUCTURE_TYPE_RENDERING_INFO);
    ASSERT_EQ(rendering.colorAttachmentCount, 1u);
    EXPECT_NE(rendering.pColorAttachments[0].imageView, static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_EQ(rendering.pColorAttachments[0].imageView, info.colorViews[0]);
    EXPECT_EQ(rendering.pColorAttachments[0].loadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
    EXPECT_EQ(rendering.pColorAttachments[0].storeOp, VK_ATTACHMENT_STORE_OP_STORE);
    EXPECT_FLOAT_EQ(rendering.pColorAttachments[0].clearValue.color.float32[1], 1.0f);

    ASSERT_NE(rendering.pDepthAttachment, nullptr);
    EXPECT_NE(rendering.pDepthAttachment->imageView, static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_EQ(rendering.pDepthAttachment->loadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
    EXPECT_FLOAT_EQ(rendering.pDepthAttachment->clearValue.depthStencil.depth, 1.0f);
    EXPECT_EQ(rendering.pStencilAttachment, nullptr);

    EXPECT_EQ(rendering.renderArea.extent.width, static_cast<gp::UInt32>(kTextureSize));
    EXPECT_EQ(rendering.renderArea.extent.height, static_cast<gp::UInt32>(kTextureSize));
    EXPECT_EQ(rendering.layerCount, 1u);
    EXPECT_EQ(rendering.viewMask, 0u);
}

TEST_F(VulkanRenderPassLiveTest, RenderTargetViewsAreCachedOnTheTexture)
{
    const VulkanViewTestTextureRef color = makeColorTarget();
    const gp::USize initialViews = color->getNumInternalViews();

    gp::UInt32 numLayers = 0u;
    const VkImageView first = getVulkanColorRenderTargetView(*color, 0u, kAllRenderTargetSlices, 0u, numLayers);
    ASSERT_NE(first, static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_EQ(numLayers, 1u);
    const gp::USize cachedViews = color->getNumInternalViews();
    EXPECT_GT(cachedViews, initialViews);

    const VkImageView second = getVulkanColorRenderTargetView(*color, 0u, kAllRenderTargetSlices, 0u, numLayers);
    EXPECT_EQ(first, second);
    EXPECT_EQ(color->getNumInternalViews(), cachedViews);
}

TEST_F(VulkanRenderPassLiveTest, CubeFacesGetDistinctColorViews)
{
    const VulkanViewTestTextureRef cube = makeTextureCube(resource::TextureCreateFlags::RenderTargetable);

    gp::UInt32 numLayers = 0u;
    const VkImageView face0 = getVulkanColorRenderTargetView(*cube, 0u, 0, 0u, numLayers);
    const VkImageView face3 = getVulkanColorRenderTargetView(*cube, 0u, 3, 0u, numLayers);

    ASSERT_NE(face0, static_cast<VkImageView>(VK_NULL_HANDLE));
    ASSERT_NE(face3, static_cast<VkImageView>(VK_NULL_HANDLE));
    EXPECT_NE(face0, face3);
}

}   // namespace gp::rhi::vulkan::tests
