// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsEnums.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Returns how uniform buffers are promoted to dynamic descriptors (`r.Vulkan.DynamicGlobalUBs`).
/// @details Dynamic uniform buffers take their offset at bind time, so a per-draw constant upload only moves an offset
///          instead of rewriting a descriptor set. Values outside `[0, 2]` are clamped.
/// @return Promotion mode; `VulkanDynamicUniformBufferMode::All` by default.
[[nodiscard]] GP_RHI_VULKAN_API VulkanDynamicUniformBufferMode getVulkanDynamicUniformBufferMode() noexcept;

/// @brief Returns how many frames an unused descriptor pool set waits before it is destroyed
/// (`r.Vulkan.NumFramesToWaitForDescriptorDelete`).
/// @return Frame count; 2 by default. Negative values read as 0.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 getVulkanNumFramesToWaitForDescriptorDelete() noexcept;

/// @brief Reports whether descriptor sets are deduplicated through `VulkanDescriptorSetCache`
/// (`r.Vulkan.DescriptorSetCache`).
/// @details With the cache, a descriptor set is keyed by the hash of everything written into it and reused across
///          draws and frames; without it, sets are allocated per draw from the per-frame pool sets and rewritten.
/// @return True when the cache is enabled; false by default.
[[nodiscard]] GP_RHI_VULKAN_API bool useVulkanDescriptorCache() noexcept;

/// @brief Returns the number of descriptor sets one cached pool should hold (`r.Vulkan.DSetCacheTargetSetsPerPool`).
/// @return Target set count; 4096 by default, at least 1.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 getVulkanDescriptorSetCacheTargetSetsPerPool() noexcept;

/// @brief Returns how many cached pools are searched before a new set is created
/// (`r.Vulkan.DSetCacheMaxPoolLookups`).
/// @return Lookup count; 2 by default, at least 1.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 getVulkanDescriptorSetCacheMaxPoolLookups() noexcept;

/// @brief Reports whether descriptor sets are rewritten on every draw, even when nothing changed
/// (`r.Vulkan.AlwaysWriteDS`).
/// @details Debugging aid: it rules out stale descriptor state at the cost of one `vkUpdateDescriptorSets` per draw.
/// @return True when forced; false by default.
[[nodiscard]] GP_RHI_VULKAN_API bool shouldVulkanAlwaysWriteDescriptors() noexcept;

}   // namespace gp::rhi::vulkan
