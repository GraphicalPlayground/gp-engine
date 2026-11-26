// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayoutInfo.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "device/DeviceEnums.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Run-time descriptor set layouts of one pipeline: the finalized description plus its native handles.
/// @details `compile()` resolves every set layout through a `VulkanDescriptorSetLayoutCache` and prepares the
///          `VkDescriptorSetAllocateInfo` that pools reuse for every allocation. The handles are **not owned**: the
///          cache destroys them, so many pipelines can share one set layout.
/// @note Not copyable or movable: the allocate info points into `m_layoutHandles`.
class GP_RHI_VULKAN_API VulkanDescriptorSetsLayout : public VulkanDescriptorSetsLayoutInfo
{
public:
    /// @brief Creates an empty layout.
    /// @param[in] device Logical device whose limits the layout is checked against.
    explicit VulkanDescriptorSetsLayout(VulkanDevice& device) noexcept;

    /// @brief Forgets the handles; the cache keeps ownership.
    ~VulkanDescriptorSetsLayout() noexcept = default;

    VulkanDescriptorSetsLayout(const VulkanDescriptorSetsLayout&) = delete;
    VulkanDescriptorSetsLayout& operator=(const VulkanDescriptorSetsLayout&) = delete;
    VulkanDescriptorSetsLayout(VulkanDescriptorSetsLayout&&) = delete;
    VulkanDescriptorSetsLayout& operator=(VulkanDescriptorSetsLayout&&) = delete;

    /// @brief Resolves the native set layouts; may be called only once, after `finalizeBindings()`.
    /// @param[in,out] cache Device set layout cache.
    /// @return False when already compiled, when the layout breaks a device limit, or when a layout cannot be created.
    [[nodiscard]] bool compile(VulkanDescriptorSetLayoutCache& cache) noexcept;

    /// @brief Checks per-type descriptor counts against the per-set limits of a device.
    /// @details AMD drivers advertise a low `maxDescriptorSetUniformBuffersDynamic` but accept more, so that one limit
    ///          is not enforced on AMD hardware.
    /// @param[in] typeCounts Descriptor counts of the layout.
    /// @param[in] limits Device limits.
    /// @param[in] vendorId Device vendor.
    /// @param[in] maxAccelerationStructures `maxDescriptorSetAccelerationStructures`, or zero without ray tracing.
    /// @return True when every limit holds.
    [[nodiscard]] static bool validateLimits(
        const VulkanDescriptorTypeCounts& typeCounts,
        const VkPhysicalDeviceLimits& limits,
        GpuVendorId vendorId,
        gp::UInt32 maxAccelerationStructures
    ) noexcept;

    /// @brief Returns the set layout handles, indexed by set.
    /// @return Handles; empty before `compile()`.
    [[nodiscard]] const gp::Vector<VkDescriptorSetLayout>& getHandles() const noexcept
    {
        return m_layoutHandles;
    }

    /// @brief Returns the set layout ids, parallel to `getHandles()`.
    /// @return Ids.
    [[nodiscard]] const gp::Vector<gp::UInt32>& getHandleIds() const noexcept
    {
        return m_layoutHandleIds;
    }

    /// @brief Returns the allocate info for one descriptor set per set layout; the pool field is filled by the pool.
    /// @return Allocate info.
    [[nodiscard]] const VkDescriptorSetAllocateInfo& getAllocateInfo() const noexcept
    {
        return m_allocateInfo;
    }

    /// @brief Reports whether `compile()` succeeded.
    /// @return True once compiled.
    [[nodiscard]] bool isCompiled() const noexcept
    {
        return m_isCompiled;
    }

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Set layout handles owned by the cache.
    gp::Vector<VkDescriptorSetLayout> m_layoutHandles{};

    /// @brief Ids of `m_layoutHandles`.
    gp::Vector<gp::UInt32> m_layoutHandleIds{};

    /// @brief Allocate info pointing at `m_layoutHandles`.
    VkDescriptorSetAllocateInfo m_allocateInfo{};

    /// @brief True once compiled.
    bool m_isCompiled{ false };
};

}   // namespace gp::rhi::vulkan
