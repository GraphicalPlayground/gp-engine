// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Immutable `VkSampler` created from a translated `VkSamplerCreateInfo`.
class VulkanSamplerState;

/// @brief Device-wide cache that deduplicates sampler states by the CRC of their create info.
class VulkanSamplerStateCache;

/// @brief Rasterizer state holding a ready-to-use `VkPipelineRasterizationStateCreateInfo`.
class VulkanRasterizerState;

/// @brief Depth-stencil state translated into `VkPipelineDepthStencilStateCreateInfo` at pipeline build time.
class VulkanDepthStencilState;

/// @brief Blend state holding one `VkPipelineColorBlendAttachmentState` per render target slot.
class VulkanBlendState;

/// @brief Cache key of a vertex declaration: element list plus its full and stride-less hashes.
struct VulkanVertexDeclarationKey;

/// @brief Vertex declaration holding its element list and its precomputed hashes.
class VulkanVertexDeclaration;

/// @brief Compacted vertex bindings and attributes generated for one vertex declaration and shader input mask.
class VulkanVertexInputStateInfo;

}   // namespace gp::rhi::vulkan
