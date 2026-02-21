// Copyright (c) - Graphical Playground. All rights reserved.

#include "RHIBuild.hpp"

namespace GP::RHI
{

// Device
class IRHIDevice;
class IRHIPhysicalDevice;
class IRHIAdapter;
class IRHIFactory;

// Commands
class IRHICommandList;
class IRHICommandContext;
class IRHICommandQueue;
class IRHICommandAllocator;
class IRHICommandListImmediate;

// Resources
class IRHIResource;
class IRHIBuffer;
class IRHITexture;
class IRHISampler;

// Views
class IRHIShaderResourceView;
class IRHIUnorderedAccessView;
class IRHIRenderTargetView;
class IRHIDepthStencilView;

// Shaders
class IRHIShader;
class IRHIShaderModule;

// Pipeline
class IRHIPipelineState;
class IRHIGraphicsPipelineState;
class IRHIComputePipelineState;
class IRHIRayTracingPipelineState;

// Descriptors
class IRHIDescriptorHeap;
class IRHIDescriptorSet;
class IRHIDescriptorSetLayout;
class IRHIRootSignature;

// Render Pass
class IRHIRenderPass;
class IRHIFramebuffer;

// SwapChain
class IRHISwapChain;

// Synchronization
class IRHIFence;
class IRHISemaphore;
class IRHIEvent;

// Query
class IRHIQueryHeap;
class IRHIQuery;

// Ray Tracing
class IRHIAccelerationStructure;
class IRHIBottomLevelAS;
class IRHITopLevelAS;

// Memory
class IRHIMemoryAllocator;
class IRHIDeviceMemory;

// Structs (forward declarations)
struct RHIDeviceDesc;
struct RHIDeviceCapabilities;
struct RHIDeviceFeatures;
struct RHIDeviceLimits;
struct RHIBufferDesc;
struct RHITextureDesc;
struct RHISamplerDesc;
struct RHIShaderResourceViewDesc;
struct RHIUnorderedAccessViewDesc;
struct RHIRenderTargetViewDesc;
struct RHIDepthStencilViewDesc;
struct RHIGraphicsPipelineDesc;
struct RHIComputePipelineDesc;
struct RHIRayTracingPipelineDesc;
struct RHIRenderPassDesc;
struct RHIFramebufferDesc;
struct RHISwapChainDesc;
struct RHIShaderDesc;
struct RHIFenceDesc;
struct RHIQueryHeapDesc;
struct RHIDescriptorHeapDesc;
struct RHIDescriptorLayoutBinding;
struct RHIRootSignatureDesc;
struct RHIAccelerationStructureDesc;

}   // namespace GP::RHI
