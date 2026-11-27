// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanPipelineTypes.hpp"
#include "RHIVulkan.hpp"
#include "shaders/ShaderTypes.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

/// @brief Collects the Vulkan shader of every graphics stage of a bound shader state input.
/// @details The RHI shader pointers are downcast with `static_cast`: every shader created by this backend is a
///          `VulkanBaseShader`, and there is no RTTI to check it.
/// @param[in] input Bound shader state input.
/// @return Shader per stage, indexed by `shader_stage::Stage`; null for an unused stage.
[[nodiscard]] GP_RHI_VULKAN_API VulkanGraphicsShaderArray
    getVulkanGraphicsShaders(const pipeline::BoundShaderStateInput& input) noexcept;

/// @brief Returns the specialization constants a bound shader state input gives one graphics stage.
/// @param[in] input Bound shader state input.
/// @param[in] stage Graphics stage.
/// @return Constants of that stage (empty for an unknown stage).
[[nodiscard]] GP_RHI_VULKAN_API const shader::SpecializationContainer& getVulkanStageSpecializationConstants(
    const pipeline::BoundShaderStateInput& input, shader_stage::Stage stage
) noexcept;

}   // namespace gp::rhi::vulkan
