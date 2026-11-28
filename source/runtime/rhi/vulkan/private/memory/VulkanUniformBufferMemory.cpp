// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanUniformBufferMemory.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/VulkanAllocation.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "memory/VulkanTempBlockAllocator.hpp"

namespace gp::rhi::vulkan
{

gp::UInt32 getVulkanUniformBufferAlignment(const VulkanDevice& device) noexcept
{
    return gp::math::max<gp::UInt32>(static_cast<gp::UInt32>(device.getLimits().minUniformBufferOffsetAlignment), 16u);
}

bool allocateVulkanUniformBufferMemory(
    VulkanMemoryManager& memoryManager, VulkanAllocation& outAllocation, const gp::UInt32 size
) noexcept
{
    const gp::UInt32 alignment = getVulkanUniformBufferAlignment(memoryManager.getDevice());
    if (!memoryManager.allocateBufferPooled(
            outAllocation,
            size,
            alignment,
            kVulkanUniformBufferUsageFlags,
            kVulkanUniformBufferMemoryFlags,
            VulkanAllocationMetaType::UniformBuffer
        ))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to allocate {} bytes of uniform buffer memory.", size);
        return false;
    }
    return true;
}

void freeVulkanUniformBufferMemory(
    VulkanMemoryManager& memoryManager,
    VulkanDeferredDeletionQueue* const deletionQueue,
    VulkanTempBlockAllocator* const tempBlockAllocator,
    VulkanAllocation& allocation
) noexcept
{
    if (allocation.isTempBlockAllocator != 0u)
    {
        GP_CHECKF(tempBlockAllocator != nullptr, "Temp block uniform buffer freed without its allocator.");
        if (tempBlockAllocator != nullptr)
        {
            if (deletionQueue != nullptr)
            {
                const VulkanAllocation allocationCopy = allocation;
                deletionQueue->enqueueFunc([tempBlockAllocator, allocationCopy](VulkanDevice&) noexcept
                {
                    tempBlockAllocator->freeReference(allocationCopy);
                });
            }
            else
            {
                tempBlockAllocator->freeReference(allocation);
            }
        }
        allocation = VulkanAllocation{};
        return;
    }

    if (allocation.hasAllocation())
    {
        memoryManager.freeVulkanAllocation(allocation);
    }
    allocation = VulkanAllocation{};
}

}   // namespace gp::rhi::vulkan
