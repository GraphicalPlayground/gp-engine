// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanResourceServices.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

bool VulkanResourceServices::isValid() const noexcept
{
    return (device != nullptr) && (memoryManager != nullptr) && (stagingManager != nullptr);
}

bool VulkanResourceServices::hasUnifiedMemory() const noexcept
{
    return (memoryManager != nullptr) && memoryManager->getDeviceMemoryManager().hasUnifiedMemory();
}

void VulkanResourceServices::enqueueBufferDeletion(const VkBuffer buffer) const noexcept
{
    if (buffer == VK_NULL_HANDLE)
    {
        return;
    }

    if (deferredDeletionQueue != nullptr)
    {
        deferredDeletionQueue->enqueueHandle(VulkanDeferredDeletionQueue::EntryType::Buffer, buffer);
        return;
    }

    if ((device != nullptr) && (dynamic_api::vkDestroyBuffer != nullptr))
    {
        vkDestroyBuffer(device->getHandle(), buffer, GP_VULKAN_CPU_ALLOCATOR);
    }
}

}   // namespace gp::rhi::vulkan

