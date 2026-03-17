// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "RHIBuild.hpp"

namespace GP
{

/// @brief Core limits and properties of the physical device.
struct FRHIDeviceLimits
{
    UInt32 maxTexture1DSize{ 0u };
    UInt32 maxTexture2DSize{ 0u };
    UInt32 maxTexture3DSize{ 0u };
    UInt32 maxTextureCubeSize{ 0u };
    UInt32 maxTextureArrayLayers{ 0u };
    UInt32 maxColorAttachments{ 0u };
    UInt32 maxFramebufferWidth{ 0u };
    UInt32 maxFramebufferHeight{ 0u };
    UInt32 maxViewports{ 0u };
    UInt32 maxDescriptorSetSamplers{ 0u };
    UInt32 maxDescriptorSetUniformBuffers{ 0u };
    UInt32 maxDescriptorSetStorageBuffers{ 0u };
    UInt32 maxDescriptorSetSampledImages{ 0u };
    UInt32 maxDescriptorSetStorageImages{ 0u };
    UInt32 maxPushConstantsSize{ 0u };   //<! In bytes.
    UInt32 maxVertexInputBindings{ 0u };
    UInt32 maxVertexInputAttributes{ 0u };
    UInt32 maxComputeWorkGroupCountX{ 0u };
    UInt32 maxComputeWorkGroupCountY{ 0u };
    UInt32 maxComputeWorkGroupCountZ{ 0u };
    UInt32 maxComputeWorkGroupSizeX{ 0u };
    UInt32 maxComputeWorkGroupSizeY{ 0u };
    UInt32 maxComputeWorkGroupSizeZ{ 0u };
    UInt32 maxComputeWorkGroupInvocations{ 0u };
    UInt32 maxDrawIndirectCount{ 0u };
    UInt64 maxBufferSize{ 0ull };
    UInt64 minUniformBufferOffsetAlignment{ 0ull };
    UInt64 minStorageBufferOffsetAlignment{ 0ull };
    UInt64 optimalBufferCopyOffsetAlign{ 0ull };
    UInt64 optimalBufferCopyRowPitchAlign{ 0ull };
    UInt64 nonCoherentAtomSize{ 0ull };
    Float32 maxSamplerAnisotropy{ 0.0f };
    Float32 timestampPeriodNS{ 0.0f };   //<! GPU timestamp tick in nanoseconds.
    UInt32 minSubgroupSize{ 0u };
    UInt32 maxSubgroupSize{ 0u };
};

}   // namespace GP
