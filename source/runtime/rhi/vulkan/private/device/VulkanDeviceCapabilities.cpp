// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanDeviceCapabilities.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/RHIGlobals.hpp"
#include "logging/LogMacros.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "platform/VulkanPlatform.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPixelFormat.hpp"
#include "templates/NumericLimits.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "translation/VulkanVertexFormat.hpp"

namespace gp::rhi::vulkan
{

namespace
{

[[nodiscard]] gp::UInt32 computeFloorLog2(gp::UInt32 value) noexcept
{
    gp::UInt32 log2Val = 0u;
    while (value > 1u)
    {
        value >>= 1u;
        ++log2Val;
    }
    return log2Val;
}

[[nodiscard]] PixelFormatCapabilities
    getRequiredCapabilitiesForFormat(const PixelFormat format, const bool renderDocFound) noexcept
{
    switch (format)
    {
    case PixelFormat::R11G11B10_Float:
    case PixelFormat::R8G8B8_UNorm:
        return PixelFormatCapabilities::TextureSample | PixelFormatCapabilities::RenderTarget;

    case PixelFormat::R64_UInt:
        return renderDocFound ? PixelFormatCapabilities::UAV
                              : (PixelFormatCapabilities::UAV | PixelFormatCapabilities::TextureAtomics);

    default:
        return PixelFormatCapabilities::TextureSample;
    }
}

}   // namespace

VulkanFormatSupportTable::VulkanFormatSupportTable() noexcept
{
    for (gp::USize index = 0u; index < kPixelFormatCount; ++index)
    {
        platformFormats[index] = VK_FORMAT_UNDEFINED;
        bufferFormats[index] = VK_FORMAT_UNDEFINED;
        srgbFormats[index] = VK_FORMAT_UNDEFINED;
        componentMappings[index] = kComponentMappingRGBA;
        supported[index] = false;
        capabilities[index] = PixelFormatCapabilities::None;
    }
}

void chooseVariableRateShadingMethod(
    OptionalVulkanDeviceExtensions& extFlags,
    OptionalVulkanDeviceExtensionProperties& extProps,
    const VulkanVariableRateShadingPreference preference,
    gp::rhi::VariableRateShadingCapabilities& outVrsCaps
) noexcept
{
    outVrsCaps = gp::rhi::VariableRateShadingCapabilities{};

    const bool fsrAvailable = (extFlags.hasKHRFragmentShadingRate != 0u) &&
                              (extProps.fragmentShadingRateFeatures.attachmentFragmentShadingRate == VK_TRUE);
    const bool fdmAvailable = (extFlags.hasEXTFragmentDensityMap != 0u);

    const bool preferFsr =
        (static_cast<gp::UInt8>(preference) <= static_cast<gp::UInt8>(VulkanVariableRateShadingPreference::RequireFSR));
    const bool allowFallback = (preference == VulkanVariableRateShadingPreference::PreferFSR) ||
                               (preference == VulkanVariableRateShadingPreference::PreferFDM);
    const bool errorIfUnavailable =
        !allowFallback && ((preference == VulkanVariableRateShadingPreference::RequireFSR) ||
                           (preference == VulkanVariableRateShadingPreference::RequireFDM));

    const auto disableFsr = [&extFlags, &extProps]() noexcept -> void
    {
        extFlags.hasKHRFragmentShadingRate = 0u;
        extProps.fragmentShadingRateFeatures.primitiveFragmentShadingRate = VK_FALSE;
        extProps.fragmentShadingRateFeatures.attachmentFragmentShadingRate = VK_FALSE;
        extProps.fragmentShadingRateFeatures.pipelineFragmentShadingRate = VK_FALSE;
    };

    const auto disableFdm = [&extFlags, &extProps]() noexcept -> void
    {
        extFlags.hasEXTFragmentDensityMap = 0u;
        extProps.fragmentDensityMapFeatures.fragmentDensityMap = VK_FALSE;
        extProps.fragmentDensityMapFeatures.fragmentDensityMapDynamic = VK_FALSE;
        extProps.fragmentDensityMapFeatures.fragmentDensityMapNonSubsampledImages = VK_FALSE;
        extFlags.hasEXTFragmentDensityMap2 = 0u;
        extProps.fragmentDensityMap2Features.fragmentDensityMapDeferred = VK_FALSE;
    };

    const bool preferredAvailable = preferFsr ? fsrAvailable : fdmAvailable;
    const bool fallbackAvailable = allowFallback && (preferFsr ? fdmAvailable : fsrAvailable);

    if (preferredAvailable)
    {
        if (preferFsr)
        {
            disableFdm();
            outVrsCaps.supportsPipeline = (extProps.fragmentShadingRateFeatures.pipelineFragmentShadingRate == VK_TRUE);
            outVrsCaps.supportsAttachment = true;
        }
        else
        {
            disableFsr();
            outVrsCaps.supportsAttachment = true;
        }
    }
    else
    {
        if (errorIfUnavailable)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "Required Variable Rate Shading method ({}) is not available on this device.",
                preferFsr ? "Fragment Shading Rate" : "Fragment Density Map"
            );
        }

        if (fallbackAvailable)
        {
            if (preferFsr)
            {
                disableFsr();
                outVrsCaps.supportsAttachment = true;
            }
            else
            {
                disableFdm();
                outVrsCaps.supportsPipeline =
                    (extProps.fragmentShadingRateFeatures.pipelineFragmentShadingRate == VK_TRUE);
                outVrsCaps.supportsAttachment = true;
            }
        }
        else
        {
            disableFsr();
            disableFdm();
        }
    }
}

void populateVertexElementTypeSupport(
    gp::rhi::VertexElementTypeSupportInfo& outSupport, const VulkanFormatPropertiesQueryFn& queryFormatProps
) noexcept
{
    outSupport = gp::rhi::VertexElementTypeSupportInfo{};

    if (!queryFormatProps)
    {
        return;
    }

    for (gp::USize index = 0u; index < pipeline::kVertexElementTypeCount; ++index)
    {
        const pipeline::VertexElementType elementType = static_cast<pipeline::VertexElementType>(index);
        const VkFormat vkFormat = vertexElementTypeToVkFormat(elementType);
        if (vkFormat == VK_FORMAT_UNDEFINED)
        {
            outSupport.setSupported(elementType, false);
            continue;
        }

        const VkFormatProperties props = queryFormatProps(vkFormat);
        const bool isVertexSupported =
            (props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) == VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
        outSupport.setSupported(elementType, isVertexSupported);
    }
}

void populateFormatSupportTable(
    VulkanFormatSupportTable& outTable,
    const VulkanPhysicalDeviceFeatures& features,
    const bool renderDocFound,
    const VulkanFormatPropertiesQueryFn& queryFormatProps,
    const VulkanMipMapSupportQueryFn& queryMipMapSupport
) noexcept
{
    outTable = VulkanFormatSupportTable{};

    if (!queryFormatProps)
    {
        return;
    }

    const bool hasGatherExtended = (features.getCore10().shaderImageGatherExtended == VK_TRUE);

    for (gp::USize index = 1u; index < kPixelFormatCount; ++index)
    {
        const PixelFormat format = static_cast<PixelFormat>(index);
        const gp::ConstVectorView<VkFormat> candidates = getVulkanFormatFallbackList(format);
        if (candidates.isEmpty())
        {
            continue;
        }

        const PixelFormatCapabilities requiredCaps = getRequiredCapabilitiesForFormat(format, renderDocFound);

        // 1. Buffer capabilities are evaluated against the primary canonical format.
        const VkFormat primaryFormat = candidates[0];
        const VkFormatProperties primaryProps = queryFormatProps(primaryFormat);
        const PixelFormatCapabilities bufferCaps =
            translateBufferFormatCapabilities(primaryFormat, primaryProps.bufferFeatures);

        PixelFormatCapabilities combinedCaps = PixelFormatCapabilities::None;
        if (bufferCaps != PixelFormatCapabilities::None)
        {
            outTable.bufferFormats[index] = primaryFormat;
            combinedCaps |= bufferCaps;
        }

        // 2. Image capabilities probe the prioritized fallback chain for the first match meeting requiredCaps.
        for (gp::USize candidateIdx = 0u; candidateIdx < candidates.size(); ++candidateIdx)
        {
            const VkFormat candidateFormat = candidates[candidateIdx];
            const VkFormatProperties candidateProps =
                (candidateIdx == 0u) ? primaryProps : queryFormatProps(candidateFormat);

            PixelFormatCapabilities imageCaps = translateImageFormatCapabilities(candidateProps);
            if ((imageCaps & PixelFormatCapabilities::AnyTexture) != PixelFormatCapabilities::None)
            {
                if (hasGatherExtended)
                {
                    imageCaps |= PixelFormatCapabilities::TextureGather;
                }
                if (queryMipMapSupport && queryMipMapSupport(candidateFormat))
                {
                    imageCaps |= PixelFormatCapabilities::TextureMipmaps;
                }
            }

            if (hasPixelFormatCapabilities(imageCaps, requiredCaps))
            {
                outTable.platformFormats[index] = candidateFormat;
                VkFormat resolvedSrgb = vkFormatToSRGB(candidateFormat);
                if (resolvedSrgb != candidateFormat)
                {
                    const VkFormatProperties srgbProps = queryFormatProps(resolvedSrgb);
                    if ((srgbProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0u)
                    {
                        resolvedSrgb = candidateFormat;
                    }
                }
                outTable.srgbFormats[index] = resolvedSrgb;
                combinedCaps |= imageCaps;
                break;
            }
        }

        // Special handling for R64_UInt: UAV-only atomic format, never exposed for texture sampling.
        if (format == PixelFormat::R64_UInt)
        {
            combinedCaps &= ~(PixelFormatCapabilities::AnyTexture | PixelFormatCapabilities::TextureSample);
        }

        const bool formatSupported =
            hasPixelFormatCapabilities(combinedCaps, requiredCaps) ||
            ((combinedCaps & PixelFormatCapabilities::Buffer) != PixelFormatCapabilities::None);
        outTable.supported[index] = formatSupported;
        outTable.capabilities[index] = combinedCaps;
        if (formatSupported)
        {
            outTable.componentMappings[index] = kVulkanPixelFormatTable[index].componentMapping;
        }
    }
}

void populateRHIGlobalsFromDevice(
    const VulkanPhysicalDeviceFeatures& features,
    const VulkanPhysicalDeviceProperties& properties,
    const OptionalVulkanDeviceExtensions& extFlags,
    const OptionalVulkanDeviceExtensionProperties& extProps,
    const VulkanQueueFamilyIndices& queueIndices,
    const gp::ConstVectorView<VkQueueFamilyProperties> queueFamilyProps,
    const VulkanFormatSupportTable& formatTable,
    const gp::rhi::VertexElementTypeSupportInfo& vertexSupport,
    const gp::rhi::VariableRateShadingCapabilities& vrsCaps,
    gp::rhi::RHIGlobals& outGlobals
) noexcept
{
    const VkPhysicalDeviceProperties& coreProps = properties.getCore10();
    const VkPhysicalDeviceLimits& limits = coreProps.limits;
    const VkPhysicalDeviceFeatures& coreFeatures = features.getCore10();

    // Core threading & capability flags
    outGlobals.supportsMultithreading = true;
    outGlobals.supportsMultithreadedResources = true;
    outGlobals.supportsAsyncTextureCreation = false;
    outGlobals.supportsTextureStreaming = true;
    outGlobals.supportsTexture3D = VulkanPlatform::supportsVolumeTextureRendering();
    outGlobals.supportsTextureViews = true;
    outGlobals.supportsUAVFormatAliasing = true;
    outGlobals.supportsPixelShaderUAVs = true;
    outGlobals.supportsSeparateRenderTargetBlendState = true;
    outGlobals.supportsMapWriteNoOverwrite = true;
    outGlobals.needsExtraDeletionLatency = true;

    outGlobals.supportsDepthBoundsTest = (coreFeatures.depthBounds == VK_TRUE);
    outGlobals.supportsDualSrcBlending = (coreFeatures.dualSrcBlend == VK_TRUE);
    outGlobals.supportsMultiDrawIndirect = (coreFeatures.multiDrawIndirect == VK_TRUE);
    outGlobals.supportsSplitBarriers = (extFlags.hasKHRSynchronization2 != 0u);

    outGlobals.supportsTimestampQueries = (limits.timestampPeriod > 0.0f);
    outGlobals.supportsOcclusionQueries = true;

    const bool hasAsyncCompute = (queueIndices.asyncComputeFamily >= 0);
    outGlobals.supportsAsyncCompute = hasAsyncCompute;
    outGlobals.supportsEfficientAsyncCompute = hasAsyncCompute;

    const bool supportsParallel = (extFlags.hasSeparateDepthStencilLayouts != 0u) &&
                                  (extFlags.hasKHRSynchronization2 != 0u) && (extFlags.hasKHRRenderPass2 != 0u);
    outGlobals.supportsParallelRHIExecute = supportsParallel;
    outGlobals.parallelRenderPasses.supported = supportsParallel;

    // 64-bit atomics require both the extension and UAV support on R64_UInt.
    const gp::USize r64Idx = static_cast<gp::USize>(PixelFormat::R64_UInt);
    const bool r64HasUav =
        (formatTable.capabilities[r64Idx] & PixelFormatCapabilities::UAV) != PixelFormatCapabilities::None;
    outGlobals.supportsAtomicUInt64 =
        ((extFlags.hasKHRShaderAtomicInt64 != 0u) || (extFlags.hasImageAtomicInt64 != 0u)) && r64HasUav;

    // Mesh shaders
    outGlobals.supportsMeshShadersTier0 = (extFlags.hasEXTMeshShader != 0u);
    outGlobals.supportsMeshShadersTier1 = (extFlags.hasEXTMeshShader != 0u);

    // Bindless descriptors
    const bool hasBindless = (extFlags.hasEXTDescriptorIndexing != 0u) || (extFlags.hasEXTDescriptorBuffer != 0u) ||
                             (extFlags.hasEXTDescriptorHeap != 0u);
    outGlobals.supportsBindlessResources = hasBindless;
    outGlobals.supportsBindlessSamplers = hasBindless;

    // Ray tracing capabilities
    outGlobals.rayTracing.supported = extFlags.hasRaytracingExtensions();
    outGlobals.rayTracing.supportsInlineRayTracing = outGlobals.rayTracing.supported && (extFlags.hasRayQuery != 0u);
    outGlobals.rayTracing.supportsAsyncBuild = outGlobals.rayTracing.supported && hasAsyncCompute;
    if (outGlobals.rayTracing.supported)
    {
        constexpr gp::UInt32 kEngineAsAlignment = 256u;
        const gp::UInt32 hwScratchAlign =
            extProps.accelerationStructureProps.minAccelerationStructureScratchOffsetAlignment;
        outGlobals.rayTracing.accelerationStructureAlignment = kEngineAsAlignment;
        outGlobals.rayTracing.scratchBufferAlignment =
            (hwScratchAlign > kEngineAsAlignment) ? hwScratchAlign : kEngineAsAlignment;
        if (extProps.rayTracingPipelineProps.shaderGroupBaseAlignment > 0u)
        {
            outGlobals.rayTracing.shaderTableAlignment = extProps.rayTracingPipelineProps.shaderGroupBaseAlignment;
        }
    }

    // Variable Rate Shading capabilities
    outGlobals.variableRateShading = vrsCaps;

    // Reserved / sparse resources
    if (queueIndices.graphicsFamily >= 0 &&
        static_cast<gp::USize>(queueIndices.graphicsFamily) < queueFamilyProps.size())
    {
        const VkQueueFlags gfxFlags = queueFamilyProps[static_cast<gp::USize>(queueIndices.graphicsFamily)].queueFlags;
        const bool gfxSupportsSparse =
            ((gfxFlags & VK_QUEUE_SPARSE_BINDING_BIT) == VK_QUEUE_SPARSE_BINDING_BIT) && supportsParallel;
        outGlobals.reservedResources.supported = gfxSupportsSparse && (coreFeatures.sparseBinding == VK_TRUE) &&
                                                 (coreFeatures.sparseResidencyBuffer == VK_TRUE) &&
                                                 (coreFeatures.sparseResidencyImage2D == VK_TRUE) &&
                                                 (coreProps.sparseProperties.residencyNonResidentStrict == VK_TRUE);
    }
    else
    {
        outGlobals.reservedResources.supported = false;
    }

    // Hardware texture & buffer limits
    outGlobals.maxTextureDimensions = limits.maxImageDimension2D;
    outGlobals.maxCubeTextureDimensions = limits.maxImageDimensionCube;
    outGlobals.maxVolumeTextureDimensions = limits.maxImageDimension3D;
    outGlobals.maxTextureArrayLayers = limits.maxImageArrayLayers;
    const gp::UInt32 computedMips =
        (limits.maxImageDimension2D > 0u) ? (computeFloorLog2(limits.maxImageDimension2D) + 1u) : 1u;
    outGlobals.maxTextureMipCount = (computedMips < kMaxTextureMipCount) ? computedMips : kMaxTextureMipCount;
    outGlobals.maxComputeSharedMemory = limits.maxComputeSharedMemorySize;
    outGlobals.maxConstantBufferByteSize = limits.maxUniformBufferRange;
    outGlobals.maxBufferSize = limits.maxStorageBufferRange;

    const gp::UInt32 maxInt32 = static_cast<gp::UInt32>(gp::NumericLimits<gp::Int32>::max());
    outGlobals.maxTextureSamplers =
        (limits.maxPerStageDescriptorSamplers < maxInt32) ? limits.maxPerStageDescriptorSamplers : maxInt32;

    const gp::UInt32 minStorageDescriptors =
        (limits.maxPerStageDescriptorStorageBuffers < limits.maxPerStageDescriptorStorageImages)
            ? limits.maxPerStageDescriptorStorageBuffers
            : limits.maxPerStageDescriptorStorageImages;
    outGlobals.maxSimultaneousUAVs = (minStorageDescriptors >= 16u) ? 16u : 8u;

    gp::UInt32 minDispatchGroups = limits.maxComputeWorkGroupCount[0];
    if (limits.maxComputeWorkGroupCount[1] < minDispatchGroups)
    {
        minDispatchGroups = limits.maxComputeWorkGroupCount[1];
    }
    if (limits.maxComputeWorkGroupCount[2] < minDispatchGroups)
    {
        minDispatchGroups = limits.maxComputeWorkGroupCount[2];
    }
    outGlobals.maxDispatchThreadGroupsPerDimension =
        (minDispatchGroups < 0x7FFFFFFFu) ? minDispatchGroups : 0x7FFFFFFFu;

    // Subgroup / wave operations
    VkShaderStageFlags deviceShaderStages =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    if (coreFeatures.geometryShader == VK_TRUE)
    {
        deviceShaderStages |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }
    if (extFlags.hasEXTMeshShader != 0u)
    {
        deviceShaderStages |= VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
    }

    const VkShaderStageFlags requiredStages = VulkanPlatform::requiredWaveOpsShaderStageFlags(deviceShaderStages);
    constexpr VkSubgroupFeatureFlags kRequiredSubgroupOps =
        VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT | VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
        VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT;

    const VkPhysicalDeviceSubgroupProperties& subgroup = properties.getSubgroup();
    const bool stagesOk = (subgroup.supportedStages & requiredStages) == requiredStages;
    const bool opsOk = (subgroup.supportedOperations & kRequiredSubgroupOps) == kRequiredSubgroupOps;

    outGlobals.supportsWaveOperations = stagesOk && opsOk;
    if (outGlobals.supportsWaveOperations)
    {
        if (extFlags.hasEXTSubgroupSizeControl != 0u && extProps.subgroupSizeControlProperties.minSubgroupSize > 0u &&
            extProps.subgroupSizeControlProperties.maxSubgroupSize > 0u)
        {
            outGlobals.minimumWaveSize = extProps.subgroupSizeControlProperties.minSubgroupSize;
            outGlobals.maximumWaveSize = extProps.subgroupSizeControlProperties.maxSubgroupSize;
        }
        else
        {
            outGlobals.minimumWaveSize = subgroup.subgroupSize;
            outGlobals.maximumWaveSize = subgroup.subgroupSize;
        }
    }
    else
    {
        outGlobals.minimumWaveSize = 0u;
        outGlobals.maximumWaveSize = 0u;
    }

    // Vertex element support & per-format pixel capabilities
    outGlobals.vertexElementTypeSupport = vertexSupport;
    outGlobals.pixelFormatCapabilities = formatTable.capabilities;
}

}   // namespace gp::rhi::vulkan
