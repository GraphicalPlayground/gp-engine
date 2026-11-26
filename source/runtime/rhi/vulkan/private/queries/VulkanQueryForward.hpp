// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi::vulkan
{

/// @brief Native query pool category.
enum class VulkanQueryPoolType : gp::UInt8;

/// @brief One result slot followed by its availability word, as written by `vkGetQueryPoolResults`.
struct VulkanQueryResultSlot;

/// @brief Per-type list of query pools recorded into one payload.
struct VulkanPayloadQueryPools;

/// @brief Native `VkQueryPool` with result destinations for every reserved query.
class VulkanQueryPool;

/// @brief Device-wide cache of reusable query pools.
class VulkanQueryPoolManager;

/// @brief Render query backed by a slot in a `VulkanQueryPool`.
class VulkanRenderQuery;

/// @brief Render query pool recycling `VulkanRenderQuery` objects.
class VulkanRenderQueryPool;

/// @brief Seam over the recording context for sync point registration on the bottom of the pipe.
class IVulkanSyncContextProvider;

}   // namespace gp::rhi::vulkan
