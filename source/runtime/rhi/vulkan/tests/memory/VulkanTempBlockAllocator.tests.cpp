// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanTempBlockAllocator.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "memory/VulkanAllocation.hpp"
#include "memory/VulkanUniformBufferMemory.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

constexpr gp::UInt32 kTestBlockSize = 64u * 1024u;

/// @brief Rounds `value` up to `alignment`, a power of two.
[[nodiscard]] constexpr gp::UInt32 alignTo(const gp::UInt32 value, const gp::UInt32 alignment) noexcept
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

}   // namespace

/// @brief Live fixture that owns a small-block temp allocator.
class VulkanTempBlockAllocatorLiveTest : public VulkanResourceLiveTest
{
protected:
    virtual void SetUp() override
    {
        VulkanResourceLiveTest::SetUp();
        if (IsSkipped() || HasFatalFailure())
        {
            return;
        }

        m_alignment = VulkanTempBlockAllocator::computeDefaultBlockAlignment(*m_device);
        m_allocator = new VulkanTempBlockAllocator(
            *m_memoryManager,
            kTestBlockSize,
            m_alignment,
            VulkanTempBlockAllocator::computeDefaultBufferUsage(*m_device)
        );
    }

    virtual void TearDown() override
    {
        delete m_allocator;
        m_allocator = nullptr;
        VulkanResourceLiveTest::TearDown();
    }

    gp::UInt32 m_alignment{ 0u };
    VulkanTempBlockAllocator* m_allocator{ nullptr };
};

TEST(VulkanTempBlockAllocatorTest, ConfiguredBlockSizeDefaultsToFourMegabytes)
{
    EXPECT_EQ(VulkanTempBlockAllocator::kDefaultBlockSize, 4u * 1024u * 1024u);
    EXPECT_EQ(VulkanTempBlockAllocator::getConfiguredBlockSize(), VulkanTempBlockAllocator::kDefaultBlockSize);
}

TEST_F(VulkanTempBlockAllocatorLiveTest, DefaultsCoverUniformBufferRequirements)
{
    const gp::UInt32 alignment = VulkanTempBlockAllocator::computeDefaultBlockAlignment(*m_device);
    EXPECT_GE(alignment, 16u);
    EXPECT_GE(static_cast<VkDeviceSize>(alignment), m_device->getLimits().minUniformBufferOffsetAlignment);
    EXPECT_EQ(alignment & (alignment - 1u), 0u);
    EXPECT_GE(alignment, getVulkanUniformBufferAlignment(*m_device));

    const VkBufferUsageFlags usage = VulkanTempBlockAllocator::computeDefaultBufferUsage(*m_device);
    EXPECT_NE(usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0u);
    EXPECT_NE(usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0u);
    const bool hasAddress = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0u;
    EXPECT_EQ(hasAddress, m_device->getOptionalExtensions().hasBufferDeviceAddress != 0u);
}

TEST_F(VulkanTempBlockAllocatorLiveTest, StartsWithOneMappedBlock)
{
    ASSERT_TRUE(m_allocator->isValid());
    EXPECT_EQ(m_allocator->getBlockSize(), kTestBlockSize);
    EXPECT_EQ(m_allocator->getBlockAlignment(), m_alignment);
    EXPECT_EQ(m_allocator->getBufferUsage(), VulkanTempBlockAllocator::computeDefaultBufferUsage(*m_device));
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 0u);
    EXPECT_EQ(m_allocator->getAvailableBlockCount(), 0u);
    EXPECT_EQ(m_allocator->getCurrentBlockReferences(), 0u);
}

TEST_F(VulkanTempBlockAllocatorLiveTest, ReferencedAllocationsAreAlignedAndMapped)
{
    VulkanAllocation first{};
    VulkanAllocation second{};
    gp::UInt8* const firstPtr = m_allocator->allocate(100u, m_alignment, nullptr, first);
    gp::UInt8* const secondPtr = m_allocator->allocate(100u, m_alignment, nullptr, second);
    ASSERT_NE(firstPtr, nullptr);
    ASSERT_NE(secondPtr, nullptr);

    // Allocations without a context sync point are reference counted.
    EXPECT_EQ(first.isTempBlockAllocator, 1u);
    EXPECT_EQ(m_allocator->getCurrentBlockReferences(), 2u);
    EXPECT_EQ(first.size, 100u);
    EXPECT_NE(first.getBufferHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(first.getBufferHandle(), second.getBufferHandle());
    EXPECT_EQ(first.offset % m_alignment, 0u);
    EXPECT_EQ(second.offset - first.offset, alignTo(100u, m_alignment));
    EXPECT_EQ(static_cast<gp::USize>(secondPtr - firstPtr), static_cast<gp::USize>(alignTo(100u, m_alignment)));

    // The returned pointer is the mapped view of the allocation.
    EXPECT_EQ(static_cast<void*>(firstPtr), first.getMappedPointer(*m_memoryManager));
    firstPtr[0] = 0xABu;
    firstPtr[99] = 0xCDu;
    EXPECT_EQ(static_cast<const gp::UInt8*>(first.getMappedPointer(*m_memoryManager))[99], 0xCDu);

    m_allocator->freeReference(first);
    m_allocator->freeReference(second);
    EXPECT_EQ(m_allocator->getCurrentBlockReferences(), 0u);
}

TEST_F(VulkanTempBlockAllocatorLiveTest, FullBlocksAreRecycledOnceUnreferenced)
{
    const gp::UInt32 largeSize = (kTestBlockSize / 2u) + 1024u;

    VulkanAllocation first{};
    ASSERT_NE(m_allocator->allocate(largeSize, m_alignment, nullptr, first), nullptr);
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 0u);

    // The second request does not fit, so the current block retires and a new one starts.
    VulkanAllocation second{};
    ASSERT_NE(m_allocator->allocate(largeSize, m_alignment, nullptr, second), nullptr);
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 1u);
    EXPECT_NE(first.getBufferHandle(), second.getBufferHandle());
    EXPECT_EQ(second.offset % m_alignment, 0u);
    EXPECT_EQ(m_allocator->getCurrentBlockReferences(), 1u);

    // The retired block is still referenced.
    m_allocator->updateBlocks();
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 1u);
    EXPECT_EQ(m_allocator->getAvailableBlockCount(), 0u);

    m_allocator->freeReference(first);
    m_allocator->updateBlocks();
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 0u);
    EXPECT_EQ(m_allocator->getAvailableBlockCount(), 1u);

    // Retiring the current block reuses the recycled one instead of creating another.
    VulkanAllocation third{};
    ASSERT_NE(m_allocator->allocate(largeSize, m_alignment, nullptr, third), nullptr);
    EXPECT_EQ(third.getBufferHandle(), first.getBufferHandle());
    EXPECT_EQ(m_allocator->getAvailableBlockCount(), 0u);
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 1u);

    m_allocator->freeReference(second);
    m_allocator->freeReference(third);
}

TEST_F(VulkanTempBlockAllocatorLiveTest, SyncPointAllocationsWaitForTheirContext)
{
    const VulkanSyncPointRef contextSyncPoint =
        VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "TempBlockContext");
    const gp::UInt32 largeSize = (kTestBlockSize / 2u) + 1024u;

    VulkanAllocation first{};
    ASSERT_NE(m_allocator->allocate(largeSize, m_alignment, &contextSyncPoint, first), nullptr);
    EXPECT_EQ(first.isTempBlockAllocator, 0u);
    EXPECT_EQ(m_allocator->getCurrentBlockReferences(), 0u);

    VulkanAllocation second{};
    ASSERT_NE(m_allocator->allocate(largeSize, m_alignment, &contextSyncPoint, second), nullptr);
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 1u);

    // The retired block stays busy until the context's work completes.
    m_allocator->updateBlocks();
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 1u);

    contextSyncPoint->signal();
    m_allocator->updateBlocks();
    EXPECT_EQ(m_allocator->getBusyBlockCount(), 0u);
    EXPECT_EQ(m_allocator->getAvailableBlockCount(), 1u);
}

TEST_F(VulkanTempBlockAllocatorLiveTest, DescriptorAndShaderTableOverloadsDescribeTheRange)
{
    const VulkanSyncPointRef contextSyncPoint =
        VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "TempBlockDescriptors");

    VulkanAllocation allocation{};
    VkDescriptorAddressInfoEXT addressInfo{};
    ASSERT_NE(m_allocator->allocate(64u, m_alignment, nullptr, allocation, &addressInfo), nullptr);
    EXPECT_EQ(addressInfo.sType, VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT);
    EXPECT_EQ(addressInfo.range, 64u);

    VkDescriptorBufferBindingInfoEXT bindingInfo{};
    VkDeviceSize bindingOffset = 0u;
    ASSERT_NE(m_allocator->allocate(32u, contextSyncPoint, bindingInfo, bindingOffset), nullptr);
    EXPECT_EQ(bindingInfo.sType, VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT);
    EXPECT_EQ(bindingInfo.usage, m_allocator->getBufferUsage());
    EXPECT_EQ(bindingOffset, static_cast<VkDeviceSize>(allocation.offset) + alignTo(64u, m_alignment));

    VkStridedDeviceAddressRegionKHR region{};
    ASSERT_NE(m_allocator->allocate(48u, contextSyncPoint, region), nullptr);
    EXPECT_EQ(region.size, 48u);
    EXPECT_EQ(region.stride, region.size);

    if (m_device->getOptionalExtensions().hasBufferDeviceAddress != 0u)
    {
        EXPECT_NE(addressInfo.address, 0u);
        EXPECT_EQ(addressInfo.address - allocation.offset, bindingInfo.address);
    }

    m_allocator->freeReference(allocation);
    contextSyncPoint->signal();
}

}   // namespace gp::rhi::vulkan::tests
