// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Byte serialization of a pipeline description plus its 64-bit hash.
class VulkanPipelineKey;

/// @brief Appends typed values to a `VulkanPipelineKey`.
class VulkanPipelineKeyWriter;

/// @brief Fixed-capacity list of `VkDynamicState` values enabled on a graphics pipeline.
struct VulkanDynamicStateList;

/// @brief Device features that decide which dynamic states a graphics pipeline enables.
struct VulkanDynamicStateFeatures;

/// @brief Native description of a graphics pipeline, the unit of graphics pipeline caching.
struct VulkanGraphicsPipelineDesc;

/// @brief Cached native graphics pipeline shared by every wrapper with the same description.
class VulkanGraphicsPipelineState;

/// @brief Cached native compute pipeline shared by every wrapper with the same shader and constants.
class VulkanComputePipelineState;

/// @brief RHI graphics pipeline handed to callers; keeps one cached `VulkanGraphicsPipelineState` alive.
class VulkanWrappedGraphicsPipelineState;

/// @brief RHI compute pipeline handed to callers; keeps one cached `VulkanComputePipelineState` alive.
class VulkanWrappedComputePipelineState;

/// @brief Owner of every graphics and compute pipeline, the pipeline layouts and the `VkPipelineCache`.
class VulkanPipelineStateCacheManager;

/// @brief Owning key of the bound shader state cache (keeps the shaders alive).
class VulkanBoundShaderStateKey;

/// @brief Non-owning lookup key of the bound shader state cache.
class VulkanBoundShaderStateLookupKey;

/// @brief Cache entry linking a bound shader state to the process-wide cache.
class VulkanCachedBoundShaderStateLink;

/// @brief Vulkan bound shader state (vertex declaration plus graphics stages).
class VulkanBoundShaderState;

}   // namespace gp::rhi::vulkan
