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
    RayTracingOpacityMicromapArray,
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

/// @brief Total number of enumerators defined in `ResourceLockMode`.
inline constexpr gp::UInt32 kResourceLockModeCount =
    static_cast<gp::UInt32>(ResourceLockMode::WriteOnlyNoOverwrite) + 1u;

/// @brief Selects what kind of hardware fast-clear value is bound to a render target.
enum class ClearBinding : gp::UInt8
{
    /// @brief No clear value is bound; platforms that require a bound clear value skip fast clears.
    NoneBound = 0,

    /// @brief A four-channel linear RGBA clear color is bound.
    ColorBound,

    /// @brief Depth and stencil clear values are bound.
    DepthStencilBound
};

/// @brief Packed depth and stencil access mode using the low nibble for depth and the high nibble for stencil.
enum class ExclusiveDepthStencilMode : gp::UInt8
{
    DepthNop = 0x00u,
    DepthRead = 0x01u,
    DepthWrite = 0x02u,
    DepthMask = 0x0Fu,

    StencilNop = 0x00u,
    StencilRead = 0x10u,
    StencilWrite = 0x20u,
    StencilMask = 0xF0u,

    DepthNopStencilNop = DepthNop | StencilNop,
    DepthReadStencilNop = DepthRead | StencilNop,
    DepthWriteStencilNop = DepthWrite | StencilNop,
    DepthNopStencilRead = DepthNop | StencilRead,
    DepthReadStencilRead = DepthRead | StencilRead,
    DepthWriteStencilRead = DepthWrite | StencilRead,
    DepthNopStencilWrite = DepthNop | StencilWrite,
    DepthReadStencilWrite = DepthRead | StencilWrite,
    DepthWriteStencilWrite = DepthWrite | StencilWrite
};

}   // namespace gp::rhi::resource
