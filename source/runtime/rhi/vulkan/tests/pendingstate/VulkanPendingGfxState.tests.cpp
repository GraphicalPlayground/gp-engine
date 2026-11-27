// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanPendingGfxState.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandBufferDynamicState.hpp"
#include "commands/VulkanSubmission.hpp"
#include "containers/arrays/Array.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "device/VulkanDevice.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pendingstate/VulkanGraphicsPipelineDescriptorState.hpp"
#include "pendingstate/VulkanPendingComputeState.hpp"
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "pipeline/ComputePipelineState.hpp"    // IWYU pragma: keep
#include "pipeline/GraphicsPipelineState.hpp"   // IWYU pragma: keep
#include "pipeline/SamplerState.hpp"            // IWYU pragma: keep
#include "pipeline/VulkanComputePipelineState.hpp"
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "states/VulkanSamplerState.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanPendingStateLiveFixture.hpp"
#include "VulkanPendingStateTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Offline fixture: the setters only store state, so they run without a device or a command buffer.
class VulkanPendingGfxStateTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_manager.init(m_device);
        VkSamplerCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        m_sampler = pipeline::SamplerStateRef(new VulkanSamplerState(m_device, createInfo, nullptr));
        m_defaultImageView.viewId = 1u;
        m_defaults.defaultSampler = static_cast<const VulkanSamplerState*>(m_sampler.get());
        m_defaults.defaultImageView = &m_defaultImageView;
    }

    virtual void TearDown() override
    {
        m_sampler = pipeline::SamplerStateRef();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }

    /// @brief Device without native handles.
    VulkanDevice m_device{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Pool manager behind the context.
    VulkanDescriptorPoolsManager m_manager{};

    /// @brief Command context services.
    FakePendingStateContext m_context{ m_manager };

    /// @brief Default sampler.
    pipeline::SamplerStateRef m_sampler{};

    /// @brief Default image view.
    VulkanTextureView m_defaultImageView{};

    /// @brief Defaults handed to the state.
    VulkanDescriptorDefaults m_defaults{};
};

}   // namespace

TEST_F(VulkanPendingGfxStateTest, FreshStateHasNoPipelineAndDefaultValues)
{
    const VulkanPendingGfxState state(m_device, m_context, m_defaults);

    EXPECT_EQ(state.getCurrentPipeline(), nullptr);
    EXPECT_EQ(state.getCurrentState(), nullptr);
    EXPECT_EQ(state.getNumPipelineStates(), 0u);
    EXPECT_EQ(state.getNumViewports(), 0u);
    EXPECT_EQ(state.getNumScissors(), 0u);
    EXPECT_FALSE(state.isScissorEnabled());
    EXPECT_EQ(state.getStencilRef(), 0u);
    EXPECT_FLOAT_EQ(state.getBlendFactor()[0], 1.0f);
    EXPECT_FLOAT_EQ(state.getMinDepthBounds(), 0.0f);
    EXPECT_FLOAT_EQ(state.getMaxDepthBounds(), 1.0f);
    EXPECT_TRUE(state.hasDirtyVertexStreams());
}

TEST_F(VulkanPendingGfxStateTest, ViewportSetsAMatchingDisabledScissor)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);

    state.setViewport(10.0f, 20.0f, 0.0f, 110.0f, 220.0f, 1.0f);

    ASSERT_EQ(state.getNumViewports(), 1u);
    const VkViewport& viewport = state.getViewports()[0];
    EXPECT_FLOAT_EQ(viewport.x, 10.0f);
    EXPECT_FLOAT_EQ(viewport.y, 20.0f);
    EXPECT_FLOAT_EQ(viewport.width, 100.0f);
    EXPECT_FLOAT_EQ(viewport.height, 200.0f);
    EXPECT_FLOAT_EQ(viewport.maxDepth, 1.0f);
    ASSERT_EQ(state.getNumScissors(), 1u);
    EXPECT_EQ(state.getScissors()[0].offset.x, 10);
    EXPECT_EQ(state.getScissors()[0].extent.height, 200u);
    EXPECT_FALSE(state.isScissorEnabled());
}

TEST_F(VulkanPendingGfxStateTest, EmptyDepthRangeIsWidened)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);

    state.setViewport(0.0f, 0.0f, 0.5f, 64.0f, 64.0f, 0.5f);

    EXPECT_FLOAT_EQ(state.getViewports()[0].minDepth, 0.5f);
    EXPECT_FLOAT_EQ(state.getViewports()[0].maxDepth, 1.5f);
}

TEST_F(VulkanPendingGfxStateTest, MultiViewportDerivesOneScissorPerViewport)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);
    gp::Array<VkViewport, 2u> viewports{};
    viewports[0].width = 32.0f;
    viewports[0].height = 16.0f;
    viewports[1].x = 32.0f;
    viewports[1].width = 8.0f;
    viewports[1].height = 4.0f;

    state.setMultiViewport(viewports.data(), 2u);

    ASSERT_EQ(state.getNumViewports(), 2u);
    ASSERT_EQ(state.getNumScissors(), 2u);
    EXPECT_EQ(state.getScissors()[1].offset.x, 32);
    EXPECT_EQ(state.getScissors()[1].extent.width, 8u);
    EXPECT_EQ(state.getScissors()[0].extent.height, 16u);
}

TEST_F(VulkanPendingGfxStateTest, MultiViewportIsClampedToCapacity)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);
    gp::Array<VkViewport, kVulkanMaxPendingViewports + 2u> viewports{};

    state.setMultiViewport(viewports.data(), kVulkanMaxPendingViewports + 2u);

    EXPECT_EQ(state.getNumViewports(), kVulkanMaxPendingViewports);
    EXPECT_EQ(state.getNumScissors(), kVulkanMaxPendingViewports);
}

TEST_F(VulkanPendingGfxStateTest, EnabledScissorUsesTheGivenRectangle)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);
    state.setViewport(0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 1.0f);

    state.setScissor(true, 10u, 20u, 30u, 60u);

    EXPECT_TRUE(state.isScissorEnabled());
    const VkRect2D& rect = state.getScissors()[0];
    EXPECT_EQ(rect.offset.x, 10);
    EXPECT_EQ(rect.offset.y, 20);
    EXPECT_EQ(rect.extent.width, 20u);
    EXPECT_EQ(rect.extent.height, 40u);
}

TEST_F(VulkanPendingGfxStateTest, DisabledScissorCoversTheViewport)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);
    state.setViewport(5.0f, 6.0f, 0.0f, 105.0f, 56.0f, 1.0f);
    state.setScissor(true, 0u, 0u, 1u, 1u);

    state.setScissor(false, 0u, 0u, 0u, 0u);

    EXPECT_FALSE(state.isScissorEnabled());
    const VkRect2D& rect = state.getScissors()[0];
    EXPECT_EQ(rect.offset.x, 5);
    EXPECT_EQ(rect.offset.y, 6);
    EXPECT_EQ(rect.extent.width, 100u);
    EXPECT_EQ(rect.extent.height, 50u);
}

TEST_F(VulkanPendingGfxStateTest, StreamSourcesAreStoredPerIndex)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);
    const VkBuffer buffer = makeFakePendingHandle<VkBuffer>(0xA000u);

    state.setStreamSource(2u, buffer, 64u);

    EXPECT_EQ(state.getVertexStream(2u).buffer, buffer);
    EXPECT_EQ(state.getVertexStream(2u).offset, 64u);
    EXPECT_EQ(state.getVertexStream(1u).buffer, VK_NULL_HANDLE);
    EXPECT_TRUE(state.hasDirtyVertexStreams());
}

TEST_F(VulkanPendingGfxStateTest, FixedFunctionValuesAreStored)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);

    state.setStencilRef(7u);
    state.setBlendFactor(gp::Array<gp::Float32, 4u>{ 0.1f, 0.2f, 0.3f, 0.4f });
    state.setDepthBounds(0.25f, 0.75f);

    EXPECT_EQ(state.getStencilRef(), 7u);
    EXPECT_FLOAT_EQ(state.getBlendFactor()[3], 0.4f);
    EXPECT_FLOAT_EQ(state.getMinDepthBounds(), 0.25f);
    EXPECT_FLOAT_EQ(state.getMaxDepthBounds(), 0.75f);
}

TEST_F(VulkanPendingGfxStateTest, ResetClearsPerDrawStateButKeepsBlendAndDepthBounds)
{
    VulkanPendingGfxState state(m_device, m_context, m_defaults);
    state.setViewport(0.0f, 0.0f, 0.0f, 64.0f, 64.0f, 1.0f);
    state.setScissor(true, 0u, 0u, 8u, 8u);
    state.setStencilRef(3u);
    state.setBlendFactor(gp::Array<gp::Float32, 4u>{ 0.5f, 0.5f, 0.5f, 0.5f });
    state.setDepthBounds(0.1f, 0.2f);
    state.setStreamSource(0u, makeFakePendingHandle<VkBuffer>(0xA000u), 0u);

    state.reset();

    EXPECT_EQ(state.getNumViewports(), 0u);
    EXPECT_EQ(state.getNumScissors(), 0u);
    EXPECT_FALSE(state.isScissorEnabled());
    EXPECT_EQ(state.getStencilRef(), 0u);
    EXPECT_EQ(state.getVertexStream(0u).buffer, VK_NULL_HANDLE);
    EXPECT_FLOAT_EQ(state.getBlendFactor()[0], 0.5f);
    EXPECT_FLOAT_EQ(state.getMaxDepthBounds(), 0.2f);
}

TEST_F(VulkanPendingStateLiveTest, GfxDrawWithoutAPipelineIsRejected)
{
    VulkanCommandBuffer* const commandBuffer = beginCommandBuffer();
    ASSERT_NE(commandBuffer, nullptr);
    {
        VulkanPendingGfxState state(*m_device, *m_context, m_defaults);
        EXPECT_FALSE(state.prepareForDraw(*commandBuffer));
    }
    EXPECT_TRUE(commandBuffer->getDynamicState().needsFullUpdate);
    EXPECT_TRUE(submitAndWait(commandBuffer, new VulkanPayload(*m_queue)));
}

TEST_F(VulkanPendingStateLiveTest, GfxPipelineSwitchesAreTrackedPerPipeline)
{
    const pipeline::GraphicsPipelineStateRef pipelineRef = createGraphicsPipeline();
    if (!pipelineRef.isValid())
    {
        GTEST_SKIP() << "Device renders through render passes; skipping dynamic rendering pipeline test.";
    }
    const VulkanGraphicsPipelineState& pipelineState = *getInnerState(pipelineRef);
    VulkanPendingGfxState state(*m_device, *m_context, m_defaults);

    EXPECT_TRUE(state.setGfxPipeline(pipelineState, false));
    EXPECT_EQ(state.getCurrentPipeline(), &pipelineState);
    ASSERT_NE(state.getCurrentState(), nullptr);
    EXPECT_EQ(state.getCurrentState()->getPipeline(), &pipelineState);
    EXPECT_TRUE(state.getPipelineDynamicStates().contains(VK_DYNAMIC_STATE_STENCIL_REFERENCE));
    EXPECT_TRUE(state.getPipelineDynamicStates().contains(VK_DYNAMIC_STATE_BLEND_CONSTANTS));

    EXPECT_FALSE(state.setGfxPipeline(pipelineState, false));
    EXPECT_FALSE(state.setGfxPipeline(pipelineState, true));
    EXPECT_EQ(state.getNumPipelineStates(), 1u);

    // A reset keeps the per-pipeline state: binding the pipeline again reuses it.
    VulkanGraphicsPipelineDescriptorState* const descriptorState = state.getCurrentState();
    state.reset();
    EXPECT_EQ(state.getCurrentPipeline(), nullptr);
    EXPECT_TRUE(state.setGfxPipeline(pipelineState, false));
    EXPECT_EQ(state.getCurrentState(), descriptorState);
    EXPECT_EQ(state.getNumPipelineStates(), 1u);

    state.notifyDeletedPipeline(pipelineState);
    EXPECT_EQ(state.getCurrentPipeline(), nullptr);
    EXPECT_EQ(state.getNumPipelineStates(), 0u);
    EXPECT_EQ(state.getPipelineDynamicStates().count, 0u);
}

TEST_F(VulkanPendingStateLiveTest, GfxDrawFlushesDynamicStateOnce)
{
    const pipeline::GraphicsPipelineStateRef pipelineRef = createGraphicsPipeline();
    if (!pipelineRef.isValid())
    {
        GTEST_SKIP() << "Device renders through render passes; skipping dynamic rendering pipeline test.";
    }
    const VulkanGraphicsPipelineState& pipelineState = *getInnerState(pipelineRef);
    VulkanCommandBuffer* const commandBuffer = beginCommandBuffer();
    ASSERT_NE(commandBuffer, nullptr);
    {
        VulkanPendingGfxState state(*m_device, *m_context, m_defaults);
        ASSERT_TRUE(state.setGfxPipeline(pipelineState, false));
        state.bind(*commandBuffer);
        state.setViewport(0.0f, 0.0f, 0.0f, 64.0f, 32.0f, 1.0f);
        state.setStencilRef(5u);
        state.setStreamSource(0u, VK_NULL_HANDLE, 0u);

        ASSERT_TRUE(state.prepareForDraw(*commandBuffer));

        const VulkanCommandBufferDynamicState& dynamicState = commandBuffer->getDynamicState();
        EXPECT_FALSE(dynamicState.needsFullUpdate);
        ASSERT_EQ(dynamicState.numViewports, 1u);
        EXPECT_FLOAT_EQ(dynamicState.viewports[0].width, 64.0f);
        EXPECT_EQ(dynamicState.numScissors, 1u);
        EXPECT_EQ(dynamicState.stencilRef, 5u);
        EXPECT_TRUE(dynamicState.hasBlendConstants);
        EXPECT_FALSE(state.hasDirtyVertexStreams());

        // Streams are dirtied only by an actual change.
        state.setStreamSource(0u, VK_NULL_HANDLE, 0u);
        EXPECT_FALSE(state.hasDirtyVertexStreams());
        state.setStreamSource(1u, VK_NULL_HANDLE, 16u);
        EXPECT_TRUE(state.hasDirtyVertexStreams());

        // Nothing changed: the second draw records nothing new and keeps the stored values.
        ASSERT_TRUE(state.prepareForDraw(*commandBuffer));
        EXPECT_EQ(commandBuffer->getDynamicState().stencilRef, 5u);

        state.setStencilRef(6u);
        ASSERT_TRUE(state.prepareForDraw(*commandBuffer));
        EXPECT_EQ(commandBuffer->getDynamicState().stencilRef, 6u);
    }
    EXPECT_TRUE(submitAndWait(commandBuffer, new VulkanPayload(*m_queue)));
}

TEST_F(VulkanPendingStateLiveTest, PrepareForDrawBindsPipelineWithoutExplicitBindAndRebindsAfterCommandBufferSplit)
{
    const pipeline::GraphicsPipelineStateRef pipelineRef = createGraphicsPipeline();
    if (!pipelineRef.isValid())
    {
        GTEST_SKIP() << "Device renders through render passes; skipping dynamic rendering pipeline test.";
    }
    const VulkanGraphicsPipelineState& pipelineState = *getInnerState(pipelineRef);
    ASSERT_NE(pipelineState.getPipelineHandle(), VK_NULL_HANDLE);

    VulkanPendingGfxState state(*m_device, *m_context, m_defaults);
    ASSERT_TRUE(state.setGfxPipeline(pipelineState, false));
    state.setViewport(0.0f, 0.0f, 0.0f, 32.0f, 32.0f, 1.0f);
    state.setStreamSource(0u, VK_NULL_HANDLE, 0u);

    // 1. First command buffer: prepareForDraw binds the graphics pipeline automatically even without a prior bind().
    VulkanCommandBuffer* const firstCmdBuffer = beginCommandBuffer();
    ASSERT_NE(firstCmdBuffer, nullptr);
    EXPECT_EQ(firstCmdBuffer->getCurrentGraphicsPipeline(), VK_NULL_HANDLE);

    ASSERT_TRUE(state.prepareForDraw(*firstCmdBuffer));
    EXPECT_EQ(firstCmdBuffer->getCurrentGraphicsPipeline(), pipelineState.getPipelineHandle());
    EXPECT_FALSE(state.hasDirtyVertexStreams());
    EXPECT_TRUE(submitAndWait(firstCmdBuffer, new VulkanPayload(*m_queue)));

    // 2. Second command buffer: onCommandBufferBegun + prepareForDraw re-binds the pipeline and vertex streams.
    VulkanCommandBuffer* const secondCmdBuffer = beginCommandBuffer();
    ASSERT_NE(secondCmdBuffer, nullptr);
    EXPECT_EQ(secondCmdBuffer->getCurrentGraphicsPipeline(), VK_NULL_HANDLE);

    state.onCommandBufferBegun();
    EXPECT_TRUE(state.hasDirtyVertexStreams());
    ASSERT_TRUE(state.prepareForDraw(*secondCmdBuffer));
    EXPECT_EQ(secondCmdBuffer->getCurrentGraphicsPipeline(), pipelineState.getPipelineHandle());
    EXPECT_FALSE(state.hasDirtyVertexStreams());
    EXPECT_TRUE(submitAndWait(secondCmdBuffer, new VulkanPayload(*m_queue)));
}

TEST_F(VulkanPendingStateLiveTest, ComputePrepareForDispatchCachesPipelineHandleOnCommandBuffer)
{
    const pipeline::ComputePipelineStateRef computeRef = createComputePipeline();
    ASSERT_TRUE(computeRef.isValid());
    const VulkanComputePipelineState& computeState = *getInnerState(computeRef);
    ASSERT_NE(computeState.getPipelineHandle(), VK_NULL_HANDLE);

    VulkanCommandBuffer* const commandBuffer = beginCommandBuffer();
    ASSERT_NE(commandBuffer, nullptr);
    EXPECT_EQ(commandBuffer->getCurrentComputePipeline(), VK_NULL_HANDLE);
    {
        VulkanPendingComputeState state(*m_device, *m_context, m_defaults);
        state.setComputePipeline(computeState);
        ASSERT_TRUE(state.prepareForDispatch(*commandBuffer));
        EXPECT_EQ(commandBuffer->getCurrentComputePipeline(), computeState.getPipelineHandle());
    }
    EXPECT_TRUE(submitAndWait(commandBuffer, new VulkanPayload(*m_queue)));
}

}   // namespace gp::rhi::vulkan::tests
