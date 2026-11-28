// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelineStateCacheManager.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/BlendState.hpp"             // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/ComputePipelineState.hpp"
#include "pipeline/DepthStencilState.hpp"   // IWYU pragma: keep
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineState.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/IVulkanPipelineDeletionListener.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/RasterizerState.hpp"   // IWYU pragma: keep
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/VulkanComputePipelineState.hpp"
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "pipeline/VulkanPipelineDeletionNotifier.hpp"
#include "pipeline/VulkanPipelineSettings.hpp"
#include "pipeline/VulkanWrappedPipelineStates.hpp"
#include "shaders/ComputeShader.hpp"   // IWYU pragma: keep
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderTypes.hpp"
#include "shaders/VertexShader.hpp"
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShaderFactory.hpp"
#include "states/VulkanStateRHI.hpp"
#include "VulkanPipelineTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Returns the inner state behind a compute wrapper.
/// @param[in] ref RHI compute pipeline created by the manager.
/// @return Inner state.
[[nodiscard]] VulkanComputePipelineState* getInnerState(const pipeline::ComputePipelineStateRef& ref) noexcept
{
    return static_cast<VulkanWrappedComputePipelineState*>(ref.get())->getState();
}

/// @brief Returns the inner state behind a graphics wrapper.
/// @param[in] ref RHI graphics pipeline created by the manager.
/// @return Inner state.
[[nodiscard]] VulkanGraphicsPipelineState* getInnerState(const pipeline::GraphicsPipelineStateRef& ref) noexcept
{
    return static_cast<VulkanWrappedGraphicsPipelineState*>(ref.get())->getState();
}

/// @brief Builds an initializer for a vertex-only pipeline without render targets.
/// @param[in] vertexShader Vertex stage.
/// @param[in] blend Blend state.
/// @param[in] rasterizer Rasterizer state.
/// @param[in] depthStencil Depth-stencil state.
/// @return Initializer.
[[nodiscard]] pipeline::GraphicsPipelineStateInitializer makeGraphicsInitializer(
    shader::VertexShader* const vertexShader,
    pipeline::BlendState* const blend,
    pipeline::RasterizerState* const rasterizer,
    pipeline::DepthStencilState* const depthStencil
) noexcept
{
    pipeline::GraphicsPipelineStateInitializer initializer{};
    initializer.boundShaderState = pipeline::BoundShaderStateInput(nullptr, vertexShader, nullptr);
    initializer.blendState = blend;
    initializer.rasterizerState = rasterizer;
    initializer.depthStencilState = depthStencil;
    initializer.primitiveType = pipeline::PrimitiveType::TriangleList;
    initializer.numSamples = 1u;
    return initializer;
}

/// @brief Shared pieces of the offline and live fixtures: shaders, fixed-function states and the manager.
class PipelineCacheTestState
{
public:
    /// @brief Creates the fixed-function states used by every graphics test.
    void createStates() noexcept
    {
        pipeline::RasterizerStateDesc rasterizerDesc{};
        rasterizerDesc.fillMode = pipeline::RasterizerFillMode::Solid;
        m_blend = createVulkanBlendState(pipeline::BlendStateDesc{});
        m_rasterizer = createVulkanRasterizerState(rasterizerDesc);
        m_depthStencil = createVulkanDepthStencilState(pipeline::DepthStencilStateDesc{});
    }

    /// @brief Creates the shaders on a device.
    /// @param[in,out] device Device owning the shaders.
    void createShaders(VulkanDevice& device) noexcept
    {
        const gp::Vector<gp::UInt8> computeBlob =
            makeTestShaderBlob(kSpirvExecutionModelGLCompute, 0x12345678u, "PipelineCS");
        const gp::Vector<gp::UInt8> vertexBlob =
            makeTestShaderBlob(kSpirvExecutionModelVertex, 0x9abcdef0u, "PipelineVS");
        m_computeShader =
            m_factory.createShader<VulkanComputeShader>(device, nullptr, gp::ConstVectorView<gp::UInt8>(computeBlob));
        m_vertexShader =
            m_factory.createShader<VulkanVertexShader>(device, nullptr, gp::ConstVectorView<gp::UInt8>(vertexBlob));
    }

    /// @brief Releases the manager, then every shader and state, flushing the RHI deletion queue in between.
    /// @details The manager owns pipeline states that hold shaders, and shaders unregister from the factory when they
    ///          die, so everything must be gone before the factory and the device.
    void release() noexcept
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        delete m_manager;
        m_manager = nullptr;
        m_computeShader.reset();
        m_vertexShader.reset();
        m_blend.reset();
        m_rasterizer.reset();
        m_depthStencil.reset();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }

    /// @brief Returns an initializer for the vertex-only pipeline.
    /// @return Initializer.
    [[nodiscard]] pipeline::GraphicsPipelineStateInitializer makeInitializer() const noexcept
    {
        return makeGraphicsInitializer(m_vertexShader.get(), m_blend.get(), m_rasterizer.get(), m_depthStencil.get());
    }

    /// @brief Shader factory.
    VulkanShaderFactory m_factory{};

    /// @brief Compute stage.
    gp::RefCountedPtr<VulkanComputeShader> m_computeShader{};

    /// @brief Vertex stage.
    gp::RefCountedPtr<VulkanVertexShader> m_vertexShader{};

    /// @brief Blend state.
    pipeline::BlendStateRef m_blend{};

    /// @brief Rasterizer state.
    pipeline::RasterizerStateRef m_rasterizer{};

    /// @brief Depth-stencil state.
    pipeline::DepthStencilStateRef m_depthStencil{};

    /// @brief Manager under test.
    VulkanPipelineStateCacheManager* m_manager{ nullptr };
};

/// @brief Listener recording every pipeline deletion it is told about.
class RecordingPipelineDeletionListener final : public IVulkanPipelineDeletionListener
{
public:
    /// @copydoc IVulkanPipelineDeletionListener::onGraphicsPipelineDeleted()
    virtual void onGraphicsPipelineDeleted(const VulkanGraphicsPipelineState& pipeline) noexcept override
    {
        ++numGraphicsDeleted;
        lastGraphics = &pipeline;
    }

    /// @copydoc IVulkanPipelineDeletionListener::onComputePipelineDeleted()
    virtual void onComputePipelineDeleted(const VulkanComputePipelineState& pipeline) noexcept override
    {
        ++numComputeDeleted;
        lastCompute = &pipeline;
    }

    /// @brief Number of graphics deletions received.
    gp::UInt32 numGraphicsDeleted{ 0u };

    /// @brief Number of compute deletions received.
    gp::UInt32 numComputeDeleted{ 0u };

    /// @brief Last graphics pipeline reported.
    const VulkanGraphicsPipelineState* lastGraphics{ nullptr };

    /// @brief Last compute pipeline reported.
    const VulkanComputePipelineState* lastCompute{ nullptr };
};

/// @brief Offline fixture: a device without native handles, so no native object is ever created.
class VulkanPipelineStateCacheManagerTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_state.createStates();
        m_state.createShaders(m_offlineDevice);
        m_state.m_manager = new VulkanPipelineStateCacheManager(m_offlineDevice, nullptr, nullptr);
    }

    virtual void TearDown() override
    {
        m_state.release();
    }

    /// @brief Device without native handles.
    VulkanDevice m_offlineDevice{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Shaders, states and manager.
    PipelineCacheTestState m_state{};
};

/// @brief Live fixture building real pipelines.
class VulkanPipelineStateCacheManagerLiveTest : public VulkanResourceLiveTest
{
protected:
    virtual void SetUp() override
    {
        VulkanResourceLiveTest::SetUp();
        if (IsSkipped())
        {
            return;
        }
        m_state.createStates();
        m_state.createShaders(*m_device);
    }

    virtual void TearDown() override
    {
        m_state.release();
        VulkanResourceLiveTest::TearDown();
    }

    /// @brief Creates the manager with LRU settings.
    /// @param[in] settings LRU configuration.
    void createManager(const VulkanPipelineLRUSettings& settings) noexcept
    {
        m_state.m_manager = new VulkanPipelineStateCacheManager(*m_device, nullptr, nullptr, settings);
    }

    /// @brief Shaders, states and manager.
    PipelineCacheTestState m_state{};
};

}   // namespace

TEST(VulkanComputePipelineKeyTest, KeyFollowsShaderAndConstants)
{
    shader::SpecializationContainer constants{};
    constants.append({ 1u, 2u });
    shader::SpecializationContainer otherConstants{};
    otherConstants.append({ 1u, 3u });
    const shader::SpecializationContainer none{};

    const gp::UInt64 key = computeVulkanComputePipelineKey(10u, constants);

    EXPECT_EQ(key, computeVulkanComputePipelineKey(10u, constants));
    EXPECT_NE(key, computeVulkanComputePipelineKey(11u, constants));
    EXPECT_NE(key, computeVulkanComputePipelineKey(10u, otherConstants));
    EXPECT_NE(key, computeVulkanComputePipelineKey(10u, none));
}

TEST_F(VulkanPipelineStateCacheManagerTest, FreshManagerIsEmpty)
{
    VulkanPipelineStateCacheManager& manager = *m_state.m_manager;

    EXPECT_EQ(manager.getNumGraphicsPipelines(), 0u);
    EXPECT_EQ(manager.getNumComputePipelines(), 0u);
    EXPECT_EQ(manager.getNumLayouts(), 0u);
    EXPECT_EQ(manager.getLRUCount(), 0u);
    EXPECT_EQ(manager.getLRUSizeInBytes(), 0u);
    EXPECT_EQ(manager.getGlobalPipelineCache(), VK_NULL_HANDLE);
}

TEST_F(VulkanPipelineStateCacheManagerTest, GraphicsWithoutStatesIsRejected)
{
    pipeline::GraphicsPipelineStateInitializer initializer = m_state.makeInitializer();
    initializer.blendState = nullptr;

    const pipeline::GraphicsPipelineStateRef pipeline = m_state.m_manager->createGraphicsPipelineState(initializer);

    EXPECT_FALSE(pipeline.isValid());
    EXPECT_EQ(m_state.m_manager->getNumGraphicsPipelines(), 0u);
}

TEST_F(VulkanPipelineStateCacheManagerTest, FailedGraphicsCreationLeavesNoEntry)
{
    const pipeline::GraphicsPipelineStateRef pipeline =
        m_state.m_manager->createGraphicsPipelineState(m_state.makeInitializer());

    EXPECT_FALSE(pipeline.isValid());
    EXPECT_EQ(m_state.m_manager->getNumGraphicsPipelines(), 0u);
    EXPECT_EQ(m_state.m_manager->getLRUCount(), 0u);
}

TEST_F(VulkanPipelineStateCacheManagerTest, ComputeWithoutShaderIsRejected)
{
    const pipeline::ComputePipelineStateInitializer initializer{};

    const pipeline::ComputePipelineStateRef pipeline = m_state.m_manager->createComputePipelineState(initializer);

    EXPECT_FALSE(pipeline.isValid());
    EXPECT_EQ(m_state.m_manager->getNumComputePipelines(), 0u);
}

TEST_F(VulkanPipelineStateCacheManagerTest, FailedComputePipelineIsCachedAndInvalid)
{
    pipeline::ComputePipelineStateInitializer initializer{};
    initializer.computeShader = m_state.m_computeShader.get();

    const pipeline::ComputePipelineStateRef first = m_state.m_manager->createComputePipelineState(initializer);
    const pipeline::ComputePipelineStateRef second = m_state.m_manager->createComputePipelineState(initializer);

    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());
    EXPECT_NE(first.get(), second.get());
    EXPECT_FALSE(first->isPipelineValid());
    EXPECT_EQ(getInnerState(first), getInnerState(second));
    EXPECT_EQ(getInnerState(first)->getNumWrappers(), 2);
    EXPECT_EQ(getInnerState(first)->getPipelineHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(getInnerState(first)->getVulkanShader(), static_cast<VulkanShader*>(m_state.m_computeShader.get()));
    EXPECT_EQ(m_state.m_manager->getNumComputePipelines(), 1u);
}

TEST_F(VulkanPipelineStateCacheManagerTest, SpecializationConstantsSplitComputeEntries)
{
    pipeline::ComputePipelineStateInitializer plain{};
    plain.computeShader = m_state.m_computeShader.get();
    pipeline::ComputePipelineStateInitializer specialized = plain;
    specialized.specializationConstants.append({ 4u, 8u });

    const pipeline::ComputePipelineStateRef first = m_state.m_manager->createComputePipelineState(plain);
    const pipeline::ComputePipelineStateRef second = m_state.m_manager->createComputePipelineState(specialized);

    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());
    EXPECT_NE(getInnerState(first), getInnerState(second));
    EXPECT_NE(getInnerState(first)->getKey(), getInnerState(second)->getKey());
    EXPECT_EQ(getInnerState(second)->getSpecializationConstants().size(), 2u);
    EXPECT_EQ(m_state.m_manager->getNumComputePipelines(), 2u);
}

TEST_F(VulkanPipelineStateCacheManagerTest, ReleasingEveryWrapperRemovesTheEntry)
{
    pipeline::ComputePipelineStateInitializer initializer{};
    initializer.computeShader = m_state.m_computeShader.get();
    {
        const pipeline::ComputePipelineStateRef first = m_state.m_manager->createComputePipelineState(initializer);
        {
            const pipeline::ComputePipelineStateRef second = m_state.m_manager->createComputePipelineState(initializer);
            ASSERT_TRUE(second.isValid());
        }
        resource::ResourceDeletionQueue::flushPendingDeletes(true);

        ASSERT_TRUE(first.isValid());
        EXPECT_EQ(getInnerState(first)->getNumWrappers(), 1);
        EXPECT_EQ(m_state.m_manager->getNumComputePipelines(), 1u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(m_state.m_manager->getNumComputePipelines(), 0u);
}

TEST_F(VulkanPipelineStateCacheManagerTest, DestroyCacheDetachesLiveWrappers)
{
    pipeline::ComputePipelineStateInitializer initializer{};
    initializer.computeShader = m_state.m_computeShader.get();
    const pipeline::ComputePipelineStateRef pipeline = m_state.m_manager->createComputePipelineState(initializer);
    ASSERT_TRUE(pipeline.isValid());
    ASSERT_EQ(getInnerState(pipeline)->getOwner(), m_state.m_manager);

    m_state.m_manager->destroyCache();

    EXPECT_EQ(m_state.m_manager->getNumComputePipelines(), 0u);
    EXPECT_EQ(getInnerState(pipeline)->getOwner(), nullptr);
    EXPECT_TRUE(getInnerState(pipeline)->isEvictedFromCache());
}

TEST_F(VulkanPipelineStateCacheManagerTest, FrameNumberRoundTrips)
{
    m_state.m_manager->setFrameNumber(42u);

    EXPECT_EQ(m_state.m_manager->getFrameNumber(), 42u);
}

TEST_F(VulkanPipelineStateCacheManagerLiveTest, ComputePipelineIsCreatedAndShared)
{
    createManager(VulkanPipelineLRUSettings{});
    pipeline::ComputePipelineStateInitializer initializer{};
    initializer.computeShader = m_state.m_computeShader.get();

    const pipeline::ComputePipelineStateRef first = m_state.m_manager->createComputePipelineState(initializer);
    const pipeline::ComputePipelineStateRef second = m_state.m_manager->createComputePipelineState(initializer);

    ASSERT_TRUE(first.isValid());
    EXPECT_TRUE(first->isPipelineValid());
    EXPECT_NE(getInnerState(first)->getPipelineHandle(), VK_NULL_HANDLE);
    ASSERT_NE(getInnerState(first)->getLayout(), nullptr);
    EXPECT_EQ(getInnerState(first), getInnerState(second));
    EXPECT_EQ(m_state.m_manager->getNumLayouts(), 1u);
    EXPECT_NE(m_state.m_manager->getGlobalPipelineCache(), VK_NULL_HANDLE);
}

TEST_F(VulkanPipelineStateCacheManagerLiveTest, SpecializedComputePipelineIsCreated)
{
    createManager(VulkanPipelineLRUSettings{});
    pipeline::ComputePipelineStateInitializer initializer{};
    initializer.computeShader = m_state.m_computeShader.get();
    initializer.specializationConstants.append({ 7u });

    const pipeline::ComputePipelineStateRef pipeline = m_state.m_manager->createComputePipelineState(initializer);

    ASSERT_TRUE(pipeline.isValid());
    EXPECT_TRUE(pipeline->isPipelineValid());
}

TEST_F(VulkanPipelineStateCacheManagerLiveTest, GraphicsPipelineIsCreatedAndShared)
{
    createManager(VulkanPipelineLRUSettings{});
    if (!m_state.m_manager->usesDynamicRendering())
    {
        GTEST_SKIP() << "Device renders through render passes; skipping dynamic rendering pipeline test.";
    }

    const pipeline::GraphicsPipelineStateRef first =
        m_state.m_manager->createGraphicsPipelineState(m_state.makeInitializer());
    const pipeline::GraphicsPipelineStateRef second =
        m_state.m_manager->createGraphicsPipelineState(m_state.makeInitializer());

    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());
    VulkanGraphicsPipelineState* const state = getInnerState(first);
    EXPECT_EQ(state, getInnerState(second));
    EXPECT_NE(state->getPipelineHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(state->getNumWrappers(), 2);
    EXPECT_EQ(state->getRenderPass(), nullptr);
    EXPECT_EQ(first->getShader(shader::ShaderFrequency::Vertex), m_state.m_vertexShader.get());
    EXPECT_EQ(first->getShader(shader::ShaderFrequency::Pixel), nullptr);
    EXPECT_EQ(m_state.m_manager->getNumGraphicsPipelines(), 1u);
    EXPECT_EQ(m_state.m_manager->getLRUCount(), 0u);
}

TEST_F(VulkanPipelineStateCacheManagerLiveTest, LRUEvictsAndRecreatesPipelines)
{
    VulkanPipelineLRUSettings settings{};
    settings.isEnabled = true;
    settings.evictAfterUnusedFrames = 1u;
    createManager(settings);
    if (!m_state.m_manager->usesDynamicRendering())
    {
        GTEST_SKIP() << "Device renders through render passes; skipping dynamic rendering pipeline test.";
    }
    {
        const pipeline::GraphicsPipelineStateRef pipeline =
            m_state.m_manager->createGraphicsPipelineState(m_state.makeInitializer());
        ASSERT_TRUE(pipeline.isValid());
        VulkanGraphicsPipelineState* const state = getInnerState(pipeline);

        EXPECT_EQ(m_state.m_manager->getLRUCount(), 1u);
        EXPECT_GT(m_state.m_manager->getLRUSizeInBytes(), 0u);
        EXPECT_TRUE(state->isInLRU());

        m_state.m_manager->setFrameNumber(10u);
        m_state.m_manager->tickLRU(10u);

        EXPECT_EQ(m_state.m_manager->getLRUCount(), 0u);
        EXPECT_EQ(m_state.m_manager->getLRUSizeInBytes(), 0u);
        EXPECT_FALSE(state->isInLRU());
        EXPECT_EQ(state->getPipelineHandle(), VK_NULL_HANDLE);
        EXPECT_EQ(m_state.m_manager->getNumGraphicsPipelines(), 1u);

        m_state.m_manager->touchGraphicsPipeline(*state);

        EXPECT_NE(state->getPipelineHandle(), VK_NULL_HANDLE);
        EXPECT_TRUE(state->isInLRU());
        EXPECT_EQ(state->getLRUFrame(), 10u);
        EXPECT_EQ(m_state.m_manager->getLRUCount(), 1u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(m_state.m_manager->getLRUCount(), 0u);
    EXPECT_EQ(m_state.m_manager->getNumGraphicsPipelines(), 0u);
}

TEST(VulkanPipelineDeletionNotifierTest, InstallingAListenerReturnsThePreviousOne)
{
    RecordingPipelineDeletionListener first{};
    RecordingPipelineDeletionListener second{};
    IVulkanPipelineDeletionListener* const original = setVulkanPipelineDeletionListener(&first);

    EXPECT_EQ(getVulkanPipelineDeletionListener(), &first);
    EXPECT_EQ(setVulkanPipelineDeletionListener(&second), &first);
    EXPECT_EQ(getVulkanPipelineDeletionListener(), &second);
    EXPECT_EQ(setVulkanPipelineDeletionListener(nullptr), &second);
    EXPECT_EQ(getVulkanPipelineDeletionListener(), nullptr);

    static_cast<void>(setVulkanPipelineDeletionListener(original));
}

TEST_F(VulkanPipelineStateCacheManagerTest, DestroyedComputePipelineNotifiesTheListener)
{
    RecordingPipelineDeletionListener listener{};
    IVulkanPipelineDeletionListener* const original = setVulkanPipelineDeletionListener(&listener);

    pipeline::ComputePipelineStateInitializer initializer{};
    initializer.computeShader = m_state.m_computeShader.get();
    const VulkanComputePipelineState* inner = nullptr;
    {
        const pipeline::ComputePipelineStateRef pipeline = m_state.m_manager->createComputePipelineState(initializer);
        ASSERT_TRUE(pipeline.isValid());
        inner = getInnerState(pipeline);
        EXPECT_EQ(listener.numComputeDeleted, 0u);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(listener.numComputeDeleted, 1u);
    EXPECT_EQ(listener.lastCompute, inner);
    EXPECT_EQ(listener.numGraphicsDeleted, 0u);

    static_cast<void>(setVulkanPipelineDeletionListener(original));
}

TEST_F(VulkanPipelineStateCacheManagerTest, DeletionWithoutAListenerIsSilent)
{
    IVulkanPipelineDeletionListener* const original = setVulkanPipelineDeletionListener(nullptr);

    pipeline::ComputePipelineStateInitializer initializer{};
    initializer.computeShader = m_state.m_computeShader.get();
    {
        const pipeline::ComputePipelineStateRef pipeline = m_state.m_manager->createComputePipelineState(initializer);
        ASSERT_TRUE(pipeline.isValid());
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(m_state.m_manager->getNumComputePipelines(), 0u);
    static_cast<void>(setVulkanPipelineDeletionListener(original));
}

}   // namespace gp::rhi::vulkan::tests
