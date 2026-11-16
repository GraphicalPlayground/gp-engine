// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi
{

/// @brief Defines the RHI feature level tiers supported by the engine.
enum class FeatureLevel : gp::Int32;

/// @brief Identifies a target shader compilation platform.
enum class ShaderPlatform : gp::UInt16;

/// @brief Defines pixel formats for textures, render targets, and typed views.
enum class PixelFormat : gp::UInt8;

/// @brief Bitmask flags describing the active color/depth/stencil channels of a PixelFormat.
enum class PixelFormatChannelFlags : gp::UInt8;

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

/// @brief Bitmask flags specifying buffer allocation, access, and binding usage.
enum class BufferUsageFlags : gp::UInt32;

/// @brief Bitmask flags specifying texture creation, usage, and memory properties.
enum class TextureCreateFlags : gp::UInt64;

/// @brief Identifies one of the six faces of a cubemap texture.
enum class CubeFace : gp::UInt32;

class Resource;
class ViewableResource;
class ResourceCollection;

class Buffer;
class StagingBuffer;
class UniformBuffer;

class Texture;
class TextureReference;

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

namespace system
{

class Viewport;
class CustomPresent;

}   // namespace system

}   // namespace gp::rhi
