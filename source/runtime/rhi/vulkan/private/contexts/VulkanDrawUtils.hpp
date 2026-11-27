// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Converts an RHI primitive count into the vertex (or index) count Vulkan draws expect.
/// @details The RHI speaks in primitives, Vulkan in vertices. Lists multiply by the vertices per primitive, strips
///          add the two vertices that prime the first triangle. Unsupported topologies (patch lists, adjacency) are
///          treated as point lists and reported through a slow check.
/// @param[in] numPrimitives Number of primitives to draw.
/// @param[in] topology Primitive topology of the bound graphics pipeline.
/// @return Vertex count for `vkCmdDraw` / index count for `vkCmdDrawIndexed`; zero when `numPrimitives` is zero.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32
    getVulkanVertexCountForPrimitiveCount(gp::UInt32 numPrimitives, VkPrimitiveTopology topology) noexcept;

/// @brief Returns how many indirect commands fit in `bufferSize` bytes starting at `offset`.
/// @param[in] bufferSize Size of the argument buffer in bytes.
/// @param[in] offset Byte offset of the first command.
/// @param[in] stride Size of one indirect command in bytes; must be non-zero.
/// @return Number of whole commands available, zero when `offset` is past the end.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32
    getVulkanIndirectCommandCapacity(gp::UInt64 bufferSize, gp::UInt64 offset, gp::UInt32 stride) noexcept;

}   // namespace gp::rhi::vulkan
