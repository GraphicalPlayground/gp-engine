// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Hardware queue roles managed by `VulkanDevice`.
enum class VulkanQueueType : gp::UInt8
{
    Graphics = 0u,
    AsyncCompute,
    Transfer,
    Count
};

/// @brief Total number of distinct `VulkanQueueType` slots.
inline constexpr gp::USize kVulkanQueueTypeCount = static_cast<gp::USize>(VulkanQueueType::Count);

/// @brief Configuration inputs governing optional async-compute and dedicated transfer queue selection.
struct VulkanQueueFamilySelectionConfig final
{
    /// @brief Mirrors `r.Vulkan.AllowAsyncCompute`.
    bool allowAsyncCompute{ true };

    /// @brief Mirrors `r.Vulkan.AllowPresentOnComputeQueue`.
    bool allowPresentOnComputeQueue{ false };

    /// @brief Mirrors `-ForceAsyncCompute`.
    bool forceAsyncCompute{ false };

    /// @brief Mirrors `-DisableAsyncCompute`.
    bool disableAsyncCompute{ false };

    /// @brief Mirrors `r.Vulkan.AllowTransferQueue`.
    bool allowTransferQueue{ false };

    /// @brief True when Synchronization2 and Timeline Semaphores are active (parallel submission supported).
    bool supportsParallelRendering{ false };

    /// @brief True when `VK_KHR_maintenance9` is active (required for cross-queue ownership transfer queries).
    bool hasMaintenance9{ false };
};

/// @brief Resolved queue family indices and active family list selected for `vkCreateDevice`.
struct VulkanQueueFamilyIndices final
{
    /// @brief Graphics queue family index, or `-1` if none found.
    gp::Int32 graphicsFamily{ -1 };

    /// @brief Dedicated async compute queue family index, or `-1` if unavailable or disabled.
    gp::Int32 asyncComputeFamily{ -1 };

    /// @brief Dedicated transfer-only queue family index, or `-1` if unavailable or disabled.
    gp::Int32 transferFamily{ -1 };

    /// @brief Unique list of selected queue family indices (1 to 3 entries).
    gp::Vector<gp::UInt32> activeQueueFamilies{};

    /// @brief True when queue family ownership transfer barriers are required between active families.
    bool needsQueueFamilyOwnershipTransfers{ true };
};

/// @brief Resolves a `VulkanQueueFamilySelectionConfig` from console variables and command-line flags.
/// @param[in] supportsParallelRendering True when Sync2 and timeline semaphores are enabled.
/// @param[in] hasMaintenance9 True when `VK_KHR_maintenance9` is enabled.
/// @return Populated `VulkanQueueFamilySelectionConfig`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanQueueFamilySelectionConfig
    resolveQueueFamilySelectionConfig(bool supportsParallelRendering, bool hasMaintenance9) noexcept;

/// @brief Selects graphics, optional async compute, and optional dedicated transfer queue families.
/// @param[in] queueFamilyProps Queue family descriptors enumerated from the physical device.
/// @param[in] config Queue selection policy flags.
/// @return Resolved `VulkanQueueFamilyIndices`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanQueueFamilyIndices selectQueueFamilies(
    gp::ConstVectorView<VkQueueFamilyProperties> queueFamilyProps, const VulkanQueueFamilySelectionConfig& config
) noexcept;

/// @brief Builds `VkDeviceQueueCreateInfo` structs and a flat `1.0f` priorities buffer for all selected families.
/// @param[in] queueFamilyProps Queue family descriptors enumerated from the physical device.
/// @param[in] indices Selected queue family indices from `selectQueueFamilies()`.
/// @param[out] outCreateInfos Populated `VkDeviceQueueCreateInfo` array referencing `outPriorities`.
/// @param[out] outPriorities Backing storage for `pQueuePriorities` (must outlive `vkCreateDevice`).
GP_RHI_VULKAN_API void buildDeviceQueueCreateInfos(
    gp::ConstVectorView<VkQueueFamilyProperties> queueFamilyProps,
    const VulkanQueueFamilyIndices& indices,
    gp::Vector<VkDeviceQueueCreateInfo>& outCreateInfos,
    gp::Vector<gp::Float32>& outPriorities
) noexcept;

}   // namespace gp::rhi::vulkan
