// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanDynamicRHI.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/IComputeContext.hpp"     // IWYU pragma: keep
#include "commands/VulkanQueue.hpp"
#include "containers/arrays/Vector.hpp"     // IWYU pragma: keep
#include "contexts/VulkanContextDesc.hpp"   // IWYU pragma: keep
#include "contexts/VulkanUploadContext.hpp"
#include "CoreMinimal.hpp"                  // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DeviceTypes.hpp"
#include "device/DynamicRHI.hpp"
#include "device/RHIGlobals.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanDynamicRHIModule.hpp"
#include "IVulkanDynamicRHI.hpp"
#include "pipeline/BlendState.hpp"          // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilState.hpp"   // IWYU pragma: keep
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/IVulkanPipelineDeletionListener.hpp"
#include "pipeline/RasterizerState.hpp"     // IWYU pragma: keep
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerState.hpp"        // IWYU pragma: keep
#include "pipeline/SamplerStateDesc.hpp"
#include "pipeline/VertexDeclaration.hpp"   // IWYU pragma: keep
#include "pipeline/VertexDeclarationDesc.hpp"
#include "pipeline/VulkanPipelineDeletionNotifier.hpp"
#include "presentation/Viewport.hpp"   // IWYU pragma: keep
#include "presentation/VulkanViewportRegistry.hpp"
#include "profiling/RHIStats.hpp"
#include "resources/Buffer.hpp"          // IWYU pragma: keep
#include "resources/BufferCreateDesc.hpp"
#include "resources/StagingBuffer.hpp"   // IWYU pragma: keep
#include "resources/Texture.hpp"         // IWYU pragma: keep
#include "resources/TextureCreateDesc.hpp"
#include "resources/UniformBuffer.hpp"   // IWYU pragma: keep
#include "resources/UniformBufferLayoutDesc.hpp"
#include "shaders/ShaderTypes.hpp"       // IWYU pragma: keep
#include "sync/GPUFence.hpp"             // IWYU pragma: keep
#include "sync/RenderQuery.hpp"          // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "views/ShaderResourceView.hpp"   // IWYU pragma: keep
#include "views/ViewDesc.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Brings a full Vulkan RHI up through the module and `rhiInit()`, the way engine startup does.
/// @details Skips when the host has no usable Vulkan driver. Commands go through the real command list executor
///          (not bypass mode), so `finalizeContexts()` and `submitCommandLists()` run on every flush.
class VulkanDynamicRHILiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_EQ(getDynamicRHI(), nullptr) << "A previous test leaked an active RHI.";

        VulkanDynamicRHIModule module{};
        if (!module.isSupported())
        {
            GTEST_SKIP() << "Vulkan backend unavailable on this host; skipping live dynamic RHI test.";
        }

        DynamicRHI* const rhi = module.createRHI(FeatureLevel::Count);
        ASSERT_NE(rhi, nullptr);
        rhiInit(rhi);
        m_isActive = true;

        m_rhi = castDynamicRHI<VulkanDynamicRHI>();
        if ((m_rhi == nullptr) || !m_rhi->isInitialized())
        {
            GTEST_SKIP() << "Vulkan RHI initialization failed on this host; skipping live dynamic RHI test.";
        }
        rhiPostInit();
    }

    virtual void TearDown() override
    {
        if (m_isActive)
        {
            command::getCommandListExecutor().setBypass(false);
            rhiExit();
            m_isActive = false;
        }
        m_rhi = nullptr;
    }

    /// @brief Finalizes and submits the given contexts, as the command list executor does.
    /// @param[in] contexts Contexts to finalize, in submission order.
    /// @return Number of platform command lists produced.
    [[nodiscard]] gp::USize finalizeAndSubmit(const gp::Vector<command::IComputeContext*>& contexts) noexcept
    {
        FinalizeContextsArgs finalizeArgs{};
        for (command::IComputeContext* const context: contexts)
        {
            finalizeArgs.contexts.pushBack(context);
        }

        FinalizeContextsResult result{};
        m_rhi->finalizeContexts(std::move(finalizeArgs), result);
        const gp::USize numCommandLists = result.commandLists.size();
        for (command::IPlatformCommandList* const commandList: result.commandLists)
        {
            EXPECT_NE(commandList, nullptr);
        }

        SubmitCommandListsArgs submitArgs{};
        submitArgs.commandLists = std::move(result.commandLists);
        m_rhi->submitCommandLists(std::move(submitArgs));
        return numCommandLists;
    }

    /// @brief RHI under test.
    VulkanDynamicRHI* m_rhi{ nullptr };

    /// @brief True while `rhiInit()` installed an RHI that `rhiExit()` must remove.
    bool m_isActive{ false };
};

}   // namespace

TEST(VulkanDynamicRHITest, UninitializedRHIReportsItsIdentity)
{
    VulkanDynamicRHI rhi{};

    EXPECT_STREQ(rhi.getName(), "Vulkan");
    EXPECT_EQ(rhi.getInterfaceType(), InterfaceType::Vulkan);
    EXPECT_FALSE(rhi.isInitialized());
    EXPECT_EQ(rhi.getDevice(), nullptr);
    EXPECT_EQ(rhi.getGraphicsQueue(), nullptr);
    EXPECT_EQ(rhi.getAsyncComputeQueue(), nullptr);
    EXPECT_EQ(rhi.getRenderPassManager(), nullptr);
    EXPECT_EQ(rhi.getPipelineStateCache(), nullptr);
    EXPECT_EQ(rhi.getResourceServices().device, nullptr);
}

TEST(VulkanDynamicRHITest, UninitializedInteropGettersReturnNeutralValues)
{
    VulkanDynamicRHI rhi{};

    EXPECT_EQ(rhi.getVulkanApiVersion(), 0u);
    EXPECT_EQ(rhi.getVulkanInstanceHandle(), nullptr);
    EXPECT_EQ(rhi.getVulkanPhysicalDeviceHandle(), nullptr);
    EXPECT_EQ(rhi.getVulkanDeviceHandle(), nullptr);
    EXPECT_EQ(rhi.getNativeDevice(), nullptr);
    EXPECT_EQ(rhi.getVulkanGraphicsQueueHandle(), nullptr);
    EXPECT_EQ(rhi.getVulkanGraphicsQueueIndex(), 0u);
    EXPECT_EQ(rhi.getVulkanGraphicsQueueFamilyIndex(), ~0u);
    EXPECT_EQ(rhi.getVulkanInstanceProcAddress("vkEnumeratePhysicalDevices"), nullptr);
    EXPECT_EQ(rhi.getVulkanDeviceProcAddress("vkCreateBuffer"), nullptr);

    const gp::UInt8* const uuid = rhi.getVulkanDeviceUUID();
    ASSERT_NE(uuid, nullptr);
    for (gp::USize index = 0u; index < VK_UUID_SIZE; ++index)
    {
        EXPECT_EQ(uuid[index], 0u);
    }
}

TEST(VulkanDynamicRHITest, UninitializedRHIHandsOutNoContexts)
{
    VulkanDynamicRHI rhi{};

    EXPECT_EQ(rhi.getDefaultContext(), nullptr);
    EXPECT_EQ(rhi.getDefaultAsyncComputeContext(), nullptr);
    EXPECT_EQ(rhi.getCommandContext(sync::PipelineStage::Graphics), nullptr);
    EXPECT_EQ(rhi.getCommandContext(sync::PipelineStage::AsyncCompute), nullptr);
    EXPECT_EQ(rhi.getNumParallelContexts(), 0u);
    EXPECT_EQ(rhi.getNumFreeParallelContexts(), 0u);
}

TEST(VulkanDynamicRHITest, UninitializedDeviceFactoriesReturnNull)
{
    VulkanDynamicRHI rhi{};

    EXPECT_FALSE(rhi.createSamplerState(pipeline::SamplerStateDesc{}).isValid());
    EXPECT_FALSE(rhi.createVertexShader({}, shader::ShaderHash{}).isValid());
    EXPECT_FALSE(rhi.createPixelShader({}, shader::ShaderHash{}).isValid());
    EXPECT_FALSE(rhi.createComputeShader({}, shader::ShaderHash{}).isValid());
    EXPECT_FALSE(rhi.createGraphicsPipelineState(pipeline::GraphicsPipelineStateInitializer{}).isValid());
    EXPECT_FALSE(rhi.createComputePipelineState(pipeline::ComputePipelineStateInitializer{}).isValid());
    EXPECT_FALSE(rhi.createGPUFence(gp::Name("UninitializedFence")).isValid());
    EXPECT_FALSE(rhi.createViewport(nullptr, 64u, 64u, false, PixelFormat::B8G8R8A8_UNorm).isValid());
    EXPECT_EQ(rhi.getPlatformTextureMaxSampleCount(), 1u);
}

TEST(VulkanDynamicRHITest, UninitializedRHIHasNoUploadContextsAndZeroMemoryStats)
{
    VulkanDynamicRHI rhi{};

    EXPECT_EQ(rhi.acquireUploadContext(), nullptr);
    EXPECT_EQ(rhi.getUploadContextPool(), nullptr);
    EXPECT_EQ(rhi.getViewportRegistry(), nullptr);

    profiling::RHIMemoryStats stats{};
    stats.dedicatedVideoMemory = 42u;
    rhi.getMemoryStats(stats);
    EXPECT_EQ(stats.dedicatedVideoMemory, 0u);
    EXPECT_EQ(stats.totalGraphicsMemory, 0u);
    EXPECT_EQ(stats.budgetVideoMemory, 0u);
}

TEST(VulkanDynamicRHITest, DeviceIndependentStatesWorkWithoutInit)
{
    VulkanDynamicRHI rhi{};

    EXPECT_TRUE(rhi.createRasterizerState(pipeline::RasterizerStateDesc{}).isValid());
    EXPECT_TRUE(rhi.createDepthStencilState(pipeline::DepthStencilStateDesc{}).isValid());
    EXPECT_TRUE(rhi.createBlendState(pipeline::BlendStateDesc{}).isValid());
    EXPECT_GT(rhi.getTransitionPrivateDataSizeInBytes(), 0u);
    EXPECT_GT(rhi.getTransitionPrivateDataAlignInBytes(), 0u);
}

TEST(VulkanDynamicRHITest, UninitializedFrameAndSubmissionCallsAreNoOps)
{
    VulkanDynamicRHI rhi{};

    rhi.postInit();
    rhi.tick(0.016f);
    rhi.beginFrame();
    EndFrameArgs endFrameArgs{};
    endFrameArgs.frameNumber = 1u;
    rhi.endFrame(endFrameArgs);
    rhi.processDeleteQueue();
    rhi.blockUntilGPUIdle();

    FinalizeContextsResult result{};
    rhi.finalizeContexts(FinalizeContextsArgs{}, result);
    EXPECT_TRUE(result.commandLists.isEmpty());
    EXPECT_EQ(result.uploadCommandList, nullptr);

    rhi.submitCommandLists(SubmitCommandListsArgs{});
    EXPECT_FALSE(rhi.isInitialized());
    EXPECT_EQ(rhi.getLastSubmittedSyncPoint(), nullptr);
}

TEST(VulkanDynamicRHITest, ShutdownWithoutInitIsIdempotent)
{
    VulkanDynamicRHI rhi{};

    rhi.shutdown();
    rhi.shutdown();

    EXPECT_FALSE(rhi.isInitialized());
    EXPECT_EQ(rhi.getVulkanDeviceHandle(), nullptr);
}

TEST(VulkanDynamicRHITest, InterfaceLookupFindsOnlyAnInstalledVulkanRHI)
{
    DynamicRHI* const previous = getDynamicRHI();
    VulkanDynamicRHI rhi{};

    setDynamicRHI(nullptr);
    EXPECT_EQ(getVulkanDynamicRHI(), nullptr);

    setDynamicRHI(&rhi);
    EXPECT_EQ(getVulkanDynamicRHI(), static_cast<IVulkanDynamicRHI*>(&rhi));

    setDynamicRHI(previous);
}

TEST_F(VulkanDynamicRHILiveTest, InitCreatesTheDeviceAndEveryService)
{
    EXPECT_TRUE(getRHIGlobals().isRHIInitialized);
    EXPECT_NE(getRHIGlobals().maxSupportedFeatureLevel, FeatureLevel::Count);
    ASSERT_NE(m_rhi->getDevice(), nullptr);
    EXPECT_TRUE(m_rhi->getDevice()->isValid());
    EXPECT_NE(m_rhi->getGraphicsQueue(), nullptr);
    EXPECT_NE(m_rhi->getRenderPassManager(), nullptr);
    EXPECT_NE(m_rhi->getPipelineStateCache(), nullptr);

    const VulkanResourceServices& services = m_rhi->getResourceServices();
    EXPECT_EQ(services.device, m_rhi->getDevice());
    EXPECT_NE(services.memoryManager, nullptr);
    EXPECT_NE(services.stagingManager, nullptr);
    EXPECT_NE(services.deferredDeletionQueue, nullptr);
    EXPECT_NE(services.contextProvider, nullptr);
    EXPECT_NE(services.tempBlockAllocator, nullptr);
    EXPECT_EQ(services.renderPassManager, m_rhi->getRenderPassManager());
}

TEST_F(VulkanDynamicRHILiveTest, InteropGettersExposeTheNativeHandles)
{
    EXPECT_EQ(getVulkanDynamicRHI(), static_cast<IVulkanDynamicRHI*>(m_rhi));
    EXPECT_GE(m_rhi->getVulkanApiVersion(), static_cast<gp::UInt32>(VK_API_VERSION_1_1));
    EXPECT_NE(m_rhi->getVulkanInstanceHandle(), nullptr);
    EXPECT_NE(m_rhi->getVulkanPhysicalDeviceHandle(), nullptr);
    EXPECT_NE(m_rhi->getVulkanDeviceHandle(), nullptr);
    EXPECT_EQ(m_rhi->getNativeDevice(), m_rhi->getVulkanDeviceHandle());
    EXPECT_NE(m_rhi->getVulkanGraphicsQueueHandle(), nullptr);
    EXPECT_NE(m_rhi->getVulkanGraphicsQueueFamilyIndex(), ~0u);
    EXPECT_EQ(m_rhi->getVulkanGraphicsQueueFamilyIndex(), m_rhi->getGraphicsQueue()->getFamilyIndex());
    EXPECT_NE(m_rhi->getVulkanDeviceUUID(), nullptr);
}

TEST_F(VulkanDynamicRHILiveTest, ProcAddressLookupsResolveCoreEntryPoints)
{
    EXPECT_NE(m_rhi->getVulkanInstanceProcAddress("vkEnumeratePhysicalDevices"), nullptr);
    EXPECT_NE(m_rhi->getVulkanDeviceProcAddress("vkCreateBuffer"), nullptr);
    EXPECT_EQ(m_rhi->getVulkanInstanceProcAddress(nullptr), nullptr);
    EXPECT_EQ(m_rhi->getVulkanDeviceProcAddress(nullptr), nullptr);
    EXPECT_EQ(m_rhi->getVulkanDeviceProcAddress("vkGraphicalPlaygroundDoesNotExist"), nullptr);
}

TEST_F(VulkanDynamicRHILiveTest, DescriptorDefaultsAreCreated)
{
    const VulkanDescriptorDefaults& defaults = m_rhi->getDescriptorDefaults();

    EXPECT_NE(defaults.defaultImageView, nullptr);
    EXPECT_NE(defaults.defaultSampler, nullptr);
    for (gp::USize index = 0u; index < kVulkanNumGlobalSamplers; ++index)
    {
        EXPECT_NE(defaults.globalSamplers[index], nullptr) << "Global sampler " << index;
    }

    // Clamped and wrapped variants of one filter are distinct samplers.
    EXPECT_NE(defaults.globalSamplers[0u], defaults.globalSamplers[1u]);
    EXPECT_NE(defaults.globalSamplers[2u], defaults.globalSamplers[4u]);
}

TEST_F(VulkanDynamicRHILiveTest, RegistersAsThePipelineDeletionListener)
{
    EXPECT_EQ(getVulkanPipelineDeletionListener(), static_cast<IVulkanPipelineDeletionListener*>(m_rhi));
}

TEST_F(VulkanDynamicRHILiveTest, ContextAccessorsFollowTheContract)
{
    command::ICommandContext* const defaultContext = m_rhi->getDefaultContext();

    ASSERT_NE(defaultContext, nullptr);
    EXPECT_EQ(m_rhi->getDefaultAsyncComputeContext(), static_cast<command::IComputeContext*>(defaultContext));
    EXPECT_EQ(m_rhi->getCommandContext(sync::PipelineStage::AsyncCompute), nullptr);
    EXPECT_EQ(m_rhi->getNumParallelContexts(), 0u);
}

TEST_F(VulkanDynamicRHILiveTest, ParallelContextsAreRecycledAfterFinalize)
{
    command::IComputeContext* const first = m_rhi->getCommandContext(sync::PipelineStage::Graphics);
    ASSERT_NE(first, nullptr);
    EXPECT_NE(first, m_rhi->getDefaultAsyncComputeContext());
    EXPECT_EQ(m_rhi->getNumParallelContexts(), 1u);
    EXPECT_EQ(m_rhi->getNumFreeParallelContexts(), 0u);

    gp::Vector<command::IComputeContext*> contexts{};
    contexts.pushBack(first);
    EXPECT_EQ(finalizeAndSubmit(contexts), 1u);
    EXPECT_EQ(m_rhi->getNumFreeParallelContexts(), 1u);

    command::IComputeContext* const reused = m_rhi->getCommandContext(sync::PipelineStage::Graphics);
    EXPECT_EQ(reused, first);
    EXPECT_EQ(m_rhi->getNumParallelContexts(), 1u);
    EXPECT_EQ(m_rhi->getNumFreeParallelContexts(), 0u);

    EXPECT_EQ(finalizeAndSubmit(contexts), 1u);
    m_rhi->blockUntilGPUIdle();
}

TEST_F(VulkanDynamicRHILiveTest, FinalizeProducesOneCommandListPerContext)
{
    command::IComputeContext* const first = m_rhi->getCommandContext(sync::PipelineStage::Graphics);
    command::IComputeContext* const second = m_rhi->getCommandContext(sync::PipelineStage::Graphics);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);

    gp::Vector<command::IComputeContext*> contexts{};
    contexts.pushBack(first);
    contexts.pushBack(second);
    EXPECT_EQ(finalizeAndSubmit(contexts), 2u);
    EXPECT_EQ(m_rhi->getNumFreeParallelContexts(), 2u);
}

TEST_F(VulkanDynamicRHILiveTest, AcquiredUploadContextReturnsToThePoolOnFinalize)
{
    VulkanUploadContextPool* const pool = m_rhi->getUploadContextPool();
    ASSERT_NE(pool, nullptr);
    command::IUploadContext* const uploadContext = m_rhi->acquireUploadContext();
    ASSERT_NE(uploadContext, nullptr);
    const gp::USize created = pool->getNumContexts();
    EXPECT_GE(created, 1u);

    // No command context: the upload context still gets a command list of its own.
    FinalizeContextsArgs finalizeArgs{};
    finalizeArgs.uploadContext = uploadContext;
    FinalizeContextsResult result{};
    m_rhi->finalizeContexts(std::move(finalizeArgs), result);
    EXPECT_EQ(result.commandLists.size(), 1u);
    EXPECT_EQ(result.uploadCommandList, nullptr);

    SubmitCommandListsArgs submitArgs{};
    submitArgs.commandLists = std::move(result.commandLists);
    m_rhi->submitCommandLists(std::move(submitArgs));

    EXPECT_EQ(pool->getNumContexts(), created);
    // Released last, so it sits on top of the free list.
    EXPECT_GE(pool->getNumFreeContexts(), 1u);

    // The next acquire reuses the warm context instead of creating one.
    command::IUploadContext* const reused = m_rhi->acquireUploadContext();
    EXPECT_EQ(reused, uploadContext);
    EXPECT_EQ(pool->getNumContexts(), created);

    FinalizeContextsArgs secondArgs{};
    secondArgs.uploadContext = reused;
    secondArgs.contexts.pushBack(m_rhi->getCommandContext(sync::PipelineStage::Graphics));
    FinalizeContextsResult secondResult{};
    m_rhi->finalizeContexts(std::move(secondArgs), secondResult);
    // Merged into the first context's list: one list per context, no extra one.
    EXPECT_EQ(secondResult.commandLists.size(), 1u);
    SubmitCommandListsArgs secondSubmit{};
    secondSubmit.commandLists = std::move(secondResult.commandLists);
    m_rhi->submitCommandLists(std::move(secondSubmit));
    m_rhi->blockUntilGPUIdle();
}

TEST_F(VulkanDynamicRHILiveTest, MemoryStatsReportTheDeviceHeaps)
{
    profiling::RHIMemoryStats stats{};
    m_rhi->getMemoryStats(stats);

    EXPECT_GT(stats.dedicatedVideoMemory, 0u);
    EXPECT_EQ(stats.dedicatedSystemMemory, 0u);
    EXPECT_EQ(stats.totalGraphicsMemory, stats.dedicatedVideoMemory + stats.sharedSystemMemory);
    EXPECT_GT(stats.budgetVideoMemory, 0u);
    EXPECT_LE(stats.availableVideoMemory, stats.budgetVideoMemory);
}

TEST_F(VulkanDynamicRHILiveTest, ViewportServicesCarryTheViewportRegistry)
{
    ASSERT_NE(m_rhi->getViewportRegistry(), nullptr);
    EXPECT_EQ(m_rhi->getViewportRegistry()->getNumViewports(), 0u);
}

TEST_F(VulkanDynamicRHILiveTest, SubmittedWorkCompletesAfterBlockingOnTheGPU)
{
    gp::Vector<command::IComputeContext*> contexts{};
    contexts.pushBack(m_rhi->getDefaultAsyncComputeContext());
    ASSERT_EQ(finalizeAndSubmit(contexts), 1u);

    const VulkanSyncPointRef syncPoint = m_rhi->getLastSubmittedSyncPoint();
    ASSERT_NE(syncPoint, nullptr);

    m_rhi->blockUntilGPUIdle();
    EXPECT_TRUE(syncPoint->isComplete());
}

TEST_F(VulkanDynamicRHILiveTest, FactoriesCreateLiveObjects)
{
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    const pipeline::SamplerStateRef sampler = m_rhi->createSamplerState(pipeline::SamplerStateDesc{});
    const pipeline::SamplerStateRef sameSampler = m_rhi->createSamplerState(pipeline::SamplerStateDesc{});
    ASSERT_TRUE(sampler.isValid());
    EXPECT_EQ(sampler.get(), sameSampler.get());
    EXPECT_TRUE(m_rhi->createVertexDeclaration(pipeline::VertexDeclarationElementList{}).isValid());

    const resource::BufferRef buffer = cmdList.createBuffer(
        resource::BufferCreateDesc::create(
            "VulkanRHITestBuffer",
            256u,
            16u,
            resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::ShaderResource
        )
    );
    ASSERT_TRUE(buffer.isValid());
    m_rhi->bindDebugLabelName(cmdList, buffer.get(), "VulkanRHITestBufferLabel");

    const resource::TextureRef texture = cmdList.createTexture(
        resource::TextureCreateDesc::create2D("VulkanRHITestTexture", 16, 16, PixelFormat::R8G8B8A8_UNorm)
            .setFlags(resource::TextureCreateFlags::ShaderResource)
    );
    ASSERT_TRUE(texture.isValid());
    m_rhi->bindDebugLabelName(cmdList, texture.get(), "VulkanRHITestTextureLabel");

    const view::ShaderResourceViewRef textureView = cmdList.createShaderResourceView(
        texture.get(), view::ViewDesc::createTextureSRV().setDimensionFromTexture(texture.get()).asViewDesc()
    );
    EXPECT_TRUE(textureView.isValid());

    resource::UniformBufferLayoutDesc layoutDesc("VulkanRHITestUB");
    layoutDesc.constantBufferSize = 64u;
    const resource::UniformBufferLayoutRef layout = createUniformBufferLayout(layoutDesc);
    const resource::UniformBufferRef uniformBuffer =
        m_rhi->createUniformBuffer(nullptr, layout.get(), resource::UniformBufferUsage::SingleFrame);
    EXPECT_TRUE(uniformBuffer.isValid());

    const sync::GPUFenceRef fence = m_rhi->createGPUFence(gp::Name("VulkanRHITestFence"));
    EXPECT_TRUE(fence.isValid());
    EXPECT_TRUE(m_rhi->createRenderQuery(sync::RenderQueryType::Occlusion).isValid());
    EXPECT_TRUE(m_rhi->createStagingBuffer().isValid());

    const resource::CalcTextureSizeResult size = m_rhi->calcTexturePlatformSize(
        resource::TextureCreateDesc::create2D("VulkanRHITestSize", 16, 16, PixelFormat::R8G8B8A8_UNorm), 0u
    );
    EXPECT_GE(size.size, 16u * 16u * 4u);
    EXPECT_GT(m_rhi->getMinimumAlignmentForBufferBackedSRV(PixelFormat::R32_Float), 0u);
    EXPECT_GE(m_rhi->getPlatformTextureMaxSampleCount(), 1u);

    cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    m_rhi->blockUntilGPUIdle();
}

TEST_F(VulkanDynamicRHILiveTest, EndFrameRunsMaintenanceAcrossFrames)
{
    command::CommandListImmediate& cmdList = command::getImmediateCommandList();

    for (gp::UInt64 frame = 1u; frame <= 3u; ++frame)
    {
        const resource::TextureRef texture = cmdList.createTexture(
            resource::TextureCreateDesc::create2D("VulkanRHIFrameTexture", 8, 8, PixelFormat::R8G8B8A8_UNorm)
                .setFlags(resource::TextureCreateFlags::ShaderResource)
        );
        EXPECT_TRUE(texture.isValid());

        m_rhi->beginFrame();
        cmdList.immediateFlush(command::ImmediateFlushType::FlushRHIThread);

        EndFrameArgs args{};
        args.frameNumber = frame;
        m_rhi->endFrame(args);
        m_rhi->tick(0.016f);
        m_rhi->processDeleteQueue();
    }

    m_rhi->blockUntilGPUIdle();
    EXPECT_TRUE(m_rhi->isInitialized());
}

TEST_F(VulkanDynamicRHILiveTest, ShutdownReleasesEverythingAndIsIdempotent)
{
    command::getImmediateCommandList().immediateFlush(command::ImmediateFlushType::FlushRHIThread);

    m_rhi->shutdown();

    EXPECT_FALSE(m_rhi->isInitialized());
    EXPECT_EQ(m_rhi->getDevice(), nullptr);
    EXPECT_EQ(m_rhi->getVulkanDeviceHandle(), nullptr);
    EXPECT_EQ(m_rhi->getVulkanInstanceHandle(), nullptr);
    EXPECT_EQ(m_rhi->getVulkanGraphicsQueueFamilyIndex(), ~0u);
    EXPECT_EQ(m_rhi->getDefaultContext(), nullptr);
    EXPECT_EQ(getVulkanPipelineDeletionListener(), nullptr);
    EXPECT_EQ(m_rhi->getViewportRegistry(), nullptr);
    EXPECT_EQ(m_rhi->getUploadContextPool(), nullptr);
    EXPECT_EQ(m_rhi->acquireUploadContext(), nullptr);

    // `rhiExit()` in TearDown shuts the RHI down a second time.
    m_rhi->shutdown();
}

}   // namespace gp::rhi::vulkan::tests
