// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "../views/VulkanViewTestFixture.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanQueue.hpp"
#include "commands/VulkanSubmission.hpp"
#include "contexts/VulkanContextDesc.hpp"
#include "contexts/VulkanImmediatePayloadSubmitter.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "sync/VulkanFence.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

/// @brief Live fixture adding a graphics queue and an immediate payload submitter to the view fixture.
/// @details Contexts need a device, a queue to draw command buffers from and a submitter to flush payloads; the
///          view fixture already provides buffers and textures to record commands against.
class VulkanContextLiveTest : public VulkanViewLiveTest
{
protected:
    /// @copydoc VulkanViewLiveTest::SetUp()
    virtual void SetUp() override
    {
        VulkanViewLiveTest::SetUp();
        if (IsSkipped() || HasFatalFailure())
        {
            return;
        }

        m_fenceManager = new VulkanFenceManager(*m_device);
        const gp::Int32 graphicsFamily = m_device->getQueueFamilyIndices().graphicsFamily;
        ASSERT_GE(graphicsFamily, 0);
        m_queue = new VulkanQueue(
            *m_device, *m_fenceManager, static_cast<gp::UInt32>(graphicsFamily), VulkanQueueType::Graphics, 0u
        );
        ASSERT_TRUE(m_queue->isValid());

        m_submitter = new VulkanImmediatePayloadSubmitter();

        m_contextServices.device = m_device;
        m_contextServices.queue = m_queue;
        m_contextServices.submitter = m_submitter;
    }

    /// @copydoc VulkanViewLiveTest::TearDown()
    virtual void TearDown() override
    {
        if (m_device != nullptr)
        {
            m_device->waitUntilIdle();
        }

        delete m_submitter;
        m_submitter = nullptr;

        delete m_queue;
        m_queue = nullptr;

        if (m_fenceManager != nullptr)
        {
            m_fenceManager->deinit();
            delete m_fenceManager;
            m_fenceManager = nullptr;
        }

        VulkanViewLiveTest::TearDown();
    }

    /// @brief Resets every command buffer of a finalized but never submitted command list.
    /// @details Payload destruction does not touch command buffers; recycling them keeps the pool consistent.
    /// @param[in,out] commandList Finalized command list.
    static void discardUnsubmitted(VulkanPlatformCommandList& commandList) noexcept
    {
        for (VulkanPayload* const payload: commandList.payloads)
        {
            for (VulkanCommandBuffer* const commandBuffer: payload->commandBuffers)
            {
                if (commandBuffer != nullptr)
                {
                    commandBuffer->reset();
                }
            }
            payload->commandBuffers.clear();
        }
    }

    /// @brief Fence manager backing the queue.
    VulkanFenceManager* m_fenceManager{ nullptr };

    /// @brief Graphics queue contexts record for.
    VulkanQueue* m_queue{ nullptr };

    /// @brief Submitter flushing payloads to `m_queue`.
    VulkanImmediatePayloadSubmitter* m_submitter{ nullptr };

    /// @brief Minimal services: device, queue and submitter.
    VulkanContextServices m_contextServices{};
};

}   // namespace gp::rhi::vulkan::tests
