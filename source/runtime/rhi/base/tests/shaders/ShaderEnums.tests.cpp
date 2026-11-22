// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/ShaderEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::shader::tests
{

static_assert(kShaderFrequencyCount == 12u, "ShaderFrequency count must be 12.");
static_assert(kGraphicsShaderFrequencyCount == 5u, "Graphics shader frequency count must be 5.");
static_assert(kStandardShaderFrequencyCount == 6u, "Standard shader frequency count must be 6.");

static_assert(isValidGraphicsFrequency(ShaderFrequency::Vertex), "Vertex must be a graphics stage.");
static_assert(isValidGraphicsFrequency(ShaderFrequency::Pixel), "Pixel must be a graphics stage.");
static_assert(!isValidGraphicsFrequency(ShaderFrequency::Compute), "Compute is not a graphics stage.");
static_assert(isValidPreRasterizationFrequency(ShaderFrequency::Vertex), "Vertex must be pre-rasterization.");
static_assert(!isValidPreRasterizationFrequency(ShaderFrequency::Pixel), "Pixel is not pre-rasterization.");
static_assert(isComputeShaderFrequency(ShaderFrequency::Compute), "Compute must be a compute stage.");
static_assert(isGeometryPipelineShaderFrequency(ShaderFrequency::Mesh), "Mesh must be a geometry pipeline stage.");
static_assert(isRayTracingShaderFrequency(ShaderFrequency::RayGen), "RayGen must be a ray tracing stage.");
static_assert(isWorkGraphShaderFrequency(ShaderFrequency::WorkGraphRoot), "WorkGraphRoot must be a work graph stage.");

static_assert(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::Texture2D), "Texture2D must be SRV.");
static_assert(!isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWTexture2D), "RWTexture2D is not SRV.");
static_assert(isShaderParameterTypeUniformBlock(UniformBufferBaseType::UniformBlock), "UniformBlock check failed.");
static_assert(
    isShaderParameterTypeIgnoredByRHI(UniformBufferBaseType::ReferencedStruct),
    "ReferencedStruct must be ignored by RHI."
);

TEST(ShaderEnumsTest, GraphicsAndPreRasterizationFrequencies)
{
    EXPECT_TRUE(isValidGraphicsFrequency(ShaderFrequency::Vertex));
    EXPECT_TRUE(isValidGraphicsFrequency(ShaderFrequency::Mesh));
    EXPECT_TRUE(isValidGraphicsFrequency(ShaderFrequency::Amplification));
    EXPECT_TRUE(isValidGraphicsFrequency(ShaderFrequency::Pixel));
    EXPECT_TRUE(isValidGraphicsFrequency(ShaderFrequency::Geometry));

    EXPECT_FALSE(isValidGraphicsFrequency(ShaderFrequency::Compute));
    EXPECT_FALSE(isValidGraphicsFrequency(ShaderFrequency::RayGen));
    EXPECT_FALSE(isValidGraphicsFrequency(ShaderFrequency::RayMiss));
    EXPECT_FALSE(isValidGraphicsFrequency(ShaderFrequency::RayHitGroup));
    EXPECT_FALSE(isValidGraphicsFrequency(ShaderFrequency::RayCallable));
    EXPECT_FALSE(isValidGraphicsFrequency(ShaderFrequency::WorkGraphRoot));
    EXPECT_FALSE(isValidGraphicsFrequency(ShaderFrequency::WorkGraphComputeNode));

    EXPECT_TRUE(isValidPreRasterizationFrequency(ShaderFrequency::Vertex));
    EXPECT_TRUE(isValidPreRasterizationFrequency(ShaderFrequency::Mesh));
    EXPECT_TRUE(isValidPreRasterizationFrequency(ShaderFrequency::Amplification));
    EXPECT_TRUE(isValidPreRasterizationFrequency(ShaderFrequency::Geometry));
    EXPECT_FALSE(isValidPreRasterizationFrequency(ShaderFrequency::Pixel));
    EXPECT_FALSE(isValidPreRasterizationFrequency(ShaderFrequency::Compute));
}

TEST(ShaderEnumsTest, ComputeGeometryRayTracingAndWorkGraphFrequencies)
{
    // Compute + ray tracing stages
    EXPECT_TRUE(isComputeShaderFrequency(ShaderFrequency::Compute));
    EXPECT_TRUE(isComputeShaderFrequency(ShaderFrequency::RayGen));
    EXPECT_TRUE(isComputeShaderFrequency(ShaderFrequency::RayMiss));
    EXPECT_TRUE(isComputeShaderFrequency(ShaderFrequency::RayHitGroup));
    EXPECT_TRUE(isComputeShaderFrequency(ShaderFrequency::RayCallable));
    EXPECT_FALSE(isComputeShaderFrequency(ShaderFrequency::Vertex));
    EXPECT_FALSE(isComputeShaderFrequency(ShaderFrequency::Pixel));
    EXPECT_FALSE(isComputeShaderFrequency(ShaderFrequency::WorkGraphRoot));

    // Mesh-shading geometry pipeline stages
    EXPECT_TRUE(isGeometryPipelineShaderFrequency(ShaderFrequency::Mesh));
    EXPECT_TRUE(isGeometryPipelineShaderFrequency(ShaderFrequency::Amplification));
    EXPECT_FALSE(isGeometryPipelineShaderFrequency(ShaderFrequency::Vertex));
    EXPECT_FALSE(isGeometryPipelineShaderFrequency(ShaderFrequency::Geometry));
    EXPECT_FALSE(isGeometryPipelineShaderFrequency(ShaderFrequency::Pixel));

    // Ray tracing stages
    EXPECT_TRUE(isRayTracingShaderFrequency(ShaderFrequency::RayGen));
    EXPECT_TRUE(isRayTracingShaderFrequency(ShaderFrequency::RayMiss));
    EXPECT_TRUE(isRayTracingShaderFrequency(ShaderFrequency::RayHitGroup));
    EXPECT_TRUE(isRayTracingShaderFrequency(ShaderFrequency::RayCallable));
    EXPECT_FALSE(isRayTracingShaderFrequency(ShaderFrequency::Compute));
    EXPECT_FALSE(isRayTracingShaderFrequency(ShaderFrequency::Vertex));

    // Work graph stages
    EXPECT_TRUE(isWorkGraphShaderFrequency(ShaderFrequency::WorkGraphRoot));
    EXPECT_TRUE(isWorkGraphShaderFrequency(ShaderFrequency::WorkGraphComputeNode));
    EXPECT_FALSE(isWorkGraphShaderFrequency(ShaderFrequency::Compute));
    EXPECT_FALSE(isWorkGraphShaderFrequency(ShaderFrequency::RayGen));
}

TEST(ShaderEnumsTest, ResourceBindingTypeSRVClassification)
{
    // Read-only SRV binding types
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::Texture2D));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::Texture2DArray));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::Texture2DMultiView));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::Texture2DMS));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::TextureCube));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::TextureCubeArray));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::Texture3D));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::ByteAddressBuffer));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::StructuredBuffer));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::Buffer));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RaytracingAccelerationStructure));
    EXPECT_TRUE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::ResourceCollection));

    // Non-SRV binding types (UAVs, samplers, metadata, descriptor ranges, invalid)
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::Invalid));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::SamplerState));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::TextureMetadata));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWTextureMetadata));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::DescriptorRange));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWTexture2D));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWTexture2DArray));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWTexture2DMultiView));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWTextureCube));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWTexture3D));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWByteAddressBuffer));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWStructuredBuffer));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RWBuffer));
    EXPECT_FALSE(isResourceBindingTypeSRV(ShaderCodeResourceBindingType::RasterizerOrderedTexture2D));
}

TEST(ShaderEnumsTest, UniformBufferParameterTypePredicates)
{
    // Read-only RHI resources
    EXPECT_TRUE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::Texture));
    EXPECT_TRUE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::SRV));
    EXPECT_TRUE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::Sampler));
    EXPECT_TRUE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::ResourceCollection));
    EXPECT_TRUE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::DescriptorRange));
    EXPECT_TRUE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::UniformBlock));

    EXPECT_FALSE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::UAV));
    EXPECT_FALSE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::Float32));
    EXPECT_FALSE(isShaderParameterTypeReadOnlyRHIResource(UniformBufferBaseType::ReferencedStruct));

    // Uniform block check
    EXPECT_TRUE(isShaderParameterTypeUniformBlock(UniformBufferBaseType::UniformBlock));
    EXPECT_FALSE(isShaderParameterTypeUniformBlock(UniformBufferBaseType::SRV));
    EXPECT_FALSE(isShaderParameterTypeUniformBlock(UniformBufferBaseType::ReferencedStruct));

    // Any RHI resource (read-only + UAV)
    EXPECT_TRUE(isShaderParameterTypeRHIResource(UniformBufferBaseType::Texture));
    EXPECT_TRUE(isShaderParameterTypeRHIResource(UniformBufferBaseType::UAV));
    EXPECT_TRUE(isShaderParameterTypeRHIResource(UniformBufferBaseType::UniformBlock));
    EXPECT_FALSE(isShaderParameterTypeRHIResource(UniformBufferBaseType::Int32));
    EXPECT_FALSE(isShaderParameterTypeRHIResource(UniformBufferBaseType::UInt32));
    EXPECT_FALSE(isShaderParameterTypeRHIResource(UniformBufferBaseType::Float32));
    EXPECT_FALSE(isShaderParameterTypeRHIResource(UniformBufferBaseType::Bool));
    EXPECT_FALSE(isShaderParameterTypeRHIResource(UniformBufferBaseType::NestedStruct));
    EXPECT_FALSE(isShaderParameterTypeRHIResource(UniformBufferBaseType::IncludedStruct));
    EXPECT_FALSE(isShaderParameterTypeRHIResource(UniformBufferBaseType::ReferencedStruct));
    EXPECT_FALSE(isShaderParameterTypeRHIResource(UniformBufferBaseType::RenderTargetBindingSlots));

    // Recorded in UniformBufferLayout
    EXPECT_TRUE(isShaderParameterTypeForUniformBufferLayout(UniformBufferBaseType::Texture));
    EXPECT_TRUE(isShaderParameterTypeForUniformBufferLayout(UniformBufferBaseType::UAV));
    EXPECT_TRUE(isShaderParameterTypeForUniformBufferLayout(UniformBufferBaseType::UniformBlock));
    EXPECT_TRUE(isShaderParameterTypeForUniformBufferLayout(UniformBufferBaseType::ReferencedStruct));
    EXPECT_TRUE(isShaderParameterTypeForUniformBufferLayout(UniformBufferBaseType::RenderTargetBindingSlots));
    EXPECT_FALSE(isShaderParameterTypeForUniformBufferLayout(UniformBufferBaseType::Float32));
    EXPECT_FALSE(isShaderParameterTypeForUniformBufferLayout(UniformBufferBaseType::NestedStruct));
    EXPECT_FALSE(isShaderParameterTypeForUniformBufferLayout(UniformBufferBaseType::IncludedStruct));

    // Ignored by RHI backend
    EXPECT_TRUE(isShaderParameterTypeIgnoredByRHI(UniformBufferBaseType::RenderTargetBindingSlots));
    EXPECT_TRUE(isShaderParameterTypeIgnoredByRHI(UniformBufferBaseType::ReferencedStruct));
    EXPECT_FALSE(isShaderParameterTypeIgnoredByRHI(UniformBufferBaseType::Texture));
    EXPECT_FALSE(isShaderParameterTypeIgnoredByRHI(UniformBufferBaseType::UniformBlock));
    EXPECT_FALSE(isShaderParameterTypeIgnoredByRHI(UniformBufferBaseType::Float32));
}

TEST(ShaderEnumsTest, UniformBufferBindingFlagsBitwiseOperations)
{
    EXPECT_EQ(
        UniformBufferBindingFlags::Static | UniformBufferBindingFlags::Shader,
        UniformBufferBindingFlags::StaticAndShader
    );
    EXPECT_EQ(
        UniformBufferBindingFlags::StaticAndShader & UniformBufferBindingFlags::Static,
        UniformBufferBindingFlags::Static
    );
    EXPECT_EQ(
        UniformBufferBindingFlags::StaticAndShader & UniformBufferBindingFlags::Shader,
        UniformBufferBindingFlags::Shader
    );
}

}   // namespace gp::rhi::shader::tests
