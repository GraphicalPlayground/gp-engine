// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"     // IWYU pragma: keep
#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "commands/CommandTypes.hpp"
#include "commands/IComputeContext.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"
#include "pipeline/PipelineStateFallbacks.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"

namespace gp::rhi::command
{

/// @brief Low-level backend graphics and compute command context implementing render passes, draws, and copies.
/// @details Extends `IComputeContext` with rasterization state, vertex stream binding, draw/mesh dispatch commands,
///          render pass lifecycle, occlusion/timestamp queries, and texture/buffer copy operations.
class GP_RHI_API ICommandContext : public IComputeContext
{
public:
    /// @brief Constructs a graphics command context.
    ICommandContext() noexcept = default;

    ICommandContext(const ICommandContext&) = delete;
    ICommandContext& operator=(const ICommandContext&) = delete;
    ICommandContext(ICommandContext&&) = delete;
    ICommandContext& operator=(ICommandContext&&) = delete;

    /// @copydoc IComputeContext::~IComputeContext()
    virtual ~ICommandContext() noexcept override = default;

public:
    /// @copydoc IComputeContext::getPipeline()
    [[nodiscard]] virtual sync::PipelineStage getPipeline() const noexcept override
    {
        return sync::PipelineStage::Graphics;
    }

    /// @brief Binds multiple rasterizer viewports simultaneously (for instanced stereo or multi-viewport rendering).
    /// @param[in] count Number of entries in `data`.
    /// @param[in] data Array of `ViewportBounds` descriptors.
    virtual void setMultipleViewports(const gp::UInt32 count, const ViewportBounds* const data) noexcept
    {
        static_cast<void>(count);
        static_cast<void>(data);
    }

    /// @brief Resummarizes hierarchical depth metadata (HTile/HiZ) for `depthTexture` after custom depth writes.
    /// @param[in] depthTexture Target depth texture.
    virtual void resummarizeHTile(resource::Texture* const depthTexture) noexcept
    {
        static_cast<void>(depthTexture);
    }

    /// @brief Begins recording a hardware render query (occlusion sample count or timestamp).
    /// @param[in] renderQuery Target query object.
    virtual void beginRenderQuery(sync::RenderQuery* renderQuery) noexcept = 0;

    /// @brief Ends recording a hardware render query.
    /// @param[in] renderQuery Target query object.
    virtual void endRenderQuery(sync::RenderQuery* renderQuery) noexcept = 0;

    /// @brief Binds a vertex buffer to input assembly stream slot `streamIndex` starting at byte `offset`.
    /// @param[in] streamIndex Zero-based vertex stream slot.
    /// @param[in] vertexBuffer Vertex buffer resource pointer (may be `nullptr` to unbind).
    /// @param[in] offset Byte offset from the start of `vertexBuffer`.
    virtual void
        setStreamSource(gp::UInt32 streamIndex, resource::Buffer* vertexBuffer, gp::UInt32 offset) noexcept = 0;

    /// @brief Sets the primary rasterizer viewport rectangle and normalized depth bounds.
    /// @param[in] minX Left edge in pixels.
    /// @param[in] minY Top edge in pixels.
    /// @param[in] minZ Minimum clip depth.
    /// @param[in] maxX Right edge in pixels.
    /// @param[in] maxY Bottom edge in pixels.
    /// @param[in] maxZ Maximum clip depth.
    virtual void setViewport(
        gp::Float32 minX, gp::Float32 minY, gp::Float32 minZ, gp::Float32 maxX, gp::Float32 maxY, gp::Float32 maxZ
    ) noexcept = 0;

    /// @brief Sets side-by-side stereo rasterizer viewports for single-pass stereo rendering.
    /// @param[in] leftMinX Left-eye left edge in pixels.
    /// @param[in] rightMinX Right-eye left edge in pixels.
    /// @param[in] leftMinY Left-eye top edge in pixels.
    /// @param[in] rightMinY Right-eye top edge in pixels.
    /// @param[in] minZ Shared minimum clip depth.
    /// @param[in] leftMaxX Left-eye right edge in pixels.
    /// @param[in] rightMaxX Right-eye right edge in pixels.
    /// @param[in] leftMaxY Left-eye bottom edge in pixels.
    /// @param[in] rightMaxY Right-eye bottom edge in pixels.
    /// @param[in] maxZ Shared maximum clip depth.
    virtual void setStereoViewport(
        const gp::Float32 leftMinX,
        const gp::Float32 rightMinX,
        const gp::Float32 leftMinY,
        const gp::Float32 rightMinY,
        const gp::Float32 minZ,
        const gp::Float32 leftMaxX,
        const gp::Float32 rightMaxX,
        const gp::Float32 leftMaxY,
        const gp::Float32 rightMaxY,
        const gp::Float32 maxZ
    ) noexcept
    {
        static_cast<void>(leftMinX);
        static_cast<void>(rightMinX);
        static_cast<void>(leftMinY);
        static_cast<void>(rightMinY);
        static_cast<void>(minZ);
        static_cast<void>(leftMaxX);
        static_cast<void>(rightMaxX);
        static_cast<void>(leftMaxY);
        static_cast<void>(rightMaxY);
        static_cast<void>(maxZ);
    }

    /// @brief Enables or disables the hardware scissor test and sets its integer clip rectangle.
    /// @param[in] enable True to enable scissor clipping; false to clip to the full viewport.
    /// @param[in] minX Left clip coordinate in pixels.
    /// @param[in] minY Top clip coordinate in pixels.
    /// @param[in] maxX Right clip coordinate in pixels.
    /// @param[in] maxY Bottom clip coordinate in pixels.
    virtual void
        setScissorRect(bool enable, gp::UInt32 minX, gp::UInt32 minY, gp::UInt32 maxX, gp::UInt32 maxY) noexcept = 0;

    /// @brief Binds an already-compiled `GraphicsPipelineState` along with its stencil reference value.
    /// @param[in] graphicsState Compiled graphics pipeline state object.
    /// @param[in] stencilRef Dynamic stencil reference value.
    /// @param[in] applyAdditionalState True to re-apply dynamic states bundled with the PSO.
    virtual void setGraphicsPipelineState(
        pipeline::GraphicsPipelineState* graphicsState, gp::UInt32 stencilRef, bool applyAdditionalState
    ) noexcept = 0;

    /// @brief Applies batched constant-buffer, resource, and bindless bindings to a graphics shader stage.
    /// @param[in] shader Target graphics shader (`VertexShader`, `PixelShader`, `GeometryShader`, `MeshShader`, or
    ///            `AmplificationShader`).
    /// @param[in] inParametersData Raw constant-data bytes referenced by `inParameters`.
    /// @param[in] inParameters Constant-buffer byte-range descriptors.
    /// @param[in] inResourceParameters Non-bindless shader resource bindings.
    /// @param[in] inBindlessParameters Bindless shader resource bindings.
    virtual void setShaderParameters(
        shader::GraphicsShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept = 0;

    using IComputeContext::setBatchedShaderParameters;
    using IComputeContext::setShaderParameters;
    using IComputeContext::setShaderUnbinds;

    /// @brief Convenience overload forwarding a `BatchedShaderParameters` packet to graphics `setShaderParameters()`
    /// and resetting it.
    /// @param[in] shader Target graphics shader.
    /// @param[in,out] batchedParameters Accumulated shader parameter batch (reset after dispatch).
    void setBatchedShaderParameters(
        shader::GraphicsShader* const shader, shader::BatchedShaderParameters& batchedParameters
    ) noexcept
    {
        setShaderParameters(
            shader,
            batchedParameters.parametersData,
            batchedParameters.parameters,
            batchedParameters.resourceParameters,
            batchedParameters.bindlessParameters
        );
        batchedParameters.reset();
    }

    /// @brief Clears active SRV or UAV bindings on a graphics shader stage.
    /// @param[in] shader Target graphics shader.
    /// @param[in] inUnbinds List of slots to unbind.
    virtual void setShaderUnbinds(
        shader::GraphicsShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept = 0;

    /// @brief Updates the dynamic stencil reference value used by stencil comparison and replace operations.
    /// @param[in] stencilRef 8-bit stencil reference value stored in a 32-bit unsigned integer.
    virtual void setStencilRef(const gp::UInt32 stencilRef) noexcept
    {
        static_cast<void>(stencilRef);
    }

    /// @brief Updates the dynamic constant blend factor color used by `BlendFactor::ConstantBlendFactor`.
    /// @param[in] blendFactor RGBA linear blend factor.
    virtual void setBlendFactor(const gp::math::LinearColor& blendFactor) noexcept
    {
        static_cast<void>(blendFactor);
    }

    /// @brief Draws non-indexed primitives from bound vertex streams.
    /// @param[in] baseVertexIndex Index of the first vertex to read.
    /// @param[in] numPrimitives Number of primitives to render.
    /// @param[in] numInstances Number of instances to draw (`>= 1`).
    virtual void
        drawPrimitive(gp::UInt32 baseVertexIndex, gp::UInt32 numPrimitives, gp::UInt32 numInstances) noexcept = 0;

    /// @brief Draws non-indexed primitives using a `DrawIndirectParameters` record in `argumentBuffer`.
    /// @param[in] argumentBuffer GPU buffer storing draw arguments.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    virtual void drawPrimitiveIndirect(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept = 0;

    /// @brief Draws indexed primitives using `indexBuffer`.
    /// @param[in] indexBuffer Bound index buffer.
    /// @param[in] baseVertexIndex Signed offset added to every fetched index.
    /// @param[in] firstInstance Starting instance ID.
    /// @param[in] numVertices Number of referenced vertices (diagnostic hint for legacy backends).
    /// @param[in] startIndex Index of the first element read from `indexBuffer`.
    /// @param[in] numPrimitives Number of primitives to render.
    /// @param[in] numInstances Number of instances to draw (`>= 1`).
    virtual void drawIndexedPrimitive(
        resource::Buffer* indexBuffer,
        gp::Int32 baseVertexIndex,
        gp::UInt32 firstInstance,
        gp::UInt32 numVertices,
        gp::UInt32 startIndex,
        gp::UInt32 numPrimitives,
        gp::UInt32 numInstances
    ) noexcept = 0;

    /// @brief Draws indexed primitives using a `DrawIndexedIndirectParameters` record in `argumentBuffer`.
    /// @param[in] indexBuffer Bound index buffer.
    /// @param[in] argumentBuffer GPU buffer storing indexed draw arguments.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    virtual void drawIndexedPrimitiveIndirect(
        resource::Buffer* indexBuffer, resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset
    ) noexcept = 0;

    /// @brief Executes up to `maxDrawArguments` indexed indirect draws from `argumentBuffer` with optional GPU draw
    ///        count buffer.
    /// @param[in] indexBuffer Bound index buffer.
    /// @param[in] argumentBuffer GPU buffer storing tightly or strided `DrawIndexedIndirectParameters` records.
    /// @param[in] argumentOffset Starting byte offset in `argumentBuffer`.
    /// @param[in] countBuffer Optional GPU buffer storing the actual draw count (`nullptr` uses `maxDrawArguments`).
    /// @param[in] countBufferOffset Byte offset into `countBuffer`.
    /// @param[in] maxDrawArguments Upper bound on indirect draw calls executed.
    /// @param[in] minMaterialIndex Minimum material sort index hint.
    /// @param[in] maxMaterialIndex Maximum material sort index hint.
    virtual void multiDrawIndexedPrimitiveIndirect(
        resource::Buffer* const indexBuffer,
        resource::Buffer* const argumentBuffer,
        const gp::UInt32 argumentOffset,
        resource::Buffer* const countBuffer,
        const gp::UInt32 countBufferOffset,
        const gp::UInt32 maxDrawArguments,
        const gp::UInt32 minMaterialIndex,
        const gp::UInt32 maxMaterialIndex
    ) noexcept
    {
        static_cast<void>(indexBuffer);
        static_cast<void>(argumentBuffer);
        static_cast<void>(argumentOffset);
        static_cast<void>(countBuffer);
        static_cast<void>(countBufferOffset);
        static_cast<void>(maxDrawArguments);
        static_cast<void>(minMaterialIndex);
        static_cast<void>(maxMaterialIndex);
    }

    /// @brief Dispatches a mesh-shader / amplification-shader workgroup grid.
    /// @param[in] threadGroupCountX Thread group count along X.
    /// @param[in] threadGroupCountY Thread group count along Y.
    /// @param[in] threadGroupCountZ Thread group count along Z.
    virtual void dispatchMeshShader(
        const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
    ) noexcept
    {
        static_cast<void>(threadGroupCountX);
        static_cast<void>(threadGroupCountY);
        static_cast<void>(threadGroupCountZ);
    }

    /// @brief Dispatches a mesh-shader workgroup grid using arguments read from `argumentBuffer`.
    /// @param[in] argumentBuffer GPU buffer storing `DispatchIndirectParameters`.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    virtual void
        dispatchIndirectMeshShader(resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset) noexcept
    {
        static_cast<void>(argumentBuffer);
        static_cast<void>(argumentOffset);
    }

    /// @brief Updates dynamic depth bounds test `[minDepth, maxDepth]` when enabled in the active PSO.
    /// @param[in] minDepth Minimum passing depth value in `[0, 1]`.
    /// @param[in] maxDepth Maximum passing depth value in `[0, 1]`.
    virtual void setDepthBounds(const gp::Float32 minDepth, const gp::Float32 maxDepth) noexcept
    {
        static_cast<void>(minDepth);
        static_cast<void>(maxDepth);
    }

    /// @brief Sets the per-draw Variable Rate Shading coarse pixel rate and combiner modes.
    /// @param[in] shadingRate Coarse pixel shading rate.
    /// @param[in] combiners Two-stage VRS combiner array (per-primitive, screen-space image).
    virtual void setShadingRate(
        const pipeline::VRSShadingRate shadingRate, const pipeline::VRSRateCombiner (&combiners)[2]
    ) noexcept
    {
        static_cast<void>(shadingRate);
        static_cast<void>(combiners);
    }

    /// @brief Begins a graphics render pass with the attachments and actions specified in `inInfo`.
    /// @param[in] inInfo Complete render pass attachment and load/store descriptor.
    /// @param[in] name Diagnostic label attached to the render pass for GPU profilers.
    virtual void beginRenderPass(const RenderPassInfo& inInfo, const char* name) noexcept = 0;

    /// @brief Ends the currently active graphics render pass, executing store and MSAA resolve actions.
    virtual void endRenderPass() noexcept = 0;

    /// @brief Advances to the next subpass inside an active tile-based render pass.
    virtual void nextSubpass() noexcept
    {}

    /// @brief Copies one or more texel regions, mip levels, and slices from `sourceTexture` to `destTexture`.
    /// @param[in] sourceTexture Source GPU texture.
    /// @param[in] destTexture Destination GPU texture.
    /// @param[in] copyInfo Region extents, mip indices, and slice counts.
    virtual void copyTexture(
        resource::Texture* sourceTexture, resource::Texture* destTexture, const CopyTextureInfo& copyInfo
    ) noexcept = 0;

    /// @brief Copies `numBytes` from `sourceBuffer + srcOffset` to `destBuffer + dstOffset` on the GPU.
    /// @param[in] destBuffer Destination GPU buffer.
    /// @param[in] dstOffset Byte offset into `destBuffer`.
    /// @param[in] sourceBuffer Source GPU buffer.
    /// @param[in] srcOffset Byte offset into `sourceBuffer`.
    /// @param[in] numBytes Number of bytes to copy.
    virtual void copyBufferRegion(
        resource::Buffer* destBuffer,
        gp::UInt64 dstOffset,
        resource::Buffer* sourceBuffer,
        gp::UInt64 srcOffset,
        gp::UInt64 numBytes
    ) noexcept = 0;

    /// @brief Returns the active `RenderPassInfo` cached inside this context.
    /// @return Immutable reference to `m_renderPassInfo`.
    [[nodiscard]] GP_FORCEINLINE_HINT const RenderPassInfo& getRenderPassInfo() const noexcept
    {
        return m_renderPassInfo;
    }

protected:
    /// @brief Cached render pass descriptor recorded on `beginRenderPass()`.
    RenderPassInfo m_renderPassInfo{};
};

/// @brief Adapter context translating monolithic PSO binds into legacy disjoint fixed-function state calls.
/// @details Backends without native PSOs (such as `rhi/null`) derive from this class together with
///          `DynamicRHIPSOFallback`. `setGraphicsPipelineState()` unpacks `GraphicsPipelineStateFallback::initializer`
///          into `setBoundShaderState()`, `setDepthStencilState()`, `setRasterizerState()`, `setBlendState()`, and
///          `enableDepthBoundsTest()`, while `setComputePipelineState()` unpacks `ComputePipelineStateFallback` into
///          `setComputeShader()`.
class GP_RHI_API ICommandContextPSOFallback : public ICommandContext
{
public:
    /// @brief Constructs a PSO-fallback command context.
    ICommandContextPSOFallback() noexcept = default;

    /// @copydoc ICommandContext::~ICommandContext()
    virtual ~ICommandContextPSOFallback() noexcept override = default;

public:
    /// @brief Binds the shader stages and vertex declaration assembled in `boundShaderState`.
    /// @param[in] boundShaderState Bound graphics shader combination.
    virtual void setBoundShaderState(pipeline::BoundShaderState* boundShaderState) noexcept = 0;

    /// @brief Binds depth-stencil fixed-function state along with its dynamic stencil reference value.
    /// @param[in] newState Target depth-stencil state object.
    /// @param[in] stencilRef Dynamic stencil reference value.
    virtual void setDepthStencilState(pipeline::DepthStencilState* newState, gp::UInt32 stencilRef) noexcept = 0;

    /// @brief Binds rasterizer fixed-function state.
    /// @param[in] newState Target rasterizer state object.
    virtual void setRasterizerState(pipeline::RasterizerState* newState) noexcept = 0;

    /// @brief Binds blend fixed-function state along with its constant blend factor color.
    /// @param[in] newState Target blend state object.
    /// @param[in] blendFactor Constant RGBA blend factor.
    virtual void setBlendState(pipeline::BlendState* newState, const gp::math::LinearColor& blendFactor) noexcept = 0;

    /// @brief Enables or disables hardware depth bounds testing and sets its `[minDepth, maxDepth]` interval.
    /// @param[in] enable True to enable depth bounds testing.
    /// @param[in] minDepth Minimum passing depth value.
    /// @param[in] maxDepth Maximum passing depth value.
    virtual void enableDepthBoundsTest(bool enable, gp::Float32 minDepth, gp::Float32 maxDepth) noexcept = 0;

    /// @brief Binds a compute shader directly for backends without native compute pipeline state objects.
    /// @param[in] computeShader Target compute shader.
    virtual void setComputeShader(shader::ComputeShader* computeShader) noexcept = 0;

public:
    /// @copydoc ICommandContext::setGraphicsPipelineState()
    virtual void setGraphicsPipelineState(
        pipeline::GraphicsPipelineState* const graphicsState,
        const gp::UInt32 stencilRef,
        const bool applyAdditionalState
    ) noexcept override
    {
        static_cast<void>(applyAdditionalState);
        if (graphicsState == nullptr)
        {
            return;
        }

        const pipeline::GraphicsPipelineStateFallback* const fallback =
            static_cast<const pipeline::GraphicsPipelineStateFallback*>(graphicsState);
        const pipeline::GraphicsPipelineStateInitializer& init = fallback->initializer;

        if (fallback->boundShaderState.isValid())
        {
            setBoundShaderState(fallback->boundShaderState.get());
        }
        setDepthStencilState(init.depthStencilState, stencilRef);
        setRasterizerState(init.rasterizerState);
        setBlendState(init.blendState, gp::math::LinearColor{ 1.0f, 1.0f, 1.0f, 1.0f });
        enableDepthBoundsTest(init.depthBounds, 0.0f, 1.0f);
    }

    /// @copydoc IComputeContext::setComputePipelineState()
    virtual void setComputePipelineState(pipeline::ComputePipelineState* const computePipelineState) noexcept override
    {
        if (computePipelineState == nullptr)
        {
            return;
        }

        const pipeline::ComputePipelineStateFallback* const fallback =
            static_cast<const pipeline::ComputePipelineStateFallback*>(computePipelineState);
        setComputeShader(fallback->getComputeShader());
    }
};

}   // namespace gp::rhi::command
