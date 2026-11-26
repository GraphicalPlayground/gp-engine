// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/VulkanTextureEnums.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Snapshot of the Vulkan texture memory counters.
struct VulkanTextureMemoryStats final
{
    /// @brief Bytes per accounting bucket, indexed by `VulkanTextureStatCategory`.
    gp::Int64 categoryBytes[kVulkanTextureStatCategoryCount]{};

    /// @brief Live textures per accounting bucket, indexed by `VulkanTextureStatCategory`.
    gp::Int64 categoryCounts[kVulkanTextureStatCategoryCount]{};

    /// @brief Total bytes of every live texture allocation.
    gp::Int64 totalTextureBytes{ 0 };
};

/// @brief Resolves the accounting bucket a texture is charged to.
/// @details CPU readback wins over everything, then targetable and UAV textures (render targets), then the
///          dimension.
/// @param[in] desc Texture description.
/// @return Accounting bucket.
[[nodiscard]] GP_RHI_VULKAN_API VulkanTextureStatCategory
    getVulkanTextureStatCategory(const resource::TextureDesc& desc) noexcept;

/// @brief Charges or refunds `textureSize` bytes to `category`.
/// @note Thread-safe; counters are updated with relaxed atomics.
/// @param[in] category Accounting bucket.
/// @param[in] textureSize Allocation size in bytes.
/// @param[in] isAllocating True when the memory is acquired, false when it is released.
GP_RHI_VULKAN_API void
    updateVulkanTextureStats(VulkanTextureStatCategory category, gp::UInt64 textureSize, bool isAllocating) noexcept;

/// @brief Reads every texture counter.
/// @note Counters are read independently; the snapshot is not atomic as a whole.
/// @return Current counters.
[[nodiscard]] GP_RHI_VULKAN_API VulkanTextureMemoryStats getVulkanTextureMemoryStats() noexcept;

}   // namespace gp::rhi::vulkan
