// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Version of the precache hash layout; bump it whenever a hashed field changes.
/// @details Stored next to persisted precache records, so a change of the layout invalidates them instead of silently
///          matching the wrong pipelines.
inline constexpr gp::UInt32 kVulkanPrecachePSOHashVersion = 6u;

/// @brief Hashes the state-object half of a graphics initializer for pipeline precaching.
/// @details Precaching compiles pipelines ahead of their first draw. The renderer predicts initializers, hashes them,
///          and later looks up the real draw's initializer by hash. The state half covers what the material decides:
///          the vertex declaration, every shader (by bytecode hash), the blend, rasterizer and depth-stencil states (by
///          content, not by pointer), the shading rate, the primitive type, depth bounds and variable rate shading.
///          The vertex declaration is left out on Arm GPUs, whose drivers do not bake vertex input into pipelines.
/// @note The result is computed once per material and stored in
/// `GraphicsPipelineStateInitializer::statePrecachePSOHash`.
/// @param[in] initializer Graphics initializer.
/// @param[in] vendorId Vendor of the device the pipelines are compiled for.
/// @return 64-bit hash (XXH64 of the fields in a fixed order).
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64 computeVulkanStatePrecachePSOHash(
    const pipeline::GraphicsPipelineStateInitializer& initializer, gp::rhi::GpuVendorId vendorId
) noexcept;

/// @brief Hashes a full graphics initializer for pipeline precaching.
/// @details Combines the state hash (reusing `initializer.statePrecachePSOHash` when it is set) with the fields the
///          render pass decides: enabled render targets, their formats and relevant flags, sample count, subpass hint
///          and index, multiview count, fragment density attachment and conservative rasterization. The depth-stencil
///          format is deliberately left out: it does not change Vulkan pipelines in practice, and hashing it splits
///          predictions that would otherwise match.
/// @param[in] initializer Graphics initializer.
/// @param[in] vendorId Vendor of the device the pipelines are compiled for.
/// @return 64-bit hash.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64 computeVulkanPrecachePSOHash(
    const pipeline::GraphicsPipelineStateInitializer& initializer, gp::rhi::GpuVendorId vendorId
) noexcept;

/// @brief Reports whether two initializers describe the same precached pipeline.
/// @details Resolves hash collisions after a precache hash lookup. Compares, from cheapest to most expensive: plain
///          fields (with relevance masks on texture flags), then shader pointers, then the content of the vertex
///          declaration and of the blend, rasterizer and depth-stencil states (two initializers may use different state
///          objects with equal content).
/// @param[in] lhs First initializer.
/// @param[in] rhs Second initializer.
/// @return True when a pipeline compiled for one can serve the other.
[[nodiscard]] GP_RHI_VULKAN_API bool matchVulkanPrecachePSOInitializers(
    const pipeline::GraphicsPipelineStateInitializer& lhs, const pipeline::GraphicsPipelineStateInitializer& rhs
) noexcept;

}   // namespace gp::rhi::vulkan
