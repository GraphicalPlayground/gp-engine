// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelineShaders.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/BoundShaderState.hpp"
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShader.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Empty constant list returned for stages that have none.
const shader::SpecializationContainer g_emptySpecializationConstants{};

/// @brief Returns the array slot of a graphics stage.
/// @param[in] stage Graphics stage.
/// @return Slot index.
[[nodiscard]] gp::USize toSlot(const shader_stage::Stage stage) noexcept
{
    return static_cast<gp::USize>(stage);
}

}   // namespace

VulkanGraphicsShaderArray getVulkanGraphicsShaders(const pipeline::BoundShaderStateInput& input) noexcept
{
    VulkanGraphicsShaderArray shaders{};
    if (input.getVertexShader() != nullptr)
    {
        shaders[toSlot(shader_stage::Stage::Vertex)] = static_cast<VulkanVertexShader*>(input.getVertexShader());
    }
    if (input.getPixelShader() != nullptr)
    {
        shaders[toSlot(shader_stage::Stage::Pixel)] = static_cast<VulkanPixelShader*>(input.getPixelShader());
    }
    if (input.getGeometryShader() != nullptr)
    {
        shaders[toSlot(shader_stage::Stage::Geometry)] = static_cast<VulkanGeometryShader*>(input.getGeometryShader());
    }
    if (input.getMeshShader() != nullptr)
    {
        shaders[toSlot(shader_stage::Stage::Mesh)] = static_cast<VulkanMeshShader*>(input.getMeshShader());
    }
    if (input.getAmplificationShader() != nullptr)
    {
        shaders[toSlot(shader_stage::Stage::Task)] = static_cast<VulkanTaskShader*>(input.getAmplificationShader());
    }
    return shaders;
}

const shader::SpecializationContainer& getVulkanStageSpecializationConstants(
    const pipeline::BoundShaderStateInput& input, const shader_stage::Stage stage
) noexcept
{
    switch (stage)
    {
    case shader_stage::Stage::Vertex:
        return input.vertexShaderSpecializationConstants;
    case shader_stage::Stage::Pixel:
        return input.pixelShaderSpecializationConstants;
    case shader_stage::Stage::Geometry:
        return input.geometryShaderSpecializationConstants;
    case shader_stage::Stage::Mesh:
        return input.meshShaderSpecializationConstants;
    case shader_stage::Stage::Task:
        return input.amplificationShaderSpecializationConstants;
    default:
        break;
    }
    return g_emptySpecializationConstants;
}

}   // namespace gp::rhi::vulkan
