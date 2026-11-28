// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetCache.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetKey.hpp"
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanDescriptorSettings.hpp"
#include "descriptors/VulkanDescriptorSetWriteContainer.hpp"
#include "descriptors/VulkanDescriptorSetWriter.hpp"
#include "descriptors/VulkanGenericDescriptorPool.hpp"
#include "device/VulkanDevice.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/SamplerState.hpp"           // IWYU pragma: keep
#include "states/VulkanSamplerState.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstdint>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Offline fixture: a device without native handles and one writer over a single sampler binding.
/// @details No native pool exists offline, so every set that holds a write fails to allocate; sets without writes
///          need no allocation and always succeed. Both paths are what the cache logic branches on.
class VulkanDescriptorSetCacheTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        VkSamplerCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        m_sampler = pipeline::SamplerStateRef(new VulkanSamplerState(m_device, createInfo, nullptr));
        m_defaultImageView.viewId = 1u;

        m_types.pushBack(VK_DESCRIPTOR_TYPE_SAMPLER);
        m_container.descriptorWrites.resize(1u, VkWriteDescriptorSet{});
        m_container.descriptorImageInfo.resize(1u, VkDescriptorImageInfo{});
        m_container.descriptorBufferInfo.resize(1u, VkDescriptorBufferInfo{});
        m_container.bindingToDynamicOffsetMap.resize(1u, gp::UInt8{ 0u });
        m_container.accelerationStructures.resize(1u, VkAccelerationStructureKHR{ VK_NULL_HANDLE });
        m_container.accelerationStructureWrites.resize(1u, VkWriteDescriptorSetAccelerationStructureKHR{});
        m_container.hashableDescriptorInfo.resize(2u, VulkanHashableDescriptorInfo{});
        static_cast<void>(m_samplerWriter.setupDescriptorWrites(
            gp::ConstVectorView<VkDescriptorType>(m_types),
            m_container.hashableDescriptorInfo.data(),
            m_container.descriptorWrites.data(),
            m_container.descriptorImageInfo.data(),
            m_container.descriptorBufferInfo.data(),
            m_container.bindingToDynamicOffsetMap.data(),
            m_container.accelerationStructureWrites.data(),
            m_container.accelerationStructures.data(),
            getSampler(),
            m_defaultImageView
        ));
    }

    virtual void TearDown() override
    {
        m_sampler = pipeline::SamplerStateRef();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }

    /// @brief Returns the sampler bound by the writer.
    /// @return Sampler state.
    [[nodiscard]] const VulkanSamplerState& getSampler() const noexcept
    {
        return *static_cast<const VulkanSamplerState*>(m_sampler.get());
    }

    /// @brief Builds a key from one integer.
    /// @param[in] value Key content.
    /// @return Key.
    [[nodiscard]] static VulkanDescriptorSetKey makeKey(const gp::UInt32 value) noexcept
    {
        VulkanDescriptorSetKey key;
        key.generateFromData(&value, sizeof(value));
        return key;
    }

    /// @brief Device without native handles.
    VulkanDevice m_device{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Unfinalized layout; the cache only reads its handles and type counts.
    VulkanDescriptorSetsLayout m_layout{ m_device };

    /// @brief Sampler bound by `m_samplerWriter`.
    pipeline::SamplerStateRef m_sampler{};

    /// @brief Default image view (unused by a sampler binding).
    VulkanTextureView m_defaultImageView{};

    /// @brief Binding types of `m_samplerWriter`.
    gp::Vector<VkDescriptorType> m_types{};

    /// @brief Storage behind `m_samplerWriter`.
    VulkanDescriptorSetWriteContainer m_container{};

    /// @brief Writer with one write; it cannot be allocated offline.
    VulkanDescriptorSetWriter m_samplerWriter{};

    /// @brief Writer without writes; its set is always null.
    VulkanDescriptorSetWriter m_emptyWriter{};
};

}   // namespace

TEST_F(VulkanDescriptorSetCacheTest, PoolCapacityScalesWithTheAllocRatio)
{
    const VulkanCachedDescriptorPool pool(m_device, 32u, kVulkanDefaultDescriptorPoolSizeRatios);

    EXPECT_EQ(pool.getMaxDescriptorSets(), 32u);
    EXPECT_EQ(pool.getSetCapacity(), static_cast<gp::UInt32>(32.0f * VulkanCachedDescriptorPool::kMaxAllocRatio));
    EXPECT_EQ(pool.getNumSets(), 0u);
    EXPECT_EQ(pool.getNumSetsEntries(), 0u);
}

TEST_F(VulkanDescriptorSetCacheTest, EmptyPoolMissesAndClampsItsAllocRatio)
{
    VulkanCachedDescriptorPool pool(m_device, 32u, kVulkanDefaultDescriptorPoolSizeRatios);
    VulkanCachedDescriptorSets sets{};

    EXPECT_FALSE(pool.findDescriptorSets(makeKey(1u), 5u, sets));
    EXPECT_FLOAT_EQ(pool.calcAllocRatio(), VulkanCachedDescriptorPool::kMinAllocRatio);

    VulkanDescriptorPoolSizeRatios ratios = kVulkanDefaultDescriptorPoolSizeRatios;
    pool.calcPoolSizesRatio(ratios);
    for (const gp::Float32 ratio: ratios)
    {
        EXPECT_FLOAT_EQ(ratio, 0.0f);
    }
}

TEST_F(VulkanDescriptorSetCacheTest, SetsWithoutWritesAreCachedAsNullSets)
{
    VulkanCachedDescriptorPool pool(m_device, 32u, kVulkanDefaultDescriptorPoolSizeRatios);
    VulkanCachedDescriptorSets created{};

    ASSERT_TRUE(pool.createDescriptorSets(makeKey(7u), m_layout, &m_emptyWriter, 1u, 3u, created));
    EXPECT_EQ(created.numSets, 1u);
    EXPECT_EQ(created.sets[0], VK_NULL_HANDLE);
    EXPECT_EQ(pool.getNumSets(), 0u);
    EXPECT_EQ(pool.getNumSetsEntries(), 1u);
    EXPECT_EQ(pool.getRecentFrame(), 3u);

    VulkanCachedDescriptorSets found{};
    ASSERT_TRUE(pool.findDescriptorSets(makeKey(7u), 9u, found));
    EXPECT_EQ(found.numSets, 1u);
    EXPECT_EQ(pool.getRecentFrame(), 9u);
    EXPECT_FALSE(pool.findDescriptorSets(makeKey(8u), 10u, found));
    EXPECT_EQ(pool.getRecentFrame(), 9u);
}

TEST_F(VulkanDescriptorSetCacheTest, SetWithWritesFailsWithoutANativePool)
{
    VulkanCachedDescriptorPool pool(m_device, 32u, kVulkanDefaultDescriptorPoolSizeRatios);
    VulkanCachedDescriptorSets sets{};

    EXPECT_FALSE(pool.createDescriptorSets(makeKey(1u), m_layout, &m_samplerWriter, 1u, 1u, sets));
    EXPECT_EQ(pool.getNumSets(), 0u);
    EXPECT_EQ(pool.getNumSetsEntries(), 0u);
}

TEST_F(VulkanDescriptorSetCacheTest, ResetForgetsBothTables)
{
    VulkanCachedDescriptorPool pool(m_device, 32u, kVulkanDefaultDescriptorPoolSizeRatios);
    VulkanCachedDescriptorSets sets{};
    ASSERT_TRUE(pool.createDescriptorSets(makeKey(7u), m_layout, &m_emptyWriter, 1u, 3u, sets));

    pool.reset();

    EXPECT_EQ(pool.getNumSetsEntries(), 0u);
    EXPECT_FALSE(pool.findDescriptorSets(makeKey(7u), 4u, sets));
}

TEST_F(VulkanDescriptorSetCacheTest, PoolIsCollectableOnlyAfterTheWaitFrames)
{
    VulkanCachedDescriptorPool pool(m_device, 32u, kVulkanDefaultDescriptorPoolSizeRatios);
    VulkanCachedDescriptorSets sets{};
    ASSERT_TRUE(pool.createDescriptorSets(makeKey(7u), m_layout, &m_emptyWriter, 1u, 10u, sets));

    EXPECT_FALSE(pool.canGC(10u, 2u));
    EXPECT_FALSE(pool.canGC(12u, 2u));
    EXPECT_TRUE(pool.canGC(13u, 2u));
    EXPECT_FALSE(pool.canGC(5u, 2u));
}

TEST_F(VulkanDescriptorSetCacheTest, FreshCacheStartsWithTheProbePool)
{
    const VulkanDescriptorSetCache cache(m_device);

    ASSERT_EQ(cache.getNumCachedPools(), 1u);
    EXPECT_EQ(cache.getCachedPool(0u).getMaxDescriptorSets(), kVulkanDescriptorSetCacheProbePoolSize);
    EXPECT_FALSE(cache.hasFreePool());
    EXPECT_FLOAT_EQ(cache.getPoolAllocRatio(), 0.0f);
}

TEST_F(VulkanDescriptorSetCacheTest, RepeatedBindingIsServedFromTheTable)
{
    VulkanDescriptorSetCache cache(m_device);
    // A non-null start value proves the lookup overwrites the output with the cached null set.
    VkDescriptorSet sets[1] = { reinterpret_cast<VkDescriptorSet>(std::uintptr_t{ 0x5E7u }) };

    ASSERT_TRUE(cache.getDescriptorSets(makeKey(3u), m_layout, &m_emptyWriter, 1u, 1u, sets));
    EXPECT_EQ(sets[0], VK_NULL_HANDLE);
    ASSERT_TRUE(cache.getDescriptorSets(makeKey(3u), m_layout, &m_emptyWriter, 1u, 2u, sets));

    EXPECT_EQ(cache.getNumCachedPools(), 1u);
    EXPECT_EQ(cache.getCachedPool(0u).getNumSetsEntries(), 1u);
    EXPECT_EQ(cache.getCachedPool(0u).getRecentFrame(), 2u);
}

TEST_F(VulkanDescriptorSetCacheTest, ExhaustedPoolGrowsByOneSizedFromTheMeasuredRatio)
{
    VulkanDescriptorSetCache cache(m_device);
    VkDescriptorSet sets[1] = { VK_NULL_HANDLE };

    // Offline, the probe pool and its replacement both fail: exactly one new pool per failed lookup.
    EXPECT_FALSE(cache.getDescriptorSets(makeKey(1u), m_layout, &m_samplerWriter, 1u, 1u, sets));

    ASSERT_EQ(cache.getNumCachedPools(), 2u);
    EXPECT_FLOAT_EQ(cache.getPoolAllocRatio(), VulkanCachedDescriptorPool::kMinAllocRatio);
    const gp::UInt32 expectedSets = static_cast<gp::UInt32>(
        (static_cast<gp::Float32>(getVulkanDescriptorSetCacheTargetSetsPerPool()) /
         VulkanCachedDescriptorPool::kMinAllocRatio) +
        0.5f
    );
    EXPECT_EQ(cache.getCachedPool(0u).getMaxDescriptorSets(), expectedSets);
    EXPECT_EQ(cache.getCachedPool(1u).getMaxDescriptorSets(), kVulkanDescriptorSetCacheProbePoolSize);
}

TEST_F(VulkanDescriptorSetCacheTest, GcKeepsTheLookupPoolsAndRecyclesTheOldest)
{
    VulkanDescriptorSetCache cache(m_device);
    VkDescriptorSet sets[1] = { VK_NULL_HANDLE };
    const gp::USize maxLookups = static_cast<gp::USize>(getVulkanDescriptorSetCacheMaxPoolLookups());
    while (cache.getNumCachedPools() <= maxLookups)
    {
        ASSERT_FALSE(cache.getDescriptorSets(makeKey(1u), m_layout, &m_samplerWriter, 1u, 1u, sets));
    }
    const gp::USize numPools = cache.getNumCachedPools();

    // Not old enough yet.
    cache.gc(1u);
    EXPECT_EQ(cache.getNumCachedPools(), numPools);
    EXPECT_FALSE(cache.hasFreePool());

    cache.gc(1000u);
    EXPECT_EQ(cache.getNumCachedPools(), maxLookups);
    EXPECT_TRUE(cache.hasFreePool());

    // The recycled pool is the small probe pool, far from the measured size: it is dropped, not reused.
    EXPECT_FALSE(cache.getDescriptorSets(makeKey(1u), m_layout, &m_samplerWriter, 1u, 1001u, sets));
    EXPECT_FALSE(cache.hasFreePool());
    EXPECT_EQ(cache.getNumCachedPools(), maxLookups + 1u);
}

}   // namespace gp::rhi::vulkan::tests
