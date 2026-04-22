// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "memory/RefCountPtr.hpp"

namespace gp::rhi
{

/// @section Command Lists

class CommandListBase;
class ComputeCommandList;
class CommandList;
class CommandListImmediate;
class SubCommandList;

/// @section Contexts

class ComputeContext;
class CommandContext;
class UploadContext;
class ContextArray;

class ResourceReplaceInfo;
class ResourceReplaceBatcher;

struct SamplerStateInitializer;
struct RasterizerStateInitializer;
struct DepthStencilStateInitializer;
class BlendStateInitializer;

/// @section Resources
class AmplificationShader;
class BlendState;
class BoundShaderState;
class Buffer;
class ComputePipelineState;
class ComputeShader;
class CustomPresent;
class DepthStencilState;
class GeometryShader;
class GPUFence;
class GraphicsPipelineState;
class MeshShader;
class PixelShader;
class RasterizerState;
class RayTracingGeometry;
class RayTracingPipelineState;
class RayTracingScene;
class ShaderBindingTable;
class RayTracingShader;
class RenderQuery;
class RenderQueryPool;
class Resource;
class ResourceCollection;
class SamplerState;
class Shader;
class ShaderData;
class ShaderLibrary;
class ShaderResourceView;
class ShaderBundle;
class StagingBuffer;
class Texture;
class TextureReference;
#if !defined(GP_RHI_NEW_GPU_PROFILER) || (GP_RHI_NEW_GPU_PROFILER == 0)
class TimestampCalibrationQuery;
#endif
class UniformBuffer;
class UnorderedAccessView;
class VertexDeclaration;
class VertexShader;
class ViewableResource;
class Viewport;
class WorkGraphPipelineState;
class WorkGraphShader;
class StreamSourceSlot;

struct UniformBufferLayout;

/// @section Pointers

using AmplificationShaderRef = gp::RefCountPtr<AmplificationShader>;
using BlendStateRef = gp::RefCountPtr<BlendState>;
using BoundShaderStateRef = gp::RefCountPtr<BoundShaderState>;
using BufferRef = gp::RefCountPtr<Buffer>;
using ComputePipelineStateRef = gp::RefCountPtr<ComputePipelineState>;
using ComputeShaderRef = gp::RefCountPtr<ComputeShader>;
using CustomPresentRef = gp::RefCountPtr<CustomPresent>;
using DepthStencilStateRef = gp::RefCountPtr<DepthStencilState>;
using GeometryShaderRef = gp::RefCountPtr<GeometryShader>;
using GPUFenceRef = gp::RefCountPtr<GPUFence>;
using GraphicsPipelineStateRef = gp::RefCountPtr<GraphicsPipelineState>;
using MeshShaderRef = gp::RefCountPtr<MeshShader>;
using PixelShaderRef = gp::RefCountPtr<PixelShader>;
using RasterizerStateRef = gp::RefCountPtr<RasterizerState>;
using RayTracingGeometryRef = gp::RefCountPtr<RayTracingGeometry>;
using RayTracingPipelineStateRef = gp::RefCountPtr<RayTracingPipelineState>;
using RayTracingSceneRef = gp::RefCountPtr<RayTracingScene>;
using RayTracingShaderRef = gp::RefCountPtr<RayTracingShader>;
using ShaderBindingTableRef = gp::RefCountPtr<ShaderBindingTable>;
using RenderQueryPoolRef = gp::RefCountPtr<RenderQueryPool>;
using RenderQueryRef = gp::RefCountPtr<RenderQuery>;
using RHIResourceCollectRef = gp::RefCountPtr<ResourceCollection>;
using RHIShaderLibrRef = gp::RefCountPtr<ShaderLibrary>;
using RHIShaderResourceVRef = gp::RefCountPtr<ShaderResourceView>;
using SamplerStateRef = gp::RefCountPtr<SamplerState>;
using ShaderResourceViewRef = gp::RefCountPtr<ShaderResourceView>;
using ShaderBundleRef = gp::RefCountPtr<ShaderBundle>;
using StagingBufferRef = gp::RefCountPtr<StagingBuffer>;
using TextureReferenceRef = gp::RefCountPtr<TextureReference>;
using TextureRef = gp::RefCountPtr<Texture>;
#if !defined(GP_RHI_NEW_GPU_PROFILER) || (GP_RHI_NEW_GPU_PROFILER == 0)
using TimestampCalibrationQueryRef = gp::RefCountPtr<TimestampCalibrationQuery>;
#endif
using UniformBufferLayoutRef = gp::RefCountPtr<const UniformBufferLayout>;
using UniformBufferRef = gp::RefCountPtr<UniformBuffer>;
using UnorderedAccessViewRef = gp::RefCountPtr<UnorderedAccessView>;
using VertexDeclarationRef = gp::RefCountPtr<VertexDeclaration>;
using VertexShaderRef = gp::RefCountPtr<VertexShader>;
using ViewportRef = gp::RefCountPtr<Viewport>;
using WorkGraphPipelineStateRef = gp::RefCountPtr<WorkGraphPipelineState>;
using StreamSourceSlotRef = gp::RefCountPtr<StreamSourceSlot>;
using WorkGraphShaderRef = gp::RefCountPtr<WorkGraphShader>;

}   // namespace gp::rhi
