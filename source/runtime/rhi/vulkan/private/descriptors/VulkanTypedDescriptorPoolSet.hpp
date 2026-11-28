// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPool.hpp"
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Growing list of descriptor pools that all have the shape of one layout.
/// @details Allocation always targets the *current* pool. When it is exhausted the set moves to the next pool, creating
///          one when it reaches the end. Pools grow geometrically, 32, then 64, then 128 allocations each, so a light
///          shape costs one small pool while a heavy one quickly reaches large pools. `reset()` resets every pool and
///          rewinds to the first, so a recycled set reuses the pools it already grew instead of creating new ones.
/// @note Not thread-safe: a pool set belongs to one `VulkanDescriptorPoolSetContainer`, which belongs to one context.
class GP_RHI_VULKAN_API VulkanTypedDescriptorPoolSet
{
public:
    /// @brief Base number of layout allocations of the first pool.
    static constexpr gp::UInt32 kMaxSetsAllocationsBase = 32u;

    /// @brief Maximum number of doublings applied to `kMaxSetsAllocationsBase`.
    static constexpr gp::UInt32 kMaxSetsAllocationsShift = 2u;

    /// @brief Creates the set and its first pool.
    /// @param[in] device Logical device.
    /// @param[in] layout Layout whose shape every pool takes; its counts are copied, so it may die before the set.
    VulkanTypedDescriptorPoolSet(VulkanDevice& device, const VulkanDescriptorSetsLayout& layout) noexcept;

    /// @brief Destroys every pool.
    ~VulkanTypedDescriptorPoolSet() noexcept = default;

    VulkanTypedDescriptorPoolSet(const VulkanTypedDescriptorPoolSet&) = delete;
    VulkanTypedDescriptorPoolSet& operator=(const VulkanTypedDescriptorPoolSet&) = delete;
    VulkanTypedDescriptorPoolSet(VulkanTypedDescriptorPoolSet&&) = delete;
    VulkanTypedDescriptorPoolSet& operator=(VulkanTypedDescriptorPoolSet&&) = delete;

    /// @brief Returns how many layout allocations the pool at `poolIndex` is sized for.
    /// @param[in] poolIndex Index of the pool in creation order.
    /// @return `32 << min(poolIndex, 2)`.
    [[nodiscard]] static constexpr gp::UInt32 computeMaxSetsAllocations(const gp::USize poolIndex) noexcept
    {
        const gp::USize shift = (poolIndex < kMaxSetsAllocationsShift) ? poolIndex : kMaxSetsAllocationsShift;
        return kMaxSetsAllocationsBase << shift;
    }

    /// @brief Allocates one descriptor set per set layout of `layout`.
    /// @param[in] layout Compiled layout with the shape of this set.
    /// @param[out] outSets Receives `layout.getHandles().size()` sets.
    /// @return True on success, or when the layout has no sets. False when even a fresh pool cannot allocate.
    [[nodiscard]] bool
        allocateDescriptorSets(const VulkanDescriptorSetsLayout& layout, VkDescriptorSet* outSets) noexcept;

    /// @brief Resets every pool and rewinds allocation to the first one.
    void reset() noexcept;

    /// @brief Returns the number of pools created so far.
    /// @return Pool count.
    [[nodiscard]] gp::USize getNumPools() const noexcept
    {
        return static_cast<gp::USize>(m_pools.size());
    }

    /// @brief Returns the pool at `index`.
    /// @param[in] index Pool index in creation order.
    /// @return Pool.
    [[nodiscard]] const VulkanDescriptorPool& getPool(const gp::USize index) const noexcept
    {
        return *m_pools[index];
    }

    /// @brief Returns the index of the pool allocations go to.
    /// @return Current pool index.
    [[nodiscard]] gp::USize getCurrentPoolIndex() const noexcept
    {
        return m_currentPool;
    }

private:
    /// @brief Appends a pool sized by `computeMaxSetsAllocations()`.
    /// @return New pool.
    VulkanDescriptorPool& pushNewPool() noexcept;

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Descriptor counts of one allocation, copied from the layout that created the set.
    VulkanDescriptorTypeCounts m_typeCounts{};

    /// @brief Number of sets one allocation creates.
    gp::UInt32 m_numSetLayouts{ 0u };

    /// @brief Pools in creation order.
    gp::Vector<gp::UniquePtr<VulkanDescriptorPool>> m_pools{};

    /// @brief Index of the pool allocations go to.
    gp::USize m_currentPool{ 0u };
};

}   // namespace gp::rhi::vulkan
