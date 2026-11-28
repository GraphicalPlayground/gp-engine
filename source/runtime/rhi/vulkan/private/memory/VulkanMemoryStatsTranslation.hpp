// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "profiling/RHIStats.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Translates the device memory snapshot into the backend-agnostic RHI memory statistics.
/// @details Vulkan exposes heaps, not adapters: every `DEVICE_LOCAL` heap counts as dedicated video memory and every
///          other heap as system memory shared with the GPU. A UMA device therefore reports its whole pool as video
///          memory. No heap is reserved for the GPU alone, so `dedicatedSystemMemory` stays zero. The budget fields
///          come from `VK_EXT_memory_budget` when it is enabled and fall back to the heap sizes otherwise (the device
///          memory manager already made that choice when it filled `stats`).
/// @param[in] stats Snapshot from `VulkanDeviceMemoryManager::getMemoryStats()`.
/// @param[in] deviceLocalTotal Physical size of every device-local heap in bytes.
/// @param[in] hostTotal Physical size of every other heap in bytes.
/// @return RHI memory statistics.
[[nodiscard]] GP_RHI_VULKAN_API profiling::RHIMemoryStats makeVulkanRHIMemoryStats(
    const VulkanMemoryStats& stats, gp::UInt64 deviceLocalTotal, gp::UInt64 hostTotal
) noexcept;

}   // namespace gp::rhi::vulkan
