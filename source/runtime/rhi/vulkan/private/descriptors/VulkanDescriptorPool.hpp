// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief One `VkDescriptorPool` sized for a fixed number of allocations of one descriptor layout shape.
/// @details The pool is created **without** `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`: sets are never freed
///          one by one, the whole pool is reset at once when its owner is recycled. That keeps the driver allocator a
///          bump allocator (cheaper allocations, cheaper `vkResetDescriptorPool`, smaller pool memory).
///
///          Pool sizes are `typeCount * maxSetsAllocations` for every descriptor type the layout uses. When pools are
///          keyed by types usage id (`GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID`), one pool serves every layout with
///          the same per-type counts, so `maxSets` cannot depend on the set count of one particular layout and is
///          `maxSetsAllocations`; the pool then runs out of sets before it runs out of descriptors, which is cheap.
/// @note The per-type counts are copied, so the pool does not depend on the lifetime of any layout object.
class GP_RHI_VULKAN_API VulkanDescriptorPool
{
public:
    /// @brief Creates a pool for the shape of `layout`.
    /// @param[in] device Logical device.
    /// @param[in] layout Compiled layout that defines the pool shape.
    /// @param[in] maxSetsAllocations Number of layout allocations the pool is sized for.
    VulkanDescriptorPool(
        VulkanDevice& device, const VulkanDescriptorSetsLayout& layout, gp::UInt32 maxSetsAllocations
    ) noexcept;

    /// @brief Creates a pool for an explicit shape.
    /// @param[in] device Logical device.
    /// @param[in] typeCounts Descriptor counts of one allocation.
    /// @param[in] numSetLayouts Number of sets one allocation creates; only used when pools are keyed by layout hash.
    /// @param[in] maxSetsAllocations Number of allocations the pool is sized for.
    VulkanDescriptorPool(
        VulkanDevice& device,
        const VulkanDescriptorTypeCounts& typeCounts,
        gp::UInt32 numSetLayouts,
        gp::UInt32 maxSetsAllocations
    ) noexcept;

    /// @brief Destroys the pool and, implicitly, every set allocated from it.
    ~VulkanDescriptorPool() noexcept;

    VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool(VulkanDescriptorPool&&) = delete;
    VulkanDescriptorPool& operator=(VulkanDescriptorPool&&) = delete;

    /// @brief Computes the `VkDescriptorPoolSize`s of a pool.
    /// @param[in] typeCounts Descriptor counts of one allocation.
    /// @param[in] maxSetsAllocations Number of allocations.
    /// @param[out] outPoolSizes Receives one entry per used type, in slot order.
    /// @return Number of entries written.
    [[nodiscard]] static gp::UInt32 computePoolSizes(
        const VulkanDescriptorTypeCounts& typeCounts,
        gp::UInt32 maxSetsAllocations,
        gp::Array<VkDescriptorPoolSize, kVulkanNumDescriptorTypeSlots>& outPoolSizes
    ) noexcept;

    /// @brief Returns the native pool.
    /// @return Handle, or `VK_NULL_HANDLE` when creation failed or the device is offline.
    [[nodiscard]] VkDescriptorPool getHandle() const noexcept
    {
        return m_descriptorPool;
    }

    /// @brief Reports whether the native pool exists.
    /// @return True when the pool was created.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_descriptorPool != VK_NULL_HANDLE;
    }

    /// @brief Reports whether one more allocation of `layout` fits in the set budget.
    /// @param[in] layout Layout to allocate.
    /// @return True when the tracked set count stays within `getMaxDescriptorSets()`.
    [[nodiscard]] bool canAllocate(const VulkanDescriptorSetsLayout& layout) const noexcept;

    /// @brief Records one allocation of `layout`.
    /// @param[in] layout Allocated layout; must have the pool shape.
    /// @return False, recording nothing, when `layout` does not have the pool shape.
    bool trackAddUsage(const VulkanDescriptorSetsLayout& layout) noexcept;

    /// @brief Records that one allocation of `layout` is no longer used.
    /// @param[in] layout Released layout; must have the pool shape.
    /// @return False, recording nothing, when `layout` does not have the pool shape or nothing is tracked.
    bool trackRemoveUsage(const VulkanDescriptorSetsLayout& layout) noexcept;

    /// @brief Reports whether no set is tracked.
    /// @return True when the tracked set count is zero.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_numAllocatedDescriptorSets == 0u;
    }

    /// @brief Returns every set to the pool with `vkResetDescriptorPool`.
    void reset() noexcept;

    /// @brief Allocates descriptor sets from this pool.
    /// @param[in] allocateInfo Allocate info of a layout; its pool field is replaced.
    /// @param[out] outSets Receives `allocateInfo.descriptorSetCount` sets.
    /// @return False when the pool is exhausted, fragmented or invalid.
    [[nodiscard]] bool
        allocateDescriptorSets(const VkDescriptorSetAllocateInfo& allocateInfo, VkDescriptorSet* outSets) noexcept;

    /// @brief Returns the `maxSets` the pool was created with.
    /// @return Set capacity.
    [[nodiscard]] gp::UInt32 getMaxDescriptorSets() const noexcept
    {
        return m_maxDescriptorSets;
    }

    /// @brief Returns the tracked set count.
    /// @return Sets allocated since the last reset.
    [[nodiscard]] gp::UInt32 getNumAllocatedDescriptorSets() const noexcept
    {
        return m_numAllocatedDescriptorSets;
    }

    /// @brief Returns the highest tracked set count.
    /// @return Peak set count.
    [[nodiscard]] gp::UInt32 getPeakAllocatedDescriptorSets() const noexcept
    {
        return m_peakAllocatedDescriptorSets;
    }

private:
    /// @brief Reports whether `layout` has the per-type counts of this pool.
    /// @param[in] layout Layout to compare.
    /// @return True when every count matches.
    [[nodiscard]] bool hasSameShape(const VulkanDescriptorSetsLayout& layout) const noexcept;

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Per-type descriptor counts of one allocation.
    VulkanDescriptorTypeCounts m_typeCounts{};

    /// @brief Set capacity.
    gp::UInt32 m_maxDescriptorSets{ 0u };

    /// @brief Tracked sets.
    gp::UInt32 m_numAllocatedDescriptorSets{ 0u };

    /// @brief Highest tracked set count.
    gp::UInt32 m_peakAllocatedDescriptorSets{ 0u };

    /// @brief Native pool.
    VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
};

}   // namespace gp::rhi::vulkan
