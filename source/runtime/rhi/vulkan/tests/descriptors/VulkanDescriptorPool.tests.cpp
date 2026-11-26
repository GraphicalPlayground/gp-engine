// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorPool.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPoolSetContainer.hpp"
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "descriptors/VulkanDescriptorSetLayoutCache.hpp"
#include "descriptors/VulkanDescriptorSetsEnums.hpp"
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanTypedDescriptorPoolSet.hpp"
#include "device/VulkanDevice.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Header of a compute shader with one uniform buffer and one sampled image.
/// @return Shader header.
[[nodiscard]] VulkanShaderHeader makeComputeHeader()
{
    VulkanShaderHeader header{};
    header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) });
    header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) });
    header.numBoundUniformBuffers = 1u;
    return header;
}

/// @brief Header of a compute shader with one storage image.
/// @return Shader header.
[[nodiscard]] VulkanShaderHeader makeStorageHeader()
{
    VulkanShaderHeader header{};
    header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) });
    return header;
}

/// @brief Finalizes a compute layout from one header.
/// @param[in,out] layout Layout to finalize.
/// @param[in] header Compute header.
/// @param[in] limits Device limits.
/// @return Result of `finalizeBindings()`.
bool finalizeCompute(
    VulkanDescriptorSetsLayout& layout, const VulkanShaderHeader& header, const VkPhysicalDeviceLimits& limits
)
{
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[0] = &header;
    return layout.finalizeBindings(
        VK_PIPELINE_BIND_POINT_COMPUTE, limits, gatherInfo, false, VulkanDynamicUniformBufferMode::None
    );
}

/// @brief Offline fixture: a device without a native handle and a finalized, uncompiled layout.
class VulkanDescriptorPoolOfflineTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(finalizeCompute(m_layout, makeComputeHeader(), m_limits));
    }

    /// @brief Device without native handles.
    VulkanDevice m_device{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Zero limits; only `finalizeBindings()` reads them, for dynamic promotion.
    VkPhysicalDeviceLimits m_limits{};

    /// @brief Layout with one uniform buffer and one sampled image.
    VulkanDescriptorSetsLayout m_layout{ m_device };
};

/// @brief Live fixture allocating descriptor sets on a real device.
class VulkanDescriptorPoolLiveTest : public VulkanResourceLiveTest
{
protected:
    /// @brief Finalizes and compiles a compute layout.
    /// @param[in,out] layout Layout to build.
    /// @param[in] header Compute header.
    /// @param[in,out] cache Set layout cache.
    /// @return True on success.
    bool buildLayout(
        VulkanDescriptorSetsLayout& layout, const VulkanShaderHeader& header, VulkanDescriptorSetLayoutCache& cache
    )
    {
        return finalizeCompute(layout, header, m_device->getLimits()) && layout.compile(cache);
    }
};

}   // namespace

static_assert(VulkanTypedDescriptorPoolSet::computeMaxSetsAllocations(0u) == 32u);
static_assert(VulkanTypedDescriptorPoolSet::computeMaxSetsAllocations(1u) == 64u);
static_assert(VulkanTypedDescriptorPoolSet::computeMaxSetsAllocations(2u) == 128u);
static_assert(VulkanTypedDescriptorPoolSet::computeMaxSetsAllocations(9u) == 128u);

TEST(VulkanDescriptorPoolSizesTest, OnlyUsedTypesGetAPoolSize)
{
    VulkanDescriptorTypeCounts counts{};
    counts[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_SAMPLER)] = 2u;
    counts[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)] = 3u;
    counts[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)] = 1u;
    gp::Array<VkDescriptorPoolSize, kVulkanNumDescriptorTypeSlots> sizes{};

    const gp::UInt32 numSizes = VulkanDescriptorPool::computePoolSizes(counts, 10u, sizes);

    ASSERT_EQ(numSizes, 3u);
    EXPECT_EQ(sizes[0].type, VK_DESCRIPTOR_TYPE_SAMPLER);
    EXPECT_EQ(sizes[0].descriptorCount, 20u);
    EXPECT_EQ(sizes[1].type, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    EXPECT_EQ(sizes[1].descriptorCount, 30u);
    EXPECT_EQ(sizes[2].type, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    EXPECT_EQ(sizes[2].descriptorCount, 10u);
}

TEST(VulkanDescriptorPoolSizesTest, EmptyCountsGiveNoPoolSizes)
{
    const VulkanDescriptorTypeCounts counts{};
    gp::Array<VkDescriptorPoolSize, kVulkanNumDescriptorTypeSlots> sizes{};

    EXPECT_EQ(VulkanDescriptorPool::computePoolSizes(counts, 32u, sizes), 0u);
}

TEST_F(VulkanDescriptorPoolOfflineTest, PoolIsInvalidButTracksUsage)
{
    VulkanDescriptorPool pool(m_device, m_layout, 4u);

    EXPECT_FALSE(pool.isValid());
#if GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID
    EXPECT_EQ(pool.getMaxDescriptorSets(), 4u);
#endif
    EXPECT_TRUE(pool.isEmpty());
    EXPECT_TRUE(pool.canAllocate(m_layout));

    ASSERT_TRUE(pool.trackAddUsage(m_layout));
    ASSERT_TRUE(pool.trackAddUsage(m_layout));
    EXPECT_EQ(pool.getNumAllocatedDescriptorSets(), 2u);
    EXPECT_EQ(pool.getPeakAllocatedDescriptorSets(), 2u);

    ASSERT_TRUE(pool.trackRemoveUsage(m_layout));
    EXPECT_EQ(pool.getNumAllocatedDescriptorSets(), 1u);
    EXPECT_EQ(pool.getPeakAllocatedDescriptorSets(), 2u);
    EXPECT_FALSE(pool.isEmpty());
}

TEST_F(VulkanDescriptorPoolOfflineTest, PoolStopsAcceptingAtMaxSets)
{
    VulkanDescriptorPool pool(m_device, m_layout, 2u);

    ASSERT_TRUE(pool.trackAddUsage(m_layout));
    ASSERT_TRUE(pool.trackAddUsage(m_layout));

    EXPECT_FALSE(pool.canAllocate(m_layout));
}

TEST_F(VulkanDescriptorPoolOfflineTest, PoolRejectsALayoutOfAnotherShape)
{
    VulkanDescriptorPool pool(m_device, m_layout, 4u);
    VulkanDescriptorSetsLayout other(m_device);
    ASSERT_TRUE(finalizeCompute(other, makeStorageHeader(), m_limits));

    EXPECT_FALSE(pool.trackAddUsage(other));
    EXPECT_FALSE(pool.trackRemoveUsage(other));
    EXPECT_TRUE(pool.isEmpty());
}

TEST_F(VulkanDescriptorPoolOfflineTest, AllocationFailsWithoutANativePool)
{
    VulkanDescriptorPool pool(m_device, m_layout, 4u);
    VkDescriptorSetAllocateInfo allocateInfo{};
    VkDescriptorSet set = VK_NULL_HANDLE;

    EXPECT_FALSE(pool.allocateDescriptorSets(allocateInfo, &set));
    EXPECT_EQ(set, VK_NULL_HANDLE);
}

TEST_F(VulkanDescriptorPoolOfflineTest, TypedPoolSetStartsWithOnePool)
{
    VulkanTypedDescriptorPoolSet poolSet(m_device, m_layout);

    ASSERT_EQ(poolSet.getNumPools(), 1u);
    EXPECT_EQ(poolSet.getCurrentPoolIndex(), 0u);
#if GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID
    EXPECT_EQ(poolSet.getPool(0u).getMaxDescriptorSets(), 32u);
#endif

    // An uncompiled layout has no set handles: there is nothing to allocate, which trivially succeeds.
    VkDescriptorSet set = VK_NULL_HANDLE;
    EXPECT_TRUE(poolSet.allocateDescriptorSets(m_layout, &set));
}

TEST_F(VulkanDescriptorPoolOfflineTest, ContainerKeysTypedPoolSetsByLayoutShape)
{
    VulkanDescriptorSetsLayout sameShape(m_device);
    ASSERT_TRUE(finalizeCompute(sameShape, makeComputeHeader(), m_limits));
    VulkanDescriptorSetsLayout otherShape(m_device);
    ASSERT_TRUE(finalizeCompute(otherShape, makeStorageHeader(), m_limits));
    VulkanDescriptorPoolSetContainer container(m_device, 5u);

    VulkanTypedDescriptorPoolSet& first = container.acquireTypedPoolSet(m_layout);
    VulkanTypedDescriptorPoolSet& again = container.acquireTypedPoolSet(sameShape);
    VulkanTypedDescriptorPoolSet& other = container.acquireTypedPoolSet(otherShape);

    EXPECT_EQ(&first, &again);
    EXPECT_NE(&first, &other);
    EXPECT_EQ(container.getNumTypedPoolSets(), 2u);
    EXPECT_EQ(
        VulkanDescriptorPoolSetContainer::computePoolKey(m_layout),
        VulkanDescriptorPoolSetContainer::computePoolKey(sameShape)
    );
}

TEST_F(VulkanDescriptorPoolOfflineTest, ContainerTracksUseAndLastFrame)
{
    VulkanDescriptorPoolSetContainer container(m_device, 7u);

    EXPECT_FALSE(container.isUnused());
    EXPECT_EQ(container.getLastFrameUsed(), 7u);

    container.setUsed(false, 9u);
    EXPECT_TRUE(container.isUnused());
    EXPECT_EQ(container.getLastFrameUsed(), 7u);

    container.setUsed(true, 12u);
    EXPECT_FALSE(container.isUnused());
    EXPECT_EQ(container.getLastFrameUsed(), 12u);
}

TEST_F(VulkanDescriptorPoolOfflineTest, ManagerRecyclesReleasedContainers)
{
    VulkanDescriptorPoolsManager manager;
    manager.init(m_device);

    VulkanDescriptorPoolSetContainer& first = manager.acquirePoolSetContainer(1u);
    VulkanDescriptorPoolSetContainer& second = manager.acquirePoolSetContainer(1u);
    EXPECT_NE(&first, &second);
    EXPECT_EQ(manager.getNumPoolSetContainers(), 2u);

    static_cast<void>(first.acquireTypedPoolSet(m_layout));
    manager.releasePoolSet(first);
    EXPECT_TRUE(first.isUnused());

    VulkanDescriptorPoolSetContainer& recycled = manager.acquirePoolSetContainer(2u);
    EXPECT_EQ(&recycled, &first);
    EXPECT_FALSE(recycled.isUnused());
    EXPECT_EQ(recycled.getLastFrameUsed(), 2u);
    EXPECT_EQ(manager.getNumPoolSetContainers(), 2u);
}

TEST_F(VulkanDescriptorPoolOfflineTest, ManagerCollectsOnlyStaleUnusedContainers)
{
    VulkanDescriptorPoolsManager manager;
    manager.init(m_device);

    VulkanDescriptorPoolSetContainer& stale = manager.acquirePoolSetContainer(10u);
    VulkanDescriptorPoolSetContainer& busy = manager.acquirePoolSetContainer(10u);
    static_cast<void>(busy);
    manager.releasePoolSet(stale);

    // Released at frame 10: not collectable until more than 3 frames have passed.
    EXPECT_FALSE(manager.gc(13u, 3u));
    EXPECT_EQ(manager.getNumPoolSetContainers(), 2u);

    // The used container is never collected, whatever its age.
    EXPECT_TRUE(manager.gc(14u, 3u));
    EXPECT_EQ(manager.getNumPoolSetContainers(), 1u);
    EXPECT_FALSE(manager.gc(1000u, 3u));
    EXPECT_EQ(manager.getNumPoolSetContainers(), 1u);
}

TEST_F(VulkanDescriptorPoolOfflineTest, ManagerCollectsOneContainerPerCall)
{
    VulkanDescriptorPoolsManager manager;
    manager.init(m_device);
    VulkanDescriptorPoolSetContainer& first = manager.acquirePoolSetContainer(0u);
    VulkanDescriptorPoolSetContainer& second = manager.acquirePoolSetContainer(0u);
    manager.releasePoolSet(first);
    manager.releasePoolSet(second);

    EXPECT_TRUE(manager.gc(100u, 2u));
    EXPECT_EQ(manager.getNumPoolSetContainers(), 1u);
    EXPECT_TRUE(manager.gc(100u, 2u));
    EXPECT_EQ(manager.getNumPoolSetContainers(), 0u);
    EXPECT_FALSE(manager.gc(100u, 2u));
}

TEST_F(VulkanDescriptorPoolLiveTest, PoolAllocatesAndResets)
{
    VulkanDescriptorSetLayoutCache cache(*m_device);
    VulkanDescriptorSetsLayout layout(*m_device);
    ASSERT_TRUE(buildLayout(layout, makeComputeHeader(), cache));
    VulkanDescriptorPool pool(*m_device, layout, 4u);
    ASSERT_TRUE(pool.isValid());

    VkDescriptorSet set = VK_NULL_HANDLE;
    ASSERT_TRUE(pool.allocateDescriptorSets(layout.getAllocateInfo(), &set));
    EXPECT_NE(set, VK_NULL_HANDLE);
    ASSERT_TRUE(pool.trackAddUsage(layout));

    pool.reset();
    EXPECT_TRUE(pool.isEmpty());
    VkDescriptorSet again = VK_NULL_HANDLE;
    EXPECT_TRUE(pool.allocateDescriptorSets(layout.getAllocateInfo(), &again));
}

TEST_F(VulkanDescriptorPoolLiveTest, TypedPoolSetGrowsPastTheFirstPool)
{
    VulkanDescriptorSetLayoutCache cache(*m_device);
    VulkanDescriptorSetsLayout layout(*m_device);
    ASSERT_TRUE(buildLayout(layout, makeComputeHeader(), cache));
    VulkanTypedDescriptorPoolSet poolSet(*m_device, layout);

    const gp::UInt32 firstPoolCapacity = poolSet.getPool(0u).getMaxDescriptorSets();
    gp::Vector<VkDescriptorSet> sets{};
    for (gp::UInt32 i = 0u; i <= firstPoolCapacity; ++i)
    {
        VkDescriptorSet set = VK_NULL_HANDLE;
        ASSERT_TRUE(poolSet.allocateDescriptorSets(layout, &set)) << "Allocation " << i << " failed.";
        EXPECT_NE(set, VK_NULL_HANDLE);
        sets.pushBack(set);
    }

    EXPECT_EQ(poolSet.getNumPools(), 2u);
    EXPECT_EQ(poolSet.getCurrentPoolIndex(), 1u);
    EXPECT_EQ(poolSet.getPool(0u).getNumAllocatedDescriptorSets(), firstPoolCapacity);
    EXPECT_GT(poolSet.getPool(1u).getMaxDescriptorSets(), firstPoolCapacity);

    // A reset rewinds to the first pool and keeps the grown pools for the next frame.
    poolSet.reset();
    EXPECT_EQ(poolSet.getCurrentPoolIndex(), 0u);
    EXPECT_EQ(poolSet.getNumPools(), 2u);
    EXPECT_TRUE(poolSet.getPool(0u).isEmpty());
    VkDescriptorSet set = VK_NULL_HANDLE;
    EXPECT_TRUE(poolSet.allocateDescriptorSets(layout, &set));
    EXPECT_EQ(poolSet.getCurrentPoolIndex(), 0u);
}

TEST_F(VulkanDescriptorPoolLiveTest, ContainerAllocatesThroughItsTypedPoolSet)
{
    VulkanDescriptorSetLayoutCache cache(*m_device);
    VulkanDescriptorSetsLayout layout(*m_device);
    ASSERT_TRUE(buildLayout(layout, makeStorageHeader(), cache));
    VulkanDescriptorPoolsManager manager;
    manager.init(*m_device);

    VulkanDescriptorPoolSetContainer& container = manager.acquirePoolSetContainer(1u);
    VkDescriptorSet set = VK_NULL_HANDLE;
    ASSERT_TRUE(container.acquireTypedPoolSet(layout).allocateDescriptorSets(layout, &set));
    EXPECT_NE(set, VK_NULL_HANDLE);

    manager.releasePoolSet(container);
    EXPECT_TRUE(manager.gc(10u, 2u));
    EXPECT_EQ(manager.getNumPoolSetContainers(), 0u);
}

}   // namespace gp::rhi::vulkan::tests
