// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/VulkanBarrierEventPool.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::rhi::vulkan::tests
{

// acquire() with a null device on an empty pool returns VK_NULL_HANDLE.
TEST(VulkanBarrierEventPoolTest, AcquireNullDeviceEmptyPoolReturnsNull)
{
    VulkanBarrierEventPool pool;

    const VkEvent event = pool.acquire(VK_NULL_HANDLE);

    EXPECT_EQ(event, VK_NULL_HANDLE);
    EXPECT_EQ(pool.getPooledCount(), 0u);
    EXPECT_EQ(pool.getCreatedCount(), 0u);

    pool.destroy(VK_NULL_HANDLE);
}

// release(VK_NULL_HANDLE) is silently ignored.
TEST(VulkanBarrierEventPoolTest, ReleaseNullEventIsIgnored)
{
    VulkanBarrierEventPool pool;

    pool.release(VK_NULL_HANDLE);

    EXPECT_EQ(pool.getPooledCount(), 0u);
    EXPECT_EQ(pool.getCreatedCount(), 0u);

    pool.destroy(VK_NULL_HANDLE);
}

// release() increments the pooled count; destroy(VK_NULL_HANDLE) clears without calling vkDestroyEvent.
TEST(VulkanBarrierEventPoolTest, ReleaseIncreasesPooledCountAndDestroyClears)
{
    VulkanBarrierEventPool pool;

    const VkEvent fakeEvent = reinterpret_cast<VkEvent>(0xDEADBEEFu);
    pool.release(fakeEvent);

    EXPECT_EQ(pool.getPooledCount(), 1u);

    pool.destroy(VK_NULL_HANDLE);

    EXPECT_EQ(pool.getPooledCount(), 0u);
}

// acquire() with a null device reuses a pooled handle without changing createdCount.
TEST(VulkanBarrierEventPoolTest, AcquireReusesPooledHandleWithNullDevice)
{
    VulkanBarrierEventPool pool;

    const VkEvent fakeEvent = reinterpret_cast<VkEvent>(0xCAFEu);
    pool.release(fakeEvent);
    EXPECT_EQ(pool.getPooledCount(), 1u);

    const VkEvent acquired = pool.acquire(VK_NULL_HANDLE);

    EXPECT_EQ(acquired, fakeEvent);
    EXPECT_EQ(pool.getPooledCount(), 0u);
    EXPECT_EQ(pool.getCreatedCount(), 0u);

    pool.destroy(VK_NULL_HANDLE);
}

// destroy() with a null device clears all pooled handles cleanly.
TEST(VulkanBarrierEventPoolTest, DestroyNullDeviceClearsMultipleEntries)
{
    VulkanBarrierEventPool pool;

    pool.release(reinterpret_cast<VkEvent>(0x1u));
    pool.release(reinterpret_cast<VkEvent>(0x2u));
    pool.release(reinterpret_cast<VkEvent>(0x3u));
    EXPECT_EQ(pool.getPooledCount(), 3u);

    pool.destroy(VK_NULL_HANDLE);

    EXPECT_EQ(pool.getPooledCount(), 0u);
}

// createdCount starts at zero and is not affected by release/destroy with a null device.
TEST(VulkanBarrierEventPoolTest, CreatedCountStartsAtZeroAndStaysZeroWithNullDevice)
{
    VulkanBarrierEventPool pool;

    EXPECT_EQ(pool.getCreatedCount(), 0u);
    pool.release(reinterpret_cast<VkEvent>(0xAu));
    EXPECT_EQ(pool.getCreatedCount(), 0u);

    pool.destroy(VK_NULL_HANDLE);
}

// Thread-safety stress: N threads each release fake handles then re-acquire them from the pool.
// With a null device, acquire() returns pooled handles without creating new events.
TEST(VulkanBarrierEventPoolTest, ConcurrentReleaseAndAcquireAllHandlesAccountedFor)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kHandlesPerThread = 100u;
    constexpr gp::UInt32 kTotalHandles = kThreadCount * kHandlesPerThread;

    VulkanBarrierEventPool pool;

    // Phase 1: all threads release their own fake handles.
    {
        std::vector<std::thread> workers;
        workers.reserve(kThreadCount);

        for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
        {
            workers.emplace_back([&pool, t]() noexcept
            {
                for (gp::UInt32 i = 0u; i < kHandlesPerThread; ++i)
                {
                    const gp::UIntPtr value = static_cast<gp::UIntPtr>(t * kHandlesPerThread + i + 1u);
                    pool.release(reinterpret_cast<VkEvent>(value));
                }
            });
        }

        for (std::thread& worker: workers)
        {
            worker.join();
        }
    }

    EXPECT_EQ(pool.getPooledCount(), static_cast<gp::USize>(kTotalHandles));

    // Phase 2: all threads acquire handles back from the pool using a null device.
    std::atomic<gp::UInt32> acquiredCount{ 0u };
    {
        std::vector<std::thread> workers;
        workers.reserve(kThreadCount);

        for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
        {
            workers.emplace_back([&pool, &acquiredCount]() noexcept
            {
                for (gp::UInt32 i = 0u; i < kHandlesPerThread; ++i)
                {
                    const VkEvent handle = pool.acquire(VK_NULL_HANDLE);
                    if (handle != VK_NULL_HANDLE)
                    {
                        acquiredCount.fetch_add(1u, std::memory_order_relaxed);
                    }
                }
            });
        }

        for (std::thread& worker: workers)
        {
            worker.join();
        }
    }

    EXPECT_EQ(acquiredCount.load(std::memory_order_relaxed), kTotalHandles);
    EXPECT_EQ(pool.getPooledCount(), 0u);
    EXPECT_EQ(pool.getCreatedCount(), 0u);

    pool.destroy(VK_NULL_HANDLE);
}

// Live-device fixture for event creation tests.
class VulkanBarrierEventPoolLiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Vulkan loader unavailable; skipping live pool tests.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-barrier-event-pool-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        if (!m_instance.initialize(desc))
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkInstance initialization failed; skipping live pool tests.";
        }

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "No Vulkan physical device found; skipping live pool tests.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            m_instance.shutdown();
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
            GTEST_SKIP() << "VkDevice initialization failed; skipping live pool tests.";
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

// acquire() with a live device creates a real event and increments createdCount.
TEST_F(VulkanBarrierEventPoolLiveTest, AcquireWithLiveDeviceCreatesEventAndIncrementsCreatedCount)
{
    VulkanBarrierEventPool pool;

    const VkEvent event = pool.acquire(m_device->getHandle());

    ASSERT_NE(event, VK_NULL_HANDLE);
    EXPECT_EQ(pool.getCreatedCount(), 1u);
    EXPECT_EQ(pool.getPooledCount(), 0u);

    pool.release(event);
    pool.destroy(m_device->getHandle());
}

// acquire() after release() reuses the pooled handle without calling vkCreateEvent again.
TEST_F(VulkanBarrierEventPoolLiveTest, AcquireAfterReleaseReusesSameHandleWithoutCreating)
{
    VulkanBarrierEventPool pool;

    const VkEvent event = pool.acquire(m_device->getHandle());
    ASSERT_NE(event, VK_NULL_HANDLE);
    EXPECT_EQ(pool.getCreatedCount(), 1u);

    pool.release(event);
    EXPECT_EQ(pool.getPooledCount(), 1u);

    const VkEvent reused = pool.acquire(m_device->getHandle());

    EXPECT_EQ(reused, event);
    EXPECT_EQ(pool.getCreatedCount(), 1u);
    EXPECT_EQ(pool.getPooledCount(), 0u);

    pool.release(reused);
    pool.destroy(m_device->getHandle());
}

// destroy() with a live device calls vkDestroyEvent for every pooled event.
TEST_F(VulkanBarrierEventPoolLiveTest, DestroyWithLiveDeviceDestroysAllPooledEvents)
{
    VulkanBarrierEventPool pool;

    const VkEvent eventA = pool.acquire(m_device->getHandle());
    const VkEvent eventB = pool.acquire(m_device->getHandle());
    ASSERT_NE(eventA, VK_NULL_HANDLE);
    ASSERT_NE(eventB, VK_NULL_HANDLE);
    EXPECT_EQ(pool.getCreatedCount(), 2u);

    pool.release(eventA);
    pool.release(eventB);
    EXPECT_EQ(pool.getPooledCount(), 2u);

    pool.destroy(m_device->getHandle());

    EXPECT_EQ(pool.getPooledCount(), 0u);
}

}   // namespace gp::rhi::vulkan::tests
