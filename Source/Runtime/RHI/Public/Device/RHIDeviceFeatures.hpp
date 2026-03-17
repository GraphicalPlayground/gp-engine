// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "RHIBuild.hpp"

namespace GP
{

/// @brief Optional hardware features; each bool indicates runtime availability.
struct FRHIDeviceFeatures
{
    // Core rasterisation
    bool geometryShader{ false };
    bool tessellationShader{ false };
    bool multiViewport{ false };
    bool depthClamp{ false };
    bool depthBiasClamp{ false };
    bool fillModeNonSolid{ false };
    bool wideLines{ false };
    bool largePoints{ false };
    bool alphaToOne{ false };
    bool multiDrawIndirect{ false };
    bool drawIndirectFirstInstance{ false };
    bool independentBlend{ false };
    bool logicOp{ false };
    bool sampleRateShading{ false };
    bool dualSrcBlend{ false };
    bool occlusionQueryPrecise{ false };
    bool pipelineStatisticsQuery{ false };
    bool textureCompressionBC{ false };
    bool textureCompressionETC2{ false };
    bool textureCompressionASTC{ false };
    bool shaderInt64{ false };
    bool shaderInt16{ false };
    bool shaderFloat16{ false };
    bool shaderFloat64{ false };
    bool shaderSubgroupOps{ false };
    bool sparseBinding{ false };
    bool sparseResidencyBuffer{ false };
    bool sparseResidencyImage2D{ false };

    // Bindless / descriptor indexing
    bool descriptorIndexing{ false };
    bool runtimeDescriptorArray{ false };
    bool descriptorBindingPartiallyBound{ false };
    bool descriptorBindingUpdateAfterBind{ false };

    // Buffer device address
    bool bufferDeviceAddress{ false };

    // Timeline semaphores
    bool timelineSemaphore{ false };

    // Dynamic rendering (Vulkan 1.3 / VK_KHR_dynamic_rendering)
    bool dynamicRendering{ false };

    // Synchronisation2 (Vulkan 1.3 / VK_KHR_synchronization2)
    bool synchronization2{ false };

    // Maintenance
    bool maintenance4{ false };

    // Mesh / task shaders
    bool meshShader{ false };
    bool taskShader{ false };

    // VRS
    bool pipelineFragmentShadingRate{ false };
    bool primitiveFragmentShadingRate{ false };
    bool attachmentFragmentShadingRate{ false };

    // Ray tracing
    bool rayTracingPipeline{ false };
    bool rayQuery{ false };   //<! Inline ray queries from any shader.
    bool accelerationStructure{ false };
    bool accelerationStructureIndirect{ false };

    // Conservative rasterisation
    bool conservativeRasterisation{ false };

    // Depth range unrestricted (VK_EXT_depth_range_unrestricted)
    bool depthRangeUnrestricted{ false };

    // Sampler ycbcr conversion
    bool samplerYcbcrConversion{ false };
};

}   // namespace GP
