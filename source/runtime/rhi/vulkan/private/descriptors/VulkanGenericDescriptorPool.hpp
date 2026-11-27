// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Descriptors per set, by core descriptor type, a generic pool reserves at least.
using VulkanDescriptorPoolSizeRatios = gp::Array<gp::Float32, kVulkanNumCoreDescriptorTypes>;

/// @brief Default per-set descriptor ratios of a generic pool, indexed by `VkDescriptorType`.
/// @details Tuned for typical material shaders: a few samplers and sampled images, one dynamic uniform buffer per
///          stage, and few storage resources.
inline constexpr VulkanDescriptorPoolSizeRatios kVulkanDefaultDescriptorPoolSizeRatios = {
    2.0f,          // VK_DESCRIPTOR_TYPE_SAMPLER
    2.0f,          // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    2.0f,          // VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
    1.0f / 8.0f,   // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    1.0f / 2.0f,   // VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
    1.0f / 8.0f,   // VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
    1.0f / 4.0f,   // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    1.0f / 8.0f,   // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    4.0f,          // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
    1.0f / 8.0f,   // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
    1.0f / 8.0f    // VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
};

/// @brief Minimum number of descriptors of each type a generic pool reserves.
inline constexpr gp::Float32 kVulkanMinGenericPoolDescriptorsPerType = 4.0f;

/// @brief Descriptor pool shared by every layout, sized from per-set ratios rather than from one layout.
/// @details The typed pools of `VulkanTypedDescriptorPoolSet` hold sets of a single layout shape. The descriptor set
///          cache instead keeps sets of every shape alive in one pool, so the pool is sized for an average set: for
///          each type, `max(ratio * maxSets, defaultRatio * maxSets, 4)` descriptors. Sets are never freed one by one;
///          the pool is reset as a whole.
class GP_RHI_VULKAN_API VulkanGenericDescriptorPool
{
public:
    /// @brief Creates the native pool.
    /// @param[in] device Logical device; without a native handle no pool is created.
    /// @param[in] maxDescriptorSets Number of sets the pool can hold.
    /// @param[in] poolSizeRatios Descriptors per set for each core type.
    VulkanGenericDescriptorPool(
        VulkanDevice& device, gp::UInt32 maxDescriptorSets, const VulkanDescriptorPoolSizeRatios& poolSizeRatios
    ) noexcept;

    /// @brief Destroys the native pool and every set allocated from it.
    ~VulkanGenericDescriptorPool() noexcept;

    VulkanGenericDescriptorPool(const VulkanGenericDescriptorPool&) = delete;
    VulkanGenericDescriptorPool& operator=(const VulkanGenericDescriptorPool&) = delete;
    VulkanGenericDescriptorPool(VulkanGenericDescriptorPool&&) = delete;
    VulkanGenericDescriptorPool& operator=(VulkanGenericDescriptorPool&&) = delete;

    /// @brief Computes the descriptor count reserved for each core type.
    /// @param[in] maxDescriptorSets Number of sets the pool can hold.
    /// @param[in] poolSizeRatios Descriptors per set for each core type.
    /// @param[out] outPoolSizes Receives one entry per core type.
    static void computePoolSizes(
        gp::UInt32 maxDescriptorSets,
        const VulkanDescriptorPoolSizeRatios& poolSizeRatios,
        gp::Array<VkDescriptorPoolSize, kVulkanNumCoreDescriptorTypes>& outPoolSizes
    ) noexcept;

    /// @brief Returns the device that owns the pool.
    /// @return Device.
    [[nodiscard]] VulkanDevice& getDevice() const noexcept
    {
        return m_device;
    }

    /// @brief Returns the number of sets the pool can hold.
    /// @return Set capacity.
    [[nodiscard]] gp::UInt32 getMaxDescriptorSets() const noexcept
    {
        return m_maxDescriptorSets;
    }

    /// @brief Returns the descriptor count reserved for one core type.
    /// @param[in] type Core descriptor type.
    /// @return Descriptor count, or 0 for a non-core type.
    [[nodiscard]] gp::UInt32 getPoolSize(VkDescriptorType type) const noexcept;

    /// @brief Returns the native pool.
    /// @return Handle, or `VK_NULL_HANDLE` when creation failed or the device has no native handle.
    [[nodiscard]] VkDescriptorPool getHandle() const noexcept
    {
        return m_descriptorPool;
    }

    /// @brief Returns every set to the pool.
    void reset() noexcept;

    /// @brief Allocates one set.
    /// @param[in] layout Native set layout.
    /// @param[out] outSet Receives the set.
    /// @return False when the pool is exhausted or was never created.
    [[nodiscard]] bool allocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet& outSet) noexcept;

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Set capacity.
    gp::UInt32 m_maxDescriptorSets{ 0u };

    /// @brief Native pool.
    VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };

    /// @brief Descriptor counts per core type, kept for statistics and tests.
    gp::Array<gp::UInt32, kVulkanNumCoreDescriptorTypes> m_poolSizes{};
};

}   // namespace gp::rhi::vulkan
