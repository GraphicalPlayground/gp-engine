// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/VulkanFence.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

VulkanFence::VulkanFence(VulkanDevice& device, VulkanFenceManager& owner, const bool createSignaled) noexcept
    : m_handle(VK_NULL_HANDLE)
    , m_state(State::NotReady)
    , m_owner(owner)
{
    if (!device.isValid() || dynamic_api::vkCreateFence == nullptr)
    {
        return;
    }

    VkFenceCreateInfo createInfo = zeroVulkanStruct<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    createInfo.flags = createSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u;

    const VkResult result = vkCreateFence(device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_handle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateFence failed with {}.", vulkanResultToString(result));
        m_handle = VK_NULL_HANDLE;
        m_state = State::NotReady;
    }
    else if (createSignaled)
    {
        m_state = State::Signaled;
    }
}

VulkanFence::~VulkanFence() noexcept
{
    GP_CHECKF(m_handle == VK_NULL_HANDLE, "VulkanFence must be destroyed through VulkanFenceManager.");
}

VulkanFenceManager::~VulkanFenceManager() noexcept
{
    deinit();
}

void VulkanFenceManager::destroyFence(VulkanFence* const fence) noexcept
{
    if (fence == nullptr)
    {
        return;
    }

    if (fence->m_handle != VK_NULL_HANDLE && m_device.isValid() && dynamic_api::vkDestroyFence != nullptr)
    {
        vkDestroyFence(m_device.getHandle(), fence->m_handle, GP_VULKAN_CPU_ALLOCATOR);
    }
    fence->m_handle = VK_NULL_HANDLE;
    delete fence;
}

void VulkanFenceManager::removeUsedFenceLocked(VulkanFence* const fence) noexcept
{
    for (gp::USize i = 0u; i < m_usedFences.size(); ++i)
    {
        if (m_usedFences[i] == fence)
        {
            const gp::USize lastIdx = m_usedFences.size() - 1u;
            m_usedFences[i] = m_usedFences[lastIdx];
            m_usedFences.popBack();
            return;
        }
    }
}

void VulkanFenceManager::deinit() noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_fenceLock);

    if (!m_usedFences.isEmpty())
    {
        GP_LOG_WARNING(
            LogVulkanRHI,
            "VulkanFenceManager::deinit() called with {} fence(s) still checked out; destroying them.",
            static_cast<gp::UInt32>(m_usedFences.size())
        );
        for (VulkanFence* const fence: m_usedFences)
        {
            destroyFence(fence);
        }
        m_usedFences.clear();
    }

    for (VulkanFence* const fence: m_freeFences)
    {
        destroyFence(fence);
    }
    m_freeFences.clear();
}

VulkanFence* VulkanFenceManager::allocateFence(const bool createSignaled) noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_fenceLock);

    if (!m_freeFences.isEmpty())
    {
        // Note: a recycled VkFence is unsignaled on the GPU; if the caller explicitly requests a signaled
        // VkFence handle (for example for vkWaitForFences), create a fresh signaled VkFence when needed.
        if (!createSignaled)
        {
            VulkanFence* const fence = m_freeFences.back();
            m_freeFences.popBack();
            fence->m_state = VulkanFence::State::NotReady;
            m_usedFences.pushBack(fence);
            return fence;
        }
    }

    if (!m_device.isValid())
    {
        return nullptr;
    }

    VulkanFence* const newFence = new VulkanFence(m_device, *this, createSignaled);
    if (newFence->getHandle() == VK_NULL_HANDLE)
    {
        delete newFence;
        return nullptr;
    }

    m_usedFences.pushBack(newFence);
    return newFence;
}

bool VulkanFenceManager::isFenceSignaled(VulkanFence* const fence) noexcept
{
    if (fence == nullptr)
    {
        return false;
    }

    if (fence->isSignaled())
    {
        return true;
    }

    return checkFenceState(fence);
}

bool VulkanFenceManager::checkFenceState(VulkanFence* const fence) noexcept
{
    if (fence == nullptr || fence->m_handle == VK_NULL_HANDLE || !m_device.isValid() ||
        dynamic_api::vkGetFenceStatus == nullptr)
    {
        return false;
    }

    const VkResult result = vkGetFenceStatus(m_device.getHandle(), fence->m_handle);
    switch (result)
    {
    case VK_SUCCESS:
        fence->m_state = VulkanFence::State::Signaled;
        return true;

    case VK_NOT_READY:
        return false;

    default:
        GP_LOG_ERROR(LogVulkanRHI, "vkGetFenceStatus failed with {}.", vulkanResultToString(result));
        return false;
    }
}

bool VulkanFenceManager::waitForFence(VulkanFence* const fence, const gp::UInt64 timeInNanoseconds) noexcept
{
    if (fence == nullptr || fence->m_handle == VK_NULL_HANDLE || !m_device.isValid() ||
        dynamic_api::vkWaitForFences == nullptr)
    {
        return false;
    }

    if (fence->isSignaled())
    {
        return true;
    }

    const VkResult result = vkWaitForFences(m_device.getHandle(), 1u, &fence->m_handle, VK_TRUE, timeInNanoseconds);
    switch (result)
    {
    case VK_SUCCESS:
        fence->m_state = VulkanFence::State::Signaled;
        return true;

    case VK_TIMEOUT:
        return false;

    default:
        GP_LOG_ERROR(LogVulkanRHI, "vkWaitForFences failed with {}.", vulkanResultToString(result));
        return false;
    }
}

bool VulkanFenceManager::waitForAnyFence(
    const gp::ConstVectorView<VulkanFence*> fences, const gp::UInt64 timeInNanoseconds
) noexcept
{
    if (fences.isEmpty() || !m_device.isValid() || dynamic_api::vkWaitForFences == nullptr)
    {
        return false;
    }

    // Fast path avoids heap allocation for typical small fence batches (e.g., per-queue waits).
    constexpr gp::USize kInlineCapacity = 8u;
    gp::Array<VkFence, kInlineCapacity> inlineHandles{};
    gp::USize inlineCount = 0u;
    gp::Vector<VkFence> overflowHandles;

    for (VulkanFence* const fence: fences)
    {
        if (fence == nullptr)
        {
            continue;
        }
        if (fence->isSignaled())
        {
            return true;
        }
        if (fence->m_handle != VK_NULL_HANDLE)
        {
            if (inlineCount < kInlineCapacity && overflowHandles.isEmpty())
            {
                inlineHandles[inlineCount++] = fence->m_handle;
            }
            else
            {
                if (overflowHandles.isEmpty())
                {
                    overflowHandles.reserve(fences.size());
                    for (gp::USize i = 0u; i < inlineCount; ++i)
                    {
                        overflowHandles.pushBack(inlineHandles[i]);
                    }
                }
                overflowHandles.pushBack(fence->m_handle);
            }
        }
    }

    const VkFence* const handlesPtr = overflowHandles.isEmpty() ? inlineHandles.data() : overflowHandles.data();
    const gp::UInt32 handleCount =
        static_cast<gp::UInt32>(overflowHandles.isEmpty() ? inlineCount : overflowHandles.size());
    if (handleCount == 0u)
    {
        return false;
    }

    const VkResult result = vkWaitForFences(m_device.getHandle(), handleCount, handlesPtr, VK_FALSE, timeInNanoseconds);

    switch (result)
    {
    case VK_SUCCESS:
        if (fences.size() == 1u && fences[0] != nullptr)
        {
            fences[0]->m_state = VulkanFence::State::Signaled;
        }
        return true;

    case VK_TIMEOUT:
        return false;

    default:
        GP_LOG_ERROR(LogVulkanRHI, "vkWaitForFences (any) failed with {}.", vulkanResultToString(result));
        return false;
    }
}

void VulkanFenceManager::resetFence(VulkanFence* const fence) noexcept
{
    if (fence == nullptr || fence->m_handle == VK_NULL_HANDLE || !m_device.isValid() ||
        dynamic_api::vkResetFences == nullptr)
    {
        return;
    }

    if (fence->m_state != VulkanFence::State::NotReady)
    {
        const VkResult result = vkResetFences(m_device.getHandle(), 1u, &fence->m_handle);
        if (result != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkResetFences failed with {}.", vulkanResultToString(result));
        }
        fence->m_state = VulkanFence::State::NotReady;
    }
}

void VulkanFenceManager::releaseFence(VulkanFence*& fence) noexcept
{
    if (fence == nullptr)
    {
        return;
    }

    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_fenceLock);
    resetFence(fence);
    removeUsedFenceLocked(fence);

#if GP_VULKAN_REUSE_FENCES
    m_freeFences.pushBack(fence);
#else
    destroyFence(fence);
#endif
    fence = nullptr;
}

void VulkanFenceManager::waitAndReleaseFence(VulkanFence*& fence, const gp::UInt64 timeInNanoseconds) noexcept
{
    if (fence == nullptr)
    {
        return;
    }

    // Perform the potentially blocking GPU wait before acquiring the pool lock so other threads are not stalled.
    if (!fence->isSignaled())
    {
        static_cast<void>(waitForFence(fence, timeInNanoseconds));
    }

    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_fenceLock);
    resetFence(fence);
    removeUsedFenceLocked(fence);

#if GP_VULKAN_REUSE_FENCES
    m_freeFences.pushBack(fence);
#else
    destroyFence(fence);
#endif
    fence = nullptr;
}

gp::USize VulkanFenceManager::getFreeFenceCount() const noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_fenceLock);
    return m_freeFences.size();
}

gp::USize VulkanFenceManager::getUsedFenceCount() const noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(m_fenceLock);
    return m_usedFences.size();
}

}   // namespace gp::rhi::vulkan
