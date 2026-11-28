// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanCommandListContext.hpp"
#include "../pipeline/VulkanPipelineTestFakes.hpp"
#include "commands/CommandEnums.hpp"
#include "commands/CommandTypes.hpp"
#include "commands/RenderPassInfo.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "contexts/VulkanCommandContextProvider.hpp"
#include "contexts/VulkanContextEnums.hpp"
#include "contexts/VulkanContextTypes.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/LinearColor.hpp"
#include "pendingstate/VulkanPendingGfxState.hpp"
#include "pendingstate/VulkanPendingStateTypes.hpp"   // IWYU pragma: keep
#include "pipeline/BlendState.hpp"                    // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"       // IWYU pragma: keep
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineState.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/RasterizerState.hpp"   // IWYU pragma: keep
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerState.hpp"      // IWYU pragma: keep
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "pipeline/VulkanPipelineStateCacheManager.hpp"
#include "pipeline/VulkanWrappedPipelineStates.hpp"
#include "renderpass/VulkanRenderPassManager.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/StagingBuffer.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanReadbackStagingBuffer.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/ShaderEnums.hpp"    // IWYU pragma: keep
#include "shaders/VertexShader.hpp"   // IWYU pragma: keep
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShaderFactory.hpp"
#include "states/VulkanSamplerState.hpp"
#include "states/VulkanStateRHI.hpp"
#include "views/UnorderedAccessView.hpp"   // IWYU pragma: keep
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"
#include "views/VulkanUnorderedAccessView.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanContextTestFixture.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

class VulkanCommandListContextLiveTest : public VulkanContextLiveTest
{
protected:
    /// @brief Creates a typed `R32_UInt` UAV over `buffer`.
    /// @param[in] buffer Buffer created with `UnorderedAccess`.
    /// @return UAV reference.
    [[nodiscard]] view::UnorderedAccessViewRef makeBufferUAV(const VulkanViewTestBufferRef& buffer) noexcept
    {
        const view::ViewDesc desc = view::ViewDesc::createBufferUAV()
                                        .setType(view::BufferViewType::Typed)
                                        .setFormat(PixelFormat::R32_UInt)
                                        .asViewDesc();
        return createVulkanUnorderedAccessView(cmdList(), buffer.get(), desc);
    }

    /// @brief Usage of the buffers cleared and copied by the tests.
    static constexpr resource::BufferUsageFlags kUAVUsage =
        resource::BufferUsageFlags::Static | resource::BufferUsageFlags::UnorderedAccess;
};

TEST_F(VulkanCommandListContextLiveTest, ConstructsWithMinimalServices)
{
    VulkanCommandListContext context(m_contextServices);
    EXPECT_EQ(context.getCommandBufferType(), VulkanCommandBufferType::Primary);
    EXPECT_FALSE(context.isInsideRenderPass());
    EXPECT_FALSE(context.isDynamicRenderingActive());
    EXPECT_EQ(context.getNumPayloads(), 0u);
    EXPECT_EQ(context.getDescriptorSetCache(), nullptr);
    EXPECT_TRUE(context.getStaticUniformBuffers().isEmpty() || (context.getStaticUniformBuffers()[0] == nullptr));
    EXPECT_EQ(context.getComputeStageUniformBuffers().dirtyBits, 0u);
    EXPECT_EQ(context.getGraphicsStageUniformBuffers(shader_stage::Stage::Vertex).dirtyBits, 0u);
}

TEST_F(VulkanCommandListContextLiveTest, CurrentFrameAndBlockOffsetAreStored)
{
    VulkanCommandListContext context(m_contextServices);
    context.setCurrentFrame(42u);
    EXPECT_EQ(context.getCurrentFrame(), 42u);

    context.setUniformBlockOffset(nullptr, 256u);
    EXPECT_EQ(context.getUniformBlockOffset(), 256u);
}

TEST_F(VulkanCommandListContextLiveTest, ViewportReachesThePendingGraphicsState)
{
    VulkanCommandListContext context(m_contextServices);
    context.setViewport(10.0f, 20.0f, 0.0f, 110.0f, 70.0f, 1.0f);

    const VulkanPendingGfxState& state = context.getPendingGfxState();
    ASSERT_EQ(state.getNumViewports(), 1u);
    EXPECT_FLOAT_EQ(state.getViewports()[0].x, 10.0f);
    EXPECT_FLOAT_EQ(state.getViewports()[0].y, 20.0f);
    EXPECT_FLOAT_EQ(state.getViewports()[0].width, 100.0f);
    EXPECT_FLOAT_EQ(state.getViewports()[0].height, 50.0f);
    EXPECT_FLOAT_EQ(state.getViewports()[0].maxDepth, 1.0f);
    EXPECT_FALSE(state.isScissorEnabled());
    EXPECT_EQ(state.getScissors()[0].extent.width, 100u);
}

TEST_F(VulkanCommandListContextLiveTest, ScissorRectEnablesClipping)
{
    VulkanCommandListContext context(m_contextServices);
    context.setViewport(0.0f, 0.0f, 0.0f, 64.0f, 64.0f, 1.0f);
    context.setScissorRect(true, 4u, 8u, 20u, 40u);

    const VulkanPendingGfxState& state = context.getPendingGfxState();
    EXPECT_TRUE(state.isScissorEnabled());
    EXPECT_EQ(state.getScissors()[0].offset.x, 4);
    EXPECT_EQ(state.getScissors()[0].offset.y, 8);
    EXPECT_EQ(state.getScissors()[0].extent.width, 16u);
    EXPECT_EQ(state.getScissors()[0].extent.height, 32u);

    context.setScissorRect(false, 0u, 0u, 0u, 0u);
    EXPECT_FALSE(state.isScissorEnabled());
    EXPECT_EQ(state.getScissors()[0].extent.width, 64u);
}

TEST_F(VulkanCommandListContextLiveTest, MultipleViewportsAreClampedToTheLimit)
{
    VulkanCommandListContext context(m_contextServices);

    command::ViewportBounds bounds[kVulkanMaxPendingViewports + 4u]{};
    for (gp::UInt32 index = 0u; index < kVulkanMaxPendingViewports + 4u; ++index)
    {
        bounds[index].topLeftX = static_cast<gp::Float32>(index);
        bounds[index].width = 8.0f;
        bounds[index].height = 8.0f;
    }
    context.setMultipleViewports(kVulkanMaxPendingViewports + 4u, bounds);

    const VulkanPendingGfxState& state = context.getPendingGfxState();
    ASSERT_EQ(state.getNumViewports(), kVulkanMaxPendingViewports);
    EXPECT_FLOAT_EQ(state.getViewports()[3].x, 3.0f);
    EXPECT_FLOAT_EQ(state.getViewports()[3].width, 8.0f);
    EXPECT_EQ(state.getNumScissors(), kVulkanMaxPendingViewports);
}

TEST_F(VulkanCommandListContextLiveTest, EmptyMultipleViewportsAreIgnored)
{
    VulkanCommandListContext context(m_contextServices);
    context.setViewport(0.0f, 0.0f, 0.0f, 32.0f, 32.0f, 1.0f);

    const command::ViewportBounds bounds{};
    context.setMultipleViewports(0u, &bounds);
    context.setMultipleViewports(4u, nullptr);
    EXPECT_EQ(context.getPendingGfxState().getNumViewports(), 1u);
}

TEST_F(VulkanCommandListContextLiveTest, FixedFunctionStateReachesThePendingGraphicsState)
{
    VulkanCommandListContext context(m_contextServices);
    context.setStencilRef(0x7Fu);
    context.setBlendFactor(gp::math::LinearColor(0.25f, 0.5f, 0.75f, 1.0f));
    context.setDepthBounds(0.1f, 0.9f);

    const VulkanPendingGfxState& state = context.getPendingGfxState();
    EXPECT_EQ(state.getStencilRef(), 0x7Fu);
    EXPECT_FLOAT_EQ(state.getBlendFactor()[0], 0.25f);
    EXPECT_FLOAT_EQ(state.getBlendFactor()[1], 0.5f);
    EXPECT_FLOAT_EQ(state.getBlendFactor()[2], 0.75f);
    EXPECT_FLOAT_EQ(state.getBlendFactor()[3], 1.0f);
    EXPECT_FLOAT_EQ(state.getMinDepthBounds(), 0.1f);
    EXPECT_FLOAT_EQ(state.getMaxDepthBounds(), 0.9f);
}

TEST_F(VulkanCommandListContextLiveTest, TransferRecorderFollowsTheActiveCommandBuffer)
{
    VulkanCommandListContext context(m_contextServices);
    IVulkanTransferCommandRecorder* const recorder = context.getTransferRecorder();
    ASSERT_NE(recorder, nullptr);
    EXPECT_EQ(context.getTransferRecorder(), recorder);
    EXPECT_TRUE(context.hasActiveCommandBuffer());

    ASSERT_TRUE(context.flushCommands(VulkanFlushFlags::WaitForCompletion));
    EXPECT_FALSE(context.hasActiveCommandBuffer());
    EXPECT_NE(context.getTransferRecorder(), nullptr);
}

TEST_F(VulkanCommandListContextLiveTest, OutsideARenderPassThereAreNoInputAttachments)
{
    VulkanCommandListContext context(m_contextServices);
    VulkanTextureView view{};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    EXPECT_FALSE(context.getInputAttachmentView(VulkanShaderAttachmentType::Depth, view, layout));
    EXPECT_FALSE(context.getInputAttachmentView(VulkanShaderAttachmentType::Color0, view, layout));
    EXPECT_EQ(layout, VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_F(VulkanCommandListContextLiveTest, NullResourcesAreIgnored)
{
    VulkanCommandListContext context(m_contextServices);
    context.copyBufferRegion(nullptr, 0u, nullptr, 0u, 64u);
    context.clearUAVUint(nullptr, resource::UIntVector4(1u, 1u, 1u, 1u));
    context.copyToStagingBuffer(nullptr, nullptr, 0u, 64u);
    context.copyTexture(nullptr, nullptr, command::CopyTextureInfo{});
    context.writeGPUFence(nullptr);
    EXPECT_EQ(context.getNumPayloads(), 0u);
}

TEST_F(VulkanCommandListContextLiveTest, ClearCopyAndReadbackRoundTripOnTheGPU)
{
    constexpr gp::UInt32 kPattern = 0x11223344u;
    constexpr gp::UInt32 kNumBytes = 256u;

    const VulkanViewTestBufferRef source = makeBuffer(kUAVUsage, kNumBytes);
    const VulkanViewTestBufferRef dest = makeBuffer(kUAVUsage, kNumBytes);
    const view::UnorderedAccessViewRef uav = makeBufferUAV(source);
    ASSERT_NE(uav.get(), nullptr);
    const resource::StagingBufferRef staging = createVulkanStagingBuffer();
    ASSERT_NE(staging.get(), nullptr);

    VulkanCommandListContext context(m_contextServices);
    context.clearUAVUint(uav.get(), resource::UIntVector4(kPattern, kPattern, kPattern, kPattern));
    context.copyBufferRegion(dest.get(), 0u, source.get(), 0u, kNumBytes);
    context.copyToStagingBuffer(dest.get(), staging.get(), 0u, kNumBytes);
    ASSERT_TRUE(context.flushCommands(VulkanFlushFlags::WaitForCompletion));

    const gp::UInt32* const data = static_cast<const gp::UInt32*>(staging->lock(0u, kNumBytes));
    ASSERT_NE(data, nullptr);
    for (gp::UInt32 index = 0u; index < kNumBytes / 4u; ++index)
    {
        ASSERT_EQ(data[index], kPattern) << "word " << index;
    }
    staging->unlock();
}

TEST_F(VulkanCommandListContextLiveTest, MultiWordBufferClearIsRejected)
{
    const VulkanViewTestBufferRef buffer = makeBuffer(kUAVUsage);
    const view::UnorderedAccessViewRef uav = makeBufferUAV(buffer);
    ASSERT_NE(uav.get(), nullptr);

    VulkanCommandListContext context(m_contextServices);
    context.clearUAVUint(uav.get(), resource::UIntVector4(1u, 2u, 3u, 4u));
    EXPECT_TRUE(context.flushCommands(VulkanFlushFlags::WaitForCompletion));
}

TEST_F(VulkanCommandListContextLiveTest, EmptyBufferCopyRecordsNothing)
{
    const VulkanViewTestBufferRef source = makeBuffer(kUAVUsage);
    const VulkanViewTestBufferRef dest = makeBuffer(kUAVUsage);

    VulkanCommandListContext context(m_contextServices);
    context.copyBufferRegion(dest.get(), 0u, source.get(), 0u, 0u);
    EXPECT_FALSE(context.hasActiveCommandBuffer());
}

TEST_F(VulkanCommandListContextLiveTest, ProviderResolvesTheContextRecorder)
{
    VulkanCommandListContext context(m_contextServices);
    VulkanCommandContextProvider provider(nullptr);
    IVulkanTransferCommandRecorder* const recorder = provider.getContextRecorder(&context);
    ASSERT_NE(recorder, nullptr);
    EXPECT_EQ(recorder, context.getTransferRecorder());
}

TEST_F(VulkanCommandListContextLiveTest, SetGraphicsPipelineAndDrawPrimitiveExecutesAcrossCommandBuffers)
{
    const VulkanViewTestTextureRef colorTarget = makeTexture2D(resource::TextureCreateFlags::RenderTargetable);
    ASSERT_NE(colorTarget.get(), nullptr);

    VulkanRenderPassManager renderPassManager(*m_device, nullptr, makeVulkanRenderTargetCaps(*m_device), false);

    VulkanShaderFactory shaderFactory{};
    const gp::Vector<gp::UInt8> vertexBlob =
        makeTestShaderBlob(kSpirvExecutionModelVertex, 0x3579bdf1u, "ContextDrawVS");
    gp::RefCountedPtr<VulkanVertexShader> vertexShader =
        shaderFactory.createShader<VulkanVertexShader>(*m_device, nullptr, gp::ConstVectorView<gp::UInt8>(vertexBlob));
    ASSERT_NE(vertexShader.get(), nullptr);

    pipeline::RasterizerStateDesc rasterizerDesc{};
    rasterizerDesc.fillMode = pipeline::RasterizerFillMode::Solid;
    pipeline::BlendStateRef blendState = createVulkanBlendState(pipeline::BlendStateDesc{});
    pipeline::RasterizerStateRef rasterizerState = createVulkanRasterizerState(rasterizerDesc);
    pipeline::DepthStencilStateRef depthStencilState = createVulkanDepthStencilState(pipeline::DepthStencilStateDesc{});

    VulkanPipelineStateCacheManager pipelineCache(*m_device, nullptr, &renderPassManager);

    pipeline::GraphicsPipelineStateInitializer psoInit{};
    psoInit.boundShaderState = pipeline::BoundShaderStateInput(nullptr, vertexShader.get(), nullptr);
    psoInit.blendState = blendState.get();
    psoInit.rasterizerState = rasterizerState.get();
    psoInit.depthStencilState = depthStencilState.get();
    psoInit.primitiveType = pipeline::PrimitiveType::TriangleList;
    psoInit.renderTargetsEnabled = 1u;
    psoInit.renderTargetFormats[0] = PixelFormat::R8G8B8A8_UNorm;
    psoInit.renderTargetFlags[0] = resource::TextureCreateFlags::RenderTargetable;
    psoInit.numSamples = 1u;

    pipeline::GraphicsPipelineStateRef pso = pipelineCache.createGraphicsPipelineState(psoInit);
    ASSERT_TRUE(pso.isValid());
    const VulkanGraphicsPipelineState* const innerPso =
        static_cast<VulkanWrappedGraphicsPipelineState*>(pso.get())->getState();
    ASSERT_NE(innerPso, nullptr);
    ASSERT_NE(innerPso->getPipelineHandle(), VK_NULL_HANDLE);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.maxLod = 1.0f;
    pipeline::SamplerStateRef defaultSampler(new VulkanSamplerState(*m_device, samplerInfo, nullptr));
    VulkanTextureView defaultImageView{};
    defaultImageView.view = colorTarget->getDefaultView();
    defaultImageView.image = colorTarget->getImage();
    defaultImageView.viewId = 1u;

    VulkanDescriptorDefaults defaults{};
    defaults.defaultSampler = static_cast<const VulkanSamplerState*>(defaultSampler.get());
    defaults.defaultImageView = &defaultImageView;

    VulkanContextServices drawServices = m_contextServices;
    drawServices.renderPassManager = &renderPassManager;
    drawServices.descriptorDefaults = &defaults;
    {
        VulkanCommandListContext context(drawServices);
        const command::RenderPassInfo rpInfo(colorTarget.get(), command::RenderTargetActions::ClearStore);

        // Pass 1 on initial command buffer: setGraphicsPipelineState + drawPrimitive binds VkPipeline cleanly.
        context.beginRenderPass(rpInfo, "Pass1");
        context.setGraphicsPipelineState(pso.get(), 0u, true);
        EXPECT_EQ(context.getPendingGfxState().getCurrentPipeline(), innerPso);
        context.drawPrimitive(0u, 1u, 1u);
        context.endRenderPass();
        ASSERT_TRUE(context.flushCommands(VulkanFlushFlags::WaitForCompletion));

        // Pass 2 on a fresh command buffer after flush: re-setting the same PSO forces a re-bind on the new handle.
        context.beginRenderPass(rpInfo, "Pass2");
        context.setGraphicsPipelineState(pso.get(), 0u, true);
        context.drawPrimitive(0u, 1u, 1u);
        context.endRenderPass();
        ASSERT_TRUE(context.flushCommands(VulkanFlushFlags::WaitForCompletion));
    }

    pso.reset();
    vertexShader.reset();
    blendState.reset();
    rasterizerState.reset();
    depthStencilState.reset();
    defaultSampler = pipeline::SamplerStateRef();
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

}   // namespace gp::rhi::vulkan::tests
