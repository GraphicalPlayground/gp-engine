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

}   // namespace gp::rhi::vulkan
