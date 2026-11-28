// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "commands/VulkanQueue.hpp"
#include "commands/VulkanSubmission.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "queries/VulkanQueryPoolManager.hpp"
#include "sync/VulkanFence.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

/// @brief Live fixture that owns a device, a graphics queue and a query pool manager.
/// @details Skips the test when no Vulkan loader, instance or device is available on the host.
class VulkanQueryLiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Host Vulkan loader library unavailable; skipping live query test.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-queries-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;
        if (!m_instance.initialize(desc))
        {
            GTEST_SKIP() << "VkInstance initialization failed; skipping live query test.";
        }
        m_instanceInitialized = true;

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            GTEST_SKIP() << "No Vulkan physical device found; skipping live query test.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            GTEST_SKIP() << "VkDevice initialization failed; skipping live query test.";
        }

        m_fenceManager = new VulkanFenceManager(*m_device);
        const gp::Int32 graphicsFamily = m_device->getQueueFamilyIndices().graphicsFamily;
        ASSERT_GE(graphicsFamily, 0);
        m_queue = new VulkanQueue(
            *m_device, *m_fenceManager, static_cast<gp::UInt32>(graphicsFamily), VulkanQueueType::Graphics, 0u
        );
        ASSERT_TRUE(m_queue->isValid());

        m_queryPoolManager = new VulkanQueryPoolManager(*m_device);
    }

    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);

        delete m_queryPoolManager;
        m_queryPoolManager = nullptr;

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

        if (m_instanceInitialized)
        {
            m_instance.shutdown();
            m_instanceInitialized = false;
        }

        if (m_loaderLoaded)
        {
            VulkanLoader::freeLibrary();
            m_loaderLoaded = false;
        }
    }

    /// @brief Acquires a primary command buffer and begins recording.
    /// @return Recording command buffer, or null on failure.
    [[nodiscard]] VulkanCommandBuffer* beginCommandBuffer() noexcept
    {
        VulkanCommandBufferPool* const pool = m_queue->acquireCommandBufferPool(VulkanCommandBufferType::Primary);
        if (pool == nullptr)
        {
            return nullptr;
        }
        VulkanCommandBuffer* const cmdBuffer = pool->acquireReadyCommandBuffer();
        if ((cmdBuffer == nullptr) || !cmdBuffer->begin())
        {
            return nullptr;
        }
        return cmdBuffer;
    }

    /// @brief Ends `cmdBuffer`, submits it in a payload and waits for the payload to complete.
    /// @param[in] cmdBuffer Recording command buffer.
    /// @param[in] payload Payload to submit; takes ownership. Its command buffers are filled by this call.
    /// @return True when the payload was submitted and completed.
    [[nodiscard]] bool submitAndWait(VulkanCommandBuffer* const cmdBuffer, VulkanPayload* const payload) noexcept
    {
        if (!cmdBuffer->end())
        {
            delete payload;
            return false;
        }
        payload->commandBuffers.pushBack(cmdBuffer);
        m_queue->enqueuePayload(payload);

        VulkanSignaledSemaphoreTracker tracker{};
        if (m_queue->submitQueuedPayloads(tracker, false) != 1)
        {
            return false;
        }
        return m_queue->processInterruptQueue(5'000'000'000ULL) == 1;
    }

    /// @brief Reports whether the graphics queue family writes timestamps.
    /// @return True when timestamps are supported on graphics queues.
    [[nodiscard]] bool supportsTimestamps() const noexcept
    {
        return (m_device->getLimits().timestampComputeAndGraphics != VK_FALSE) &&
               (m_device->getLimits().timestampPeriod > 0.0f);
    }

    bool m_loaderLoaded{ false };
    bool m_instanceInitialized{ false };
    VulkanInstance m_instance{};
    VulkanDevice* m_device{ nullptr };
    VulkanFenceManager* m_fenceManager{ nullptr };
    VulkanQueue* m_queue{ nullptr };
    VulkanQueryPoolManager* m_queryPoolManager{ nullptr };
};

}   // namespace gp::rhi::vulkan::tests
