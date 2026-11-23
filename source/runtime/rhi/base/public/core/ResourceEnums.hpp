// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi::resource
{

/// @brief Runtime type tag identifying concrete `Resource` subclasses without RTTI.
enum class ResourceType : gp::UInt8
{
    None = 0,
    SamplerState,
    RasterizerState,
    DepthStencilState,
    BlendState,
    VertexDeclaration,
    VertexShader,
    MeshShader,
    AmplificationShader,
    PixelShader,
    GeometryShader,
    RayTracingShader,
    ComputeShader,
    GraphicsPipelineState,
    ComputePipelineState,
    RayTracingPipelineState,
    BoundShaderState,
    UniformBufferLayout,
    UniformBuffer,
    Buffer,
    Texture,
    TextureReference,
    GPUFence,
    RenderQuery,
    RenderQueryPool,
    Viewport,
    UnorderedAccessView,
    ShaderResourceView,
    RayTracingGeometry,
    RayTracingScene,
    RayTracingShaderBindingTable,
    StagingBuffer,
    CustomPresent,
    ShaderLibrary,
    ShaderBundle,
    WorkGraphShader,
    WorkGraphPipelineState,
    StreamSourceSlot,
    ResourceCollection,
    DescriptorRange
};

/// @brief Total number of enumerators defined in `ResourceType`.
inline constexpr gp::USize kResourceTypeCount = static_cast<gp::USize>(ResourceType::DescriptorRange) + 1u;

/// @brief Checks whether `type` identifies a compiled GPU pipeline state resource.
/// @param[in] type Resource type tag to inspect.
/// @return True for graphics, compute, ray tracing, or work graph pipeline states; false otherwise.
[[nodiscard]] inline constexpr bool isPipelineStateResourceType(const ResourceType type) noexcept
{
    switch (type)
    {
    case ResourceType::GraphicsPipelineState:
    case ResourceType::ComputePipelineState:
    case ResourceType::RayTracingPipelineState:
    case ResourceType::WorkGraphPipelineState:
        return true;

    default:
        return false;
    }
}

/// @brief CPU mapping access mode when locking a GPU buffer or texture.
enum class ResourceLockMode : gp::UInt8
{
    ReadOnly = 0,
    WriteOnly,
    WriteOnlyNoOverwrite
};

}   // namespace gp::rhi::resource

