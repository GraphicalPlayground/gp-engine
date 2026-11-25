// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/VulkanQueue.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanSubmission.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanDeviceCapabilities.hpp"   // IWYU pragma: keep
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "logging/LogMacros.hpp"
#include "memory/MemoryOrder.hpp"
#include "sync/VulkanFence.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarWaitForIdleOnSubmit(
    "r.Vulkan.WaitForIdleOnSubmit",
    0,
    "Waits for the Vulkan GPU queue to become idle after every submission (debugging aid).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllowTimelineSemaphores(
    "r.Vulkan.Submission.AllowTimelineSemaphores",
    1,
    "Enables timeline semaphores for queue submission completion tracking when supported by the device.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

/// @brief Resolves `vkQueueSubmit2KHR` or its Vulkan 1.3 core equivalent for `device`.
/// @param[in] device Active logical `VkDevice`.
/// @return Resolved function pointer, or `nullptr` if unavailable.
[[nodiscard]] PFN_vkQueueSubmit2 resolveQueueSubmit2Fn(const VkDevice device) noexcept
{
    if (dynamic_api::vkQueueSubmit2KHR != nullptr)
    {
        return dynamic_api::vkQueueSubmit2KHR;
    }
    if (device != VK_NULL_HANDLE)
    {
        return reinterpret_cast<PFN_vkQueueSubmit2>(VulkanLoader::getDeviceProcAddr(device, "vkQueueSubmit2"));
    }
    return nullptr;
}

}   // namespace

VulkanQueue::VulkanQueue(
    VulkanDevice& device,
    VulkanFenceManager& fenceManager,
    const gp::UInt32 familyIndex,
    const VulkanQueueType queueType,
    const gp::UInt32 queueIndex
) noexcept
    : m_device(device)
    , m_fenceManager(fenceManager)
    , m_familyIndex(familyIndex)
    , m_queueIndex(queueIndex)
    , m_queueType(queueType)
{
    if (!m_device.isValid())
    {
        return;
    }

    if (dynamic_api::vkGetDeviceQueue != nullptr)
    {
        vkGetDeviceQueue(m_device.getHandle(), m_familyIndex, m_queueIndex, &m_queue);
    }
    else if (m_queueIndex == 0u)
    {
        m_queue = m_device.getQueueHandle(m_queueType);
    }

    fillSupportedStageBits();

    const bool allowTimeline = g_cvarAllowTimelineSemaphores.getValueOnAnyThread(true) != 0;
    if (m_queue != VK_NULL_HANDLE && allowTimeline && m_device.getOptionalExtensions().hasKHRTimelineSemaphore != 0u)
    {
        VulkanSemaphoreRef timelineSem = VulkanSemaphore::create(
            m_device, VulkanSemaphoreFlags::Timeline | VulkanSemaphoreFlags::ImmediateDeletion, 0u
        );
        if (timelineSem && timelineSem->isValid())
        {
            m_timelineSemaphore = std::move(timelineSem);
        }
    }
}

VulkanQueue::~VulkanQueue() noexcept
{
    if (m_queue != VK_NULL_HANDLE)
    {
        waitUntilIdle();
    }

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_interruptMutex);
        for (gp::USize index = 0u; index < m_pendingInterrupt.size(); ++index)
        {
            VulkanPayload* const payload = m_pendingInterrupt[index];
            if (payload != nullptr)
            {
                if (payload->fence != nullptr)
                {
                    [[maybe_unused]] const bool signaled = m_fenceManager.isFenceSignaled(payload->fence);
                }
                payload->complete();
                delete payload;
            }
        }
        m_pendingInterrupt.clear();
    }

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_submissionMutex);
        for (gp::USize index = 0u; index < m_pendingSubmission.size(); ++index)
        {
            delete m_pendingSubmission[index];
        }
        m_pendingSubmission.clear();
    }

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_poolMutex);
        for (gp::USize typeIndex = 0u; typeIndex < kVulkanCommandBufferTypeCount; ++typeIndex)
        {
            gp::Vector<VulkanCommandBufferPool*>& pools = m_freeCommandBufferPools[typeIndex];
            for (gp::USize poolIndex = 0u; poolIndex < pools.size(); ++poolIndex)
            {
                delete pools[poolIndex];
            }
            pools.clear();
        }
    }

    m_timelineSemaphore.reset();
    m_queue = VK_NULL_HANDLE;
}

void VulkanQueue::computeSupportedStagesAndAccess(
    const VkQueueFlags queueFlags,
    const VkPhysicalDeviceFeatures& coreFeatures,
    const OptionalVulkanDeviceExtensions& optionalExtensions,
    VkPipelineStageFlags& outSupportedStages,
    VkAccessFlags& outSupportedAccess
) noexcept
{
    outSupportedStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT |
                         VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    outSupportedAccess =
        VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    const bool hasGraphics = (queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u;
    const bool hasCompute = (queueFlags & VK_QUEUE_COMPUTE_BIT) != 0u;
    const bool hasTransfer = (queueFlags & VK_QUEUE_TRANSFER_BIT) != 0u;

    if (hasTransfer || hasGraphics || hasCompute)
    {
        outSupportedStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        outSupportedAccess |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (hasCompute)
    {
        outSupportedStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        outSupportedAccess |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        if (optionalExtensions.hasAccelerationStructure != 0u)
        {
            outSupportedStages |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            outSupportedAccess |=
                VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        }

        if (optionalExtensions.hasRayTracingPipeline != 0u)
        {
            outSupportedStages |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
    }

    if (hasGraphics)
    {
        outSupportedStages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                              VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        outSupportedAccess |=
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
            VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        if (coreFeatures.geometryShader != VK_FALSE)
        {
            outSupportedStages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        }

        if (coreFeatures.tessellationShader != VK_FALSE)
        {
            outSupportedStages |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                                  VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        }

        if (optionalExtensions.hasEXTFragmentDensityMap != 0u)
        {
            outSupportedStages |= VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
            outSupportedAccess |= VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
        }

        if (optionalExtensions.hasKHRFragmentShadingRate != 0u)
        {
            outSupportedStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
            outSupportedAccess |= VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
        }

        if (optionalExtensions.hasEXTMeshShader != 0u)
        {
            outSupportedStages |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
        }
    }
}

void VulkanQueue::fillSupportedStageBits() noexcept
{
    const gp::ConstVectorView<VkQueueFamilyProperties> familyProps = m_device.getQueueFamilyProperties();
    const VkQueueFlags queueFlags = (m_familyIndex < familyProps.size()) ? familyProps[m_familyIndex].queueFlags : 0u;

    computeSupportedStagesAndAccess(
        queueFlags,
        m_device.getFeatures().getCore10(),
        m_device.getOptionalExtensions(),
        m_supportedStages,
        m_supportedAccess
    );
}

VulkanCommandBufferPool* VulkanQueue::acquireCommandBufferPool(const VulkanCommandBufferType type) noexcept
{
    const gp::USize typeIndex = static_cast<gp::USize>(type);
    if (typeIndex >= kVulkanCommandBufferTypeCount || !m_device.isValid())
    {
        return nullptr;
    }

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_poolMutex);
        gp::Vector<VulkanCommandBufferPool*>& freePools = m_freeCommandBufferPools[typeIndex];
        if (!freePools.isEmpty())
        {
            VulkanCommandBufferPool* const reusedPool = freePools.back();
            freePools.popBack();
            return reusedPool;
        }
    }

    VulkanCommandBufferPool* const newPool = new VulkanCommandBufferPool(m_device, type);
    if (!newPool->create(m_familyIndex))
    {
        delete newPool;
        return nullptr;
    }

    return newPool;
}

void VulkanQueue::releaseCommandBufferPool(VulkanCommandBufferPool* const pool) noexcept
{
    if (pool == nullptr)
    {
        return;
    }

    const gp::USize typeIndex = static_cast<gp::USize>(pool->getCommandBufferType());
    if (typeIndex >= kVulkanCommandBufferTypeCount)
    {
        delete pool;
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_poolMutex);
    m_freeCommandBufferPools[typeIndex].pushBack(pool);
}

void VulkanQueue::enqueuePayload(VulkanPayload* const payload) noexcept
{
    if (payload == nullptr)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_submissionMutex);
    m_pendingSubmission.pushBack(payload);
}

bool VulkanQueue::areWaitSemaphoresReady(
    const VulkanPayload& payload, const VulkanSignaledSemaphoreTracker& signaledSemaphores
) noexcept
{
    for (gp::USize index = 0u; index < payload.waitSemaphores.size(); ++index)
    {
        const VulkanSemaphoreRef& semaphore = payload.waitSemaphores[index];
        if (!semaphore || !semaphore->isValid())
        {
            continue;
        }

        if (semaphore->isExternallySignaled() || semaphore->isTimeline())
        {
            continue;
        }

        if (!signaledSemaphores.contains(semaphore.get()))
        {
            return false;
        }
    }

    return true;
}

VkResult VulkanQueue::submit(const gp::ConstVectorView<VkSubmitInfo> submitInfos, VulkanFence* const fence) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> queueLock(m_queueExecutionMutex);
    if (m_queue == VK_NULL_HANDLE || dynamic_api::vkQueueSubmit == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkFence fenceHandle = (fence != nullptr) ? fence->getHandle() : VK_NULL_HANDLE;
    const gp::UInt32 submitCount = static_cast<gp::UInt32>(submitInfos.size());
    const VkSubmitInfo* const submitData = (submitCount > 0u) ? submitInfos.data() : nullptr;

    const VkResult result = vkQueueSubmit(m_queue, submitCount, submitData, fenceHandle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkQueueSubmit failed on {} queue with {}.",
            getVulkanQueueTypeName(m_queueType),
            vulkanResultToString(result)
        );
    }

    return result;
}

VkResult VulkanQueue::submit2(const gp::ConstVectorView<VkSubmitInfo2> submitInfos, VulkanFence* const fence) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> queueLock(m_queueExecutionMutex);
    if (m_queue == VK_NULL_HANDLE)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const PFN_vkQueueSubmit2 submit2Fn = resolveQueueSubmit2Fn(m_device.getHandle());
    if (submit2Fn == nullptr)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    const VkFence fenceHandle = (fence != nullptr) ? fence->getHandle() : VK_NULL_HANDLE;
    const gp::UInt32 submitCount = static_cast<gp::UInt32>(submitInfos.size());
    const VkSubmitInfo2* const submitData = (submitCount > 0u) ? submitInfos.data() : nullptr;

    const VkResult result = submit2Fn(m_queue, submitCount, submitData, fenceHandle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkQueueSubmit2 failed on {} queue with {}.",
            getVulkanQueueTypeName(m_queueType),
            vulkanResultToString(result)
        );
    }

    return result;
}

VkResult VulkanQueue::submitPayloadLegacy(VulkanPayload& payload) noexcept
{
    if (m_queue == VK_NULL_HANDLE || dynamic_api::vkQueueSubmit == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    gp::Vector<VkSemaphore> waitHandles{};
    gp::Vector<VkPipelineStageFlags> waitStageMasks{};
    gp::Vector<gp::UInt64> waitValues{};
    for (gp::USize index = 0u; index < payload.waitSemaphores.size(); ++index)
    {
        const VulkanSemaphoreRef& sem = payload.waitSemaphores[index];
        if (!sem || !sem->isValid())
        {
            continue;
        }
        waitHandles.pushBack(sem->getHandle());
        const VkPipelineStageFlags stageMask =
            (index < payload.waitFlags.size()) ? payload.waitFlags[index] : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        waitStageMasks.pushBack(stageMask);
        waitValues.pushBack(0u);
    }

    gp::Vector<VkCommandBuffer> cmdHandles{};
    for (gp::USize index = 0u; index < payload.commandBuffers.size(); ++index)
    {
        VulkanCommandBuffer* const cmdBuffer = payload.commandBuffers[index];
        if (cmdBuffer != nullptr && cmdBuffer->isValid() && cmdBuffer->hasEnded())
        {
            cmdHandles.pushBack(cmdBuffer->getHandle());
        }
    }

    gp::Vector<VkSemaphore> signalHandles{};
    gp::Vector<gp::UInt64> signalValues{};
    for (gp::USize index = 0u; index < payload.signalSemaphores.size(); ++index)
    {
        const VulkanSemaphoreRef& sem = payload.signalSemaphores[index];
        if (!sem || !sem->isValid())
        {
            continue;
        }
        signalHandles.pushBack(sem->getHandle());
        signalValues.pushBack(0u);
    }

    VkTimelineSemaphoreSubmitInfo timelineInfo =
        zeroVulkanStruct<VkTimelineSemaphoreSubmitInfo>(VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
    const bool hasTimeline = usesTimelineSemaphore();
    if (hasTimeline)
    {
        signalHandles.pushBack(m_timelineSemaphore->getHandle());
        signalValues.pushBack(payload.timelineSemaphoreValue);
        timelineInfo.waitSemaphoreValueCount = static_cast<gp::UInt32>(waitValues.size());
        timelineInfo.pWaitSemaphoreValues = waitValues.isEmpty() ? nullptr : waitValues.data();
        timelineInfo.signalSemaphoreValueCount = static_cast<gp::UInt32>(signalValues.size());
        timelineInfo.pSignalSemaphoreValues = signalValues.isEmpty() ? nullptr : signalValues.data();
    }

    VkSubmitInfo submitInfo = zeroVulkanStruct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
    submitInfo.waitSemaphoreCount = static_cast<gp::UInt32>(waitHandles.size());
    submitInfo.pWaitSemaphores = waitHandles.isEmpty() ? nullptr : waitHandles.data();
    submitInfo.pWaitDstStageMask = waitStageMasks.isEmpty() ? nullptr : waitStageMasks.data();
    submitInfo.commandBufferCount = static_cast<gp::UInt32>(cmdHandles.size());
    submitInfo.pCommandBuffers = cmdHandles.isEmpty() ? nullptr : cmdHandles.data();
    submitInfo.signalSemaphoreCount = static_cast<gp::UInt32>(signalHandles.size());
    submitInfo.pSignalSemaphores = signalHandles.isEmpty() ? nullptr : signalHandles.data();

    if (hasTimeline)
    {
        addToPNext(submitInfo, timelineInfo);
    }

    const VkFence fenceHandle = (payload.fence != nullptr) ? payload.fence->getHandle() : VK_NULL_HANDLE;
    const VkResult result = vkQueueSubmit(m_queue, 1u, &submitInfo, fenceHandle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkQueueSubmit failed on {} queue with {}.",
            getVulkanQueueTypeName(m_queueType),
            vulkanResultToString(result)
        );
    }

    return result;
}

VkResult VulkanQueue::submitPayloadSync2(VulkanPayload& payload) noexcept
{
    if (m_queue == VK_NULL_HANDLE)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const PFN_vkQueueSubmit2 submit2Fn = resolveQueueSubmit2Fn(m_device.getHandle());
    if (submit2Fn == nullptr)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    gp::Vector<VkSemaphoreSubmitInfo> waitInfos{};
    for (gp::USize index = 0u; index < payload.waitSemaphores.size(); ++index)
    {
        const VulkanSemaphoreRef& sem = payload.waitSemaphores[index];
        if (!sem || !sem->isValid())
        {
            continue;
        }
        VkSemaphoreSubmitInfo info = zeroVulkanStruct<VkSemaphoreSubmitInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO);
        info.semaphore = sem->getHandle();
        info.value = 0u;
        info.stageMask = static_cast<VkPipelineStageFlags2>(
            (index < payload.waitFlags.size()) ? payload.waitFlags[index] : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
        );
        waitInfos.pushBack(info);
    }

    gp::Vector<VkCommandBufferSubmitInfo> cmdInfos{};
    for (gp::USize index = 0u; index < payload.commandBuffers.size(); ++index)
    {
        VulkanCommandBuffer* const cmdBuffer = payload.commandBuffers[index];
        if (cmdBuffer != nullptr && cmdBuffer->isValid() && cmdBuffer->hasEnded())
        {
            VkCommandBufferSubmitInfo info =
                zeroVulkanStruct<VkCommandBufferSubmitInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO);
            info.commandBuffer = cmdBuffer->getHandle();
            cmdInfos.pushBack(info);
        }
    }

    gp::Vector<VkSemaphoreSubmitInfo> signalInfos{};
    for (gp::USize index = 0u; index < payload.signalSemaphores.size(); ++index)
    {
        const VulkanSemaphoreRef& sem = payload.signalSemaphores[index];
        if (!sem || !sem->isValid())
        {
            continue;
        }
        VkSemaphoreSubmitInfo info = zeroVulkanStruct<VkSemaphoreSubmitInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO);
        info.semaphore = sem->getHandle();
        info.value = 0u;
        info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        signalInfos.pushBack(info);
    }

    if (usesTimelineSemaphore())
    {
        VkSemaphoreSubmitInfo timelineSignal =
            zeroVulkanStruct<VkSemaphoreSubmitInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO);
        timelineSignal.semaphore = m_timelineSemaphore->getHandle();
        timelineSignal.value = payload.timelineSemaphoreValue;
        timelineSignal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        signalInfos.pushBack(timelineSignal);
    }

    VkSubmitInfo2 submitInfo = zeroVulkanStruct<VkSubmitInfo2>(VK_STRUCTURE_TYPE_SUBMIT_INFO_2);
    submitInfo.waitSemaphoreInfoCount = static_cast<gp::UInt32>(waitInfos.size());
    submitInfo.pWaitSemaphoreInfos = waitInfos.isEmpty() ? nullptr : waitInfos.data();
    submitInfo.commandBufferInfoCount = static_cast<gp::UInt32>(cmdInfos.size());
    submitInfo.pCommandBufferInfos = cmdInfos.isEmpty() ? nullptr : cmdInfos.data();
    submitInfo.signalSemaphoreInfoCount = static_cast<gp::UInt32>(signalInfos.size());
    submitInfo.pSignalSemaphoreInfos = signalInfos.isEmpty() ? nullptr : signalInfos.data();

    const VkFence fenceHandle = (payload.fence != nullptr) ? payload.fence->getHandle() : VK_NULL_HANDLE;
    const VkResult result = submit2Fn(m_queue, 1u, &submitInfo, fenceHandle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkQueueSubmit2 failed on {} queue with {}.",
            getVulkanQueueTypeName(m_queueType),
            vulkanResultToString(result)
        );
    }

    return result;
}

gp::Int32 VulkanQueue::submitQueuedPayloads(
    VulkanSignaledSemaphoreTracker& signaledSemaphores, const bool useSubmit2
) noexcept
{
    if (m_queue == VK_NULL_HANDLE)
    {
        return 0;
    }

    gp::Int32 submittedCount = 0;
    const bool canUseSubmit2 = useSubmit2 && (m_device.getOptionalExtensions().hasKHRSynchronization2 != 0u) &&
                               (resolveQueueSubmit2Fn(m_device.getHandle()) != nullptr);

    while (true)
    {
        VulkanPayload* payload = nullptr;
        {
            gp::thread::UniqueLock<gp::thread::Mutex> lock(m_submissionMutex);
            if (m_pendingSubmission.isEmpty())
            {
                break;
            }

            VulkanPayload* const head = m_pendingSubmission[0u];
            if (head == nullptr)
            {
                m_pendingSubmission.erase(m_pendingSubmission.begin());
                continue;
            }

            if (!areWaitSemaphoresReady(*head, signaledSemaphores))
            {
                break;
            }

            payload = head;
            m_pendingSubmission.erase(m_pendingSubmission.begin());
        }

        payload->preExecute();

        bool commandBuffersReady = true;
        for (gp::USize index = 0u; index < payload->commandBuffers.size(); ++index)
        {
            VulkanCommandBuffer* const cmdBuffer = payload->commandBuffers[index];
            if (cmdBuffer == nullptr)
            {
                continue;
            }

            if (cmdBuffer->isInsideRenderPass())
            {
                GP_LOG_ERROR(
                    LogVulkanRHI,
                    "VulkanQueue::submitQueuedPayloads() encountered a command buffer still inside a render pass."
                );
                commandBuffersReady = false;
                break;
            }

            if (cmdBuffer->isOutsideRenderPass() && !cmdBuffer->end())
            {
                commandBuffersReady = false;
                break;
            }
        }

        VkResult submitResult = VK_ERROR_VALIDATION_FAILED_EXT;
        if (commandBuffersReady)
        {
            gp::thread::UniqueLock<gp::thread::Mutex> queueLock(m_queueExecutionMutex);
            if (usesTimelineSemaphore())
            {
                payload->timelineSemaphoreValue =
                    m_timelineSemaphoreValue.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel) + 1u;
            }
            else
            {
                payload->fence = m_fenceManager.allocateFence(false);
            }

            submitResult = canUseSubmit2 ? submitPayloadSync2(*payload) : submitPayloadLegacy(*payload);

            if (submitResult == VK_SUCCESS)
            {
                for (gp::USize index = 0u; index < payload->commandBuffers.size(); ++index)
                {
                    VulkanCommandBuffer* const cmdBuffer = payload->commandBuffers[index];
                    if (cmdBuffer != nullptr && cmdBuffer->hasEnded())
                    {
                        cmdBuffer->setSubmitted();
                    }
                }

                for (gp::USize index = 0u; index < payload->waitSemaphores.size(); ++index)
                {
                    const VulkanSemaphoreRef& sem = payload->waitSemaphores[index];
                    if (sem && !sem->isExternallySignaled() && !sem->isTimeline())
                    {
                        signaledSemaphores.remove(sem.get());
                    }
                }

                for (gp::USize index = 0u; index < payload->signalSemaphores.size(); ++index)
                {
                    const VulkanSemaphoreRef& sem = payload->signalSemaphores[index];
                    if (sem && !sem->isExternallyWaited() && !sem->isTimeline())
                    {
                        signaledSemaphores.addOrUpdate(
                            sem.get(), VulkanBinarySemaphoreSignalInfo{ payload->timelineSemaphoreValue, m_queueType }
                        );
                    }
                }

                {
                    gp::thread::UniqueLock<gp::thread::Mutex> interruptLock(m_interruptMutex);
                    m_pendingInterrupt.pushBack(payload);
                }
            }
        }

        if (submitResult != VK_SUCCESS)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "Payload submission failed on {} queue ({}); completing payload immediately.",
                getVulkanQueueTypeName(m_queueType),
                vulkanResultToString(submitResult)
            );
            for (gp::USize index = 0u; index < payload->submissionEvents.size(); ++index)
            {
                if (payload->submissionEvents[index])
                {
                    payload->submissionEvents[index]->signal();
                }
            }
            payload->complete();
            delete payload;
            continue;
        }

        for (gp::USize index = 0u; index < payload->submissionEvents.size(); ++index)
        {
            if (payload->submissionEvents[index])
            {
                payload->submissionEvents[index]->signal();
            }
        }
        payload->submissionEvents.clear();

        ++submittedCount;

        if (g_cvarWaitForIdleOnSubmit.getValueOnAnyThread(true) != 0)
        {
            waitUntilIdle();
        }
    }

    return submittedCount;
}

gp::Int32 VulkanQueue::processInterruptQueue(const gp::UInt64 timeoutNanoseconds) noexcept
{
    gp::Vector<VulkanPayload*> completedPayloads{};

    {
        gp::thread::UniqueLock<gp::thread::Mutex> consumerLock(m_interruptConsumerMutex);

        if (usesTimelineSemaphore())
        {
            gp::UInt64 targetValue = 0u;
            bool hasPending = false;
            {
                gp::thread::UniqueLock<gp::thread::Mutex> lock(m_interruptMutex);
                while (!m_pendingInterrupt.isEmpty() && m_pendingInterrupt[0u] == nullptr)
                {
                    m_pendingInterrupt.erase(m_pendingInterrupt.begin());
                }
                if (!m_pendingInterrupt.isEmpty())
                {
                    hasPending = true;
                    targetValue = m_pendingInterrupt[0u]->timelineSemaphoreValue;
                }
            }

            if (!hasPending)
            {
                return 0;
            }

            gp::UInt64 completedValue = 0u;
            if (!m_timelineSemaphore->getTimelineSemaphoreValue(completedValue))
            {
                return 0;
            }

            if (completedValue < targetValue && timeoutNanoseconds > 0u)
            {
                if (m_timelineSemaphore->waitForTimelineSemaphoreValue(targetValue, timeoutNanoseconds))
                {
                    if (!m_timelineSemaphore->getTimelineSemaphoreValue(completedValue) || completedValue < targetValue)
                    {
                        completedValue = targetValue;
                    }
                }
            }

            m_completedTimelineValue.store(completedValue, gp::memory::MemoryOrder::Release);

            {
                gp::thread::UniqueLock<gp::thread::Mutex> lock(m_interruptMutex);
                while (!m_pendingInterrupt.isEmpty())
                {
                    VulkanPayload* const candidate = m_pendingInterrupt[0u];
                    if (candidate == nullptr)
                    {
                        m_pendingInterrupt.erase(m_pendingInterrupt.begin());
                        continue;
                    }

                    if (candidate->timelineSemaphoreValue <= completedValue)
                    {
                        completedPayloads.pushBack(candidate);
                        m_pendingInterrupt.erase(m_pendingInterrupt.begin());
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            bool waitedOnce = false;
            while (true)
            {
                VulkanFence* headFence = nullptr;
                {
                    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_interruptMutex);
                    while (!m_pendingInterrupt.isEmpty() && m_pendingInterrupt[0u] == nullptr)
                    {
                        m_pendingInterrupt.erase(m_pendingInterrupt.begin());
                    }
                    if (m_pendingInterrupt.isEmpty())
                    {
                        break;
                    }
                    headFence = m_pendingInterrupt[0u]->fence;
                }

                bool isComplete = (headFence == nullptr) || m_fenceManager.isFenceSignaled(headFence);
                if (!isComplete && !waitedOnce && timeoutNanoseconds > 0u)
                {
                    waitedOnce = true;
                    isComplete = m_fenceManager.waitForFence(headFence, timeoutNanoseconds);
                }

                if (!isComplete)
                {
                    break;
                }

                {
                    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_interruptMutex);
                    if (!m_pendingInterrupt.isEmpty() && m_pendingInterrupt[0u] != nullptr &&
                        m_pendingInterrupt[0u]->fence == headFence)
                    {
                        completedPayloads.pushBack(m_pendingInterrupt[0u]);
                        m_pendingInterrupt.erase(m_pendingInterrupt.begin());
                    }
                }
            }
        }
    }

    for (gp::USize index = 0u; index < completedPayloads.size(); ++index)
    {
        VulkanPayload* const payload = completedPayloads[index];
        payload->complete();
        delete payload;
    }

    return static_cast<gp::Int32>(completedPayloads.size());
}

void VulkanQueue::waitUntilIdle() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> queueLock(m_queueExecutionMutex);
    if (m_queue != VK_NULL_HANDLE && dynamic_api::vkQueueWaitIdle != nullptr)
    {
        const VkResult result = vkQueueWaitIdle(m_queue);
        if (result != VK_SUCCESS)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "vkQueueWaitIdle failed on {} queue with {}.",
                getVulkanQueueTypeName(m_queueType),
                vulkanResultToString(result)
            );
        }
    }
}

gp::USize VulkanQueue::getPendingSubmissionCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_submissionMutex);
    return m_pendingSubmission.size();
}

gp::USize VulkanQueue::getPendingInterruptCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_interruptMutex);
    return m_pendingInterrupt.size();
}

}   // namespace gp::rhi::vulkan
