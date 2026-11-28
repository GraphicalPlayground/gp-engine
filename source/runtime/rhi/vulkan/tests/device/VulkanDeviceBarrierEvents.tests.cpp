// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "sync/VulkanBarrierEventPool.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

class VulkanDeviceBarrierEventsTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Vulkan loader unavailable; skipping device barrier event tests.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-device-barrier-event-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        if (!m_instance.initialize(desc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping device barrier event tests.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No Vulkan physical device found; skipping device barrier event tests.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping device barrier event tests.";
        }
    }

    virtual void TearDown() override
    {
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

// needsQueueFamilyOwnershipTransfers() matches the value from getQueueFamilyIndices().
TEST_F(VulkanDeviceBarrierEventsTest, NeedsQueueFamilyOwnershipTransfersMatchesIndices)
{
    const bool fromMethod = m_device->needsQueueFamilyOwnershipTransfers();
    const bool fromIndices = m_device->getQueueFamilyIndices().needsQueueFamilyOwnershipTransfers;

    EXPECT_EQ(fromMethod, fromIndices);
}

// getBarrierEventPool() returns a const reference to the same pool accessed through the device.
TEST_F(VulkanDeviceBarrierEventsTest, GetBarrierEventPoolReturnsInternalPool)
{
    const VulkanBarrierEventPool& pool = m_device->getBarrierEventPool();

    EXPECT_EQ(pool.getPooledCount(), 0u);
    EXPECT_EQ(pool.getCreatedCount(), 0u);
}

// getBarrierEvent() and releaseBarrierEvent() require synchronization2 support.
TEST_F(VulkanDeviceBarrierEventsTest, GetAndReleaseBarrierEventRequiresParallelRendering)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping getBarrierEvent() tests.";
    }

    const gp::USize pooledBefore = m_device->getBarrierEventPool().getPooledCount();
    const gp::USize createdBefore = m_device->getBarrierEventPool().getCreatedCount();

    const VkEvent event = m_device->getBarrierEvent();
    ASSERT_NE(event, VK_NULL_HANDLE);
    EXPECT_EQ(m_device->getBarrierEventPool().getCreatedCount(), createdBefore + 1u);
    EXPECT_EQ(m_device->getBarrierEventPool().getPooledCount(), pooledBefore);

    m_device->releaseBarrierEvent(event);
    EXPECT_EQ(m_device->getBarrierEventPool().getPooledCount(), pooledBefore + 1u);
}

// releaseBarrierEvent(VK_NULL_HANDLE) is ignored; pool count stays the same.
TEST_F(VulkanDeviceBarrierEventsTest, ReleaseNullBarrierEventIsIgnored)
{
    const gp::USize pooledBefore = m_device->getBarrierEventPool().getPooledCount();

    m_device->releaseBarrierEvent(VK_NULL_HANDLE);

    EXPECT_EQ(m_device->getBarrierEventPool().getPooledCount(), pooledBefore);
}

// Pooled count becomes zero after device->destroy() destroys pooled events.
TEST_F(VulkanDeviceBarrierEventsTest, DeviceDestroyEmptiesBarrierEventPool)
{
    if (!m_device->supportsParallelRendering())
    {
        GTEST_SKIP() << "Device lacks synchronization2; skipping pool-destroy test.";
    }

    const VkEvent event = m_device->getBarrierEvent();
    ASSERT_NE(event, VK_NULL_HANDLE);

    m_device->releaseBarrierEvent(event);
    EXPECT_EQ(m_device->getBarrierEventPool().getPooledCount(), 1u);

    // destroy() will drain the pool via the barrier event pool's destroy() path.
    m_device->destroy();
    // After destroy the pool is cleared.
    EXPECT_EQ(m_device->getBarrierEventPool().getPooledCount(), 0u);

    // Prevent double-destroy in TearDown.
    delete m_device;
    m_device = nullptr;
}

}   // namespace gp::rhi::vulkan::tests
