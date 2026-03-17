// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Memory/SharedPtr.hpp"
#include "RHIBuild.hpp"

namespace GP
{

// Device
class IRHIDevice;

// Resources
class IRHIResource;
class IRHIBuffer;
class IRHIBufferView;
class IRHITexture;
class IRHITextureView;
class IRHISampler;
class IRHIShader;

// Pipeline
class IRHIPipelineState;
class IRHIGraphicsPipeline;
class IRHIComputePipeline;
class IRHIPipelineLayout;

#if GP_RHI_ENABLE_RAY_TRACING
class IRHIRayTracingPipeline;
class IRHIAccelerationStructure;
class IRHIShaderBindingTable;
#endif

// Commands
class IRHICommandList;
class IRHICommandQueue;

// Render Pass
class IRHIRenderPass;

// Descriptors
class IRHIDescriptorSetLayout;
class IRHIDescriptorSet;
class IRHIDescriptorPool;

// Memory
class IRHIMemoryHeap;

// Swap chain
class IRHISwapChain;

// Synchronization
class IRHIFence;
class IRHISemaphore;

// Query
class IRHIQueryPool;

struct FRHIDeviceDesc;
struct FRHIDeviceCaps;
struct FRHIBufferDesc;
struct FRHIBufferViewDesc;
struct FRHITextureDesc;
struct FRHITextureViewDesc;
struct FRHISamplerDesc;
struct FRHIShaderDesc;
struct FRHIPipelineLayoutDesc;
struct FRHIGraphicsPipelineDesc;
struct FRHIComputePipelineDesc;
struct FRHIRenderPassDesc;
struct FRHIDescriptorSetLayoutDesc;
struct FRHIDescriptorPoolDesc;
struct FRHISwapChainDesc;
struct FRHIFenceDesc;
struct FRHISemaphoreDesc;
struct FRHIQueryPoolDesc;
struct FRHIMemoryDesc;

#if GP_RHI_ENABLE_RAY_TRACING
struct FRHIRayTracingPipelineDesc;
struct FRHIAccelerationStructureDesc;
struct FRHIShaderBindingTableDesc;
#endif

using RHIDeviceRef = TSharedPtr<IRHIDevice>;
using RHIBufferRef = TSharedPtr<IRHIBuffer>;
using RHIBufferViewRef = TSharedPtr<IRHIBufferView>;
using RHITextureRef = TSharedPtr<IRHITexture>;
using RHITextureViewRef = TSharedPtr<IRHITextureView>;
using RHISamplerRef = TSharedPtr<IRHISampler>;
using RHIShaderRef = TSharedPtr<IRHIShader>;
using RHIPipelineLayoutRef = TSharedPtr<IRHIPipelineLayout>;
using RHIGraphicsPSORef = TSharedPtr<IRHIGraphicsPipeline>;
using RHIComputePSORef = TSharedPtr<IRHIComputePipeline>;
using RHICommandListRef = TSharedPtr<IRHICommandList>;
using RHICommandQueueRef = TSharedPtr<IRHICommandQueue>;
using RHIRenderPassRef = TSharedPtr<IRHIRenderPass>;
using RHIDescLayoutRef = TSharedPtr<IRHIDescriptorSetLayout>;
using RHIDescSetRef = TSharedPtr<IRHIDescriptorSet>;
using RHIDescPoolRef = TSharedPtr<IRHIDescriptorPool>;
using RHIMemoryHeapRef = TSharedPtr<IRHIMemoryHeap>;
using RHISwapChainRef = TSharedPtr<IRHISwapChain>;
using RHIFenceRef = TSharedPtr<IRHIFence>;
using RHISemaphoreRef = TSharedPtr<IRHISemaphore>;
using RHIQueryPoolRef = TSharedPtr<IRHIQueryPool>;

#if GP_RHI_ENABLE_RAY_TRACING
using RHIRayTracingPSORef = TSharedPtr<IRHIRayTracingPipeline>;
using RHIASRef = TSharedPtr<IRHIAccelerationStructure>;
using RHISBTRef = TSharedPtr<IRHIShaderBindingTable>;
#endif

}   // namespace GP
