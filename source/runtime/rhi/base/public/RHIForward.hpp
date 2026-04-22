// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

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

}   // namespace gp::rhi
