// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanGenericDescriptorPool.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "device/VulkanDevice.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Live fixture creating generic pools on a real device.
class VulkanGenericDescriptorPoolLiveTest : public VulkanResourceLiveTest
{};

/// @brief Ratios of zero for every type, so only the floors apply.
/// @return Ratios.
[[nodiscard]] VulkanDescriptorPoolSizeRatios makeZeroRatios() noexcept
{
    VulkanDescriptorPoolSizeRatios ratios{};
    for (gp::Float32& ratio: ratios)
    {
        ratio = 0.0f;
    }
    return ratios;
}

}   // namespace

TEST(VulkanGenericDescriptorPoolTest, DefaultRatiosSizeEveryCoreType)
{
    gp::Array<VkDescriptorPoolSize, kVulkanNumCoreDescriptorTypes> sizes{};

    VulkanGenericDescriptorPool::computePoolSizes(64u, kVulkanDefaultDescriptorPoolSizeRatios, sizes);

    for (gp::USize slot = 0u; slot < kVulkanNumCoreDescriptorTypes; ++slot)
    {
        EXPECT_EQ(sizes[slot].type, getVulkanDescriptorTypeForSlot(slot));
    }
    EXPECT_EQ(sizes[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_SAMPLER)].descriptorCount, 128u);
    EXPECT_EQ(sizes[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)].descriptorCount, 256u);
    EXPECT_EQ(sizes[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)].descriptorCount, 16u);
    EXPECT_EQ(sizes[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)].descriptorCount, 8u);
}

TEST(VulkanGenericDescriptorPoolTest, DefaultRatiosAreAFloorForMeasuredRatios)
{
    gp::Array<VkDescriptorPoolSize, kVulkanNumCoreDescriptorTypes> sizes{};

    VulkanGenericDescriptorPool::computePoolSizes(64u, makeZeroRatios(), sizes);

    EXPECT_EQ(sizes[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_SAMPLER)].descriptorCount, 128u);
    EXPECT_EQ(sizes[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)].descriptorCount, 8u);
}

TEST(VulkanGenericDescriptorPoolTest, SmallPoolsKeepAMinimumPerType)
{
    gp::Array<VkDescriptorPoolSize, kVulkanNumCoreDescriptorTypes> sizes{};

    VulkanGenericDescriptorPool::computePoolSizes(1u, makeZeroRatios(), sizes);

    for (gp::USize slot = 0u; slot < kVulkanNumCoreDescriptorTypes; ++slot)
    {
        EXPECT_GE(sizes[slot].descriptorCount, static_cast<gp::UInt32>(kVulkanMinGenericPoolDescriptorsPerType));
    }
}

TEST(VulkanGenericDescriptorPoolTest, MeasuredRatiosAboveTheDefaultsWin)
{
    VulkanDescriptorPoolSizeRatios ratios = kVulkanDefaultDescriptorPoolSizeRatios;
    ratios[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)] = 3.0f;
    gp::Array<VkDescriptorPoolSize, kVulkanNumCoreDescriptorTypes> sizes{};

    VulkanGenericDescriptorPool::computePoolSizes(10u, ratios, sizes);

    EXPECT_EQ(sizes[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)].descriptorCount, 30u);
}

TEST(VulkanGenericDescriptorPoolTest, OfflinePoolKeepsSizesButHasNoHandle)
{
    VulkanDevice device{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };
    VulkanGenericDescriptorPool pool(device, 64u, kVulkanDefaultDescriptorPoolSizeRatios);

    EXPECT_EQ(pool.getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(pool.getMaxDescriptorSets(), 64u);
    EXPECT_EQ(pool.getPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER), 128u);
    EXPECT_EQ(pool.getPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR), 0u);

    VkDescriptorSet set = VK_NULL_HANDLE;
    EXPECT_FALSE(pool.allocateDescriptorSet(VK_NULL_HANDLE, set));
    pool.reset();
}

TEST_F(VulkanGenericDescriptorPoolLiveTest, LivePoolIsCreatedAndResets)
{
    VulkanGenericDescriptorPool pool(*m_device, 16u, kVulkanDefaultDescriptorPoolSizeRatios);

    EXPECT_NE(pool.getHandle(), VK_NULL_HANDLE);
    pool.reset();
    EXPECT_NE(pool.getHandle(), VK_NULL_HANDLE);
}

}   // namespace gp::rhi::vulkan::tests
