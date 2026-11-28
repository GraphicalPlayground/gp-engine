// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanCommandBufferBarrierRecorder.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "sync/VulkanBarrierEventPool.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

class VulkanBarrierRecorderTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Vulkan loader unavailable; skipping barrier recorder tests.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-barrier-recorder-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        if (!m_instance.initialize(desc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping barrier recorder tests.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No physical device found; skipping barrier recorder tests.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping barrier recorder tests.";
        }

        const gp::Int32 graphicsFamily = m_device->getQueueFamilyIndices().graphicsFamily;
        if (graphicsFamily < 0)
        {
            m_device->destroy();
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No graphics queue family; skipping barrier recorder tests.";
        }

        m_pool = new VulkanCommandBufferPool(*m_device, VulkanCommandBufferType::Primary);
        if (!m_pool->create(static_cast<gp::UInt32>(graphicsFamily)))
        {
            delete m_pool;
            m_pool = nullptr;
            m_device->destroy();
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "Command buffer pool creation failed; skipping barrier recorder tests.";
        }
    }

    virtual void TearDown() override
    {
        delete m_pool;
        m_pool = nullptr;

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
    VulkanCommandBufferPool* m_pool{ nullptr };
    bool m_loaderLoaded{ false };
};

// getCommandBuffer() returns the command buffer the recorder was bound to.
TEST_F(VulkanBarrierRecorderTest, GetCommandBufferReturnsBindTarget)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    EXPECT_EQ(&recorder.getCommandBuffer(), cmd);

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// addSignalSemaphore appends to the signal list.
TEST_F(VulkanBarrierRecorderTest, AddSignalSemaphoreAppendsToList)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    const VulkanSemaphoreRef semA = VulkanSemaphore::create(*m_device);
    const VulkanSemaphoreRef semB = VulkanSemaphore::create(*m_device);
    EXPECT_EQ(recorder.getSignalSemaphores().size(), 0u);

    recorder.addSignalSemaphore(semA);
    EXPECT_EQ(recorder.getSignalSemaphores().size(), 1u);

    recorder.addSignalSemaphore(semB);
    EXPECT_EQ(recorder.getSignalSemaphores().size(), 2u);

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// addWaitSemaphore appends stage masks and semaphores in parallel, keeping the same size.
TEST_F(VulkanBarrierRecorderTest, AddWaitSemaphoreKeepsParallelLists)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    const VulkanSemaphoreRef sem = VulkanSemaphore::create(*m_device);
    recorder.addWaitSemaphore(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, sem);
    recorder.addWaitSemaphore(VK_PIPELINE_STAGE_TRANSFER_BIT, VulkanSemaphoreRef{});

    EXPECT_EQ(recorder.getWaitSemaphores().size(), 2u);
    EXPECT_EQ(recorder.getWaitStageMasks().size(), 2u);
    EXPECT_EQ(
        recorder.getWaitStageMasks()[0u],
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
    );
    EXPECT_EQ(recorder.getWaitStageMasks()[1u], static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT));

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// addReservedResourceCommits appends; multiple calls accumulate all entries.
TEST_F(VulkanBarrierRecorderTest, AddReservedResourceCommitsAccumulates)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    VulkanCommitReservedResourceDesc descA{};
    descA.resource = nullptr;
    descA.commitSizeInBytes = 1024u;

    VulkanCommitReservedResourceDesc descB{};
    descB.resource = nullptr;
    descB.commitSizeInBytes = 2048u;

    recorder.addReservedResourceCommits(gp::ConstVectorView<VulkanCommitReservedResourceDesc>(&descA, 1u));
    EXPECT_EQ(recorder.getReservedResourceCommits().size(), 1u);

    recorder.addReservedResourceCommits(gp::ConstVectorView<VulkanCommitReservedResourceDesc>(&descB, 1u));
    EXPECT_EQ(recorder.getReservedResourceCommits().size(), 2u);
    EXPECT_EQ(recorder.getReservedResourceCommits()[0u].commitSizeInBytes, 1024u);
    EXPECT_EQ(recorder.getReservedResourceCommits()[1u].commitSizeInBytes, 2048u);

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// clearPendingSubmissionState resets all queued semaphores and commits.
TEST_F(VulkanBarrierRecorderTest, ClearPendingSubmissionStateClearsAll)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    const VulkanSemaphoreRef sem = VulkanSemaphore::create(*m_device);
    recorder.addSignalSemaphore(sem);
    recorder.addWaitSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, sem);

    VulkanCommitReservedResourceDesc desc{};
    recorder.addReservedResourceCommits(gp::ConstVectorView<VulkanCommitReservedResourceDesc>(&desc, 1u));

    EXPECT_EQ(recorder.getSignalSemaphores().size(), 1u);
    EXPECT_EQ(recorder.getWaitSemaphores().size(), 1u);
    EXPECT_EQ(recorder.getReservedResourceCommits().size(), 1u);

    recorder.clearPendingSubmissionState();

    EXPECT_EQ(recorder.getSignalSemaphores().size(), 0u);
    EXPECT_EQ(recorder.getWaitSemaphores().size(), 0u);
    EXPECT_EQ(recorder.getWaitStageMasks().size(), 0u);
    EXPECT_EQ(recorder.getReservedResourceCommits().size(), 0u);

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// releaseBarrierEvent sends the event to the device's barrier event pool.
TEST_F(VulkanBarrierRecorderTest, ReleaseBarrierEventIncreasesDevicePoolCount)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    const VkEvent fakeEvent = reinterpret_cast<VkEvent>(0xABCDu);
    const gp::USize pooledBefore = m_device->getBarrierEventPool().getPooledCount();

    recorder.releaseBarrierEvent(fakeEvent);

    EXPECT_EQ(m_device->getBarrierEventPool().getPooledCount(), pooledBefore + 1u);

    // Take the fake event back out so device teardown does not call vkDestroyEvent on it.
    const VkEvent taken = m_device->getBarrierEventPool().acquire(VK_NULL_HANDLE);
    EXPECT_EQ(taken, fakeEvent);

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// pipelineBarrier (legacy) records into a command buffer that ends successfully.
TEST_F(VulkanBarrierRecorderTest, PipelineBarrierLegacyRecordsSuccessfully)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    // Empty barrier.
    recorder.pipelineBarrier(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0u,
        gp::ConstVectorView<VkMemoryBarrier>{},
        gp::ConstVectorView<VkBufferMemoryBarrier>{},
        gp::ConstVectorView<VkImageMemoryBarrier>{}
    );

    // One memory barrier.
    VkMemoryBarrier memBarrier = zeroVulkanStruct<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    recorder.pipelineBarrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0u,
        gp::ConstVectorView<VkMemoryBarrier>(&memBarrier, 1u),
        gp::ConstVectorView<VkBufferMemoryBarrier>{},
        gp::ConstVectorView<VkImageMemoryBarrier>{}
    );

    EXPECT_TRUE(cmd->end());
    cmd->reset();
}

// pipelineBarrier2 records into a command buffer that ends successfully (skipped if unavailable).
TEST_F(VulkanBarrierRecorderTest, PipelineBarrier2RecordsSuccessfully)
{
    if (dynamic_api::vkCmdPipelineBarrier2KHR == nullptr)
    {
        GTEST_SKIP() << "vkCmdPipelineBarrier2KHR unavailable; skipping pipelineBarrier2 test.";
    }

    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    recorder.pipelineBarrier2(dep);

    EXPECT_TRUE(cmd->end());
    cmd->reset();
}

// beginSplitBarrier / endSplitBarrier record into a command buffer that ends successfully.
TEST_F(VulkanBarrierRecorderTest, SplitBarrierRecordsSuccessfully)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping split barrier recording test.";
    }

    const VkEvent event = m_device->getBarrierEvent();
    ASSERT_NE(event, VK_NULL_HANDLE);

    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VulkanCommandBufferBarrierRecorder recorder(*cmd);

    VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    recorder.beginSplitBarrier(event, dep);
    recorder.endSplitBarrier(event, dep);

    EXPECT_TRUE(cmd->end());

    // reset() returns the event to the pool via releasePooledBarrierEvents().
    cmd->reset();
}

}   // namespace gp::rhi::vulkan::tests
