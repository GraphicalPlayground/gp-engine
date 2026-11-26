// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanUniformBufferMemory.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "memory/VulkanAllocation.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "memory/VulkanTempBlockAllocator.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

static_assert((kVulkanUniformBufferUsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0u);
static_assert((kVulkanUniformBufferUsageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0u);
static_assert((kVulkanUniformBufferMemoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u);
static_assert((kVulkanUniformBufferMemoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0u);

/// @brief Live fixture for uniform buffer memory helpers.
class VulkanUniformBufferMemoryLiveTest : public VulkanResourceLiveTest
{};

TEST_F(VulkanUniformBufferMemoryLiveTest, AlignmentHonoursTheDeviceLimit)
{
    const gp::UInt32 alignment = getVulkanUniformBufferAlignment(*m_device);
    EXPECT_GE(alignment, 16u);
    EXPECT_GE(static_cast<VkDeviceSize>(alignment), m_device->getLimits().minUniformBufferOffsetAlignment);
    EXPECT_EQ(alignment & (alignment - 1u), 0u);
}

TEST_F(VulkanUniformBufferMemoryLiveTest, PooledAllocationIsMappedAndFreedImmediately)
{
    VulkanAllocation allocation{};
    ASSERT_TRUE(allocateVulkanUniformBufferMemory(*m_memoryManager, allocation, 256u));
    EXPECT_TRUE(allocation.hasAllocation());
    EXPECT_NE(allocation.getBufferHandle(), VK_NULL_HANDLE);
    EXPECT_GE(allocation.size, 256u);
    EXPECT_EQ(allocation.isTempBlockAllocator, 0u);
    EXPECT_EQ(allocation.offset % getVulkanUniformBufferAlignment(*m_device), 0u);

    gp::UInt8* const mapped = static_cast<gp::UInt8*>(allocation.getMappedPointer(*m_memoryManager));
    ASSERT_NE(mapped, nullptr);
    mapped[0] = 0x11u;
    mapped[255] = 0x22u;

    freeVulkanUniformBufferMemory(*m_memoryManager, nullptr, nullptr, allocation);
    EXPECT_FALSE(allocation.hasAllocation());
}

TEST_F(VulkanUniformBufferMemoryLiveTest, FreeingAnEmptyAllocationIsANoOp)
{
    VulkanAllocation allocation{};
    freeVulkanUniformBufferMemory(*m_memoryManager, nullptr, nullptr, allocation);
    EXPECT_FALSE(allocation.hasAllocation());
}

TEST_F(VulkanUniformBufferMemoryLiveTest, TempAllocationsReleaseTheirReferenceThroughTheDeletionQueue)
{
    VulkanTempBlockAllocator tempAllocator(
        *m_memoryManager,
        64u * 1024u,
        VulkanTempBlockAllocator::computeDefaultBlockAlignment(*m_device),
        VulkanTempBlockAllocator::computeDefaultBufferUsage(*m_device)
    );
    ASSERT_TRUE(tempAllocator.isValid());
    VulkanDeferredDeletionQueue deletionQueue(*m_device, *m_deviceMemoryManager, *m_memoryManager);

    VulkanAllocation allocation{};
    ASSERT_NE(tempAllocator.allocate(128u, getVulkanUniformBufferAlignment(*m_device), nullptr, allocation), nullptr);
    EXPECT_EQ(tempAllocator.getCurrentBlockReferences(), 1u);

    // The reference is dropped once the GPU can no longer read the range.
    freeVulkanUniformBufferMemory(*m_memoryManager, &deletionQueue, &tempAllocator, allocation);
    EXPECT_FALSE(allocation.hasAllocation());
    EXPECT_EQ(tempAllocator.getCurrentBlockReferences(), 1u);
    EXPECT_EQ(deletionQueue.getPendingEntryCount(), 1u);

    deletionQueue.releaseResources(true, nullptr, 0u);
    EXPECT_EQ(tempAllocator.getCurrentBlockReferences(), 0u);
}

TEST_F(VulkanUniformBufferMemoryLiveTest, TempAllocationsWithoutAQueueReleaseImmediately)
{
    VulkanTempBlockAllocator tempAllocator(
        *m_memoryManager,
        64u * 1024u,
        VulkanTempBlockAllocator::computeDefaultBlockAlignment(*m_device),
        VulkanTempBlockAllocator::computeDefaultBufferUsage(*m_device)
    );
    ASSERT_TRUE(tempAllocator.isValid());

    VulkanAllocation allocation{};
    ASSERT_NE(tempAllocator.allocate(128u, getVulkanUniformBufferAlignment(*m_device), nullptr, allocation), nullptr);
    freeVulkanUniformBufferMemory(*m_memoryManager, nullptr, &tempAllocator, allocation);
    EXPECT_FALSE(allocation.hasAllocation());
    EXPECT_EQ(tempAllocator.getCurrentBlockReferences(), 0u);
}

}   // namespace gp::rhi::vulkan::tests
