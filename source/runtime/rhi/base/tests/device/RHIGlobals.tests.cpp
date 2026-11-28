// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIGlobals.hpp"
#include "device/DeviceEnums.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "views/ViewEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::tests
{

TEST(RHIGlobalsTest, RHIGlobalPreviewOverrideClamping)
{
    RHIGlobal<bool> featureFlag(true);
    EXPECT_TRUE(static_cast<bool>(featureFlag));
    EXPECT_FALSE(featureFlag.hasPreviewOverride());
    EXPECT_TRUE(featureFlag.getHardwareValue());

    // Preview false clamps true hardware capability down to false; hardware false stays false even with preview true.
    featureFlag.setPreviewOverride(false);
    EXPECT_TRUE(featureFlag.hasPreviewOverride());
    EXPECT_TRUE(featureFlag.getHardwareValue());
    EXPECT_FALSE(featureFlag.getValue());

    featureFlag = false;
    featureFlag.setPreviewOverride(true);
    EXPECT_FALSE(featureFlag.getValue());

    featureFlag.clearPreviewOverride();
    featureFlag = true;
    EXPECT_FALSE(featureFlag.hasPreviewOverride());
    EXPECT_TRUE(featureFlag.getValue());

    // Integral RHIGlobal clamps via min(previewValue, hardwareValue).
    RHIGlobal<gp::UInt32> maxDim(16384u);
    EXPECT_EQ(static_cast<gp::UInt32>(maxDim), 16384u);
    maxDim.setPreviewOverride(4096u);
    EXPECT_EQ(maxDim.getValue(), 4096u);
    EXPECT_EQ(maxDim.getHardwareValue(), 16384u);
    maxDim.setPreviewOverride(32768u);
    EXPECT_EQ(maxDim.getValue(), 16384u);

    // Non-boolean, non-integral enum RHIGlobal returns previewValue when overridden.
    RHIGlobal<FeatureLevel> maxFL(FeatureLevel::SM6);
    maxFL.setPreviewOverride(FeatureLevel::ES3_1);
    EXPECT_EQ(maxFL.getValue(), FeatureLevel::ES3_1);
    maxFL.clearPreviewOverride();
    EXPECT_EQ(maxFL.getValue(), FeatureLevel::SM6);
}

TEST(RHIGlobalsTest, DefaultMSAASampleOffsetsAndCapabilityReset)
{
    struct ScopedGlobalsReset final
    {
        ~ScopedGlobalsReset() noexcept
        {
            getRHIGlobals().reset();
        }
    } const resetGuard{};

    RHIGlobals& globals = getRHIGlobals();
    globals.reset();

    EXPECT_EQ(calculateMSAASampleArrayIndex(1u, 0u), 0u);
    EXPECT_EQ(calculateMSAASampleArrayIndex(2u, 0u), 1u);
    EXPECT_EQ(calculateMSAASampleArrayIndex(4u, 3u), 6u);
    EXPECT_EQ(calculateMSAASampleArrayIndex(8u, 7u), 14u);
    EXPECT_EQ(calculateMSAASampleArrayIndex(16u, 15u), 30u);

    // 1x MSAA sample 0 sits at pixel center (0, 0); 2x MSAA samples sit symmetrically at (+0.5, +0.5) and (-0.5, -0.5)
    // in [-1, 1] space.
    const gp::math::Vector2<gp::Float32> center1x = getMSAASampleOffsets(1u, 0u);
    EXPECT_FLOAT_EQ(center1x.x, 0.0f);
    EXPECT_FLOAT_EQ(center1x.y, 0.0f);

    const gp::math::Vector2<gp::Float32> msaa2x0 = getMSAASampleOffsets(2u, 0u);
    const gp::math::Vector2<gp::Float32> msaa2x1 = getMSAASampleOffsets(2u, 1u);
    EXPECT_FLOAT_EQ(msaa2x0.x, 0.5f);
    EXPECT_FLOAT_EQ(msaa2x0.y, 0.5f);
    EXPECT_FLOAT_EQ(msaa2x1.x, -0.5f);
    EXPECT_FLOAT_EQ(msaa2x1.y, -0.5f);

    // Mutate memory counters, capability structs, and descriptor groups, then verify reset() restores clean defaults.
    globals.streamingTextureMemorySizeInKB = 4096u;
    globals.nonStreamingTextureMemorySizeInKB = 8192u;
    globals.texturePoolSize = 512ll * 1024ll * 1024ll;
    globals.poolSizeVRAMPercentage = 70;
    globals.demotedLocalMemorySize = 65536u;
    globals.bufferMemorySize = 1048576u;
    globals.uniformBufferMemorySize = 131072u;
    globals.descriptorTypeGroups.emplaceBack(view::DescriptorTypeMask::Standard);
    globals.rayTracing.supported = true;
    globals.rayTracing.instanceDescriptorSize = 64u;
    globals.variableRateShading.supportsAttachment = true;
    globals.variableRateShading.imageDataType = pipeline::VRSImageDataType::Palette;
    globals.shaderBundles.supportsWorkGraphs = true;
    globals.reservedResources.supported = true;
    globals.reservedResources.virtualSize = 16ll * 1024ll * 1024ll;
    globals.parallelRenderPasses.supported = true;
    globals.setPixelFormatCapabilities(PixelFormat::R8G8B8A8_UNorm, PixelFormatCapabilities::RenderTarget);

    globals.reset();

    EXPECT_EQ(globals.streamingTextureMemorySizeInKB, 0u);
    EXPECT_EQ(globals.nonStreamingTextureMemorySizeInKB, 0u);
    EXPECT_EQ(globals.texturePoolSize, 0);
    EXPECT_EQ(globals.poolSizeVRAMPercentage, 0);
    EXPECT_EQ(globals.demotedLocalMemorySize, 0u);
    EXPECT_EQ(globals.bufferMemorySize, 0u);
    EXPECT_EQ(globals.uniformBufferMemorySize, 0u);
    EXPECT_TRUE(globals.descriptorTypeGroups.isEmpty());
    EXPECT_FALSE(globals.rayTracing.supported);
    EXPECT_EQ(globals.rayTracing.instanceDescriptorSize, 0u);
    EXPECT_FALSE(globals.variableRateShading.supportsAttachment);
    EXPECT_EQ(globals.variableRateShading.imageDataType, pipeline::VRSImageDataType::NotSupported);
    EXPECT_FALSE(globals.shaderBundles.supportsWorkGraphs);
    EXPECT_FALSE(globals.reservedResources.supported);
    EXPECT_EQ(globals.reservedResources.virtualSize, 0);
    EXPECT_FALSE(globals.parallelRenderPasses.supported);
    EXPECT_EQ(globals.getPixelFormatCapabilities(PixelFormat::R8G8B8A8_UNorm), PixelFormatCapabilities::None);
    EXPECT_FLOAT_EQ(getMSAASampleOffsets(2u, 0u).x, 0.5f);
}

}   // namespace gp::rhi::tests
