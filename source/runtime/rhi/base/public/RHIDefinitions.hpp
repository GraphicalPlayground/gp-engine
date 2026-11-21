// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/LogMacros.hpp"
#include "RHIBase.hpp"
#include "RHIEnums.hpp"

namespace gp
{

/// @brief Built-in log category for `gp::rhi` diagnostics.
GP_DECLARE_EXPORTED_LOG_CATEGORY_EXTERN(GP_RHI_API, LogRHI);

}   // namespace gp

namespace gp::rhi
{

/// @brief Required byte alignment for shader parameter structs.
inline constexpr gp::UInt32 kShaderParameterStructAlignment = 16u;

/// @brief Byte alignment between consecutive elements of array shader parameters.
inline constexpr gp::UInt32 kShaderArrayElementAlignBytes = 16u;

/// @brief Byte alignment required for raw RHI buffer views.
inline constexpr gp::UInt32 kRawViewAlignment = 16u;

/// @brief Minimum byte alignment enforced on pointer fields in uniform buffer structs across C++ and shaders.
inline constexpr gp::UInt32 kShaderParameterPointerAlignment = sizeof(gp::UInt64);
static_assert(
    sizeof(void*) <= kShaderParameterPointerAlignment, "Pointer size exceeds shader parameter pointer alignment."
);

/// @brief Maximum number of mip levels supported by an RHI texture.
inline constexpr gp::UInt32 kMaxTextureMipCount = 15u;

/// @brief Maximum number of LOD levels supported for static and skeletal meshes.
inline constexpr gp::UInt32 kMaxMeshLodCount = 8u;

/// @brief Maximum number of vertex attributes in a single vertex declaration.
inline constexpr gp::UInt32 kMaxVertexElementCount = 16u;

/// @brief Maximum number of color render targets bound simultaneously.
inline constexpr gp::UInt32 kMaxSimultaneousRenderTargets = 8u;

/// @brief Maximum number of global static uniform buffer binding slots.
inline constexpr gp::UInt8 kMaxUniformBufferStaticSlots = 255u;

/// @brief Converts a raw PCI vendor ID into a recognized `GpuVendorId`.
/// @param[in] vendorId Raw 32-bit PCI vendor identifier reported by the driver or OS.
/// @return Matching `GpuVendorId`, `GpuVendorId::NotQueried` if zero, or `GpuVendorId::Unknown` if unrecognized.
[[nodiscard]] inline constexpr GpuVendorId convertToGpuVendorId(const gp::UInt32 vendorId) noexcept
{
    switch (static_cast<GpuVendorId>(vendorId))
    {
    case GpuVendorId::NotQueried:
        return GpuVendorId::NotQueried;

    case GpuVendorId::Amd:
    case GpuVendorId::Kazan:
    case GpuVendorId::Codeplay:
    case GpuVendorId::Mesa:
    case GpuVendorId::ImgTec:
    case GpuVendorId::Nvidia:
    case GpuVendorId::Arm:
    case GpuVendorId::Broadcom:
    case GpuVendorId::Qualcomm:
    case GpuVendorId::Intel:
    case GpuVendorId::Apple:
    case GpuVendorId::Vivante:
    case GpuVendorId::VeriSilicon:
    case GpuVendorId::SamsungAMD:
    case GpuVendorId::Microsoft:
        return static_cast<GpuVendorId>(vendorId);

    case GpuVendorId::Unknown:
    default:
        return GpuVendorId::Unknown;
    }
}

/// @brief Checks whether bindless descriptor indexing is disabled.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when bindless is disabled.
[[nodiscard]] inline constexpr bool isBindlessDisabled(const BindlessConfiguration configuration) noexcept
{
    return configuration == BindlessConfiguration::Disabled;
}

/// @brief Checks whether bindless is restricted strictly to ray tracing shaders.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when only ray tracing shaders use bindless indexing.
[[nodiscard]] inline constexpr bool
    isBindlessEnabledForOnlyRayTracing(const BindlessConfiguration configuration) noexcept
{
    return configuration == BindlessConfiguration::RayTracing;
}

/// @brief Checks whether ray tracing shaders can use bindless descriptor indexing.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when bindless is enabled for ray tracing or broader scopes.
[[nodiscard]] inline constexpr bool isBindlessEnabledForRayTracing(const BindlessConfiguration configuration) noexcept
{
    return configuration >= BindlessConfiguration::RayTracing;
}

/// @brief Checks whether graphics shaders can use bindless descriptor indexing.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when bindless is enabled for minimal or full shader scopes.
[[nodiscard]] inline constexpr bool isBindlessEnabledForAnyGraphics(const BindlessConfiguration configuration) noexcept
{
    return configuration >= BindlessConfiguration::Minimal;
}

/// @brief Checks whether all shader stages can use bindless descriptor indexing.
/// @param[in] configuration Active bindless configuration tier.
/// @return True when bindless is fully enabled.
[[nodiscard]] inline constexpr bool isBindlessFullyEnabled(const BindlessConfiguration configuration) noexcept
{
    return configuration == BindlessConfiguration::All;
}

namespace resource
{

/// @brief Number of faces on a cubemap texture.
inline constexpr gp::UInt32 kCubeFaceCount = 6u;

/// @brief Index type for global static uniform buffer slots.
using UniformBufferStaticSlot = gp::UInt8;

/// @brief Checks whether a static uniform buffer slot index is within valid bounds.
/// @param[in] slot Static slot index to test.
/// @return True when `slot` is less than `kMaxUniformBufferStaticSlots`.
[[nodiscard]] inline constexpr bool isUniformBufferStaticSlotValid(const UniformBufferStaticSlot slot) noexcept
{
    return slot < kMaxUniformBufferStaticSlots;
}

}   // namespace resource

namespace view
{

/// @brief Converts a single `DescriptorType` into its corresponding `DescriptorTypeMask` bit.
/// @param[in] type Descriptor type to convert.
/// @return Single-bit mask for `type`, or `DescriptorTypeMask::None` for `DescriptorType::Invalid`.
[[nodiscard]] inline constexpr DescriptorTypeMask descriptorTypeMaskFromType(const DescriptorType type) noexcept
{
    if (type == DescriptorType::Invalid)
    {
        return DescriptorTypeMask::None;
    }

    return static_cast<DescriptorTypeMask>(1u << static_cast<gp::UInt16>(type));
}

}   // namespace view

namespace shader
{

/// @brief Total number of enumerators in `ShaderFrequency`.
inline constexpr gp::UInt8 kShaderFrequencyCount = 12u;

/// @brief Number of graphics pipeline stages in `ShaderFrequency` (Vertex through Geometry).
inline constexpr gp::UInt8 kGraphicsShaderFrequencyCount = 5u;

/// @brief Number of standard rasterization and compute stages in `ShaderFrequency` (Vertex through Compute).
inline constexpr gp::UInt8 kStandardShaderFrequencyCount = 6u;

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

}   // namespace shader

}   // namespace gp::rhi
