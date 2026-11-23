// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryForward.hpp"

namespace gp::rhi::pipeline
{

/// @brief Texture sampling filter mode.
enum class SamplerFilter : gp::UInt8;

/// @brief Texture coordinate wrapping behavior outside `[0, 1]`.
enum class SamplerAddressMode : gp::UInt8;

/// @brief Depth comparison mode for shadow/comparison samplers.
enum class SamplerCompareFunction : gp::UInt8;

/// @brief Polygon rasterization fill mode.
enum class RasterizerFillMode : gp::UInt8;

/// @brief Triangle face culling mode.
enum class RasterizerCullMode : gp::UInt8;

/// @brief Depth clipping vs. clamping behavior outside the near/far planes.
enum class RasterizerDepthClipMode : gp::UInt8;

/// @brief Per-channel color write enable mask for blend targets.
enum class ColorWriteMask : gp::UInt8;

/// @brief Comparison operator used by depth and stencil tests.
enum class CompareFunction : gp::UInt8;

/// @brief Predefined stencil bitmask presets.
enum class StencilMask : gp::UInt8;

/// @brief Operation applied to the stencil buffer on test pass or fail.
enum class StencilOp : gp::UInt8;

/// @brief Arithmetic operation combining source and destination blend terms.
enum class BlendOperation : gp::UInt8;

/// @brief Multiplier applied to source or destination channels during blending.
enum class BlendFactor : gp::UInt8;

/// @brief Component format of a vertex input attribute.
enum class VertexElementType : gp::UInt8;

/// @brief Coarse geometry topology class specified on a graphics pipeline state.
enum class PrimitiveTopologyType : gp::UInt8;

/// @brief Exact primitive topology passed to draw commands.
enum class PrimitiveType : gp::UInt8;

/// @brief Per-axis pixel coarseness for Variable Rate Shading.
enum class VRSAxisShadingRate : gp::UInt8;

/// @brief Combined X/Y shading rate tile dimensions for Variable Rate Shading.
enum class VRSShadingRate : gp::UInt8;

/// @brief Combiner applied between draw-level, primitive-level, and image-level VRS rates.
enum class VRSRateCombiner : gp::UInt8;

/// @brief Encoding format of a screen-space VRS rate attachment.
enum class VRSImageDataType : gp::UInt8;

/// @brief Creation parameters for a GPU texture sampler state.
struct SamplerStateDesc;

/// @brief Creation parameters for a fixed-function polygon rasterizer state.
struct RasterizerStateDesc;

/// @brief Creation parameters for a depth and stencil test state.
struct DepthStencilStateDesc;

/// @brief Per-render-target blend configuration inside `BlendStateDesc`.
struct RenderTargetBlendDesc;

/// @brief Creation parameters for a multi-target color blend state.
struct BlendStateDesc;

/// @brief Single vertex buffer attribute layout entry.
struct VertexElement;

/// @brief Per-pipeline usage counters tracking frame activity and total bind count.
struct PipelineStateStats;

/// @brief Input shader stage set and vertex layout used to build a bound shader state or graphics pipeline.
class BoundShaderStateInput;

/// @brief Immutable GPU texture sampler state resource.
class SamplerState;

/// @brief Immutable GPU rasterizer state resource.
class RasterizerState;

/// @brief Immutable GPU depth-stencil state resource.
class DepthStencilState;

/// @brief Immutable GPU blend state resource.
class BlendState;

/// @brief Immutable GPU vertex input layout resource.
class VertexDeclaration;

/// @brief Compiled combination of graphics shader stages and vertex input declaration.
class BoundShaderState;

/// @brief Compiled graphics pipeline state object combining shaders, fixed-function states, and target formats.
class GraphicsPipelineState;

/// @brief Compiled compute pipeline state object wrapping a compute shader stage.
class ComputePipelineState;

/// @brief Compiled ray tracing pipeline state object.
class RayTracingPipelineState;

/// @brief Compiled GPU work graph pipeline state object.
class WorkGraphPipelineState;

/// @brief Intrusive smart pointer handle to a `SamplerState`.
using SamplerStateRef = gp::RefCountedPtr<SamplerState, gp::DefaultRefCountPolicy<SamplerState>>;

/// @brief Intrusive smart pointer handle to a `RasterizerState`.
using RasterizerStateRef = gp::RefCountedPtr<RasterizerState, gp::DefaultRefCountPolicy<RasterizerState>>;

/// @brief Intrusive smart pointer handle to a `DepthStencilState`.
using DepthStencilStateRef = gp::RefCountedPtr<DepthStencilState, gp::DefaultRefCountPolicy<DepthStencilState>>;

/// @brief Intrusive smart pointer handle to a `BlendState`.
using BlendStateRef = gp::RefCountedPtr<BlendState, gp::DefaultRefCountPolicy<BlendState>>;

/// @brief Intrusive smart pointer handle to a `VertexDeclaration`.
using VertexDeclarationRef = gp::RefCountedPtr<VertexDeclaration, gp::DefaultRefCountPolicy<VertexDeclaration>>;

/// @brief Intrusive smart pointer handle to a `BoundShaderState`.
using BoundShaderStateRef = gp::RefCountedPtr<BoundShaderState, gp::DefaultRefCountPolicy<BoundShaderState>>;

/// @brief Intrusive smart pointer handle to a `GraphicsPipelineState`.
using GraphicsPipelineStateRef =
    gp::RefCountedPtr<GraphicsPipelineState, gp::DefaultRefCountPolicy<GraphicsPipelineState>>;

/// @brief Intrusive smart pointer handle to a `ComputePipelineState`.
using ComputePipelineStateRef =
    gp::RefCountedPtr<ComputePipelineState, gp::DefaultRefCountPolicy<ComputePipelineState>>;

/// @brief Intrusive smart pointer handle to a `RayTracingPipelineState`.
using RayTracingPipelineStateRef =
    gp::RefCountedPtr<RayTracingPipelineState, gp::DefaultRefCountPolicy<RayTracingPipelineState>>;

/// @brief Intrusive smart pointer handle to a `WorkGraphPipelineState`.
using WorkGraphPipelineStateRef =
    gp::RefCountedPtr<WorkGraphPipelineState, gp::DefaultRefCountPolicy<WorkGraphPipelineState>>;

}   // namespace gp::rhi::pipeline
