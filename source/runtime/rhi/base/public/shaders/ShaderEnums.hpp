// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::shader
{

/// @brief Total number of enumerators in `ShaderFrequency`.
inline constexpr gp::UInt8 kShaderFrequencyCount = 12u;

/// @brief Number of graphics pipeline stages in `ShaderFrequency` (Vertex through Geometry).
inline constexpr gp::UInt8 kGraphicsShaderFrequencyCount = 5u;

/// @brief Number of standard rasterization and compute stages in `ShaderFrequency` (Vertex through Compute).
inline constexpr gp::UInt8 kStandardShaderFrequencyCount = 6u;

/// @brief Programmable pipeline stage represented by a shader object.
enum class ShaderFrequency : gp::UInt8
{
    Vertex = 0,
    Mesh = 1,
    Amplification = 2,
    Pixel = 3,
    Geometry = 4,
    Compute = 5,
    RayGen = 6,
    RayMiss = 7,
    RayHitGroup = 8,
    RayCallable = 9,
    WorkGraphRoot = 10,
    WorkGraphComputeNode = 11
};

/// @brief Reflected shader resource binding kind extracted from compiled shader metadata.
enum class ShaderCodeResourceBindingType : gp::UInt8
{
    Invalid = 0,
    SamplerState,
    Texture2D,
    Texture2DArray,
    Texture2DMS,
    Texture3D,
    TextureCube,
    TextureCubeArray,
    TextureMetadata,
    Buffer,
    StructuredBuffer,
    ByteAddressBuffer,
    RaytracingAccelerationStructure,
    RWTexture2D,
    RWTexture2DArray,
    RWTexture3D,
    RWTextureCube,
    RWTextureMetadata,
    RWBuffer,
    RWStructuredBuffer,
    RWByteAddressBuffer,
    RasterizerOrderedTexture2D,
    ResourceCollection,
    DescriptorRange,
    Texture2DMultiView,
    RWTexture2DMultiView
};

/// @brief Member types permitted inside a reflected uniform buffer parameter layout.
enum class UniformBufferBaseType : gp::UInt8
{
    Invalid = 0,
    /// @brief Reserved to diagnose illegal `bool` members in shader parameter structs.
    Bool,
    Int32,
    UInt32,
    Float32,
    Texture,
    SRV,
    UAV,
    Sampler,
    NestedStruct,
    IncludedStruct,
    ReferencedStruct,
    RenderTargetBindingSlots,
    ResourceCollection,
    DescriptorRange,
    UniformBlock
};

/// @brief Binding models permitted for a uniform buffer layout.
enum class UniformBufferBindingFlags : gp::UInt8
{
    None = 0x00u,
    /// @brief Bound per shader via `setShaderUniformBuffer`.
    Shader = 1u << 0,
    /// @brief Bound globally through a static slot.
    Static = 1u << 1,
    /// @brief Allows either binding model; validation rejects using both simultaneously on one instance.
    StaticAndShader = (1u << 0) | (1u << 1)
};

/// @brief Execution path used to dispatch shader bundles.
enum class ShaderBundleMethod : gp::UInt8
{
    Emulation = 0,
    Native = 1
};

/// @brief Checks whether `frequency` is a graphics pipeline stage.
/// @param[in] frequency Shader stage to test.
/// @return True for Vertex, Mesh, Amplification, Pixel, or Geometry stages.
[[nodiscard]] inline constexpr bool isValidGraphicsFrequency(const ShaderFrequency frequency) noexcept
{
    switch (frequency)
    {
    case ShaderFrequency::Vertex:
    case ShaderFrequency::Mesh:
    case ShaderFrequency::Amplification:
    case ShaderFrequency::Pixel:
    case ShaderFrequency::Geometry:
        return true;

    default:
        break;
    }

    return false;
}

/// @brief Checks whether `frequency` is a pre-rasterization graphics stage.
/// @param[in] frequency Shader stage to test.
/// @return True when `frequency` is a graphics stage other than Pixel.
[[nodiscard]] inline constexpr bool isValidPreRasterizationFrequency(const ShaderFrequency frequency) noexcept
{
    return isValidGraphicsFrequency(frequency) && (frequency != ShaderFrequency::Pixel);
}

/// @brief Checks whether `frequency` executes on compute or ray tracing shader pipelines.
/// @param[in] frequency Shader stage to test.
/// @return True for Compute and ray tracing stages.
[[nodiscard]] inline constexpr bool isComputeShaderFrequency(const ShaderFrequency frequency) noexcept
{
    switch (frequency)
    {
    case ShaderFrequency::Compute:
    case ShaderFrequency::RayGen:
    case ShaderFrequency::RayMiss:
    case ShaderFrequency::RayHitGroup:
    case ShaderFrequency::RayCallable:
        return true;

    default:
        break;
    }

    return false;
}

/// @brief Checks whether `frequency` belongs to the mesh-shading geometry pipeline.
/// @param[in] frequency Shader stage to test.
/// @return True for Mesh and Amplification stages.
[[nodiscard]] inline constexpr bool isGeometryPipelineShaderFrequency(const ShaderFrequency frequency) noexcept
{
    return (frequency == ShaderFrequency::Mesh) || (frequency == ShaderFrequency::Amplification);
}

/// @brief Checks whether `frequency` is a ray tracing stage.
/// @param[in] frequency Shader stage to test.
/// @return True for RayGen, RayMiss, RayHitGroup, or RayCallable stages.
[[nodiscard]] inline constexpr bool isRayTracingShaderFrequency(const ShaderFrequency frequency) noexcept
{
    switch (frequency)
    {
    case ShaderFrequency::RayGen:
    case ShaderFrequency::RayMiss:
    case ShaderFrequency::RayHitGroup:
    case ShaderFrequency::RayCallable:
        return true;

    default:
        break;
    }

    return false;
}

/// @brief Checks whether `frequency` is a work graph stage.
/// @param[in] frequency Shader stage to test.
/// @return True for WorkGraphRoot and WorkGraphComputeNode stages.
[[nodiscard]] inline constexpr bool isWorkGraphShaderFrequency(const ShaderFrequency frequency) noexcept
{
    return (frequency == ShaderFrequency::WorkGraphRoot) || (frequency == ShaderFrequency::WorkGraphComputeNode);
}

/// @brief Checks whether a shader resource binding type represents a read-only shader resource view (SRV).
/// @param[in] type Reflected resource binding kind.
/// @return True for read-only texture, buffer, acceleration structure, and resource collection views.
[[nodiscard]] inline constexpr bool isResourceBindingTypeSRV(const ShaderCodeResourceBindingType type) noexcept
{
    switch (type)
    {
    case ShaderCodeResourceBindingType::Texture2D:
    case ShaderCodeResourceBindingType::Texture2DArray:
    case ShaderCodeResourceBindingType::Texture2DMultiView:
    case ShaderCodeResourceBindingType::Texture2DMS:
    case ShaderCodeResourceBindingType::TextureCube:
    case ShaderCodeResourceBindingType::TextureCubeArray:
    case ShaderCodeResourceBindingType::Texture3D:
    case ShaderCodeResourceBindingType::ByteAddressBuffer:
    case ShaderCodeResourceBindingType::StructuredBuffer:
    case ShaderCodeResourceBindingType::Buffer:
    case ShaderCodeResourceBindingType::RaytracingAccelerationStructure:
    case ShaderCodeResourceBindingType::ResourceCollection:
        return true;

    case ShaderCodeResourceBindingType::RWTexture2D:
    case ShaderCodeResourceBindingType::RWTexture2DArray:
    case ShaderCodeResourceBindingType::RWTexture2DMultiView:
    case ShaderCodeResourceBindingType::RWTextureCube:
    case ShaderCodeResourceBindingType::RWTexture3D:
    case ShaderCodeResourceBindingType::RWByteAddressBuffer:
    case ShaderCodeResourceBindingType::RWStructuredBuffer:
    case ShaderCodeResourceBindingType::RWBuffer:
    case ShaderCodeResourceBindingType::RasterizerOrderedTexture2D:
    case ShaderCodeResourceBindingType::SamplerState:
    case ShaderCodeResourceBindingType::TextureMetadata:
    case ShaderCodeResourceBindingType::RWTextureMetadata:
    case ShaderCodeResourceBindingType::DescriptorRange:
    case ShaderCodeResourceBindingType::Invalid:
    default:
        break;
    }

    return false;
}

/// @brief Checks whether a uniform buffer member type represents a read-only RHI resource.
/// @param[in] baseType Uniform buffer parameter base type.
/// @return True for textures, SRVs, samplers, resource collections, descriptor ranges, and uniform blocks.
[[nodiscard]] inline constexpr bool
    isShaderParameterTypeReadOnlyRHIResource(const UniformBufferBaseType baseType) noexcept
{
    switch (baseType)
    {
    case UniformBufferBaseType::Texture:
    case UniformBufferBaseType::SRV:
    case UniformBufferBaseType::Sampler:
    case UniformBufferBaseType::ResourceCollection:
    case UniformBufferBaseType::DescriptorRange:
    case UniformBufferBaseType::UniformBlock:
        return true;

    default:
        break;
    }

    return false;
}

/// @brief Checks whether a uniform buffer member type binds a buffer view as a uniform block.
/// @param[in] baseType Uniform buffer parameter base type.
/// @return True when `baseType` is `UniformBufferBaseType::UniformBlock`.
[[nodiscard]] inline constexpr bool isShaderParameterTypeUniformBlock(const UniformBufferBaseType baseType) noexcept
{
    return baseType == UniformBufferBaseType::UniformBlock;
}

/// @brief Checks whether a uniform buffer member type references any RHI resource (read-only or UAV).
/// @param[in] baseType Uniform buffer parameter base type.
/// @return True when `baseType` is a read-only resource or a UAV.
[[nodiscard]] inline constexpr bool isShaderParameterTypeRHIResource(const UniformBufferBaseType baseType) noexcept
{
    return isShaderParameterTypeReadOnlyRHIResource(baseType) || (baseType == UniformBufferBaseType::UAV);
}

/// @brief Checks whether a member type must be recorded in `UniformBufferLayout` during creation.
/// @param[in] baseType Uniform buffer parameter base type.
/// @return True when the parameter participates in uniform buffer layout metadata.
[[nodiscard]] inline constexpr bool
    isShaderParameterTypeForUniformBufferLayout(const UniformBufferBaseType baseType) noexcept
{
    return isShaderParameterTypeRHIResource(baseType) || (baseType == UniformBufferBaseType::ReferencedStruct) ||
           (baseType == UniformBufferBaseType::RenderTargetBindingSlots) || isShaderParameterTypeUniformBlock(baseType);
}

/// @brief Checks whether a `UniformBufferLayout` entry is ignored by the RHI backend itself.
/// @param[in] baseType Uniform buffer parameter base type.
/// @return True for metadata entries consumed only by higher-level graph or binding layers.
[[nodiscard]] inline constexpr bool isShaderParameterTypeIgnoredByRHI(const UniformBufferBaseType baseType) noexcept
{
    return (baseType == UniformBufferBaseType::RenderTargetBindingSlots) ||
           (baseType == UniformBufferBaseType::ReferencedStruct);
}

}   // namespace gp::rhi::shader

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::shader::UniformBufferBindingFlags);
