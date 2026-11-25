// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/IComputeContext.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "commands/VulkanQueue.hpp"
#include "commands/VulkanSubmission.hpp"
#include "console/ConsoleManager.hpp"        // IWYU pragma: keep
#include "console/IConsoleVariable.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanDeviceCapabilities.hpp"   // IWYU pragma: keep
#include "device/VulkanQueueFamilySelection.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "sync/VulkanFence.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanCommandEnumsTest, QueueTypeNamesAndSignaledSemaphoreTracker)
{
    EXPECT_STREQ(getVulkanQueueTypeName(VulkanQueueType::Graphics), "Graphics");
    EXPECT_STREQ(getVulkanQueueTypeName(VulkanQueueType::AsyncCompute), "AsyncCompute");
    EXPECT_STREQ(getVulkanQueueTypeName(VulkanQueueType::Transfer), "Transfer");
    EXPECT_STREQ(getVulkanQueueTypeName(VulkanQueueType::Count), "Unknown");

    VulkanSignaledSemaphoreTracker tracker{};
    EXPECT_TRUE(tracker.isEmpty());
    EXPECT_EQ(tracker.size(), 0u);
    EXPECT_FALSE(tracker.contains(nullptr));
    EXPECT_EQ(tracker.find(nullptr), nullptr);
    EXPECT_FALSE(tracker.remove(nullptr));

    const VulkanSemaphore* const fakeSemA = reinterpret_cast<const VulkanSemaphore*>(0x1000u);
    const VulkanSemaphore* const fakeSemB = reinterpret_cast<const VulkanSemaphore*>(0x2000u);

    tracker.addOrUpdate(nullptr, VulkanBinarySemaphoreSignalInfo{ 1u, VulkanQueueType::Graphics });
    EXPECT_TRUE(tracker.isEmpty());

    tracker.addOrUpdate(fakeSemA, VulkanBinarySemaphoreSignalInfo{ 10u, VulkanQueueType::Graphics });
    tracker.addOrUpdate(fakeSemB, VulkanBinarySemaphoreSignalInfo{ 20u, VulkanQueueType::AsyncCompute });
    EXPECT_EQ(tracker.size(), 2u);
    EXPECT_TRUE(tracker.contains(fakeSemA));
    EXPECT_TRUE(tracker.contains(fakeSemB));

    const VulkanBinarySemaphoreSignalInfo* const infoA = tracker.find(fakeSemA);
    ASSERT_NE(infoA, nullptr);
    EXPECT_EQ(infoA->timelineValue, 10u);
    EXPECT_EQ(infoA->queueType, VulkanQueueType::Graphics);

    // Updating an existing semaphore overwrites metadata in place without growing size.
    tracker.addOrUpdate(fakeSemA, VulkanBinarySemaphoreSignalInfo{ 42u, VulkanQueueType::Transfer });
    EXPECT_EQ(tracker.size(), 2u);
    ASSERT_NE(tracker.find(fakeSemA), nullptr);
    EXPECT_EQ(tracker.find(fakeSemA)->timelineValue, 42u);
    EXPECT_EQ(tracker.find(fakeSemA)->queueType, VulkanQueueType::Transfer);

    EXPECT_TRUE(tracker.remove(fakeSemA));
    EXPECT_FALSE(tracker.contains(fakeSemA));
    EXPECT_TRUE(tracker.contains(fakeSemB));
    EXPECT_EQ(tracker.size(), 1u);
    EXPECT_FALSE(tracker.remove(fakeSemA));

    tracker.clear();
    EXPECT_TRUE(tracker.isEmpty());
}

TEST(VulkanQueueStagesTest, ComputeSupportedStagesAndAccess)
{
    VkPhysicalDeviceFeatures features{};
    OptionalVulkanDeviceExtensions optionalExts{};
    VkPipelineStageFlags stages = 0u;
    VkAccessFlags access = 0u;

    // Transfer-only queue.
    VulkanQueue::computeSupportedStagesAndAccess(VK_QUEUE_TRANSFER_BIT, features, optionalExts, stages, access);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_HOST_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_TRANSFER_BIT, 0u);
    EXPECT_EQ(stages & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u);
    EXPECT_EQ(stages & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0u);
    EXPECT_NE(access & VK_ACCESS_TRANSFER_READ_BIT, 0u);
    EXPECT_NE(access & VK_ACCESS_TRANSFER_WRITE_BIT, 0u);
    EXPECT_EQ(access & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0u);

    // Compute-only queue with acceleration structures and ray tracing pipeline.
    optionalExts.hasAccelerationStructure = 1u;
    optionalExts.hasRayTracingPipeline = 1u;
    VulkanQueue::computeSupportedStagesAndAccess(VK_QUEUE_COMPUTE_BIT, features, optionalExts, stages, access);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0u);
    EXPECT_EQ(stages & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u);
    EXPECT_NE(access & VK_ACCESS_SHADER_WRITE_BIT, 0u);
    EXPECT_NE(access & VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, 0u);

    // Graphics queue with geometry, tessellation, fragment density map, fragment shading rate, and mesh shaders.
    features.geometryShader = VK_TRUE;
    features.tessellationShader = VK_TRUE;
    optionalExts.hasEXTFragmentDensityMap = 1u;
    optionalExts.hasKHRFragmentShadingRate = 1u;
    optionalExts.hasEXTMeshShader = 1u;
    VulkanQueue::computeSupportedStagesAndAccess(
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, features, optionalExts, stages, access
    );
    EXPECT_NE(stages & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT, 0u);
    EXPECT_NE(stages & VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, 0u);
    EXPECT_NE(access & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0u);
    EXPECT_NE(access & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0u);
    EXPECT_NE(access & VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT, 0u);
    EXPECT_NE(access & VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR, 0u);
}

TEST(VulkanCommandsOfflineTest, NullDeviceGuardsAndPlatformCommandList)
{
    VulkanDevice nullDevice(VK_NULL_HANDLE, VK_NULL_HANDLE, 0u, false);
    VulkanFenceManager fenceManager(nullDevice);
    VulkanQueue queue(nullDevice, fenceManager, 0u, VulkanQueueType::Graphics, 0u);

    EXPECT_FALSE(queue.isValid());
    EXPECT_EQ(queue.getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(queue.getQueueType(), VulkanQueueType::Graphics);
    EXPECT_EQ(queue.getFamilyIndex(), 0u);
    EXPECT_EQ(queue.getQueueIndex(), 0u);
    EXPECT_FALSE(queue.usesTimelineSemaphore());
    EXPECT_EQ(queue.acquireCommandBufferPool(VulkanCommandBufferType::Primary), nullptr);
    EXPECT_EQ(queue.acquireCommandBufferPool(VulkanCommandBufferType::Count), nullptr);

    VulkanCommandBufferPool pool(nullDevice, VulkanCommandBufferType::Primary);
    EXPECT_FALSE(pool.isValid());
    EXPECT_FALSE(pool.create(0u));
    EXPECT_EQ(pool.acquireReadyCommandBuffer(), nullptr);
    EXPECT_FALSE(pool.resetPool());
    EXPECT_EQ(pool.freeUnusedCmdBuffers(true), 0u);
    EXPECT_EQ(pool.getAllocatedCount(), 0u);
    EXPECT_EQ(pool.getFreeCount(), 0u);

    const VkSubmitInfo emptySubmit{};
    EXPECT_EQ(queue.submit(gp::ConstVectorView<VkSubmitInfo>(&emptySubmit, 1u)), VK_ERROR_INITIALIZATION_FAILED);
    const VkSubmitInfo2 emptySubmit2{};
    EXPECT_EQ(queue.submit2(gp::ConstVectorView<VkSubmitInfo2>(&emptySubmit2, 1u)), VK_ERROR_INITIALIZATION_FAILED);

    VulkanSignaledSemaphoreTracker tracker{};
    EXPECT_EQ(queue.submitQueuedPayloads(tracker), 0);
    EXPECT_EQ(queue.processInterruptQueue(0u), 0);

    // Payload preExecute, merge, and complete behavior without live GPU handles.
    VulkanPayload primaryPayload(queue);
    bool preExecuteCalled = false;
    VkQueue receivedQueue = reinterpret_cast<VkQueue>(0x1u);
    primaryPayload.preExecuteCallback = [&preExecuteCalled, &receivedQueue](const VkQueue q) noexcept -> void
    {
        preExecuteCalled = true;
        receivedQueue = q;
    };
    primaryPayload.preExecute();
    EXPECT_TRUE(preExecuteCalled);
    EXPECT_EQ(receivedQueue, VK_NULL_HANDLE);

    VulkanPayload mergedPayload(queue);
    const VulkanSyncPointRef sp1 = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "SP1");
    const VulkanSyncPointRef sp2 = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "SP2");
    primaryPayload.syncPoints.pushBack(sp1);
    mergedPayload.syncPoints.pushBack(sp2);

    primaryPayload.merge(mergedPayload);
    EXPECT_TRUE(mergedPayload.syncPoints.isEmpty());
    EXPECT_EQ(primaryPayload.syncPoints.size(), 2u);

    primaryPayload.complete();
    EXPECT_TRUE(sp1->isComplete());
    EXPECT_TRUE(sp2->isComplete());

    // Polymorphic IPlatformCommandList ownership of unsubmitted payloads.
    VulkanPlatformCommandList* const concreteList = new VulkanPlatformCommandList();
    concreteList->payloads.pushBack(new VulkanPayload(queue));
    concreteList->payloads.pushBack(new VulkanPayload(queue));
    EXPECT_EQ(concreteList->payloads.size(), 2u);

    gp::rhi::command::IPlatformCommandList* const baseList = concreteList;
    delete baseList;
}

class VulkanCommandsLiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Host Vulkan loader library unavailable; skipping live queue/command buffer test.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-commands-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        if (!m_instance.initialize(desc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping live queue/command buffer test.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No Vulkan physical device found; skipping live queue/command buffer test.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping live queue/command buffer test.";
        }

        m_fenceManager = new VulkanFenceManager(*m_device);
        const gp::Int32 graphicsFamily = m_device->getQueueFamilyIndices().graphicsFamily;
        ASSERT_GE(graphicsFamily, 0);
        m_queue = new VulkanQueue(
            *m_device, *m_fenceManager, static_cast<gp::UInt32>(graphicsFamily), VulkanQueueType::Graphics, 0u
        );
        ASSERT_TRUE(m_queue->isValid());
    }

    virtual void TearDown() override
    {
        delete m_queue;
        m_queue = nullptr;

        if (m_fenceManager != nullptr)
        {
            m_fenceManager->deinit();
            delete m_fenceManager;
            m_fenceManager = nullptr;
        }

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

    bool m_loaderLoaded{ false };
    VulkanInstance m_instance{};
    VulkanDevice* m_device{ nullptr };
    VulkanFenceManager* m_fenceManager{ nullptr };
    VulkanQueue* m_queue{ nullptr };
};

TEST_F(VulkanCommandsLiveTest, QueueCommandBufferLifecycleAndPoolRecycling)
{
    EXPECT_NE(m_queue->getSupportedStageBits() & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0u);
    EXPECT_NE(m_queue->getSupportedAccessBits() & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0u);

    VulkanCommandBufferPool* const pool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(pool->isValid());
    EXPECT_EQ(pool->getCommandBufferType(), VulkanCommandBufferType::Primary);
    EXPECT_EQ(pool->getQueueFamilyIndex(), m_queue->getFamilyIndex());

    // Release and re-acquire verifies pool reuse.
    m_queue->releaseCommandBufferPool(pool);
    VulkanCommandBufferPool* const reusedPool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
    EXPECT_EQ(reusedPool, pool);

    VulkanCommandBuffer* const cmdBuffer = pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmdBuffer, nullptr);
    EXPECT_TRUE(cmdBuffer->isValid());
    EXPECT_TRUE(cmdBuffer->isAllocated());
    EXPECT_EQ(cmdBuffer->getState(), VulkanCommandBufferState::ReadyForBegin);
    EXPECT_EQ(pool->getAllocatedCount(), 1u);
    EXPECT_EQ(pool->getFreeCount(), 0u);

    // Invalid state transitions fail cleanly.
    EXPECT_FALSE(cmdBuffer->end());
    EXPECT_FALSE(cmdBuffer->endRenderPass());

    ASSERT_TRUE(cmdBuffer->begin());
    EXPECT_TRUE(cmdBuffer->hasBegun());
    EXPECT_TRUE(cmdBuffer->isOutsideRenderPass());
    EXPECT_FALSE(cmdBuffer->isInsideRenderPass());
    EXPECT_FALSE(cmdBuffer->begin());

    // Dynamic state & pipeline cache helpers.
    cmdBuffer->markDynamicStateDirty();
    EXPECT_TRUE(cmdBuffer->needsDynamicStateSet());
    cmdBuffer->setBoundaryContext(true);
    EXPECT_TRUE(cmdBuffer->isBoundaryContext());
    EXPECT_EQ(cmdBuffer->getCurrentGraphicsPipeline(), VK_NULL_HANDLE);
    EXPECT_EQ(cmdBuffer->getCurrentComputePipeline(), VK_NULL_HANDLE);

    ASSERT_TRUE(cmdBuffer->end());
    EXPECT_TRUE(cmdBuffer->hasEnded());
    EXPECT_FALSE(cmdBuffer->hasBegun());

    // Submit via VulkanPayload with preExecuteCallback, submissionEvents, and syncPoints.
    bool preExecuteRan = false;
    VkQueue preExecuteQueue = VK_NULL_HANDLE;
    const VulkanSyncPointRef submitEvent = VulkanSyncPoint::create(VulkanSyncPointType::Context, "OnSubmit");
    const VulkanSyncPointRef completeEvent = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "OnComplete");

    VulkanPayload* const payload = new VulkanPayload(*m_queue);
    payload->commandBuffers.pushBack(cmdBuffer);
    payload->submissionEvents.pushBack(submitEvent);
    payload->syncPoints.pushBack(completeEvent);
    payload->preExecuteCallback = [&preExecuteRan, &preExecuteQueue](const VkQueue q) noexcept -> void
    {
        preExecuteRan = true;
        preExecuteQueue = q;
    };

    m_queue->enqueuePayload(payload);
    EXPECT_EQ(m_queue->getPendingSubmissionCount(), 1u);

    VulkanSignaledSemaphoreTracker tracker{};
    const gp::Int32 submitted = m_queue->submitQueuedPayloads(tracker, false);
    EXPECT_EQ(submitted, 1);
    EXPECT_TRUE(preExecuteRan);
    EXPECT_EQ(preExecuteQueue, m_queue->getHandle());
    EXPECT_TRUE(submitEvent->isComplete());
    EXPECT_TRUE(cmdBuffer->isSubmitted());
    EXPECT_EQ(m_queue->getPendingSubmissionCount(), 0u);
    EXPECT_EQ(m_queue->getPendingInterruptCount(), 1u);

    const gp::Int32 completed = m_queue->processInterruptQueue(5'000'000'000ULL);
    EXPECT_EQ(completed, 1);
    EXPECT_TRUE(completeEvent->isComplete());
    EXPECT_EQ(m_queue->getPendingInterruptCount(), 0u);
    EXPECT_EQ(cmdBuffer->getState(), VulkanCommandBufferState::NeedReset);
    EXPECT_EQ(pool->getFreeCount(), 1u);

    // Re-acquiring returns the recycled NeedReset buffer; begin() resets it cleanly.
    VulkanCommandBuffer* const recycled = pool->acquireReadyCommandBuffer();
    EXPECT_EQ(recycled, cmdBuffer);
    ASSERT_TRUE(recycled->begin());
    ASSERT_TRUE(recycled->end());
    recycled->reset();

    m_queue->releaseCommandBufferPool(pool);
}

TEST_F(VulkanCommandsLiveTest, SecondaryCommandBufferAndSubmit2Path)
{
    VulkanCommandBufferPool* const primaryPool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
    VulkanCommandBufferPool* const secondaryPool =
        m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Secondary);
    ASSERT_NE(primaryPool, nullptr);
    ASSERT_NE(secondaryPool, nullptr);

    VulkanCommandBuffer* const primaryCmd = primaryPool->acquireReadyCommandBuffer();
    VulkanCommandBuffer* const secondaryCmd = secondaryPool->acquireReadyCommandBuffer();
    ASSERT_NE(primaryCmd, nullptr);
    ASSERT_NE(secondaryCmd, nullptr);

    ASSERT_TRUE(secondaryCmd->begin());
    ASSERT_TRUE(secondaryCmd->end());

    ASSERT_TRUE(primaryCmd->begin());
    ASSERT_TRUE(primaryCmd->executeSecondaryCommandBuffer(secondaryCmd));
    EXPECT_TRUE(secondaryCmd->isSubmitted());
    ASSERT_TRUE(primaryCmd->end());

    const VulkanSyncPointRef completeSp = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "Submit2Complete");
    VulkanPayload* const payload = new VulkanPayload(*m_queue);
    payload->commandBuffers.pushBack(primaryCmd);
    payload->syncPoints.pushBack(completeSp);

    m_queue->enqueuePayload(payload);

    VulkanSignaledSemaphoreTracker tracker{};
    const gp::Int32 submitted = m_queue->submitQueuedPayloads(tracker, true);
    EXPECT_EQ(submitted, 1);

    const gp::Int32 completed = m_queue->processInterruptQueue(5'000'000'000ULL);
    EXPECT_EQ(completed, 1);
    EXPECT_TRUE(completeSp->isComplete());

    // Primary reset must have recursively reset and recycled the executed secondary command buffer.
    EXPECT_EQ(primaryCmd->getState(), VulkanCommandBufferState::NeedReset);
    EXPECT_EQ(secondaryCmd->getState(), VulkanCommandBufferState::NeedReset);
    EXPECT_EQ(primaryPool->getFreeCount(), 1u);
    EXPECT_EQ(secondaryPool->getFreeCount(), 1u);

    m_queue->releaseCommandBufferPool(secondaryPool);
    m_queue->releaseCommandBufferPool(primaryPool);
}

TEST_F(VulkanCommandsLiveTest, BinarySemaphoreGatingAndStaleBufferCleanup)
{
    const VulkanSemaphoreRef binarySem = VulkanSemaphore::create(*m_device, VulkanSemaphoreFlags::None);
    ASSERT_NE(binarySem, nullptr);
    ASSERT_TRUE(binarySem->isValid());

    VulkanSignaledSemaphoreTracker tracker{};

    // Payload B waits on binarySem before any producer has signaled it: must stay queued.
    const VulkanSyncPointRef consumerDone = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "ConsumerDone");
    VulkanPayload* const consumerPayload = new VulkanPayload(*m_queue);
    consumerPayload->waitSemaphores.pushBack(binarySem);
    consumerPayload->waitFlags.pushBack(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    consumerPayload->syncPoints.pushBack(consumerDone);
    m_queue->enqueuePayload(consumerPayload);

    EXPECT_EQ(m_queue->submitQueuedPayloads(tracker), 0);
    EXPECT_EQ(m_queue->getPendingSubmissionCount(), 1u);

    // Submit a producer directly on a second queue instance (same family) to signal binarySem.
    VulkanQueue producerQueue(*m_device, *m_fenceManager, m_queue->getFamilyIndex(), VulkanQueueType::Graphics, 0u);
    ASSERT_TRUE(producerQueue.isValid());

    const VulkanSyncPointRef producerDone = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "ProducerDone");
    VulkanPayload* const producerPayload = new VulkanPayload(producerQueue);
    producerPayload->signalSemaphores.pushBack(binarySem);
    producerPayload->syncPoints.pushBack(producerDone);
    producerQueue.enqueuePayload(producerPayload);

    EXPECT_EQ(producerQueue.submitQueuedPayloads(tracker), 1);
    EXPECT_TRUE(tracker.contains(binarySem.get()));

    // Now the consumer queue unblocks, consumes binarySem from the tracker, and completes.
    EXPECT_EQ(m_queue->submitQueuedPayloads(tracker), 1);
    EXPECT_FALSE(tracker.contains(binarySem.get()));

    EXPECT_EQ(producerQueue.processInterruptQueue(5'000'000'000ULL), 1);
    EXPECT_EQ(m_queue->processInterruptQueue(5'000'000'000ULL), 1);
    EXPECT_TRUE(producerDone->isComplete());
    EXPECT_TRUE(consumerDone->isComplete());

    // Stale command buffer sweep & vkTrimCommandPool.
    VulkanCommandBufferPool* const pool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
    ASSERT_NE(pool, nullptr);
    VulkanCommandBuffer* const staleCmd = pool->acquireReadyCommandBuffer();
    ASSERT_NE(staleCmd, nullptr);
    staleCmd->reset();
    staleCmd->setLastActiveTimestampNs(0u);
    EXPECT_EQ(pool->getAllocatedCount(), 1u);
    EXPECT_EQ(pool->getFreeCount(), 1u);

    const gp::UInt32 freed = pool->freeUnusedCmdBuffers(true, 1u);
    EXPECT_EQ(freed, 1u);
    EXPECT_EQ(pool->getAllocatedCount(), 0u);
    EXPECT_EQ(pool->getFreeCount(), 0u);
    EXPECT_TRUE(pool->resetPool());

    m_queue->releaseCommandBufferPool(pool);
}

TEST_F(VulkanCommandsLiveTest, FenceFallbackPathSplitBarriersAndPoolEdgeCases)
{
    // Exercise timeline getters on the default queue first (if timeline semaphores are active).
    if (m_queue->usesTimelineSemaphore())
    {
        ASSERT_NE(m_queue->getTimelineSemaphore(), nullptr);
        EXPECT_TRUE(m_queue->getTimelineSemaphore()->isValid());
        const gp::UInt64 beforeSubmitted = m_queue->getSubmittedTimelineValue();
        VulkanPayload* const pingPayload = new VulkanPayload(*m_queue);
        m_queue->enqueuePayload(pingPayload);
        VulkanSignaledSemaphoreTracker tracker{};
        EXPECT_EQ(m_queue->submitQueuedPayloads(tracker), 1);
        EXPECT_GT(m_queue->getSubmittedTimelineValue(), beforeSubmitted);
        EXPECT_EQ(m_queue->processInterruptQueue(5'000'000'000ULL), 1);
        EXPECT_GE(m_queue->getCompletedTimelineValue(), m_queue->getSubmittedTimelineValue());
    }

    // Exercise the fence-backed fallback path by disabling timeline semaphores via CVar.
    gp::console::IConsoleVariable* const timelineCvar =
        gp::console::ConsoleManager::get().findConsoleVariable("r.Vulkan.Submission.AllowTimelineSemaphores");
    ASSERT_NE(timelineCvar, nullptr);
    const gp::Int32 prevTimelineValue = timelineCvar->getInt();
    (void)timelineCvar->set(0, gp::console::ConsoleVarSetBy::CommandLine);

    {
        VulkanQueue fenceQueue(*m_device, *m_fenceManager, m_queue->getFamilyIndex(), VulkanQueueType::Graphics, 0u);
        ASSERT_TRUE(fenceQueue.isValid());
        EXPECT_FALSE(fenceQueue.usesTimelineSemaphore());

        // Null guards and direct submit() + waitUntilIdle().
        fenceQueue.enqueuePayload(nullptr);
        fenceQueue.releaseCommandBufferPool(nullptr);
        VulkanFence* const directFence = m_fenceManager->allocateFence(false);
        ASSERT_NE(directFence, nullptr);
        const VkSubmitInfo emptyInfo = zeroVulkanStruct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        EXPECT_EQ(fenceQueue.submit(gp::ConstVectorView<VkSubmitInfo>(&emptyInfo, 1u), directFence), VK_SUCCESS);
        fenceQueue.waitUntilIdle();
        EXPECT_TRUE(m_fenceManager->isFenceSignaled(directFence));
        VulkanFence* fenceToRelease = directFence;
        m_fenceManager->releaseFence(fenceToRelease);

        // Fence-backed payload submission + completion.
        const VulkanSyncPointRef doneSp = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "FencePathDone");
        VulkanPayload* const fencePayload = new VulkanPayload(fenceQueue);
        fencePayload->syncPoints.pushBack(doneSp);
        fenceQueue.enqueuePayload(fencePayload);

        VulkanSignaledSemaphoreTracker tracker{};
        EXPECT_EQ(fenceQueue.submitQueuedPayloads(tracker, false), 1);
        EXPECT_EQ(fenceQueue.processInterruptQueue(5'000'000'000ULL), 1);
        EXPECT_TRUE(doneSp->isComplete());
        EXPECT_EQ(m_fenceManager->getUsedFenceCount(), 0u);

        // Teardown with pending interrupt + pending submission payloads must recycle fences cleanly.
        VulkanPayload* const pendingInterruptPayload = new VulkanPayload(fenceQueue);
        fenceQueue.enqueuePayload(pendingInterruptPayload);
        EXPECT_EQ(fenceQueue.submitQueuedPayloads(tracker, false), 1);

        VulkanPayload* const unsubmittedPayload = new VulkanPayload(fenceQueue);
        fenceQueue.enqueuePayload(unsubmittedPayload);
    }
    EXPECT_EQ(m_fenceManager->getUsedFenceCount(), 0u);

    (void)timelineCvar->set(prevTimelineValue, gp::console::ConsoleVarSetBy::CommandLine);

    // Split barriers, wait semaphores, pipeline cache, secondary negative paths, and pool resetPool() state sync.
    VulkanCommandBufferPool* const primaryPool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
    VulkanCommandBufferPool* const secondaryPool =
        m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Secondary);
    VulkanCommandBufferPool* const parallelPool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Parallel);
    ASSERT_NE(primaryPool, nullptr);
    ASSERT_NE(secondaryPool, nullptr);
    ASSERT_NE(parallelPool, nullptr);

    // Pool create() is idempotent and recycle deduplicates.
    EXPECT_TRUE(primaryPool->create(m_queue->getFamilyIndex()));
    primaryPool->recycleCommandBuffer(nullptr);

    VulkanCommandBuffer* const primaryCmd = primaryPool->acquireReadyCommandBuffer();
    VulkanCommandBuffer* const secondaryCmd = secondaryPool->acquireReadyCommandBuffer();
    VulkanCommandBuffer* const parallelCmd = parallelPool->acquireReadyCommandBuffer();
    ASSERT_NE(primaryCmd, nullptr);
    ASSERT_NE(secondaryCmd, nullptr);
    ASSERT_NE(parallelCmd, nullptr);

    // Wait semaphore attachment & clear + pipeline cache setters.
    const VulkanSemaphoreRef dummySem = VulkanSemaphore::create(*m_device, VulkanSemaphoreFlags::ExternallySignaled);
    primaryCmd->addWaitSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VulkanSemaphoreRef{});
    EXPECT_EQ(primaryCmd->getWaitSemaphores().size(), 0u);
    primaryCmd->addWaitSemaphore(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, dummySem);
    EXPECT_EQ(primaryCmd->getWaitSemaphores().size(), 1u);
    EXPECT_EQ(primaryCmd->getWaitFlags().size(), 1u);
    primaryCmd->clearWaitSemaphores();
    EXPECT_EQ(primaryCmd->getWaitSemaphores().size(), 0u);

    const VkPipeline fakePipe = reinterpret_cast<VkPipeline>(0x1234u);
    primaryCmd->setCurrentGraphicsPipeline(fakePipe);
    primaryCmd->setCurrentComputePipeline(fakePipe);
    EXPECT_EQ(primaryCmd->getCurrentGraphicsPipeline(), fakePipe);
    EXPECT_EQ(primaryCmd->getCurrentComputePipeline(), fakePipe);

    // Secondary command buffer rejects render-pass/dynamic-rendering commands and invalid executeSecondary calls.
    ASSERT_TRUE(secondaryCmd->begin());
    const VkRenderPassBeginInfo dummyRpInfo =
        zeroVulkanStruct<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    const VkRenderingInfo dummyDynInfo = zeroVulkanStruct<VkRenderingInfo>(VK_STRUCTURE_TYPE_RENDERING_INFO);
    EXPECT_FALSE(secondaryCmd->beginRenderPass(dummyRpInfo));
    EXPECT_FALSE(secondaryCmd->endRenderPass());
    EXPECT_FALSE(secondaryCmd->beginDynamicRendering(dummyDynInfo));
    EXPECT_FALSE(secondaryCmd->endDynamicRendering());
    EXPECT_FALSE(secondaryCmd->executeSecondaryCommandBuffer(secondaryCmd));
    ASSERT_TRUE(secondaryCmd->end());

    // Live VkEvent split barrier recording + automatic vkCmdResetEvent on end().
    VkEvent splitEvent = VK_NULL_HANDLE;
    const VkEventCreateInfo eventInfo = zeroVulkanStruct<VkEventCreateInfo>(VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
    ASSERT_EQ(vkCreateEvent(m_device->getHandle(), &eventInfo, GP_VULKAN_CPU_ALLOCATOR, &splitEvent), VK_SUCCESS);

    ASSERT_TRUE(primaryCmd->begin());
    EXPECT_EQ(primaryCmd->getCurrentGraphicsPipeline(), VK_NULL_HANDLE);
    EXPECT_FALSE(primaryCmd->beginSplitBarrier(VK_NULL_HANDLE, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));
    EXPECT_TRUE(primaryCmd->beginSplitBarrier(splitEvent, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));
    EXPECT_TRUE(
        primaryCmd->endSplitBarrier(splitEvent, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)
    );
    EXPECT_FALSE(primaryCmd->executeSecondaryCommandBuffer(nullptr));
    EXPECT_FALSE(primaryCmd->executeSecondaryCommandBuffer(primaryCmd));
    ASSERT_TRUE(primaryCmd->end());

    // Merge parallel command buffer into a payload.
    ASSERT_TRUE(parallelCmd->begin());
    ASSERT_TRUE(parallelCmd->end());
    VulkanPayload mergedTarget(*m_queue);
    VulkanPayload parallelPayload(*m_queue);
    parallelPayload.commandBuffers.pushBack(parallelCmd);
    mergedTarget.merge(parallelPayload);
    EXPECT_EQ(mergedTarget.commandBuffers.size(), 1u);
    EXPECT_TRUE(parallelPayload.commandBuffers.isEmpty());
    mergedTarget.complete();

    // resetPool() brings all owned buffers back to ReadyForBegin and recycles them cleanly.
    EXPECT_TRUE(primaryPool->resetPool());
    EXPECT_EQ(primaryCmd->getState(), VulkanCommandBufferState::ReadyForBegin);
    EXPECT_EQ(primaryPool->getFreeCount(), 1u);
    primaryPool->recycleCommandBuffer(primaryCmd);
    EXPECT_EQ(primaryPool->getFreeCount(), 1u);

    vkDestroyEvent(m_device->getHandle(), splitEvent, GP_VULKAN_CPU_ALLOCATOR);

    m_queue->releaseCommandBufferPool(parallelPool);
    m_queue->releaseCommandBufferPool(secondaryPool);
    m_queue->releaseCommandBufferPool(primaryPool);
}

}   // namespace gp::rhi::vulkan::tests
