// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanTransitionProcessor.hpp"
#include "barriers/VulkanBarrierContext.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierEnums.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"   // IWYU pragma: keep
#include "barriers/VulkanTransitionData.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/Transition.hpp"
#include "sync/TransitionDesc.hpp"
#include "VulkanBarrierTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

// Global state used by testOnCreateVulkanTransition / testOnReleaseVulkanTransition.
// These are plain function pointers; they cannot capture locals, so device and settings live at file scope.
VulkanDevice* g_procTestDevice{ nullptr };
VulkanBarrierSettings g_procTestSettings{};

void testOnCreateVulkanTransition(
    sync::Transition* const transition, const sync::TransitionCreateInfo& createInfo
) noexcept
{
    createVulkanTransition(*g_procTestDevice, transition, createInfo, g_procTestSettings);
}

void testOnReleaseVulkanTransition(sync::Transition* const transition) noexcept
{
    releaseVulkanTransition(transition);
}

// Fake buffer handle. It is never dereferenced.
const VkBuffer kProcFakeVkBuffer = reinterpret_cast<VkBuffer>(static_cast<gp::UIntPtr>(0x3000u));

// Fake buffer resource pointer for FakeResolver. The processor never dereferences it.
resource::Buffer* const kProcFakeBufferPtr = reinterpret_cast<resource::Buffer*>(static_cast<gp::UIntPtr>(0x5000u));

// Builds a VulkanBarrierContext for processor tests with configurable device descriptor.
VulkanBarrierContext makeProcContext(
    DummyComputeContext& ctx,
    const FakeResolver& resolver,
    RecordingRecorder& recorder,
    const sync::PipelineStage executingPipeline,
    const VulkanBarrierDeviceDesc& deviceDesc,
    const VulkanBarrierSettings& settings = VulkanBarrierSettings{}
) noexcept
{
    return VulkanBarrierContext(
        ctx, resolver, recorder, executingPipeline, deviceDesc, VulkanBarrierTranslationDesc{}, settings
    );
}

// Returns a VulkanBarrierDeviceDesc configured for single-queue (Graphics only), no parallel rendering.
VulkanBarrierDeviceDesc makeLegacyDeviceDesc() noexcept
{
    VulkanBarrierDeviceDesc desc{};
    desc.supportsParallelRendering = false;
    desc.graphicsQueueFamilyIndex = 0u;
    desc.computeQueueFamilyIndex = 0u;
    desc.needsQueueFamilyOwnershipTransfers = false;
    desc.hasAsyncComputeQueue = false;
    return desc;
}

// Returns a VulkanBarrierDeviceDesc configured with sync2 support and separate queue families.
VulkanBarrierDeviceDesc makeSync2DeviceDesc() noexcept
{
    VulkanBarrierDeviceDesc desc{};
    desc.supportsParallelRendering = true;
    desc.graphicsQueueFamilyIndex = 0u;
    desc.computeQueueFamilyIndex = 1u;
    desc.needsQueueFamilyOwnershipTransfers = true;
    desc.hasAsyncComputeQueue = true;
    desc.computeSupportedStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    desc.computeSupportedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT |
                                  VK_ACCESS_TRANSFER_WRITE_BIT;
    return desc;
}

// Creates a Graphics→Graphics non-split transition that carries a single buffer barrier.
// The resolver must have kProcFakeBufferPtr → kProcFakeVkBuffer registered before the context is used.
sync::Transition* createSameQueueBufferTransition() noexcept
{
    const sync::TransitionInfo bufferInfo(
        kProcFakeBufferPtr, sync::ResourceAccess::UAVCompute, sync::ResourceAccess::SRVCompute
    );

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::NoSplit;
    createInfo.transitionInfos = gp::ConstVectorView<sync::TransitionInfo>(&bufferInfo, 1u);

    return sync::TransitionController::create(createInfo);
}

// Creates a Graphics→AsyncCompute transition (may acquire a semaphore from the device).
sync::Transition* createCrossQueueTransition() noexcept
{
    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::AsyncCompute;
    createInfo.flags = sync::TransitionCreateFlags::None;

    return sync::TransitionController::create(createInfo);
}

// Creates a buffer transition between the given pipelines. NoSplit keeps it from taking a split-barrier event.
sync::Transition* createBufferTransition(
    const sync::PipelineStage srcPipelines,
    const sync::PipelineStage dstPipelines,
    const sync::ResourceAccess accessBefore,
    const sync::ResourceAccess accessAfter
) noexcept
{
    const sync::TransitionInfo bufferInfo(kProcFakeBufferPtr, accessBefore, accessAfter);

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = srcPipelines;
    createInfo.dstPipelines = dstPipelines;
    createInfo.flags = sync::TransitionCreateFlags::NoSplit;
    createInfo.transitionInfos = gp::ConstVectorView<sync::TransitionInfo>(&bufferInfo, 1u);

    return sync::TransitionController::create(createInfo);
}

// Fills `count` memory barriers with distinct source stages so each batch can be told apart.
void fillMemoryBarriers(VkMemoryBarrier2* const barriers, const gp::UInt32 count) noexcept
{
    for (gp::UInt32 index = 0u; index < count; ++index)
    {
        barriers[index] = VkMemoryBarrier2{};
        barriers[index].sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        barriers[index].srcStageMask = static_cast<VkPipelineStageFlags2>(1u) << index;
        barriers[index].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }
}

}   // namespace

// Live device fixture for all VulkanTransitionProcessor tests.
class VulkanTransitionProcessorTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Vulkan loader unavailable; skipping processor tests.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc instanceDesc{};
        instanceDesc.applicationName = "gp-rhi-vulkan-processor-test";
        instanceDesc.allowRetryWithoutLayers = true;
        instanceDesc.allowMissingSurfaceFunctions = true;
        instanceDesc.enableDebugCallbacks = false;

        if (!m_instance.initialize(instanceDesc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping processor tests.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No physical device found; skipping processor tests.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping processor tests.";
        }

        g_procTestDevice = m_device;
        g_procTestSettings = VulkanBarrierSettings{};
        g_procTestSettings.allowSplitBarriers = false;
        g_procTestSettings.useMemoryBarrierOpt = true;
        g_procTestSettings.maxBarriersPerBatch = -1;

        sync::TransitionBackendHooks hooks{};
        hooks.privateDataSizeInBytes = getVulkanTransitionPrivateDataSize();
        hooks.privateDataAlignInBytes = getVulkanTransitionPrivateDataAlign();
        hooks.onCreate = &testOnCreateVulkanTransition;
        hooks.onRelease = &testOnReleaseVulkanTransition;
        sync::TransitionController::configureBackend(hooks);
    }

    virtual void TearDown() override
    {
        sync::TransitionController::resetBackend();
        g_procTestDevice = nullptr;

        if (m_device != nullptr)
        {
            m_device->destroy();
            delete m_device;
            m_device = nullptr;
        }

        m_instance.shutdown();

        if (m_loaderLoaded)
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
        }
    }

    VulkanInstance m_instance{};
    VulkanDevice* m_device{ nullptr };
    bool m_loaderLoaded{ false };
};

// beginVulkanTransitions on a legacy device (no sync2) records nothing.
TEST_F(VulkanTransitionProcessorTest, BeginVulkanTransitions_LegacyDevice_NothingRecorded)
{
    sync::Transition* const transition = createSameQueueBufferTransition();
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeLegacyDeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    beginVulkanTransitions(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u));

    // Legacy path has nothing to record at begin.
    EXPECT_TRUE(recorder.pipelineBarrierCalls.isEmpty());
    EXPECT_TRUE(recorder.barrier2Calls.isEmpty());
    EXPECT_TRUE(recorder.splitCalls.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// endVulkanTransitions on a legacy device issues one pipelineBarrier call for a same-queue buffer.
TEST_F(VulkanTransitionProcessorTest, EndVulkanTransitions_LegacyDevice_CallsPipelineBarrier)
{
    sync::Transition* const transition = createSameQueueBufferTransition();
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    RecordingRecorder recorder{};

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = false;   // force explicit buffer barrier, not memory barrier

    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeLegacyDeviceDesc(), settings);

    const sync::Transition* const transitions[] = { transition };
    endVulkanTransitions(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u));

    EXPECT_EQ(recorder.pipelineBarrierCalls.size(), 1u);
    EXPECT_TRUE(recorder.barrier2Calls.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// processVulkanTransitionLegacy emits one pipelineBarrier that carries a buffer barrier.
TEST_F(VulkanTransitionProcessorTest, LegacyPath_SameQueue_Buffer_ProducesBufferBarrier)
{
    sync::Transition* const transition = createSameQueueBufferTransition();
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(
        static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer, 256u)
    );
    RecordingRecorder recorder{};

    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = false;

    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeLegacyDeviceDesc(), settings);

    const sync::Transition* const transitions[] = { transition };
    processVulkanTransitionLegacy(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u));

    ASSERT_EQ(recorder.pipelineBarrierCalls.size(), 1u);
    EXPECT_EQ(recorder.pipelineBarrierCalls[0u].bufferBarriers.size(), 1u);
    EXPECT_EQ(recorder.pipelineBarrierCalls[0u].bufferBarriers[0u].buffer, kProcFakeVkBuffer);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// processVulkanTransitionSync2(begin=true) on a same-queue no-event transition records nothing.
TEST_F(VulkanTransitionProcessorTest, Sync2_SameQueue_NoEvent_Begin_RecordsNothing)
{
    sync::Transition* const transition = createSameQueueBufferTransition();
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    processVulkanTransitionSync2(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), true);

    // Same-queue, no event: begin half is skipped.
    EXPECT_TRUE(recorder.barrier2Calls.isEmpty());
    EXPECT_TRUE(recorder.pipelineBarrierCalls.isEmpty());
    EXPECT_TRUE(recorder.splitCalls.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// processVulkanTransitionSync2(begin=false) on a same-queue no-event transition issues pipelineBarrier2.
TEST_F(VulkanTransitionProcessorTest, Sync2_SameQueue_NoEvent_End_CallsPipelineBarrier2)
{
    sync::Transition* const transition = createSameQueueBufferTransition();
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    processVulkanTransitionSync2(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    EXPECT_EQ(recorder.barrier2Calls.size(), 1u);
    EXPECT_TRUE(recorder.splitCalls.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// beginVulkanTransitions with sync2 on a same-queue no-event transition records nothing.
TEST_F(VulkanTransitionProcessorTest, BeginVulkanTransitions_Sync2_SameQueue_NoEvent_RecordsNothing)
{
    sync::Transition* const transition = createSameQueueBufferTransition();
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    beginVulkanTransitions(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u));

    EXPECT_TRUE(recorder.pipelineBarrierCalls.isEmpty());
    EXPECT_TRUE(recorder.barrier2Calls.isEmpty());
    EXPECT_TRUE(recorder.splitCalls.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// endVulkanTransitions with sync2 on a same-queue no-event transition issues pipelineBarrier2.
TEST_F(VulkanTransitionProcessorTest, EndVulkanTransitions_Sync2_SameQueue_CallsPipelineBarrier2)
{
    sync::Transition* const transition = createSameQueueBufferTransition();
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    endVulkanTransitions(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u));

    EXPECT_EQ(recorder.barrier2Calls.size(), 1u);
    EXPECT_TRUE(recorder.pipelineBarrierCalls.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// processVulkanTransitionSync2 (begin=true) on a split-barrier transition calls beginSplitBarrier.
TEST_F(VulkanTransitionProcessorTest, Sync2_SplitBarrier_Begin_CallsBeginSplitBarrier)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping split barrier begin test.";
    }

    g_procTestSettings.allowSplitBarriers = true;
    g_procTestSettings.maxBarriersPerBatch = -1;

    // No NoSplit flag: createVulkanTransition will take an event when canUseVulkanSplitBarrier is true.
    const sync::TransitionInfo bufferInfo(
        kProcFakeBufferPtr, sync::ResourceAccess::UAVCompute, sync::ResourceAccess::SRVCompute
    );

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::None;
    createInfo.transitionInfos = gp::ConstVectorView<sync::TransitionInfo>(&bufferInfo, 1u);

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    if (data->eventHandle == VK_NULL_HANDLE)
    {
        // Event pool exhausted; return early so the test doesn't falsely fail.
        sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
        sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
        GTEST_SKIP() << "No barrier event available from pool; skipping split begin test.";
    }

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    processVulkanTransitionSync2(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), true);

    ASSERT_EQ(recorder.splitCalls.size(), 1u);
    EXPECT_TRUE(recorder.splitCalls[0u].isBegin);
    EXPECT_EQ(recorder.splitCalls[0u].event, data->eventHandle);
    EXPECT_TRUE(recorder.barrier2Calls.isEmpty());

    // The event is consumed by the split barrier; return it to the pool before retiring.
    m_device->releaseBarrierEvent(data->eventHandle);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// processVulkanTransitionSync2 (begin=false) on a split-barrier transition calls endSplitBarrier.
TEST_F(VulkanTransitionProcessorTest, Sync2_SplitBarrier_End_CallsEndSplitBarrier)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping split barrier end test.";
    }

    g_procTestSettings.allowSplitBarriers = true;
    g_procTestSettings.maxBarriersPerBatch = -1;

    const sync::TransitionInfo bufferInfo(
        kProcFakeBufferPtr, sync::ResourceAccess::UAVCompute, sync::ResourceAccess::SRVCompute
    );

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::None;
    createInfo.transitionInfos = gp::ConstVectorView<sync::TransitionInfo>(&bufferInfo, 1u);

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    if (data->eventHandle == VK_NULL_HANDLE)
    {
        sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
        sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
        GTEST_SKIP() << "No barrier event available from pool; skipping split end test.";
    }

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    processVulkanTransitionSync2(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    ASSERT_EQ(recorder.splitCalls.size(), 1u);
    EXPECT_FALSE(recorder.splitCalls[0u].isBegin);
    EXPECT_EQ(recorder.splitCalls[0u].event, data->eventHandle);

    m_device->releaseBarrierEvent(data->eventHandle);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// processVulkanCrossQueueSemaphores (begin=true): signals the semaphore when executing on the source queue.
TEST_F(VulkanTransitionProcessorTest, CrossQueueSemaphores_Begin_SignalsSemaphore)
{
    sync::Transition* const transition = createCrossQueueTransition();
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    if (!data->semaphore)
    {
        sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
        sync::TransitionController::markEnd(transition, sync::PipelineStage::AsyncCompute);
        GTEST_SKIP() << "Transition carries no semaphore; skipping signal test.";
    }

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    // Graphics executes the signal (source) half.
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    processVulkanCrossQueueSemaphores(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), true);

    EXPECT_EQ(recorder.signalSemaphores.size(), 1u);
    EXPECT_TRUE(recorder.waitSemaphores.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::AsyncCompute);
}

// processVulkanCrossQueueSemaphores (begin=false): waits on the semaphore when executing on the destination queue.
TEST_F(VulkanTransitionProcessorTest, CrossQueueSemaphores_End_WaitsSemaphore)
{
    sync::Transition* const transition = createCrossQueueTransition();
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);

    if (!data->semaphore)
    {
        sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
        sync::TransitionController::markEnd(transition, sync::PipelineStage::AsyncCompute);
        GTEST_SKIP() << "Transition carries no semaphore; skipping wait test.";
    }

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    // AsyncCompute executes the wait (destination) half.
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::AsyncCompute, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    processVulkanCrossQueueSemaphores(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    EXPECT_TRUE(recorder.signalSemaphores.isEmpty());
    EXPECT_EQ(recorder.waitSemaphores.size(), 1u);
    EXPECT_EQ(recorder.waitSemaphores[0u].waitStageMask, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::AsyncCompute);
}

// handleVulkanReservedResourceCommits: transition with no commitInfo forwards nothing to the recorder.
TEST_F(VulkanTransitionProcessorTest, ReservedCommits_NoCommitInfo_NothingForwarded)
{
    sync::Transition* const transition = createSameQueueBufferTransition();
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    handleVulkanReservedResourceCommits(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u));

    EXPECT_TRUE(recorder.lastCommitArgs.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// handleVulkanReservedResourceCommits: transition with commitInfo forwards one commit descriptor.
TEST_F(VulkanTransitionProcessorTest, ReservedCommits_WithCommitInfo_ForwardsToRecorder)
{
    // Transition with a CommitResourceInfo on its buffer entry.
    const sync::TransitionInfo commitInfo(
        kProcFakeBufferPtr,
        sync::ResourceAccess::UAVCompute,
        sync::ResourceAccess::CopyDest,
        sync::CommitResourceInfo(4096u)
    );

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::NoSplit;
    createInfo.transitionInfos = gp::ConstVectorView<sync::TransitionInfo>(&commitInfo, 1u);

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    handleVulkanReservedResourceCommits(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u));

    ASSERT_EQ(recorder.lastCommitArgs.size(), 1u);
    EXPECT_EQ(recorder.lastCommitArgs[0u].resource, kProcFakeBufferPtr);
    EXPECT_EQ(recorder.lastCommitArgs[0u].commitSizeInBytes, 4096u);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// submitVulkanSync2Barriers with no barriers records one pipelineBarrier2 call with zero counts.
TEST_F(VulkanTransitionProcessorTest, SubmitSync2Barriers_EmptyBarriers_RecordsNothing)
{
    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    submitVulkanSync2Barriers(
        context,
        false,
        gp::ConstVectorView<VkMemoryBarrier2>{},
        gp::ConstVectorView<VkBufferMemoryBarrier2>{},
        gp::ConstVectorView<VkImageMemoryBarrier2>{}
    );

    // Nothing is recorded when every view is empty.
    EXPECT_TRUE(recorder.barrier2Calls.isEmpty());
}

// sendVulkanBatchedBarriers splits five barriers into calls of 2, 2 and 1, then clears the pointer and count.
TEST(VulkanTransitionProcessorBatchTest, SendBatchedBarriers_FiveBarriersBatchOfTwo_RecordsThreeCallsAndResets)
{
    VkMemoryBarrier2 barriers[5]{};
    fillMemoryBarriers(barriers, 5u);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    VulkanBarrierSettings settings{};
    settings.maxBarriersPerBatch = 2;
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc(), settings);

    VkDependencyInfo batchInfo{};
    batchInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    batchInfo.pMemoryBarriers = barriers;
    batchInfo.memoryBarrierCount = 0u;

    sendVulkanBatchedBarriers(context, batchInfo, batchInfo.pMemoryBarriers, batchInfo.memoryBarrierCount, 5);

    ASSERT_EQ(recorder.barrier2Calls.size(), 3u);
    ASSERT_EQ(recorder.barrier2Calls[0u].memoryBarriers.size(), 2u);
    ASSERT_EQ(recorder.barrier2Calls[1u].memoryBarriers.size(), 2u);
    ASSERT_EQ(recorder.barrier2Calls[2u].memoryBarriers.size(), 1u);
    EXPECT_EQ(recorder.barrier2Calls[0u].memoryBarriers[0u].srcStageMask, barriers[0].srcStageMask);
    EXPECT_EQ(recorder.barrier2Calls[1u].memoryBarriers[0u].srcStageMask, barriers[2].srcStageMask);
    EXPECT_EQ(recorder.barrier2Calls[2u].memoryBarriers[0u].srcStageMask, barriers[4].srcStageMask);
    EXPECT_EQ(batchInfo.pMemoryBarriers, nullptr);
    EXPECT_EQ(batchInfo.memoryBarrierCount, 0u);
}

// sendVulkanBatchedBarriers with no barriers records nothing and still clears the pointer and count.
TEST(VulkanTransitionProcessorBatchTest, SendBatchedBarriers_ZeroBarriers_RecordsNothingAndResets)
{
    VkMemoryBarrier2 barrier{};

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    VulkanBarrierSettings settings{};
    settings.maxBarriersPerBatch = 4;
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc(), settings);

    VkDependencyInfo batchInfo{};
    batchInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    batchInfo.pMemoryBarriers = &barrier;
    batchInfo.memoryBarrierCount = 0u;

    sendVulkanBatchedBarriers(context, batchInfo, batchInfo.pMemoryBarriers, batchInfo.memoryBarrierCount, 0);

    EXPECT_TRUE(recorder.barrier2Calls.isEmpty());
    EXPECT_EQ(batchInfo.pMemoryBarriers, nullptr);
    EXPECT_EQ(batchInfo.memoryBarrierCount, 0u);
}

// submitVulkanSync2Barriers sends each barrier type in its own batches once the total reaches the limit.
TEST(VulkanTransitionProcessorBatchTest, SubmitSync2Barriers_OverBatchLimit_BatchesEachBarrierType)
{
    VkMemoryBarrier2 memoryBarriers[3]{};
    fillMemoryBarriers(memoryBarriers, 3u);

    VkBufferMemoryBarrier2 bufferBarriers[2]{};
    for (VkBufferMemoryBarrier2& barrier: bufferBarriers)
    {
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier.buffer = kProcFakeVkBuffer;
    }

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    VulkanBarrierSettings settings{};
    settings.maxBarriersPerBatch = 2;
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc(), settings);

    submitVulkanSync2Barriers(
        context,
        false,
        gp::ConstVectorView<VkMemoryBarrier2>(memoryBarriers, 3u),
        gp::ConstVectorView<VkBufferMemoryBarrier2>(bufferBarriers, 2u),
        gp::ConstVectorView<VkImageMemoryBarrier2>{}
    );

    // Memory barriers go out as 2 + 1, then the buffer barriers as one batch of 2.
    ASSERT_EQ(recorder.barrier2Calls.size(), 3u);
    EXPECT_EQ(recorder.barrier2Calls[0u].memoryBarriers.size(), 2u);
    EXPECT_TRUE(recorder.barrier2Calls[0u].bufferBarriers.isEmpty());
    EXPECT_EQ(recorder.barrier2Calls[1u].memoryBarriers.size(), 1u);
    EXPECT_TRUE(recorder.barrier2Calls[2u].memoryBarriers.isEmpty());
    EXPECT_EQ(recorder.barrier2Calls[2u].bufferBarriers.size(), 2u);
}

// submitVulkanSync2Barriers below the batch limit sends everything in one call.
TEST(VulkanTransitionProcessorBatchTest, SubmitSync2Barriers_UnderBatchLimit_RecordsSingleCall)
{
    VkMemoryBarrier2 memoryBarriers[2]{};
    fillMemoryBarriers(memoryBarriers, 2u);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    VulkanBarrierSettings settings{};
    settings.maxBarriersPerBatch = 3;
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc(), settings);

    submitVulkanSync2Barriers(
        context,
        false,
        gp::ConstVectorView<VkMemoryBarrier2>(memoryBarriers, 2u),
        gp::ConstVectorView<VkBufferMemoryBarrier2>{},
        gp::ConstVectorView<VkImageMemoryBarrier2>{}
    );

    ASSERT_EQ(recorder.barrier2Calls.size(), 1u);
    EXPECT_EQ(recorder.barrier2Calls[0u].memoryBarriers.size(), 2u);
}

// A split transition that converts to no barrier at all returns its event to the pool at end.
TEST_F(VulkanTransitionProcessorTest, Sync2_SplitBarrier_EndWithoutBarriers_ReleasesEvent)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping split barrier release test.";
    }

    g_procTestSettings.allowSplitBarriers = true;
    g_procTestSettings.maxBarriersPerBatch = -1;

    sync::TransitionCreateInfo createInfo{};
    createInfo.srcPipelines = sync::PipelineStage::Graphics;
    createInfo.dstPipelines = sync::PipelineStage::Graphics;
    createInfo.flags = sync::TransitionCreateFlags::None;

    sync::Transition* const transition = sync::TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    const VulkanTransitionData* const data = transition->getPrivateData<VulkanTransitionData>();
    ASSERT_NE(data, nullptr);
    ASSERT_NE(data->eventHandle, VK_NULL_HANDLE);

    DummyComputeContext ctx{};
    FakeResolver resolver{};
    RecordingRecorder recorder{};
    const VulkanBarrierContext context =
        makeProcContext(ctx, resolver, recorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc());

    const sync::Transition* const transitions[] = { transition };
    processVulkanTransitionSync2(context, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    EXPECT_TRUE(recorder.splitCalls.isEmpty());
    EXPECT_TRUE(recorder.barrier2Calls.isEmpty());
    ASSERT_EQ(recorder.releasedEvents.size(), 1u);
    EXPECT_EQ(recorder.releasedEvents[0u], data->eventHandle);

    // The fake recorder does not touch the pool, so give the event back here.
    m_device->releaseBarrierEvent(data->eventHandle);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::Graphics);
}

// A 1..1 cross-queue transition with ownership transfers records a release on graphics and an acquire on compute.
TEST_F(VulkanTransitionProcessorTest, Sync2_CrossQueueOneToOne_WithOwnershipTransfers_RecordsOnBothQueues)
{
    sync::Transition* const transition = createBufferTransition(
        sync::PipelineStage::Graphics,
        sync::PipelineStage::AsyncCompute,
        sync::ResourceAccess::UAVCompute,
        sync::ResourceAccess::SRVCompute
    );
    ASSERT_NE(transition, nullptr);

    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = false;

    const sync::Transition* const transitions[] = { transition };

    DummyComputeContext graphicsCtx{};
    RecordingRecorder graphicsRecorder{};
    const VulkanBarrierContext graphicsContext = makeProcContext(
        graphicsCtx, resolver, graphicsRecorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc(), settings
    );
    processVulkanTransitionSync2(graphicsContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), true);

    DummyComputeContext computeCtx{};
    RecordingRecorder computeRecorder{};
    const VulkanBarrierContext computeContext = makeProcContext(
        computeCtx, resolver, computeRecorder, sync::PipelineStage::AsyncCompute, makeSync2DeviceDesc(), settings
    );
    processVulkanTransitionSync2(computeContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    ASSERT_EQ(graphicsRecorder.barrier2Calls.size(), 1u);
    EXPECT_EQ(graphicsRecorder.barrier2Calls[0u].bufferBarriers.size(), 1u);
    ASSERT_EQ(computeRecorder.barrier2Calls.size(), 1u);
    ASSERT_EQ(computeRecorder.barrier2Calls[0u].bufferBarriers.size(), 1u);

    // The acquire half runs on the compute queue, so only compute-supported bits remain.
    const VkBufferMemoryBarrier2& acquire = computeRecorder.barrier2Calls[0u].bufferBarriers[0u];
    const VkPipelineStageFlags2 computeStages = makeSync2DeviceDesc().computeSupportedStages;
    EXPECT_EQ(acquire.srcStageMask & ~computeStages, 0u);
    EXPECT_EQ(acquire.dstStageMask & ~computeStages, 0u);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::AsyncCompute);
}

// Without ownership transfers, a 1..1 cross-queue transition is recorded once, on the graphics queue.
TEST_F(VulkanTransitionProcessorTest, Sync2_CrossQueueOneToOne_NoOwnershipTransfers_RecordsOnGraphicsOnly)
{
    sync::Transition* const transition = createBufferTransition(
        sync::PipelineStage::Graphics,
        sync::PipelineStage::AsyncCompute,
        sync::ResourceAccess::UAVCompute,
        sync::ResourceAccess::SRVCompute
    );
    ASSERT_NE(transition, nullptr);

    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = false;

    VulkanBarrierDeviceDesc deviceDesc = makeSync2DeviceDesc();
    deviceDesc.needsQueueFamilyOwnershipTransfers = false;

    const sync::Transition* const transitions[] = { transition };

    DummyComputeContext graphicsCtx{};
    RecordingRecorder graphicsRecorder{};
    const VulkanBarrierContext graphicsContext =
        makeProcContext(graphicsCtx, resolver, graphicsRecorder, sync::PipelineStage::Graphics, deviceDesc, settings);
    processVulkanTransitionSync2(graphicsContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), true);

    DummyComputeContext computeCtx{};
    RecordingRecorder computeRecorder{};
    const VulkanBarrierContext computeContext =
        makeProcContext(computeCtx, resolver, computeRecorder, sync::PipelineStage::AsyncCompute, deviceDesc, settings);
    processVulkanTransitionSync2(computeContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    EXPECT_EQ(graphicsRecorder.barrier2Calls.size(), 1u);
    EXPECT_TRUE(computeRecorder.barrier2Calls.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::AsyncCompute);
}

// A 1..N buffer transition records its barrier at begin on the single source pipeline only.
TEST_F(VulkanTransitionProcessorTest, Sync2_OneToMany_Buffer_RecordsOnSourceAtBegin)
{
    sync::Transition* const transition = createBufferTransition(
        sync::PipelineStage::Graphics,
        sync::PipelineStage::All,
        sync::ResourceAccess::UAVCompute,
        sync::ResourceAccess::SRVCompute
    );
    ASSERT_NE(transition, nullptr);

    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = false;

    const sync::Transition* const transitions[] = { transition };

    DummyComputeContext graphicsCtx{};
    RecordingRecorder graphicsRecorder{};
    const VulkanBarrierContext graphicsContext = makeProcContext(
        graphicsCtx, resolver, graphicsRecorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc(), settings
    );
    processVulkanTransitionSync2(graphicsContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), true);
    processVulkanTransitionSync2(graphicsContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    DummyComputeContext computeCtx{};
    RecordingRecorder computeRecorder{};
    const VulkanBarrierContext computeContext = makeProcContext(
        computeCtx, resolver, computeRecorder, sync::PipelineStage::AsyncCompute, makeSync2DeviceDesc(), settings
    );
    processVulkanTransitionSync2(computeContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    // Begin on graphics records the barrier; neither end records anything for a buffer.
    ASSERT_EQ(graphicsRecorder.barrier2Calls.size(), 1u);
    EXPECT_EQ(graphicsRecorder.barrier2Calls[0u].bufferBarriers.size(), 1u);
    EXPECT_TRUE(computeRecorder.barrier2Calls.isEmpty());

    sync::TransitionController::markBegin(transition, sync::PipelineStage::Graphics);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::All);
}

// An N..1 buffer transition records its barrier at end on the compute destination, with compute-only bits.
TEST_F(VulkanTransitionProcessorTest, Sync2_ManyToOne_Buffer_RecordsMaskedOnComputeAtEnd)
{
    sync::Transition* const transition = createBufferTransition(
        sync::PipelineStage::All,
        sync::PipelineStage::AsyncCompute,
        sync::ResourceAccess::SRVGraphics | sync::ResourceAccess::SRVCompute,
        sync::ResourceAccess::UAVCompute
    );
    ASSERT_NE(transition, nullptr);

    FakeResolver resolver{};
    resolver.addMapping(static_cast<const void*>(kProcFakeBufferPtr), makeBufferBarrierResource(kProcFakeVkBuffer));
    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = false;

    const sync::Transition* const transitions[] = { transition };

    DummyComputeContext graphicsCtx{};
    RecordingRecorder graphicsRecorder{};
    const VulkanBarrierContext graphicsContext = makeProcContext(
        graphicsCtx, resolver, graphicsRecorder, sync::PipelineStage::Graphics, makeSync2DeviceDesc(), settings
    );
    processVulkanTransitionSync2(graphicsContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), true);

    DummyComputeContext computeCtx{};
    RecordingRecorder computeRecorder{};
    const VulkanBarrierContext computeContext = makeProcContext(
        computeCtx, resolver, computeRecorder, sync::PipelineStage::AsyncCompute, makeSync2DeviceDesc(), settings
    );
    processVulkanTransitionSync2(computeContext, gp::ConstVectorView<const sync::Transition*>(transitions, 1u), false);

    // The release half has no image to hand over, so graphics records nothing.
    EXPECT_TRUE(graphicsRecorder.barrier2Calls.isEmpty());
    ASSERT_EQ(computeRecorder.barrier2Calls.size(), 1u);
    ASSERT_EQ(computeRecorder.barrier2Calls[0u].bufferBarriers.size(), 1u);

    const VkBufferMemoryBarrier2& barrier = computeRecorder.barrier2Calls[0u].bufferBarriers[0u];
    const VulkanBarrierDeviceDesc deviceDesc = makeSync2DeviceDesc();
    EXPECT_EQ(barrier.srcStageMask & ~static_cast<VkPipelineStageFlags2>(deviceDesc.computeSupportedStages), 0u);
    EXPECT_EQ(barrier.srcAccessMask & ~static_cast<VkAccessFlags2>(deviceDesc.computeSupportedAccess), 0u);
    EXPECT_NE(barrier.srcStageMask & VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 0u);

    sync::TransitionController::markBegin(transition, sync::PipelineStage::All);
    sync::TransitionController::markEnd(transition, sync::PipelineStage::AsyncCompute);
}

}   // namespace gp::rhi::vulkan::tests
