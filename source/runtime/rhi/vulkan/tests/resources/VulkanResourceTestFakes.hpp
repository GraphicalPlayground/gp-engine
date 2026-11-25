// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "memory/VulkanStagingManager.hpp"
#include "resources/IVulkanCommandContextProvider.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

/// @brief Transfer recorder that records every call instead of writing a command buffer.
class FakeTransferRecorder final : public IVulkanTransferCommandRecorder
{
public:
    /// @brief One recorded vkCmdPipelineBarrier.
    struct BarrierCall final
    {
        VkPipelineStageFlags srcStageMask{ 0u };
        VkPipelineStageFlags dstStageMask{ 0u };
        VkAccessFlags srcAccessMask{ 0u };
        VkAccessFlags dstAccessMask{ 0u };
    };

    /// @brief One recorded vkCmdCopyBuffer with a single region.
    struct CopyCall final
    {
        VkBuffer srcBuffer{ VK_NULL_HANDLE };
        VkBuffer dstBuffer{ VK_NULL_HANDLE };
        VkBufferCopy region{};
        gp::USize regionCount{ 0u };
    };

    /// @brief One recorded vkCmdFillBuffer.
    struct FillCall final
    {
        VkBuffer dstBuffer{ VK_NULL_HANDLE };
        VkDeviceSize offset{ 0u };
        VkDeviceSize size{ 0u };
        gp::UInt32 data{ 0u };
    };

    static constexpr gp::USize kMaxCalls = 8u;

    /// @copydoc IVulkanTransferCommandRecorder::copyBuffer()
    virtual void copyBuffer(
        const VkBuffer srcBuffer, const VkBuffer dstBuffer, const gp::ConstVectorView<VkBufferCopy> regions
    ) noexcept override
    {
        if (copyCount < kMaxCalls)
        {
            CopyCall& call = copies[copyCount];
            call.srcBuffer = srcBuffer;
            call.dstBuffer = dstBuffer;
            call.regionCount = regions.size();
            call.region = regions.isEmpty() ? VkBufferCopy{} : regions[0u];
        }
        ++copyCount;
    }

    /// @copydoc IVulkanTransferCommandRecorder::fillBuffer()
    virtual void fillBuffer(
        const VkBuffer dstBuffer, const VkDeviceSize offset, const VkDeviceSize size, const gp::UInt32 data
    ) noexcept override
    {
        if (fillCount < kMaxCalls)
        {
            fills[fillCount] = FillCall{ dstBuffer, offset, size, data };
        }
        ++fillCount;
    }

    /// @copydoc IVulkanTransferCommandRecorder::memoryBarrier()
    virtual void memoryBarrier(
        const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask, const VkMemoryBarrier& barrier
    ) noexcept override
    {
        if (barrierCount < kMaxCalls)
        {
            barriers[barrierCount] =
                BarrierCall{ srcStageMask, dstStageMask, barrier.srcAccessMask, barrier.dstAccessMask };
        }
        ++barrierCount;
    }

    /// @copydoc IVulkanTransferCommandRecorder::debugHeavyWeightBarrier()
    virtual void debugHeavyWeightBarrier(const gp::Int32 categoryMask) noexcept override
    {
        lastDebugCategory = categoryMask;
        ++debugBarrierCount;
    }

    /// @copydoc IVulkanTransferCommandRecorder::isOutsideRenderPass()
    [[nodiscard]] virtual bool isOutsideRenderPass() const noexcept override
    {
        return outsideRenderPass;
    }

    /// @copydoc IVulkanTransferCommandRecorder::getCompletionSyncPoint()
    [[nodiscard]] virtual VulkanSyncPointRef getCompletionSyncPoint() const noexcept override
    {
        return VulkanSyncPointRef{};
    }

    /// @brief Returns the total number of recorded commands of every kind.
    /// @return Copy + fill + barrier count.
    [[nodiscard]] gp::USize totalCommandCount() const noexcept
    {
        return copyCount + fillCount + barrierCount;
    }

    CopyCall copies[kMaxCalls]{};
    FillCall fills[kMaxCalls]{};
    BarrierCall barriers[kMaxCalls]{};
    gp::USize copyCount{ 0u };
    gp::USize fillCount{ 0u };
    gp::USize barrierCount{ 0u };
    gp::USize debugBarrierCount{ 0u };
    gp::Int32 lastDebugCategory{ 0 };
    bool outsideRenderPass{ true };
};

/// @brief Context provider that hands out two distinct fake recorders and counts GPU flushes.
class FakeContextProvider final : public IVulkanCommandContextProvider
{
public:
    /// @copydoc IVulkanCommandContextProvider::getCommandListRecorder()
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getCommandListRecorder([[maybe_unused]] command::CommandListBase& cmdList) noexcept override
    {
        return &commandListRecorder;
    }

    /// @copydoc IVulkanCommandContextProvider::getUploadRecorder()
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getUploadRecorder([[maybe_unused]] command::CommandListBase& cmdList) noexcept override
    {
        return &uploadRecorder;
    }

    /// @copydoc IVulkanCommandContextProvider::getContextRecorder()
    [[nodiscard]] virtual IVulkanTransferCommandRecorder*
        getContextRecorder([[maybe_unused]] command::IComputeContext* const context) noexcept override
    {
        return hasContextRecorder ? &commandListRecorder : nullptr;
    }

    /// @copydoc IVulkanCommandContextProvider::getTrackedPipelines()
    [[nodiscard]] virtual sync::PipelineStage getTrackedPipelines(
        [[maybe_unused]] command::IComputeContext* const context,
        [[maybe_unused]] const resource::Resource* const resource
    ) noexcept override
    {
        return trackedPipelines;
    }

    /// @copydoc IVulkanCommandContextProvider::submitAndBlockUntilGPUIdle()
    virtual void submitAndBlockUntilGPUIdle([[maybe_unused]] command::CommandListImmediate& cmdList) noexcept override
    {
        ++submitCount;
    }

    /// @copydoc IVulkanCommandContextProvider::needsExtraTransitions()
    [[nodiscard]] virtual bool
        needsExtraTransitions([[maybe_unused]] const command::CommandListBase& cmdList) const noexcept override
    {
        return needsTransitions;
    }

    FakeTransferRecorder commandListRecorder{};
    FakeTransferRecorder uploadRecorder{};
    sync::PipelineStage trackedPipelines{ sync::PipelineStage::Graphics };
    gp::UInt32 submitCount{ 0u };
    bool needsTransitions{ false };
    bool hasContextRecorder{ true };
};

/// @brief Brings up a real device, memory manager and staging manager, plus a bypassed immediate command list.
/// @details With bypass on, enqueued lambdas run inline, so every bottom-of-pipe effect is visible right away.
///          Tests skip when no Vulkan loader or device is available.
class VulkanResourceLiveTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        if (!VulkanLoader::loadLibrary())
        {
            GTEST_SKIP() << "Host Vulkan loader library unavailable; skipping live resource test.";
        }
        m_loaderLoaded = true;

        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "gp-rhi-vulkan-resource-test";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;
        if (!m_instance.initialize(desc))
        {
            GTEST_SKIP() << "VkInstance initialization failed; skipping live resource test.";
        }
        m_instanceInitialized = true;

        VulkanPhysicalDeviceInfo gpuInfo{};
        const VkPhysicalDevice physicalDevice =
            selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), gpuInfo);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            GTEST_SKIP() << "No Vulkan physical device found; skipping live resource test.";
        }

        m_device = new VulkanDevice(m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), false);
        if (!m_device->initialize(false))
        {
            delete m_device;
            m_device = nullptr;
            GTEST_SKIP() << "VkDevice initialization failed; skipping live resource test.";
        }

        m_deviceMemoryManager = new VulkanDeviceMemoryManager();
        m_deviceMemoryManager->init(m_device);

        m_memoryManager = new VulkanMemoryManager(*m_device, *m_deviceMemoryManager);
        m_memoryManager->init();

        m_stagingManager = new VulkanStagingManager();
        m_stagingManager->init(m_device, m_memoryManager);

        m_services.device = m_device;
        m_services.memoryManager = m_memoryManager;
        m_services.stagingManager = m_stagingManager;
        m_services.deferredDeletionQueue = nullptr;
        m_services.contextProvider = &m_provider;

        m_previousBypass = command::getCommandListExecutor().bypass();
        command::getCommandListExecutor().setBypass(true);
        m_bypassChanged = true;
    }

    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);

        if (m_bypassChanged)
        {
            command::getCommandListExecutor().setBypass(m_previousBypass);
            m_bypassChanged = false;
        }

        if (m_stagingManager != nullptr)
        {
            m_stagingManager->deinit();
            delete m_stagingManager;
            m_stagingManager = nullptr;
        }

        if (m_memoryManager != nullptr)
        {
            m_memoryManager->setDeferredDeletionQueue(nullptr);
            m_memoryManager->deinit();
            delete m_memoryManager;
            m_memoryManager = nullptr;
        }

        if (m_deviceMemoryManager != nullptr)
        {
            m_deviceMemoryManager->deinit();
            delete m_deviceMemoryManager;
            m_deviceMemoryManager = nullptr;
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

    /// @brief Returns the bypassed immediate command list.
    /// @return The global immediate command list.
    [[nodiscard]] command::CommandListImmediate& cmdList() noexcept
    {
        return command::getImmediateCommandList();
    }

    /// @brief Reports whether the device exposes host-visible device-local memory for every buffer.
    /// @return True on unified memory architectures.
    [[nodiscard]] bool isUnifiedMemory() const noexcept
    {
        return m_services.hasUnifiedMemory();
    }

    bool m_loaderLoaded{ false };
    bool m_instanceInitialized{ false };
    bool m_bypassChanged{ false };
    bool m_previousBypass{ false };
    VulkanInstance m_instance{};
    VulkanDevice* m_device{ nullptr };
    VulkanDeviceMemoryManager* m_deviceMemoryManager{ nullptr };
    VulkanMemoryManager* m_memoryManager{ nullptr };
    VulkanStagingManager* m_stagingManager{ nullptr };
    FakeContextProvider m_provider{};
    VulkanResourceServices m_services{};
};

}   // namespace gp::rhi::vulkan::tests

