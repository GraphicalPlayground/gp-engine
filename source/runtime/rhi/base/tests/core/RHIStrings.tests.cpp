// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/RHIStrings.hpp"
#include "containers/strings/Name.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "device/DeviceEnums.hpp"
#include "device/RHIGlobals.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/ShaderEnums.hpp"
#include "sync/SyncEnums.hpp"
#include "views/ViewEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::tests
{

TEST(RHIStringsTest, VendorAndInterfaceTypeFormatting)
{
    struct ScopedGlobalsReset final
    {
        ~ScopedGlobalsReset() noexcept
        {
            getRHIGlobals().reset();
        }
    } const resetGuard{};

    getRHIGlobals().reset();
    EXPECT_STREQ(rhiVendorIdToString(), "Not Queried");

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Nvidia;
    EXPECT_STREQ(rhiVendorIdToString(), "NVIDIA");

    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::Amd), "AMD");
    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::Intel), "Intel");
    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::Apple), "Apple");
    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::Arm), "ARM");
    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::Qualcomm), "Qualcomm");
    EXPECT_STREQ(rhiVendorIdToString(static_cast<GpuVendorId>(kQualcommAlternateVendorId)), "Qualcomm");
    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::SamsungAMD), "SamsungAMD");
    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::Microsoft), "Microsoft");
    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::Mesa), "Mesa");
    EXPECT_STREQ(rhiVendorIdToString(GpuVendorId::Unknown), "Unknown");

    EXPECT_STREQ(getInterfaceTypeString(InterfaceType::NotSet), "NotSet");
    EXPECT_STREQ(getInterfaceTypeString(InterfaceType::Null), "Null");
    EXPECT_STREQ(getInterfaceTypeString(InterfaceType::D3D12), "D3D12");
    EXPECT_STREQ(getInterfaceTypeString(InterfaceType::Vulkan), "Vulkan");
    EXPECT_STREQ(getInterfaceTypeString(InterfaceType::Metal), "Metal");
    EXPECT_STREQ(getInterfaceTypeString(static_cast<InterfaceType>(250u)), "Unknown");
}

TEST(RHIStringsTest, FeatureLevelShadingPathAndBindlessRoundTrip)
{
    EXPECT_EQ(featureLevelToString(FeatureLevel::ES2Removed), gp::String("ES2_REMOVED"));
    EXPECT_EQ(featureLevelToString(FeatureLevel::ES3_1), gp::String("ES3_1"));
    EXPECT_EQ(featureLevelToString(FeatureLevel::SM4Removed), gp::String("SM4_REMOVED"));
    EXPECT_EQ(featureLevelToString(FeatureLevel::SM5), gp::String("SM5"));
    EXPECT_EQ(featureLevelToString(FeatureLevel::SM6), gp::String("SM6"));
    EXPECT_EQ(featureLevelToString(FeatureLevel::Count), gp::String("UnknownFeatureLevel"));

    gp::String flStr{};
    gp::Name flName{};
    getFeatureLevelName(FeatureLevel::SM6, flStr);
    getFeatureLevelName(FeatureLevel::SM6, flName);
    EXPECT_EQ(flStr, gp::String("SM6"));
    EXPECT_EQ(flName, gp::Name("SM6"));

    FeatureLevel parsedFL = FeatureLevel::Count;
    EXPECT_TRUE(getFeatureLevelFromName("es3_1", parsedFL));
    EXPECT_EQ(parsedFL, FeatureLevel::ES3_1);
    EXPECT_TRUE(getFeatureLevelFromName(gp::Name("SM5"), parsedFL));
    EXPECT_EQ(parsedFL, FeatureLevel::SM5);
    EXPECT_FALSE(getFeatureLevelFromName("SM99", parsedFL));
    EXPECT_EQ(parsedFL, FeatureLevel::Count);

    getFeatureLevelName(FeatureLevel::Count, flStr);
    EXPECT_EQ(flStr, gp::String("InvalidFeatureLevel"));

    gp::String spStr{};
    gp::Name spName{};
    getShadingPathName(ShadingPath::Deferred, spStr);
    getShadingPathName(ShadingPath::Mobile, spName);
    EXPECT_EQ(spStr, gp::String("Deferred"));
    EXPECT_EQ(spName, gp::Name("Mobile"));
    EXPECT_STREQ(getShadingPathString(static_cast<ShadingPath>(99u)), "InvalidShadingPath");

    ShadingPath parsedSP = ShadingPath::Deferred;
    EXPECT_TRUE(getShadingPathFromName("forward", parsedSP));
    EXPECT_EQ(parsedSP, ShadingPath::Forward);
    EXPECT_TRUE(getShadingPathFromName(gp::Name("Mobile"), parsedSP));
    EXPECT_EQ(parsedSP, ShadingPath::Mobile);
    EXPECT_FALSE(getShadingPathFromName("Clustered", parsedSP));

    view::BindlessConfiguration bindlessCfg = view::BindlessConfiguration::Disabled;
    EXPECT_STREQ(getBindlessConfigurationString(view::BindlessConfiguration::RayTracing), "RayTracing");
    EXPECT_TRUE(getBindlessConfigurationFromString("minimal", bindlessCfg));
    EXPECT_EQ(bindlessCfg, view::BindlessConfiguration::Minimal);
    EXPECT_TRUE(getBindlessConfigurationFromString("ALL", bindlessCfg));
    EXPECT_EQ(bindlessCfg, view::BindlessConfiguration::All);
    EXPECT_FALSE(getBindlessConfigurationFromString("InvalidConfig", bindlessCfg));
}

TEST(RHIStringsTest, DescriptorResourceBufferAndTextureFormatting)
{
    EXPECT_STREQ(getDescriptorHeapTypeString(view::DescriptorHeapType::Standard), "Standard");
    EXPECT_STREQ(getDescriptorHeapTypeString(view::DescriptorHeapType::Sampler), "Sampler");
    EXPECT_STREQ(getDescriptorHeapTypeString(view::DescriptorHeapType::Invalid), "UnknownDescriptorHeapType");

    EXPECT_EQ(getDescriptorTypeMaskString(view::DescriptorTypeMask::None), gp::String("<empty>"));
    EXPECT_EQ(
        getDescriptorTypeMaskString(view::DescriptorTypeMask::BufferSRV | view::DescriptorTypeMask::CBV),
        gp::String("BufferSRV|CBV")
    );

    // Every ResourceType enumerator must round-trip cleanly through stringFromRHIResourceType /
    // rhiResourceTypeFromString.
    for (gp::USize idx = 0u; idx < resource::kResourceTypeCount; ++idx)
    {
        const resource::ResourceType rt = static_cast<resource::ResourceType>(idx);
        const char* const name = stringFromRHIResourceType(rt);
        ASSERT_NE(name, nullptr);
        EXPECT_STRNE(name, "<unknown>");
        EXPECT_EQ(rhiResourceTypeFromString(name), rt);
    }
    EXPECT_STREQ(stringFromRHIResourceType(static_cast<resource::ResourceType>(250u)), "<unknown>");
    EXPECT_EQ(rhiResourceTypeFromString("NotAResourceType"), resource::ResourceType::None);

    EXPECT_EQ(getBufferUsageFlagsName(resource::BufferUsageFlags::None), gp::String("BUF_None"));
    EXPECT_EQ(
        getBufferUsageFlagsName(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::ShaderResource),
        gp::String("BUF_ShaderResource|BUF_VertexBuffer")
    );
    // RayTracingScratch sets both UnorderedAccess (bit 3) and bit 19; neither bit should format as BUF_Unknown.
    EXPECT_EQ(
        getBufferUsageFlagsName(resource::BufferUsageFlags::RayTracingScratch),
        gp::String("BUF_UnorderedAccess|BUF_RayTracingScratch")
    );
    EXPECT_STREQ(getBufferUsageFlagString(resource::BufferUsageFlags::StructuredBuffer), "StructuredBuffer");

    EXPECT_STREQ(getTextureDimensionString(resource::TextureDimension::TextureCubeArray), "TextureCubeArray");
    EXPECT_STREQ(getTextureDimensionString(static_cast<resource::TextureDimension>(99u)), "");
    EXPECT_EQ(getTextureCreateFlagsName(resource::TextureCreateFlags::None), gp::String("None"));
    EXPECT_EQ(
        getTextureCreateFlagsName(
            resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ShaderResource |
            resource::TextureCreateFlags::ReservedResource
        ),
        gp::String("RenderTargetable|ShaderResource|ReservedResource")
    );
}

TEST(RHIStringsTest, SyncPipelineFormatAndShaderBindingStrings)
{
    EXPECT_EQ(getRHIAccessName(sync::ResourceAccess::Unknown), gp::String("Unknown"));
    EXPECT_EQ(getRHIAccessName(sync::ResourceAccess::SRVGraphics), gp::String("SRVGraphicsPixel|SRVGraphicsNonPixel"));

    EXPECT_EQ(getResourceTransitionFlagsName(sync::ResourceTransitionFlags::None), gp::String("None"));
    EXPECT_EQ(
        getResourceTransitionFlagsName(sync::ResourceTransitionFlags::Mask),
        gp::String("MaintainCompression|IgnoreAfterState")
    );

    EXPECT_EQ(getRHIPipelineName(sync::PipelineStage::None), gp::String("None"));
    EXPECT_EQ(getRHIPipelineName(sync::PipelineStage::All), gp::String("Graphics|AsyncCompute"));

    EXPECT_STREQ(getRenderQueryTypeString(sync::RenderQueryType::Occlusion), "GPUOcclusion");
    EXPECT_EQ(getRenderQueryTypeName(sync::RenderQueryType::AbsoluteTime), gp::Name("GPUAbsoluteTime"));

    EXPECT_STREQ(getVertexElementTypeString(pipeline::VertexElementType::Float4), "Float4");
    EXPECT_STREQ(getVertexElementTypeString(static_cast<pipeline::VertexElementType>(200u)), "Unknown");
    EXPECT_STREQ(getPrimitiveTypeString(pipeline::PrimitiveType::TriangleStrip), "TriangleStrip");
    EXPECT_STREQ(getPrimitiveTypeString(static_cast<pipeline::PrimitiveType>(200u)), "Unknown");

    EXPECT_STREQ(getPixelFormatString(PixelFormat::R8G8B8A8_UNorm), "R8G8B8A8_UNorm");
    EXPECT_EQ(getPixelFormatCapabilitiesName(PixelFormatCapabilities::None), gp::String("None"));
    EXPECT_EQ(
        getPixelFormatCapabilitiesName(PixelFormatCapabilities::Texture2D | PixelFormatCapabilities::RenderTarget),
        gp::String("Texture2D|RenderTarget")
    );

    EXPECT_STREQ(getUniformBufferBaseTypeString(shader::UniformBufferBaseType::UniformBlock), "UBMT_UNIFORM_BLOCK");
    EXPECT_STREQ(
        getShaderCodeResourceBindingTypeName(shader::ShaderCodeResourceBindingType::RaytracingAccelerationStructure),
        "RaytracingAccelerationStructure"
    );
    EXPECT_STREQ(getShaderFrequencyString(shader::ShaderFrequency::Compute, true), "SF_Compute");
    EXPECT_STREQ(getShaderFrequencyString(shader::ShaderFrequency::Compute, false), "Compute");
}

}   // namespace gp::rhi::tests
