// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanAllocation.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

VulkanAllocation::~VulkanAllocation() noexcept
{
    GP_CHECKF(
        !hasAllocation(),
        "VulkanAllocation destroyed while still owning GPU memory; call free() or enqueue for deferred deletion."
    );
}

void VulkanAllocation::init(
    const VulkanAllocationType allocType,
    const VulkanAllocationMetaType allocMetaType,
    const gp::UInt64 handle,
    const gp::UInt32 allocSize,
    const gp::UInt32 alignedOffset,
    const gp::UInt32 allocIndex,
    const gp::UInt32 blockIndex,
    const gp::UInt32 bufferId
) noexcept
{
    GP_CHECK(!hasAllocation());
    GP_CHECK(allocIndex <= 0xFFFFu);
    vulkanHandle = handle;
    handleId = bufferId;
    size = allocSize;
    offset = alignedOffset;
    allocationIndex = blockIndex;
    allocatorIndex = static_cast<gp::UInt16>(allocIndex);
    metaType = allocMetaType;
    type = static_cast<gp::UInt8>(allocType);
    isPoolAllocated = 0u;
    hasOwnership = 1u;
    isTransient = 0u;
    isTempBlockAllocator = 0u;
    isUnmovable = 0u;
}

void VulkanAllocation::free(VulkanMemoryManager& memoryManager, const VulkanFreeFlags freeFlags) noexcept
{
    if (hasAllocation())
    {
        memoryManager.freeVulkanAllocation(*this, freeFlags);
    }
}

void VulkanAllocation::swap(VulkanAllocation& other) noexcept
{
    std::swap(*this, other);
}

void VulkanAllocation::reference(const VulkanAllocation& other) noexcept
{
    GP_CHECK(!hasAllocation());
    *this = other;
    hasOwnership = 0u;
}

void* VulkanAllocation::getMappedPointer(VulkanMemoryManager& memoryManager) const noexcept
{
    VulkanDeviceMemoryAllocation* const deviceAlloc = memoryManager.getDeviceMemoryAllocation(*this);
    if (deviceAlloc == nullptr || !deviceAlloc->isMapped())
    {
        return nullptr;
    }

    gp::UInt8* const basePtr = static_cast<gp::UInt8*>(deviceAlloc->getMappedPointer());
    return (basePtr != nullptr) ? static_cast<void*>(basePtr + offset) : nullptr;
}

void VulkanAllocation::flushMappedMemory(VulkanMemoryManager& memoryManager) const noexcept
{
    VulkanDeviceMemoryAllocation* const deviceAlloc = memoryManager.getDeviceMemoryAllocation(*this);
    if (deviceAlloc != nullptr)
    {
        flushMappedMemoryRange(memoryManager.getDevice(), *deviceAlloc, offset, size);
    }
}

void VulkanAllocation::invalidateMappedMemory(VulkanMemoryManager& memoryManager) const noexcept
{
    VulkanDeviceMemoryAllocation* const deviceAlloc = memoryManager.getDeviceMemoryAllocation(*this);
    if (deviceAlloc != nullptr)
    {
        invalidateMappedMemoryRange(memoryManager.getDevice(), *deviceAlloc, offset, size);
    }
}

VkDeviceMemory VulkanAllocation::getDeviceMemoryHandle(VulkanMemoryManager& memoryManager) const noexcept
{
    const VulkanDeviceMemoryAllocation* const deviceAlloc = memoryManager.getDeviceMemoryAllocation(*this);
    return (deviceAlloc != nullptr) ? deviceAlloc->getHandle() : VK_NULL_HANDLE;
}

VkMemoryPropertyFlags VulkanAllocation::getMemoryPropertyFlags(VulkanMemoryManager& memoryManager) const noexcept
{
    const VulkanDeviceMemoryAllocation* const deviceAlloc = memoryManager.getDeviceMemoryAllocation(*this);
    if (deviceAlloc == nullptr)
    {
        return 0u;
    }

    const VkPhysicalDeviceMemoryProperties& memProps = memoryManager.getDeviceMemoryManager().getMemoryProperties();
    const gp::UInt32 memTypeIndex = deviceAlloc->getMemoryTypeIndex();
    return (memTypeIndex < memProps.memoryTypeCount) ? memProps.memoryTypes[memTypeIndex].propertyFlags : 0u;
}

VkResult VulkanAllocation::bindBuffer(VulkanMemoryManager& memoryManager, const VkBuffer buffer) const noexcept
{
    const VulkanDevice& device = memoryManager.getDevice();
    const VkDeviceMemory memHandle = getDeviceMemoryHandle(memoryManager);
    if (!device.isValid() || buffer == VK_NULL_HANDLE || memHandle == VK_NULL_HANDLE ||
        dynamic_api::vkBindBufferMemory == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkBindBufferMemory(device.getHandle(), buffer, memHandle, offset);
}

VkResult VulkanAllocation::bindImage(VulkanMemoryManager& memoryManager, const VkImage image) const noexcept
{
    const VulkanDevice& device = memoryManager.getDevice();
    const VkDeviceMemory memHandle = getDeviceMemoryHandle(memoryManager);
    if (!device.isValid() || image == VK_NULL_HANDLE || memHandle == VK_NULL_HANDLE ||
        dynamic_api::vkBindImageMemory == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkBindImageMemory(device.getHandle(), image, memHandle, offset);
}

}   // namespace gp::rhi::vulkan
