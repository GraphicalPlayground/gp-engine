// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "shaders/ShaderEnums.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Tests whether every bit set in `contains` is also set in `flags`.
/// @tparam InFlagsType Vulkan bitmask flags type (`VkFlags` or `VkFlags64`).
/// @tparam InBitsType Vulkan flag bit enum or integer mask type.
/// @param[in] flags Combined Vulkan bitmask flags.
/// @param[in] contains Required bit pattern to check.
/// @return True when `(flags & contains) == contains`.
template <typename InFlagsType, typename InBitsType>
[[nodiscard]] inline constexpr bool vkHasAllFlags(const InFlagsType flags, const InBitsType contains) noexcept
{
    const InFlagsType mask = static_cast<InFlagsType>(contains);
    return (flags & mask) == mask;
}

/// @brief Tests whether at least one bit set in `contains` is also set in `flags`.
/// @tparam InFlagsType Vulkan bitmask flags type (`VkFlags` or `VkFlags64`).
/// @tparam InBitsType Vulkan flag bit enum or integer mask type.
/// @param[in] flags Combined Vulkan bitmask flags.
/// @param[in] contains Bit pattern to test for overlap.
/// @return True when `(flags & contains) != 0`.
template <typename InFlagsType, typename InBitsType>
[[nodiscard]] inline constexpr bool vkHasAnyFlags(const InFlagsType flags, const InBitsType contains) noexcept
{
    return (flags & static_cast<InFlagsType>(contains)) != static_cast<InFlagsType>(0u);
}

/// @brief Translates an RHI `ShaderFrequency` into its corresponding `VkShaderStageFlagBits`.
/// @details Ray hit groups map to `VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR` for single-bit stage queries.
///          Work-graph or unrecognized frequencies return `VK_SHADER_STAGE_ALL`.
/// @param[in] frequency Engine shader stage enumerator.
/// @return Matching `VkShaderStageFlagBits`, or `VK_SHADER_STAGE_ALL` for unmapped stages.
[[nodiscard]] inline constexpr VkShaderStageFlagBits
    shaderFrequencyToVkStageBit(const shader::ShaderFrequency frequency) noexcept
{
    switch (frequency)
    {
    case shader::ShaderFrequency::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case shader::ShaderFrequency::Mesh:
        return VK_SHADER_STAGE_MESH_BIT_EXT;
    case shader::ShaderFrequency::Amplification:
        return VK_SHADER_STAGE_TASK_BIT_EXT;
    case shader::ShaderFrequency::Pixel:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case shader::ShaderFrequency::Geometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case shader::ShaderFrequency::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    case shader::ShaderFrequency::RayGen:
        return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case shader::ShaderFrequency::RayMiss:
        return VK_SHADER_STAGE_MISS_BIT_KHR;
    case shader::ShaderFrequency::RayHitGroup:
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case shader::ShaderFrequency::RayCallable:
        return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    default:
        return VK_SHADER_STAGE_ALL;
    }
}

/// @brief Translates a single `VkShaderStageFlagBits` value back to its engine `ShaderFrequency`.
/// @details Any-hit, closest-hit, and intersection stage bits all map to `ShaderFrequency::RayHitGroup`.
/// @param[in] flagBits Single Vulkan shader stage bit.
/// @return Matching `ShaderFrequency`, or `static_cast<ShaderFrequency>(kShaderFrequencyCount)` if unrecognized.
[[nodiscard]] inline constexpr shader::ShaderFrequency
    vkStageBitToShaderFrequency(const VkShaderStageFlagBits flagBits) noexcept
{
    switch (flagBits)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return shader::ShaderFrequency::Vertex;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return shader::ShaderFrequency::Pixel;
    case VK_SHADER_STAGE_MESH_BIT_EXT:
        return shader::ShaderFrequency::Mesh;
    case VK_SHADER_STAGE_TASK_BIT_EXT:
        return shader::ShaderFrequency::Amplification;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return shader::ShaderFrequency::Geometry;
    case VK_SHADER_STAGE_COMPUTE_BIT:
        return shader::ShaderFrequency::Compute;
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        return shader::ShaderFrequency::RayGen;
    case VK_SHADER_STAGE_MISS_BIT_KHR:
        return shader::ShaderFrequency::RayMiss;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        return shader::ShaderFrequency::RayCallable;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        return shader::ShaderFrequency::RayHitGroup;
    default:
        return static_cast<shader::ShaderFrequency>(shader::kShaderFrequencyCount);
    }
}

/// @brief Translates a compact graphics `shader_stage::Stage` index to its `VkShaderStageFlagBits`.
/// @param[in] stage Graphics shader stage (`Vertex`, `Pixel`, `Geometry`, `Mesh`, or `Task`).
/// @return Matching `VkShaderStageFlagBits`, or `VK_SHADER_STAGE_ALL` for invalid stage indices.
[[nodiscard]] inline constexpr VkShaderStageFlagBits gfxStageToVkStageBit(const shader_stage::Stage stage) noexcept
{
    switch (stage)
    {
    case shader_stage::Stage::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case shader_stage::Stage::Pixel:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case shader_stage::Stage::Geometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case shader_stage::Stage::Mesh:
        return VK_SHADER_STAGE_MESH_BIT_EXT;
    case shader_stage::Stage::Task:
        return VK_SHADER_STAGE_TASK_BIT_EXT;
    default:
        return VK_SHADER_STAGE_ALL;
    }
}

/// @brief Returns the number of compact shader stages belonging to a Vulkan pipeline bind point.
/// @param[in] bindPoint Pipeline bind point (`GRAPHICS`, `COMPUTE`, or `RAY_TRACING_KHR`).
/// @return Stage count (`5` for graphics, `1` for compute, `4` for ray tracing), or `0` for unknown bind points.
[[nodiscard]] inline constexpr gp::Int32 getNumStagesForBindPoint(const VkPipelineBindPoint bindPoint) noexcept
{
    switch (bindPoint)
    {
    case VK_PIPELINE_BIND_POINT_GRAPHICS:
        return static_cast<gp::Int32>(shader_stage::Stage::NumGraphicsStages);
    case VK_PIPELINE_BIND_POINT_COMPUTE:
        return static_cast<gp::Int32>(shader_stage::Stage::NumComputeStages);
    case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
        return static_cast<gp::Int32>(shader_stage::Stage::NumRayTracingStages);
    default:
        return 0;
    }
}

}   // namespace gp::rhi::vulkan
