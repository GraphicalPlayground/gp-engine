// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "sync/VulkanBarrierEventPool.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

class VulkanCommandBufferSplitBarrierTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Vulkan loader unavailable; skipping split barrier tests.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-split-barrier-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        if (!m_instance.initialize(desc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping split barrier tests.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No physical device found; skipping split barrier tests.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping split barrier tests.";
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
            GTEST_SKIP() << "No graphics queue family; skipping split barrier tests.";
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
            GTEST_SKIP() << "Command buffer pool creation failed; skipping split barrier tests.";
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

// getDevice() returns a reference to the device that owns the command buffer.
TEST_F(VulkanCommandBufferSplitBarrierTest, GetDeviceReturnsOwningDevice)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);

    EXPECT_EQ(&cmd->getDevice(), m_device);

    cmd->reset();
}

// beginSplitBarrier(VkDependencyInfo) with a null event returns false.
TEST_F(VulkanCommandBufferSplitBarrierTest, BeginSplitBarrierNullEventReturnsFalse)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    const VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    EXPECT_FALSE(cmd->beginSplitBarrier(VK_NULL_HANDLE, dep));

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// endSplitBarrier(VkDependencyInfo) with a null event returns false.
TEST_F(VulkanCommandBufferSplitBarrierTest, EndSplitBarrierNullEventReturnsFalse)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    const VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    EXPECT_FALSE(cmd->endSplitBarrier(VK_NULL_HANDLE, dep));

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// beginSplitBarrier(VkDependencyInfo) outside IsInsideBegin state returns false.
TEST_F(VulkanCommandBufferSplitBarrierTest, BeginSplitBarrierOutsideBeginReturnsFalse)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);

    const VkEvent fakeEvent = reinterpret_cast<VkEvent>(0x1u);
    const VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    EXPECT_FALSE(cmd->beginSplitBarrier(fakeEvent, dep));

    cmd->reset();
}

// endSplitBarrier(VkDependencyInfo) outside IsInsideBegin state returns false.
TEST_F(VulkanCommandBufferSplitBarrierTest, EndSplitBarrierOutsideBeginReturnsFalse)
{
    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);

    const VkEvent fakeEvent = reinterpret_cast<VkEvent>(0x1u);
    const VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    EXPECT_FALSE(cmd->endSplitBarrier(fakeEvent, dep));

    cmd->reset();
}

// beginSplitBarrier returns false when vkCmdSetEvent2KHR is unavailable.
TEST_F(VulkanCommandBufferSplitBarrierTest, BeginSplitBarrierReturnsFalseWhenEntryPointMissing)
{
    if (dynamic_api::vkCmdSetEvent2KHR != nullptr)
    {
        GTEST_SKIP() << "vkCmdSetEvent2KHR is loaded; skipping missing-entry-point test.";
    }

    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    const VkEvent fakeEvent = reinterpret_cast<VkEvent>(0x1u);
    const VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    EXPECT_FALSE(cmd->beginSplitBarrier(fakeEvent, dep));

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// endSplitBarrier returns false when vkCmdWaitEvents2KHR is unavailable.
TEST_F(VulkanCommandBufferSplitBarrierTest, EndSplitBarrierReturnsFalseWhenEntryPointMissing)
{
    if (dynamic_api::vkCmdWaitEvents2KHR != nullptr)
    {
        GTEST_SKIP() << "vkCmdWaitEvents2KHR is loaded; skipping missing-entry-point test.";
    }

    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    const VkEvent fakeEvent = reinterpret_cast<VkEvent>(0x1u);
    const VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
    EXPECT_FALSE(cmd->endSplitBarrier(fakeEvent, dep));

    ASSERT_TRUE(cmd->end());
    cmd->reset();
}

// On success, endSplitBarrier adds the event to getPendingPooledBarrierEvents().
// reset() after end() returns pooled events to the device pool and empties the pending list.
TEST_F(VulkanCommandBufferSplitBarrierTest, EndSplitBarrierAddsEventAndResetReturnsToPool)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping live split barrier test.";
    }

    const VkEvent event = m_device->getBarrierEvent();
    ASSERT_NE(event, VK_NULL_HANDLE);

    VulkanCommandBuffer* const cmd = m_pool->acquireReadyCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->begin());

    VkDependencyInfo dep = zeroVulkanStruct<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);

    EXPECT_TRUE(cmd->beginSplitBarrier(event, dep));
    EXPECT_TRUE(cmd->endSplitBarrier(event, dep));

    // The event must appear in the pending pooled list.
    EXPECT_EQ(cmd->getPendingPooledBarrierEvents().size(), 1u);
    EXPECT_EQ(cmd->getPendingPooledBarrierEvents()[0u], event);

    ASSERT_TRUE(cmd->end());

    const gp::USize pooledBefore = m_device->getBarrierEventPool().getPooledCount();

    // reset() on a HasEnded buffer releases pooled events back to the device pool.
    cmd->reset();

    EXPECT_EQ(m_device->getBarrierEventPool().getPooledCount(), pooledBefore + 1u);
}

}   // namespace gp::rhi::vulkan::tests
