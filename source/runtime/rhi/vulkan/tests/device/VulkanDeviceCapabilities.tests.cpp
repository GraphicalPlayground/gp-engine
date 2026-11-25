// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanDeviceCapabilities.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/RHIGlobals.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanPhysicalDeviceFeatures.hpp"
#include "device/VulkanPhysicalDeviceProperties.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanDeviceCapabilitiesTest, ChooseVariableRateShadingMethodResolvesPreferences)
{
    // Both FSR and FDM available, PreferFSR -> disables FDM, enables FSR.
    {
        OptionalVulkanDeviceExtensions extFlags{};
        extFlags.hasKHRFragmentShadingRate = 1u;
        extFlags.hasEXTFragmentDensityMap = 1u;
        extFlags.hasEXTFragmentDensityMap2 = 1u;

        OptionalVulkanDeviceExtensionProperties extProps{};
        extProps.fragmentShadingRateFeatures.attachmentFragmentShadingRate = VK_TRUE;
        extProps.fragmentShadingRateFeatures.pipelineFragmentShadingRate = VK_TRUE;
        extProps.fragmentDensityMapFeatures.fragmentDensityMap = VK_TRUE;

        gp::rhi::VariableRateShadingCapabilities vrsCaps{};
        chooseVariableRateShadingMethod(extFlags, extProps, VulkanVariableRateShadingPreference::PreferFSR, vrsCaps);

        EXPECT_EQ(extFlags.hasKHRFragmentShadingRate, 1u);
        EXPECT_EQ(extFlags.hasEXTFragmentDensityMap, 0u);
        EXPECT_EQ(extFlags.hasEXTFragmentDensityMap2, 0u);
        EXPECT_EQ(extProps.fragmentDensityMapFeatures.fragmentDensityMap, VK_FALSE);
        EXPECT_TRUE(vrsCaps.supportsPipeline);
        EXPECT_TRUE(vrsCaps.supportsAttachment);
    }

    // Only FDM available, PreferFSR -> falls back to FDM.
    {
        OptionalVulkanDeviceExtensions extFlags{};
        extFlags.hasEXTFragmentDensityMap = 1u;
        OptionalVulkanDeviceExtensionProperties extProps{};
        extProps.fragmentDensityMapFeatures.fragmentDensityMap = VK_TRUE;

        gp::rhi::VariableRateShadingCapabilities vrsCaps{};
        chooseVariableRateShadingMethod(extFlags, extProps, VulkanVariableRateShadingPreference::PreferFSR, vrsCaps);

        EXPECT_EQ(extFlags.hasEXTFragmentDensityMap, 1u);
        EXPECT_FALSE(vrsCaps.supportsPipeline);
        EXPECT_TRUE(vrsCaps.supportsAttachment);
    }

    // Only FDM available, RequireFSR -> no fallback, both disabled.
    {
        OptionalVulkanDeviceExtensions extFlags{};
        extFlags.hasEXTFragmentDensityMap = 1u;
        OptionalVulkanDeviceExtensionProperties extProps{};
        extProps.fragmentDensityMapFeatures.fragmentDensityMap = VK_TRUE;

        gp::rhi::VariableRateShadingCapabilities vrsCaps{};
        chooseVariableRateShadingMethod(extFlags, extProps, VulkanVariableRateShadingPreference::RequireFSR, vrsCaps);

        EXPECT_EQ(extFlags.hasKHRFragmentShadingRate, 0u);
        EXPECT_EQ(extFlags.hasEXTFragmentDensityMap, 0u);
        EXPECT_FALSE(vrsCaps.supportsAttachment);
    }

    // Both available, PreferFDM -> disables FSR, enables FDM.
    {
        OptionalVulkanDeviceExtensions extFlags{};
        extFlags.hasKHRFragmentShadingRate = 1u;
        extFlags.hasEXTFragmentDensityMap = 1u;
        OptionalVulkanDeviceExtensionProperties extProps{};
        extProps.fragmentShadingRateFeatures.attachmentFragmentShadingRate = VK_TRUE;
        extProps.fragmentDensityMapFeatures.fragmentDensityMap = VK_TRUE;

        gp::rhi::VariableRateShadingCapabilities vrsCaps{};
        chooseVariableRateShadingMethod(extFlags, extProps, VulkanVariableRateShadingPreference::PreferFDM, vrsCaps);

        EXPECT_EQ(extFlags.hasEXTFragmentDensityMap, 1u);
        EXPECT_EQ(extFlags.hasKHRFragmentShadingRate, 0u);
        EXPECT_TRUE(vrsCaps.supportsAttachment);
    }
}

TEST(VulkanDeviceCapabilitiesTest, PopulateVertexElementTypeSupportAndFormatSupportTable)
{
    VulkanFormatPropertiesQueryFn mockFormatQuery = [](const VkFormat format) noexcept -> VkFormatProperties
    {
        VkFormatProperties props{};
        if (format == VK_FORMAT_R32G32B32A32_SFLOAT || format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            props.bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT | VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT |
                                   VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;
            props.optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                          VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                          VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                                          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        }
        else if (format == VK_FORMAT_R8G8B8A8_SRGB)
        {
            props.optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        }
        else if (format == VK_FORMAT_R8_UNORM)
        {
            // R8_UNORM supported, but R8_SRGB intentionally lacks SAMPLED_IMAGE_BIT to test sRGB fallback.
            props.optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        }
        else if (format == VK_FORMAT_R16G16B16A16_SFLOAT)
        {
            props.optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        }
        else if (format == VK_FORMAT_B10G11R11_UFLOAT_PACK32)
        {
            // Sampled only (missing COLOR_ATTACHMENT_BIT) so R11G11B10_Float must fall back to R16G16B16A16_SFLOAT.
            props.optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        }
        else if (format == VK_FORMAT_R32_UINT)
        {
            props.bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT | VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT |
                                   VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;
            props.optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        }
        else if (format == VK_FORMAT_R64_UINT)
        {
            props.optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                                          VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;
        }
        return props;
    };

    VulkanMipMapSupportQueryFn mockMipQuery = [](const VkFormat format) noexcept -> bool
    {
        return format == VK_FORMAT_R8G8B8A8_UNORM;
    };

    gp::rhi::VertexElementTypeSupportInfo vertexSupport{};
    populateVertexElementTypeSupport(vertexSupport, mockFormatQuery);
    EXPECT_FALSE(vertexSupport.isSupported(pipeline::VertexElementType::None));
    EXPECT_TRUE(vertexSupport.isSupported(pipeline::VertexElementType::Float4));
    EXPECT_TRUE(vertexSupport.isSupported(pipeline::VertexElementType::UByte4N));
    EXPECT_TRUE(vertexSupport.isSupported(pipeline::VertexElementType::UInt));
    EXPECT_FALSE(vertexSupport.isSupported(pipeline::VertexElementType::Float3));

    VulkanPhysicalDeviceFeatures features;
    features.getCore10Mutable().shaderImageGatherExtended = VK_TRUE;

    VulkanFormatSupportTable table{};
    populateFormatSupportTable(table, features, false, mockFormatQuery, mockMipQuery);

    const gp::USize rgba8Idx = static_cast<gp::USize>(PixelFormat::R8G8B8A8_UNorm);
    EXPECT_TRUE(table.supported[rgba8Idx]);
    EXPECT_EQ(table.platformFormats[rgba8Idx], VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(table.srgbFormats[rgba8Idx], VK_FORMAT_R8G8B8A8_SRGB);
    EXPECT_EQ(table.bufferFormats[rgba8Idx], VK_FORMAT_R8G8B8A8_UNORM);

    // R8_UNorm falls back to linear VK_FORMAT_R8_UNORM when VK_FORMAT_R8_SRGB lacks SAMPLED_IMAGE_BIT.
    const gp::USize r8Idx = static_cast<gp::USize>(PixelFormat::R8_UNorm);
    EXPECT_TRUE(table.supported[r8Idx]);
    EXPECT_EQ(table.srgbFormats[r8Idx], VK_FORMAT_R8_UNORM);
    EXPECT_TRUE(hasPixelFormatCapabilities(
        table.capabilities[rgba8Idx],
        PixelFormatCapabilities::TextureSample | PixelFormatCapabilities::RenderTarget |
            PixelFormatCapabilities::TextureGather | PixelFormatCapabilities::TextureMipmaps
    ));

    // R11G11B10_Float falls back to R16G16B16A16_SFLOAT because B10G11R11 lacked RenderTarget.
    const gp::USize r11g11b10Idx = static_cast<gp::USize>(PixelFormat::R11G11B10_Float);
    EXPECT_TRUE(table.supported[r11g11b10Idx]);
    EXPECT_EQ(table.platformFormats[r11g11b10Idx], VK_FORMAT_R16G16B16A16_SFLOAT);

    // R32_UInt exposes IndexBuffer capability.
    const gp::USize r32uIdx = static_cast<gp::USize>(PixelFormat::R32_UInt);
    EXPECT_NE(table.capabilities[r32uIdx] & PixelFormatCapabilities::IndexBuffer, PixelFormatCapabilities::None);

    // R64_UInt keeps UAV + TextureAtomics but strips AnyTexture and TextureSample.
    const gp::USize r64Idx = static_cast<gp::USize>(PixelFormat::R64_UInt);
    EXPECT_TRUE(table.supported[r64Idx]);
    EXPECT_TRUE(hasPixelFormatCapabilities(
        table.capabilities[r64Idx], PixelFormatCapabilities::UAV | PixelFormatCapabilities::TextureAtomics
    ));
    EXPECT_EQ(
        table.capabilities[r64Idx] & (PixelFormatCapabilities::AnyTexture | PixelFormatCapabilities::TextureSample),
        PixelFormatCapabilities::None
    );
}

TEST(VulkanDeviceCapabilitiesTest, PopulateRHIGlobalsFromDeviceSetsLimitsWaveOpsAndFeatures)
{
    VulkanPhysicalDeviceFeatures features;
    features.getCore10Mutable().depthBounds = VK_TRUE;
    features.getCore10Mutable().dualSrcBlend = VK_TRUE;
    features.getCore10Mutable().multiDrawIndirect = VK_TRUE;
    features.getCore10Mutable().geometryShader = VK_TRUE;
    features.getCore10Mutable().sparseBinding = VK_TRUE;
    features.getCore10Mutable().sparseResidencyBuffer = VK_TRUE;
    features.getCore10Mutable().sparseResidencyImage2D = VK_TRUE;

    VulkanPhysicalDeviceProperties props;
    VkPhysicalDeviceProperties& coreProps = props.getCore10Mutable();
    coreProps.limits.maxImageDimension2D = 16384u;
    coreProps.limits.maxImageDimensionCube = 16384u;
    coreProps.limits.maxImageDimension3D = 2048u;
    coreProps.limits.maxImageArrayLayers = 2048u;
    coreProps.limits.maxComputeSharedMemorySize = 49152u;
    coreProps.limits.maxUniformBufferRange = 65536u;
    coreProps.limits.maxStorageBufferRange = 2147483648u;
    coreProps.limits.maxPerStageDescriptorSamplers = 1024u;
    coreProps.limits.maxPerStageDescriptorStorageBuffers = 32u;
    coreProps.limits.maxPerStageDescriptorStorageImages = 32u;
    coreProps.limits.maxComputeWorkGroupCount[0] = 65535u;
    coreProps.limits.maxComputeWorkGroupCount[1] = 65535u;
    coreProps.limits.maxComputeWorkGroupCount[2] = 65535u;
    coreProps.limits.timestampPeriod = 1.0f;
    coreProps.sparseProperties.residencyNonResidentStrict = VK_TRUE;

    VkPhysicalDeviceSubgroupProperties& subgroup = props.getSubgroupMutable();
    subgroup.subgroupSize = 32u;
    subgroup.supportedStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                               VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_TASK_BIT_EXT |
                               VK_SHADER_STAGE_MESH_BIT_EXT;
    subgroup.supportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT |
                                   VK_SUBGROUP_FEATURE_ARITHMETIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT |
                                   VK_SUBGROUP_FEATURE_SHUFFLE_BIT;

    OptionalVulkanDeviceExtensions extFlags{};
    extFlags.hasSeparateDepthStencilLayouts = 1u;
    extFlags.hasKHRSynchronization2 = 1u;
    extFlags.hasKHRRenderPass2 = 1u;
    extFlags.hasKHRTimelineSemaphore = 1u;
    extFlags.hasEXTMeshShader = 1u;
    extFlags.hasEXTDescriptorIndexing = 1u;
    extFlags.hasEXTSubgroupSizeControl = 1u;
    extFlags.hasKHRShaderAtomicInt64 = 1u;
    extFlags.hasAccelerationStructure = 1u;
    extFlags.hasRayQuery = 1u;
    extFlags.hasBufferDeviceAddress = 1u;
    extFlags.hasDeferredHostOperations = 1u;
    extFlags.hasSPIRV_14 = 1u;
    extFlags.hasShaderFloatControls = 1u;

    OptionalVulkanDeviceExtensionProperties extProps{};
    extProps.subgroupSizeControlProperties.minSubgroupSize = 32u;
    extProps.subgroupSizeControlProperties.maxSubgroupSize = 64u;
    extProps.accelerationStructureProps.minAccelerationStructureScratchOffsetAlignment = 512u;
    extProps.rayTracingPipelineProps.shaderGroupBaseAlignment = 64u;

    VulkanQueueFamilyIndices queueIndices{};
    queueIndices.graphicsFamily = 0;
    queueIndices.asyncComputeFamily = 1;

    gp::Vector<VkQueueFamilyProperties> queueProps;
    VkQueueFamilyProperties gfxProps{};
    gfxProps.queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_SPARSE_BINDING_BIT;
    gfxProps.queueCount = 1u;
    queueProps.pushBack(gfxProps);

    VulkanFormatSupportTable formatTable{};
    formatTable.capabilities[static_cast<gp::USize>(PixelFormat::R64_UInt)] = PixelFormatCapabilities::UAV;

    gp::rhi::VertexElementTypeSupportInfo vertexSupport{};
    vertexSupport.setSupported(pipeline::VertexElementType::Float4, true);

    gp::rhi::VariableRateShadingCapabilities vrsCaps{};
    vrsCaps.supportsPipeline = true;
    vrsCaps.supportsAttachment = true;

    gp::rhi::RHIGlobals globals{};
    populateRHIGlobalsFromDevice(
        features,
        props,
        extFlags,
        extProps,
        queueIndices,
        gp::ConstVectorView<VkQueueFamilyProperties>(queueProps.data(), queueProps.size()),
        formatTable,
        vertexSupport,
        vrsCaps,
        globals
    );

    EXPECT_TRUE(globals.supportsDepthBoundsTest);
    EXPECT_TRUE(globals.supportsDualSrcBlending);
    EXPECT_TRUE(globals.supportsMultiDrawIndirect);
    EXPECT_TRUE(globals.supportsSplitBarriers);
    EXPECT_TRUE(globals.supportsAsyncCompute);
    EXPECT_TRUE(globals.supportsParallelRHIExecute);
    EXPECT_TRUE(globals.parallelRenderPasses.supported);
    EXPECT_TRUE(globals.supportsAtomicUInt64);
    EXPECT_TRUE(globals.supportsMeshShadersTier0);
    EXPECT_TRUE(globals.supportsMeshShadersTier1);
    EXPECT_TRUE(globals.supportsBindlessResources);
    EXPECT_TRUE(globals.rayTracing.supported);
    EXPECT_TRUE(globals.rayTracing.supportsInlineRayTracing);
    EXPECT_EQ(globals.rayTracing.scratchBufferAlignment, 512u);
    EXPECT_TRUE(globals.reservedResources.supported);
    EXPECT_EQ(globals.maxTextureDimensions, 16384u);
    EXPECT_EQ(globals.maxTextureMipCount, 15u);
    EXPECT_EQ(globals.maxSimultaneousUAVs, 16u);
    EXPECT_EQ(globals.maxComputeSharedMemory, 49152u);
    EXPECT_TRUE(globals.supportsWaveOperations);
    EXPECT_EQ(globals.minimumWaveSize, 32u);
    EXPECT_EQ(globals.maximumWaveSize, 64u);
    EXPECT_TRUE(globals.vertexElementTypeSupport.isSupported(pipeline::VertexElementType::Float4));
    EXPECT_TRUE(globals.variableRateShading.supportsPipeline);
    EXPECT_TRUE(globals.supportsTimestampQueries);
    EXPECT_TRUE(globals.supportsOcclusionQueries);

    // Negative / fallback case: wave ops disabled, low UAV count (8), no sparse queue.
    OptionalVulkanDeviceExtensions minimalExts{};
    VulkanPhysicalDeviceProperties minimalProps;
    minimalProps.getCore10Mutable().limits.maxPerStageDescriptorStorageBuffers = 8u;
    minimalProps.getCore10Mutable().limits.maxPerStageDescriptorStorageImages = 8u;
    minimalProps.getCore10Mutable().limits.timestampPeriod = 0.0f;
    VulkanQueueFamilyIndices noQueueIndices{};
    gp::rhi::RHIGlobals fallbackGlobals{};
    fallbackGlobals.reservedResources.supported = true;

    populateRHIGlobalsFromDevice(
        features,
        minimalProps,
        minimalExts,
        extProps,
        noQueueIndices,
        {},
        VulkanFormatSupportTable{},
        vertexSupport,
        gp::rhi::VariableRateShadingCapabilities{},
        fallbackGlobals
    );
    EXPECT_FALSE(fallbackGlobals.supportsWaveOperations);
    EXPECT_EQ(fallbackGlobals.minimumWaveSize, 0u);
    EXPECT_EQ(fallbackGlobals.maximumWaveSize, 0u);
    EXPECT_EQ(fallbackGlobals.maxSimultaneousUAVs, 8u);
    EXPECT_FALSE(fallbackGlobals.supportsAtomicUInt64);
    EXPECT_FALSE(fallbackGlobals.reservedResources.supported);
    EXPECT_FALSE(fallbackGlobals.supportsTimestampQueries);
}

TEST(VulkanDeviceTest, NullHandleDeviceSafelyFailsInitializeAndDestroysCleanly)
{
    VulkanDevice nullDevice(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false);
    EXPECT_FALSE(nullDevice.isValid());
    EXPECT_FALSE(nullDevice.initialize(false));
    EXPECT_EQ(nullDevice.getLastCreateResult(), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT_EQ(nullDevice.getQueueHandle(VulkanQueueType::Graphics), VK_NULL_HANDLE);
    nullDevice.destroy();
    EXPECT_FALSE(nullDevice.isValid());
}

TEST(VulkanDeviceTest, LiveDeviceCreationAndFormatSetupWhenDriverAvailable)
{
    if (!VulkanLoader::loadLibrary())
    {
        GTEST_SKIP() << "Vulkan loader library not available on this host.";
    }

    VulkanInstance instance;
    VulkanInstanceCreateDesc instDesc{};
    instDesc.applicationName = "VulkanDeviceUnitTest";
    instDesc.allowRetryWithoutLayers = true;
    instDesc.allowMissingSurfaceFunctions = true;
    instDesc.enableDebugCallbacks = false;

    if (!instance.initialize(instDesc))
    {
        VulkanLoader::freeLibrary();
        GTEST_SKIP() << "No Vulkan ICD available on this host.";
    }

    VulkanPhysicalDeviceInfo gpuInfo{};
    const VkPhysicalDevice physicalDevice =
        selectPhysicalDevice(instance.getHandle(), instance.getApiVersion(), gpuInfo);
    if (physicalDevice == VK_NULL_HANDLE)
    {
        instance.shutdown();
        VulkanLoader::freeLibrary();
        GTEST_SKIP() << "No compatible Vulkan physical device available on this host.";
    }

    VulkanDevice device(
        instance.getHandle(),
        physicalDevice,
        instance.getApiVersion(),
        instance.getActiveDebugLayerExtension() == ActiveDebugLayerExtension::DebugUtilsExtension
    );

    ASSERT_TRUE(device.initialize(false));
    EXPECT_TRUE(device.isValid());
    EXPECT_NE(device.getHandle(), VK_NULL_HANDLE);
    EXPECT_NE(device.getQueueHandle(VulkanQueueType::Graphics), VK_NULL_HANDLE);
    EXPECT_GE(device.getQueueFamilyIndices().graphicsFamily, 0);
    EXPECT_TRUE(device.getFormatSupportTable().supported[static_cast<gp::USize>(PixelFormat::R8G8B8A8_UNorm)]);
    EXPECT_TRUE(device.getVertexElementTypeSupport().isSupported(pipeline::VertexElementType::Float4));

    device.waitUntilIdle();
    device.destroy();
    EXPECT_FALSE(device.isValid());

    instance.shutdown();
    VulkanLoader::freeLibrary();
}

}   // namespace gp::rhi::vulkan::tests
