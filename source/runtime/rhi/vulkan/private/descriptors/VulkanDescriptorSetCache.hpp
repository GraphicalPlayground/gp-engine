// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetKey.hpp"
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanGenericDescriptorPool.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Number of sets of the probe pool the cache starts with, before it has measured any usage.
inline constexpr gp::UInt32 kVulkanDescriptorSetCacheProbePoolSize = 128u;

/// @brief Descriptor sets of one pipeline, as returned by one cache lookup.
struct VulkanCachedDescriptorSets
{
    /// @brief Set per set index; `VK_NULL_HANDLE` for a set with no binding.
    gp::Array<VkDescriptorSet, kVulkanMaxNumDescriptorStages> sets{};

    /// @brief Number of meaningful entries in `sets`.
    gp::UInt32 numSets{ 0u };
};

/// @brief One generic pool and the sets it holds, indexed by content.
/// @details Two lookup tables share the pool:
///          - The *sets* table maps the key of a whole pipeline binding (every set of every stage) to its sets, so a
///            repeated draw finds everything in one lookup.
///          - The *set* table maps the key of one set to its handle, so two pipelines that bind the same resources in
///            one stage share that set even when their other stages differ.
///          A set is written once, when it is created, and never rewritten: its content is its key.
/// @note Tables are linear scans over vectors, compared by hash first; a pool holds a few thousand sets at most.
class GP_RHI_VULKAN_API VulkanCachedDescriptorPool
{
public:
    /// @brief Ratio between the number of distinct sets and the pool capacity below which a pool counts as sparse.
    static constexpr gp::Float32 kMinAllocRatio = 0.5f;

    /// @brief Distinct sets a pool may index, as a multiple of its native capacity.
    static constexpr gp::Float32 kMaxAllocRatio = 16.0f;

    /// @brief Creates the pool.
    /// @param[in] device Logical device.
    /// @param[in] maxDescriptorSets Native set capacity.
    /// @param[in] poolSizeRatios Descriptors per set for each core type.
    VulkanCachedDescriptorPool(
        VulkanDevice& device, gp::UInt32 maxDescriptorSets, const VulkanDescriptorPoolSizeRatios& poolSizeRatios
    ) noexcept;

    /// @brief Destroys the pool.
    ~VulkanCachedDescriptorPool() noexcept = default;

    VulkanCachedDescriptorPool(const VulkanCachedDescriptorPool&) = delete;
    VulkanCachedDescriptorPool& operator=(const VulkanCachedDescriptorPool&) = delete;
    VulkanCachedDescriptorPool(VulkanCachedDescriptorPool&&) = delete;
    VulkanCachedDescriptorPool& operator=(VulkanCachedDescriptorPool&&) = delete;

    /// @brief Returns the native set capacity.
    /// @return Capacity.
    [[nodiscard]] gp::UInt32 getMaxDescriptorSets() const noexcept
    {
        return m_pool.getMaxDescriptorSets();
    }

    /// @brief Returns the number of distinct sets the pool may index.
    /// @return `maxDescriptorSets * kMaxAllocRatio`.
    [[nodiscard]] gp::UInt32 getSetCapacity() const noexcept
    {
        return m_setCapacity;
    }

    /// @brief Returns the number of distinct sets created from the pool.
    /// @return Set count.
    [[nodiscard]] gp::USize getNumSets() const noexcept
    {
        return static_cast<gp::USize>(m_setCache.size());
    }

    /// @brief Returns the number of pipeline bindings indexed by the pool.
    /// @return Entry count.
    [[nodiscard]] gp::USize getNumSetsEntries() const noexcept
    {
        return static_cast<gp::USize>(m_setsCache.size());
    }

    /// @brief Returns the frame the pool was last used in.
    /// @return Frame number.
    [[nodiscard]] gp::UInt64 getRecentFrame() const noexcept
    {
        return m_recentFrame;
    }

    /// @brief Returns the descriptors of one type used by every set created from the pool.
    /// @param[in] type Core descriptor type.
    /// @return Descriptor count, or 0 for a non-core type.
    [[nodiscard]] gp::UInt32 getPoolSizeStatistic(VkDescriptorType type) const noexcept;

    /// @brief Returns every set to the pool and forgets both tables.
    void reset() noexcept;

    /// @brief Reports whether the pool went unused long enough to be recycled.
    /// @param[in] currentFrame Current frame.
    /// @param[in] numFramesToWait Frames a set may still be in flight after its last use.
    /// @return True when more than `numFramesToWait` frames passed since the last use.
    [[nodiscard]] bool canGC(gp::UInt64 currentFrame, gp::UInt32 numFramesToWait) const noexcept;

    /// @brief Returns how full the pool got: distinct sets per unit of native capacity.
    /// @return Ratio, at least `kMinAllocRatio`.
    [[nodiscard]] gp::Float32 calcAllocRatio() const noexcept;

    /// @brief Returns the measured descriptors per set for each core type.
    /// @param[out] outPoolSizeRatios Receives the average descriptor count of each type over the pool's sets.
    void calcPoolSizesRatio(VulkanDescriptorPoolSizeRatios& outPoolSizeRatios) const noexcept;

    /// @brief Looks up the sets of a pipeline binding.
    /// @param[in] setsKey Key of the whole binding.
    /// @param[in] currentFrame Current frame; recorded as the last use on a hit.
    /// @param[out] outSets Receives the sets on a hit.
    /// @return True on a hit.
    [[nodiscard]] bool findDescriptorSets(
        const VulkanDescriptorSetKey& setsKey, gp::UInt64 currentFrame, VulkanCachedDescriptorSets& outSets
    ) noexcept;

    /// @brief Creates (or reuses per set) the sets of a pipeline binding and writes the new ones.
    /// @param[in] setsKey Key of the whole binding.
    /// @param[in] layout Set layouts of the pipeline.
    /// @param[in,out] writers One writer per set index; a writer with no write gets a null set.
    /// @param[in] numWriters Number of writers.
    /// @param[in] currentFrame Current frame; recorded as the last use.
    /// @param[out] outSets Receives the sets.
    /// @return False when the pool is exhausted (natively, or because it indexes `getSetCapacity()` sets).
    [[nodiscard]] bool createDescriptorSets(
        const VulkanDescriptorSetKey& setsKey,
        const VulkanDescriptorSetsLayout& layout,
        VulkanDescriptorSetWriter* writers,
        gp::UInt32 numWriters,
        gp::UInt64 currentFrame,
        VulkanCachedDescriptorSets& outSets
    ) noexcept;

private:
    /// @brief Sets of one pipeline binding.
    struct SetsEntry
    {
        /// @brief Key of the binding.
        VulkanDescriptorSetKey key{};

        /// @brief Sets of the binding.
        VulkanCachedDescriptorSets sets{};
    };

    /// @brief One set.
    struct SetEntry
    {
        /// @brief Key of the set.
        VulkanDescriptorSetKey key{};

        /// @brief Native set.
        VkDescriptorSet set{ VK_NULL_HANDLE };
    };

    /// @brief Native pool.
    VulkanGenericDescriptorPool m_pool;

    /// @brief Distinct sets the pool may index.
    gp::UInt32 m_setCapacity{ 0u };

    /// @brief Pipeline bindings.
    gp::Vector<SetsEntry> m_setsCache{};

    /// @brief Individual sets.
    gp::Vector<SetEntry> m_setCache{};

    /// @brief Frame of the last lookup hit or creation.
    gp::UInt64 m_recentFrame{ 0u };

    /// @brief Descriptors of each core type used by the sets of the pool.
    gp::Array<gp::UInt32, kVulkanNumCoreDescriptorTypes> m_poolSizesStatistic{};
};

/// @brief Content-addressed descriptor sets shared by every pipeline and every frame.
/// @details The newest pool sits at index 0 and receives every new set. Lookups search the first
///          `r.Vulkan.DSetCacheMaxPoolLookups` pools; a binding found only in an older pool is recreated in the newest
///          one, so pools age out naturally. When the newest pool is full, a new one is sized from what was measured:
///          `r.Vulkan.DSetCacheTargetSetsPerPool / allocRatio` sets, with descriptor ratios taken from its usage.
///
///          `gc` moves the oldest pool out once it went unused long enough. It is kept as the *free pool* and reused
///          (after a reset) when the next pool to create has a similar size, which avoids recreating native pools.
/// @note Not thread-safe: owned by the device and used from the thread that records descriptor updates.
class GP_RHI_VULKAN_API VulkanDescriptorSetCache
{
public:
    /// @brief Creates the cache with one probe pool of `kVulkanDescriptorSetCacheProbePoolSize` sets.
    /// @param[in] device Logical device.
    explicit VulkanDescriptorSetCache(VulkanDevice& device) noexcept;

    /// @brief Destroys every pool.
    ~VulkanDescriptorSetCache() noexcept = default;

    VulkanDescriptorSetCache(const VulkanDescriptorSetCache&) = delete;
    VulkanDescriptorSetCache& operator=(const VulkanDescriptorSetCache&) = delete;
    VulkanDescriptorSetCache(VulkanDescriptorSetCache&&) = delete;
    VulkanDescriptorSetCache& operator=(VulkanDescriptorSetCache&&) = delete;

    /// @brief Finds or creates the sets of a pipeline binding.
    /// @param[in] setsKey Key of the whole binding (every hashable descriptor info of every set).
    /// @param[in] layout Set layouts of the pipeline.
    /// @param[in,out] writers One writer per set index.
    /// @param[in] numWriters Number of writers; at most `kVulkanMaxNumDescriptorStages`.
    /// @param[in] currentFrame Current frame.
    /// @param[out] outSets Receives `numWriters` sets.
    /// @return False when even a freshly created pool could not hold the sets.
    [[nodiscard]] bool getDescriptorSets(
        const VulkanDescriptorSetKey& setsKey,
        const VulkanDescriptorSetsLayout& layout,
        VulkanDescriptorSetWriter* writers,
        gp::UInt32 numWriters,
        gp::UInt64 currentFrame,
        VkDescriptorSet* outSets
    ) noexcept;

    /// @brief Recycles the oldest pools that went unused long enough.
    /// @param[in] currentFrame Current frame.
    void gc(gp::UInt64 currentFrame) noexcept;

    /// @brief Returns the number of live pools.
    /// @return Pool count.
    [[nodiscard]] gp::USize getNumCachedPools() const noexcept
    {
        return static_cast<gp::USize>(m_cachedPools.size());
    }

    /// @brief Returns one live pool; index 0 is the newest.
    /// @param[in] index Pool index.
    /// @return Pool.
    [[nodiscard]] const VulkanCachedDescriptorPool& getCachedPool(const gp::USize index) const noexcept
    {
        return *m_cachedPools[index];
    }

    /// @brief Reports whether a recycled pool waits for reuse.
    /// @return True when a free pool is kept.
    [[nodiscard]] bool hasFreePool() const noexcept
    {
        return m_freePool.get() != nullptr;
    }

    /// @brief Returns the filtered pool fill ratio used to size new pools.
    /// @return Ratio; 0 until the first pool filled up.
    [[nodiscard]] gp::Float32 getPoolAllocRatio() const noexcept
    {
        return m_poolAllocRatio;
    }

private:
    /// @brief Folds the fill ratio of the newest pool into `m_poolAllocRatio` (weight 1/3 once a value exists).
    void updateAllocRatio() noexcept;

    /// @brief Puts a new (or the recycled free) pool at index 0.
    void addCachedPool() noexcept;

    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Live pools, newest first.
    gp::Vector<gp::UniquePtr<VulkanCachedDescriptorPool>> m_cachedPools{};

    /// @brief Recycled pool kept for reuse.
    gp::UniquePtr<VulkanCachedDescriptorPool> m_freePool{};

    /// @brief Filtered fill ratio.
    gp::Float32 m_poolAllocRatio{ 0.0f };
};

}   // namespace gp::rhi::vulkan
