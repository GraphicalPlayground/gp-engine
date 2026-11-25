// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/VulkanCommandBuffer.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "logging/LogMacros.hpp"
#include "platforms/base/system/Time.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Resolves `vkCmdBeginRenderingKHR` or its Vulkan 1.3 core equivalent for `device`.
/// @param[in] device Active logical `VkDevice`.
/// @return Resolved function pointer, or `nullptr` if unavailable.
[[nodiscard]] PFN_vkCmdBeginRendering resolveCmdBeginRenderingFn(const VkDevice device) noexcept
{
    if (dynamic_api::vkCmdBeginRenderingKHR != nullptr)
    {
        return dynamic_api::vkCmdBeginRenderingKHR;
    }
    if (device != VK_NULL_HANDLE)
    {
        return reinterpret_cast<PFN_vkCmdBeginRendering>(
            VulkanLoader::getDeviceProcAddr(device, "vkCmdBeginRendering")
        );
    }
    return nullptr;
}

/// @brief Resolves `vkCmdEndRenderingKHR` or its Vulkan 1.3 core equivalent for `device`.
/// @param[in] device Active logical `VkDevice`.
/// @return Resolved function pointer, or `nullptr` if unavailable.
[[nodiscard]] PFN_vkCmdEndRendering resolveCmdEndRenderingFn(const VkDevice device) noexcept
{
    if (dynamic_api::vkCmdEndRenderingKHR != nullptr)
    {
        return dynamic_api::vkCmdEndRenderingKHR;
    }
    if (device != VK_NULL_HANDLE)
    {
        return reinterpret_cast<PFN_vkCmdEndRendering>(VulkanLoader::getDeviceProcAddr(device, "vkCmdEndRendering"));
    }
    return nullptr;
}

/// @brief Resolves `vkTrimCommandPool` or `vkTrimCommandPoolKHR` for `device`.
/// @param[in] device Active logical `VkDevice`.
/// @return Resolved function pointer, or `nullptr` if unavailable.
[[nodiscard]] PFN_vkTrimCommandPool resolveTrimCommandPoolFn(const VkDevice device) noexcept
{
    if (dynamic_api::vkTrimCommandPool != nullptr)
    {
        return dynamic_api::vkTrimCommandPool;
    }
    if (device != VK_NULL_HANDLE)
    {
        const PFN_vkTrimCommandPool fn =
            reinterpret_cast<PFN_vkTrimCommandPool>(VulkanLoader::getDeviceProcAddr(device, "vkTrimCommandPool"));
        if (fn != nullptr)
        {
            return fn;
        }
        return reinterpret_cast<PFN_vkTrimCommandPool>(VulkanLoader::getDeviceProcAddr(device, "vkTrimCommandPoolKHR"));
    }
    return nullptr;
}

}   // namespace

VulkanCommandBuffer::VulkanCommandBuffer(
    VulkanDevice& device, VulkanCommandBufferPool& owner, const VulkanCommandBufferType commandBufferType
) noexcept
    : m_device(device)
    , m_owner(owner)
    , m_commandBufferType(commandBufferType)
    , m_state(VulkanCommandBufferState::NotAllocated)
{
    allocateMemory();
}

VulkanCommandBuffer::~VulkanCommandBuffer() noexcept
{
    freeMemory();
}

void VulkanCommandBuffer::allocateMemory() noexcept
{
    if (!m_device.isValid() || !m_owner.isValid() || dynamic_api::vkAllocateCommandBuffers == nullptr)
    {
        m_handle = VK_NULL_HANDLE;
        m_state = VulkanCommandBufferState::NotAllocated;
        return;
    }

    VkCommandBufferAllocateInfo allocInfo =
        zeroVulkanStruct<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    allocInfo.commandPool = m_owner.getHandle();
    allocInfo.level = (m_commandBufferType == VulkanCommandBufferType::Secondary) ? VK_COMMAND_BUFFER_LEVEL_SECONDARY
                                                                                  : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1u;

    const VkResult result = vkAllocateCommandBuffers(m_device.getHandle(), &allocInfo, &m_handle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkAllocateCommandBuffers failed with {}.", vulkanResultToString(result));
        m_handle = VK_NULL_HANDLE;
        m_state = VulkanCommandBufferState::NotAllocated;
        return;
    }

    m_state = VulkanCommandBufferState::ReadyForBegin;
    m_lastActiveTimestampNs = gp::platform::Time::nowNanoseconds();
    resetDynamicState();
}

void VulkanCommandBuffer::freeMemory() noexcept
{
    if (m_handle != VK_NULL_HANDLE && m_device.isValid() && m_owner.isValid() &&
        dynamic_api::vkFreeCommandBuffers != nullptr)
    {
        vkFreeCommandBuffers(m_device.getHandle(), m_owner.getHandle(), 1u, &m_handle);
    }

    m_handle = VK_NULL_HANDLE;
    m_state = VulkanCommandBufferState::NotAllocated;
    m_executedSecondaryBuffers.clear();
    m_endedBarrierEvents.clear();
    releasePooledBarrierEvents();
    m_waitFlags.clear();
    m_waitSemaphores.clear();
}

void VulkanCommandBuffer::resetDynamicState() noexcept
{
    m_needsDynamicStateSet = true;
    m_currentGraphicsPipeline = VK_NULL_HANDLE;
    m_currentComputePipeline = VK_NULL_HANDLE;
    m_endedBarrierEvents.clear();
}

void VulkanCommandBuffer::releasePooledBarrierEvents() noexcept
{
    for (gp::USize index = 0u; index < m_pooledBarrierEvents.size(); ++index)
    {
        m_device.releaseBarrierEvent(m_pooledBarrierEvents[index]);
    }
    m_pooledBarrierEvents.clear();
}

bool VulkanCommandBuffer::begin(
    const VkRenderPass renderPassHandle, const gp::UInt32 subpass, const VkFramebuffer framebufferHandle
) noexcept
{
    if (m_handle == VK_NULL_HANDLE || dynamic_api::vkBeginCommandBuffer == nullptr)
    {
        return false;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> poolLock(m_owner.m_mutex);

    if (m_state == VulkanCommandBufferState::NeedReset)
    {
        if (dynamic_api::vkResetCommandBuffer == nullptr)
        {
            return false;
        }

        const VkResult resetResult = vkResetCommandBuffer(m_handle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        if (resetResult != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkResetCommandBuffer failed with {}.", vulkanResultToString(resetResult));
            return false;
        }

        m_state = VulkanCommandBufferState::ReadyForBegin;
    }

    if (m_state != VulkanCommandBufferState::ReadyForBegin)
    {
        GP_LOG_ERROR(
            LogVulkanRHI, "VulkanCommandBuffer::begin() called in invalid state ({}).", static_cast<gp::UInt32>(m_state)
        );
        return false;
    }

    VkCommandBufferBeginInfo beginInfo =
        zeroVulkanStruct<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkCommandBufferInheritanceInfo inheritanceInfo =
        zeroVulkanStruct<VkCommandBufferInheritanceInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
    const bool isSecondaryContinuingRenderPass =
        (m_commandBufferType == VulkanCommandBufferType::Secondary) && (renderPassHandle != VK_NULL_HANDLE);
    if (m_commandBufferType == VulkanCommandBufferType::Secondary)
    {
        if (isSecondaryContinuingRenderPass)
        {
            beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            inheritanceInfo.renderPass = renderPassHandle;
            inheritanceInfo.subpass = subpass;
            inheritanceInfo.framebuffer = framebufferHandle;
        }
        beginInfo.pInheritanceInfo = &inheritanceInfo;
    }

    const VkResult result = vkBeginCommandBuffer(m_handle, &beginInfo);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkBeginCommandBuffer failed with {}.", vulkanResultToString(result));
        return false;
    }

    m_state = isSecondaryContinuingRenderPass ? VulkanCommandBufferState::IsInsideRenderPass
                                              : VulkanCommandBufferState::IsInsideBegin;
    m_lastActiveTimestampNs = gp::platform::Time::nowNanoseconds();
    resetDynamicState();
    return true;
}

bool VulkanCommandBuffer::end() noexcept
{
    if (m_handle == VK_NULL_HANDLE || dynamic_api::vkEndCommandBuffer == nullptr)
    {
        return false;
    }

    const bool canEndFromState = (m_state == VulkanCommandBufferState::IsInsideBegin) ||
                                 ((m_commandBufferType == VulkanCommandBufferType::Secondary) &&
                                  (m_state == VulkanCommandBufferState::IsInsideRenderPass));
    if (!canEndFromState)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "VulkanCommandBuffer::end() called in invalid state ({}); expected IsInsideBegin.",
            static_cast<gp::UInt32>(m_state)
        );
        return false;
    }

    if (m_state == VulkanCommandBufferState::IsInsideBegin && !m_endedBarrierEvents.isEmpty() &&
        dynamic_api::vkCmdResetEvent != nullptr)
    {
        for (gp::USize index = 0u; index < m_endedBarrierEvents.size(); ++index)
        {
            if (m_endedBarrierEvents[index] != VK_NULL_HANDLE)
            {
                vkCmdResetEvent(m_handle, m_endedBarrierEvents[index], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            }
        }
    }

    const VkResult result = vkEndCommandBuffer(m_handle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkEndCommandBuffer failed with {}.", vulkanResultToString(result));
        return false;
    }

    m_endedBarrierEvents.clear();
    m_state = VulkanCommandBufferState::HasEnded;
    m_lastActiveTimestampNs = gp::platform::Time::nowNanoseconds();
    return true;
}

void VulkanCommandBuffer::setSubmitted() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> poolLock(m_owner.m_mutex);
    GP_CHECK(m_state == VulkanCommandBufferState::HasEnded);
    m_state = VulkanCommandBufferState::Submitted;
    m_lastActiveTimestampNs = gp::platform::Time::nowNanoseconds();
}

void VulkanCommandBuffer::reset() noexcept
{
    gp::Vector<VulkanCommandBuffer*> secondaryToReset = std::move(m_executedSecondaryBuffers);
    m_executedSecondaryBuffers.clear();

    for (gp::USize index = 0u; index < secondaryToReset.size(); ++index)
    {
        VulkanCommandBuffer* const secondaryBuffer = secondaryToReset[index];
        if (secondaryBuffer != nullptr)
        {
            secondaryBuffer->reset();
        }
    }

    m_waitFlags.clear();
    m_waitSemaphores.clear();
    resetDynamicState();

    gp::thread::UniqueLock<gp::thread::Mutex> poolLock(m_owner.m_mutex);
    if (m_state == VulkanCommandBufferState::Submitted || m_state == VulkanCommandBufferState::HasEnded)
    {
        // The GPU is done with this buffer, so its split-barrier events can be reused.
        releasePooledBarrierEvents();
        m_state = VulkanCommandBufferState::NeedReset;
        m_lastActiveTimestampNs = gp::platform::Time::nowNanoseconds();
        m_owner.recycleCommandBufferLocked(this);
    }
    else if (m_state == VulkanCommandBufferState::ReadyForBegin || m_state == VulkanCommandBufferState::NeedReset)
    {
        m_lastActiveTimestampNs = gp::platform::Time::nowNanoseconds();
        m_owner.recycleCommandBufferLocked(this);
    }
}

bool VulkanCommandBuffer::beginRenderPass(
    const VkRenderPassBeginInfo& beginInfo, const VkSubpassContents contents
) noexcept
{
    if (m_handle == VK_NULL_HANDLE || m_commandBufferType == VulkanCommandBufferType::Secondary ||
        m_state != VulkanCommandBufferState::IsInsideBegin || dynamic_api::vkCmdBeginRenderPass == nullptr)
    {
        return false;
    }

    vkCmdBeginRenderPass(m_handle, &beginInfo, contents);
    m_state = VulkanCommandBufferState::IsInsideRenderPass;
    return true;
}

bool VulkanCommandBuffer::endRenderPass() noexcept
{
    if (m_handle == VK_NULL_HANDLE || m_commandBufferType == VulkanCommandBufferType::Secondary ||
        m_state != VulkanCommandBufferState::IsInsideRenderPass || dynamic_api::vkCmdEndRenderPass == nullptr)
    {
        return false;
    }

    vkCmdEndRenderPass(m_handle);
    m_state = VulkanCommandBufferState::IsInsideBegin;
    return true;
}

bool VulkanCommandBuffer::beginDynamicRendering(const VkRenderingInfo& renderingInfo) noexcept
{
    if (m_handle == VK_NULL_HANDLE || m_commandBufferType == VulkanCommandBufferType::Secondary ||
        m_state != VulkanCommandBufferState::IsInsideBegin)
    {
        return false;
    }

    const PFN_vkCmdBeginRendering beginRenderingFn = resolveCmdBeginRenderingFn(m_device.getHandle());
    if (beginRenderingFn == nullptr)
    {
        return false;
    }

    beginRenderingFn(m_handle, &renderingInfo);
    m_state = VulkanCommandBufferState::IsInsideRenderPass;
    return true;
}

bool VulkanCommandBuffer::endDynamicRendering() noexcept
{
    if (m_handle == VK_NULL_HANDLE || m_commandBufferType == VulkanCommandBufferType::Secondary ||
        m_state != VulkanCommandBufferState::IsInsideRenderPass)
    {
        return false;
    }

    const PFN_vkCmdEndRendering endRenderingFn = resolveCmdEndRenderingFn(m_device.getHandle());
    if (endRenderingFn == nullptr)
    {
        return false;
    }

    endRenderingFn(m_handle);
    m_state = VulkanCommandBufferState::IsInsideBegin;
    return true;
}

bool VulkanCommandBuffer::executeSecondaryCommandBuffer(VulkanCommandBuffer* const secondaryCmdBuffer) noexcept
{
    if (m_handle == VK_NULL_HANDLE || secondaryCmdBuffer == nullptr || !secondaryCmdBuffer->isValid() ||
        m_commandBufferType == VulkanCommandBufferType::Secondary ||
        secondaryCmdBuffer->getCommandBufferType() != VulkanCommandBufferType::Secondary || !hasBegun() ||
        !secondaryCmdBuffer->hasEnded() || dynamic_api::vkCmdExecuteCommands == nullptr)
    {
        return false;
    }

    const VkCommandBuffer secondaryHandle = secondaryCmdBuffer->getHandle();
    vkCmdExecuteCommands(m_handle, 1u, &secondaryHandle);
    secondaryCmdBuffer->setSubmitted();
    m_executedSecondaryBuffers.pushBack(secondaryCmdBuffer);
    return true;
}

bool VulkanCommandBuffer::beginSplitBarrier(const VkEvent event, const VkPipelineStageFlags srcStageMask) noexcept
{
    if (m_handle == VK_NULL_HANDLE || event == VK_NULL_HANDLE || m_state != VulkanCommandBufferState::IsInsideBegin ||
        dynamic_api::vkCmdSetEvent == nullptr)
    {
        return false;
    }

    vkCmdSetEvent(m_handle, event, srcStageMask);
    return true;
}

bool VulkanCommandBuffer::endSplitBarrier(
    const VkEvent event,
    const VkPipelineStageFlags srcStageMask,
    const VkPipelineStageFlags dstStageMask,
    const VkMemoryBarrier* const memoryBarrier
) noexcept
{
    if (m_handle == VK_NULL_HANDLE || event == VK_NULL_HANDLE || m_state != VulkanCommandBufferState::IsInsideBegin ||
        dynamic_api::vkCmdWaitEvents == nullptr)
    {
        return false;
    }

    const gp::UInt32 memoryBarrierCount = (memoryBarrier != nullptr) ? 1u : 0u;
    vkCmdWaitEvents(
        m_handle, 1u, &event, srcStageMask, dstStageMask, memoryBarrierCount, memoryBarrier, 0u, nullptr, 0u, nullptr
    );
    m_endedBarrierEvents.pushBack(event);
    return true;
}

bool VulkanCommandBuffer::beginSplitBarrier(const VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept
{
    if (m_handle == VK_NULL_HANDLE || event == VK_NULL_HANDLE || m_state != VulkanCommandBufferState::IsInsideBegin ||
        dynamic_api::vkCmdSetEvent2KHR == nullptr)
    {
        return false;
    }

    vkCmdSetEvent2KHR(m_handle, event, &dependencyInfo);
    return true;
}

bool VulkanCommandBuffer::endSplitBarrier(const VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept
{
    if (m_handle == VK_NULL_HANDLE || event == VK_NULL_HANDLE || m_state != VulkanCommandBufferState::IsInsideBegin ||
        dynamic_api::vkCmdWaitEvents2KHR == nullptr)
    {
        return false;
    }

    vkCmdWaitEvents2KHR(m_handle, 1u, &event, &dependencyInfo);

    // Reset when the buffer ends, then returned to the device pool when the buffer is recycled.
    m_endedBarrierEvents.pushBack(event);
    m_pooledBarrierEvents.pushBack(event);
    return true;
}

void VulkanCommandBuffer::addWaitSemaphore(
    const VkPipelineStageFlags waitFlags, const VulkanSemaphoreRef& semaphore
) noexcept
{
    if (!semaphore || !semaphore->isValid())
    {
        return;
    }

    m_waitFlags.pushBack(waitFlags);
    m_waitSemaphores.pushBack(semaphore);
}

void VulkanCommandBuffer::clearWaitSemaphores() noexcept
{
    m_waitFlags.clear();
    m_waitSemaphores.clear();
}

VulkanCommandBufferPool::VulkanCommandBufferPool(
    VulkanDevice& device, const VulkanCommandBufferType commandBufferType
) noexcept
    : m_device(device)
    , m_commandBufferType(commandBufferType)
{}

VulkanCommandBufferPool::~VulkanCommandBufferPool() noexcept
{
    destroy();
}

bool VulkanCommandBufferPool::create(const gp::UInt32 queueFamilyIndex) noexcept
{
    if (m_handle != VK_NULL_HANDLE)
    {
        return true;
    }

    if (!m_device.isValid() || dynamic_api::vkCreateCommandPool == nullptr)
    {
        return false;
    }

    m_queueFamilyIndex = queueFamilyIndex;

    VkCommandPoolCreateInfo createInfo =
        zeroVulkanStruct<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = m_queueFamilyIndex;

    const VkResult result = vkCreateCommandPool(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_handle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateCommandPool failed with {}.", vulkanResultToString(result));
        m_handle = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void VulkanCommandBufferPool::destroy() noexcept
{
    gp::Vector<VulkanCommandBuffer*> buffersToDelete{};
    VkCommandPool poolToDestroy = VK_NULL_HANDLE;

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        buffersToDelete = std::move(m_cmdBuffers);
        m_cmdBuffers.clear();
        m_freeCmdBuffers.clear();
        poolToDestroy = m_handle;
        m_handle = VK_NULL_HANDLE;
    }

    // Temporarily restore m_handle while freeing individual command buffers? No: vkDestroyCommandPool frees all
    // command buffers implicitly, so deleting wrappers with m_handle == VK_NULL_HANDLE avoids redundant per-buffer
    // vkFreeCommandBuffers calls before destroying the pool.
    for (gp::USize index = 0u; index < buffersToDelete.size(); ++index)
    {
        delete buffersToDelete[index];
    }

    if (poolToDestroy != VK_NULL_HANDLE && m_device.isValid() && dynamic_api::vkDestroyCommandPool != nullptr)
    {
        vkDestroyCommandPool(m_device.getHandle(), poolToDestroy, GP_VULKAN_CPU_ALLOCATOR);
    }
}

VulkanCommandBuffer* VulkanCommandBufferPool::acquireReadyCommandBuffer() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (m_handle == VK_NULL_HANDLE)
    {
        return nullptr;
    }

    while (!m_freeCmdBuffers.isEmpty())
    {
        VulkanCommandBuffer* const candidate = m_freeCmdBuffers.back();
        m_freeCmdBuffers.popBack();

        if (candidate != nullptr && (candidate->getState() == VulkanCommandBufferState::ReadyForBegin ||
                                     candidate->getState() == VulkanCommandBufferState::NeedReset))
        {
            return candidate;
        }
    }

    VulkanCommandBuffer* const newBuffer = new VulkanCommandBuffer(m_device, *this, m_commandBufferType);
    if (!newBuffer->isValid())
    {
        delete newBuffer;
        return nullptr;
    }

    m_cmdBuffers.pushBack(newBuffer);
    return newBuffer;
}

void VulkanCommandBufferPool::recycleCommandBufferLocked(VulkanCommandBuffer* const cmdBuffer) noexcept
{
    if (cmdBuffer == nullptr || m_handle == VK_NULL_HANDLE)
    {
        return;
    }

    for (gp::USize index = 0u; index < m_freeCmdBuffers.size(); ++index)
    {
        if (m_freeCmdBuffers[index] == cmdBuffer)
        {
            return;
        }
    }

    m_freeCmdBuffers.pushBack(cmdBuffer);
}

void VulkanCommandBufferPool::recycleCommandBuffer(VulkanCommandBuffer* const cmdBuffer) noexcept
{
    if (cmdBuffer == nullptr)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    recycleCommandBufferLocked(cmdBuffer);
}

gp::UInt32
    VulkanCommandBufferPool::freeUnusedCmdBuffers(const bool trimMemory, const gp::UInt64 staleTimeoutNs) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (m_handle == VK_NULL_HANDLE)
    {
        return 0u;
    }

    gp::UInt32 freedCount = 0u;
    const gp::UInt64 nowNs = gp::platform::Time::nowNanoseconds();

#if GP_VULKAN_DELETE_STALE_CMDBUFFERS
    const bool shouldDeleteStale = true;
#else
    const bool shouldDeleteStale = trimMemory;
#endif

    if (shouldDeleteStale)
    {
        gp::USize writeIndex = 0u;
        for (gp::USize readIndex = 0u; readIndex < m_freeCmdBuffers.size(); ++readIndex)
        {
            VulkanCommandBuffer* const cmdBuffer = m_freeCmdBuffers[readIndex];
            const bool isIdleState =
                (cmdBuffer != nullptr) && (cmdBuffer->getState() == VulkanCommandBufferState::ReadyForBegin ||
                                           cmdBuffer->getState() == VulkanCommandBufferState::NeedReset);
            const gp::UInt64 lastActiveNs = (cmdBuffer != nullptr) ? cmdBuffer->getLastActiveTimestampNs() : 0u;
            const bool isStale = isIdleState && (nowNs >= lastActiveNs) && ((nowNs - lastActiveNs) >= staleTimeoutNs);

            if (isStale)
            {
                for (gp::USize allIndex = 0u; allIndex < m_cmdBuffers.size(); ++allIndex)
                {
                    if (m_cmdBuffers[allIndex] == cmdBuffer)
                    {
                        const gp::USize lastIndex = m_cmdBuffers.size() - 1u;
                        if (allIndex != lastIndex)
                        {
                            m_cmdBuffers[allIndex] = m_cmdBuffers[lastIndex];
                        }
                        m_cmdBuffers.popBack();
                        break;
                    }
                }

                delete cmdBuffer;
                ++freedCount;
            }
            else
            {
                m_freeCmdBuffers[writeIndex] = cmdBuffer;
                ++writeIndex;
            }
        }
        m_freeCmdBuffers.resize(writeIndex);
    }

    if (trimMemory && m_device.isValid())
    {
        const PFN_vkTrimCommandPool trimFn = resolveTrimCommandPoolFn(m_device.getHandle());
        if (trimFn != nullptr)
        {
            trimFn(m_device.getHandle(), m_handle, 0u);
        }
    }

    return freedCount;
}

bool VulkanCommandBufferPool::resetPool(const VkCommandPoolResetFlags flags) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (m_handle == VK_NULL_HANDLE || !m_device.isValid() || dynamic_api::vkResetCommandPool == nullptr)
    {
        return false;
    }

    const VkResult result = vkResetCommandPool(m_device.getHandle(), m_handle, flags);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkResetCommandPool failed with {}.", vulkanResultToString(result));
        return false;
    }

    const gp::UInt64 nowNs = gp::platform::Time::nowNanoseconds();
    for (gp::USize index = 0u; index < m_cmdBuffers.size(); ++index)
    {
        VulkanCommandBuffer* const cmdBuffer = m_cmdBuffers[index];
        if (cmdBuffer != nullptr && cmdBuffer->isValid())
        {
            cmdBuffer->m_executedSecondaryBuffers.clear();
            cmdBuffer->m_waitFlags.clear();
            cmdBuffer->m_waitSemaphores.clear();
            cmdBuffer->resetDynamicState();
            cmdBuffer->releasePooledBarrierEvents();
            cmdBuffer->m_state = VulkanCommandBufferState::ReadyForBegin;
            cmdBuffer->m_lastActiveTimestampNs = nowNs;
            recycleCommandBufferLocked(cmdBuffer);
        }
    }

    return true;
}

gp::USize VulkanCommandBufferPool::getAllocatedCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_cmdBuffers.size();
}

gp::USize VulkanCommandBufferPool::getFreeCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_freeCmdBuffers.size();
}

}   // namespace gp::rhi::vulkan
