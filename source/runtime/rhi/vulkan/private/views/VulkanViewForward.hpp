// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Kind of native object currently held by a `VulkanView`.
enum class VulkanViewKind : gp::UInt8;

/// @brief State of a `VulkanView` that holds no native object.
struct VulkanInvalidatedViewState;

/// @brief Native state of a typed (texel) buffer view.
struct VulkanTypedBufferView;

/// @brief Native state of a structured or raw buffer view.
struct VulkanStructuredBufferView;

/// @brief Native state of an acceleration structure view.
struct VulkanAccelerationStructureView;

/// @brief Native state of a texture view.
struct VulkanTextureView;

/// @brief Backend description of a `VkImageView` derived from an RHI texture view.
struct VulkanTextureViewDesc;

/// @brief `VkImageViewCreateInfo` that owns the storage of its `pNext` chain.
struct VulkanImageViewCreateInfo;

/// @brief Native view state (image view, buffer view, buffer range or acceleration structure) of one RHI view.
class VulkanView;

/// @brief Vulkan implementation of a shader resource view.
class VulkanShaderResourceView;

/// @brief Vulkan implementation of an unordered access view.
class VulkanUnorderedAccessView;

}   // namespace gp::rhi::vulkan
