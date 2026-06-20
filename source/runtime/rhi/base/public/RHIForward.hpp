// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi
{

enum ShaderPlatform : UInt16;
enum CubeFace : UInt32;
enum PixelFormat : UInt8;
enum class PixelFormatChannelFlags : UInt8;
enum class BufferUsageFlags : UInt32;
enum class TextureCreateFlags : UInt64;

namespace command
{

// Contexts
class IComputeContext;
class ICommandContext;
class IUploadContext;
class ContextArray;

// Command Lists
class CommandListBase;
class ComputeCommandList;
class CommandList;
class CommandListImmediate;
class SubCommandList;

}   // namespace command

namespace resource
{

class Resource;
class ViewableResource;
class ResourceCollection;

class Buffer;
class StagingBuffer;
class UniformBuffer;

class Texture;
class TextureReference;

class Viewport;
class CustomPresent;

// Resource Management / Streaming
class ResourceReplaceInfo;
class ResourceReplaceBatcher;

}   // namespace resource

namespace view
{

class ShaderResourceView;
class UnorderedAccessView;
class DescriptorRange;

}   // namespace view

namespace shader
{

class Shader;
class ShaderData;
class ShaderLibrary;
class ShaderBundle;

// Stages
class VertexShader;
class PixelShader;
class GeometryShader;
class ComputeShader;
class MeshShader;
class AmplificationShader;
class WorkGraphShader;

// Layouts
struct UniformBufferLayout;

}   // namespace shader

namespace state
{

// Fixed-Function States
class BlendState;
class DepthStencilState;
class RasterizerState;
class SamplerState;

// Initializers
struct SamplerStateInitializer;
struct RasterizerStateInitializer;
struct DepthStencilStateInitializer;
struct BlendStateInitializer;

// Input Assembly
class VertexDeclaration;
class StreamSourceSlot;

// Pipelines
class BoundShaderState;
class GraphicsPipelineState;
class ComputePipelineState;
class WorkGraphPipelineState;

}   // namespace state

namespace raytracing
{

class Geometry;
class Scene;
class Shader;
class PipelineState;
class ShaderBindingTable;

}   // namespace raytracing

namespace sync
{

class GPUFence;
class RenderQuery;
class RenderQueryPool;

}   // namespace sync

}   // namespace gp::rhi
