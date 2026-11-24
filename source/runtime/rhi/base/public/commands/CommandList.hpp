// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "commands/CommandPacket.hpp"
#include "commands/CommandTypes.hpp"
#include "commands/ComputeCommandList.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/LinearColor.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncForward.hpp"
#include <type_traits>
#include <utility>

namespace gp::rhi::command
{

/// @brief Command list interface exposing full graphics, render pass, draw, mesh dispatch, query, and copy recording.
/// @details Adds zero data members beyond `ComputeCommandList` / `CommandListBase`. Every method dispatches directly
///          to `getContext()` in bypass/executing mode or appends a command packet into the linear arena when queued.
class GP_RHI_API CommandList : public ComputeCommandList
{
public:
    /// @brief Constructs a graphics command list bound to `gpuMask`.
    /// @param[in] gpuMask Initial multi-GPU execution mask.
    /// @param[in] isImmediateList True only for `CommandListImmediate`.
    explicit CommandList(const GPUMask gpuMask = GPUMask::all(), const bool isImmediateList = false) noexcept
        : ComputeCommandList(gpuMask, isImmediateList)
    {}

    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
    CommandList(CommandList&&) = delete;
    CommandList& operator=(CommandList&&) = delete;

public:
    using ComputeCommandList::setBatchedShaderParameters;
    using ComputeCommandList::setBatchedShaderUnbinds;

    /// @brief Appends a lambda receiving `CommandList&` on replay (or executes immediately in bypass mode).
    /// @tparam InLambdaType Movable callable invocable with `(CommandList&)`.
    /// @param[in,out] inLambda Callable to execute.
    template <typename InLambdaType>
    void enqueueLambda(InLambdaType&& inLambda) noexcept
    {
        if (isBottomOfPipe())
        {
            inLambda(*this);
            return;
        }
        using CmdType = LambdaCommand<CommandList, std::decay_t<InLambdaType>>;
        emplaceCommand<CmdType>(std::forward<InLambdaType>(inLambda));
    }

    /// @brief Copies active render pass attachment formats, flags, and MSAA count into `psoInit`.
    /// @param[in,out] psoInit Graphics PSO initializer updated with the active render pass metadata.
    void applyCachedRenderTargets(pipeline::GraphicsPipelineStateInitializer& psoInit) const noexcept;

    /// @brief Binds a compiled graphics pipeline state object along with its dynamic stencil reference.
    /// @param[in] graphicsState Compiled graphics PSO.
    /// @param[in] stencilRef Dynamic stencil reference value.
    /// @param[in] applyAdditionalState True to re-apply bundled dynamic states.
    void setGraphicsPipelineState(
        pipeline::GraphicsPipelineState* graphicsState, gp::UInt32 stencilRef = 0u, bool applyAdditionalState = true
    ) noexcept;

    /// @brief Applies batched constant and resource bindings to a graphics shader and resets `batchedParameters`.
    /// @param[in] shader Target graphics shader.
    /// @param[in,out] batchedParameters Parameter batch consumed and reset by this call.
    void setBatchedShaderParameters(
        shader::GraphicsShader* shader, shader::BatchedShaderParameters& batchedParameters
    ) noexcept;

    /// @brief Clears active SRV/UAV slots on a graphics shader and resets `batchedUnbinds`.
    /// @param[in] shader Target graphics shader.
    /// @param[in,out] batchedUnbinds Unbind batch consumed and reset by this call.
    void setBatchedShaderUnbinds(shader::GraphicsShader* shader, shader::BatchedShaderUnbinds& batchedUnbinds) noexcept;

    /// @brief Sets the dynamic stencil reference value.
    /// @param[in] stencilRef Stencil reference value.
    void setStencilRef(gp::UInt32 stencilRef) noexcept;

    /// @brief Sets the dynamic constant blend factor color.
    /// @param[in] blendFactor RGBA linear blend factor.
    void setBlendFactor(const gp::math::LinearColor& blendFactor) noexcept;

    /// @brief Binds a vertex buffer to input assembly slot `streamIndex`.
    /// @param[in] streamIndex Zero-based vertex stream slot.
    /// @param[in] vertexBuffer Vertex buffer pointer (may be `nullptr`).
    /// @param[in] offset Byte offset into `vertexBuffer`.
    void setStreamSource(gp::UInt32 streamIndex, resource::Buffer* vertexBuffer, gp::UInt32 offset) noexcept;

    /// @brief Binds a `StreamSourceSlot` indirection object to input assembly slot `streamIndex`.
    /// @param[in] streamIndex Zero-based vertex stream slot.
    /// @param[in] slot Stream source slot pointer.
    /// @param[in] offset Additional byte offset added to `slot->getOffset()`.
    void setStreamSourceSlot(gp::UInt32 streamIndex, resource::StreamSourceSlot* slot, gp::UInt32 offset = 0u) noexcept;

    /// @brief Sets the primary rasterizer viewport rectangle and clip depth bounds.
    /// @param[in] minX Left edge in pixels.
    /// @param[in] minY Top edge in pixels.
    /// @param[in] minZ Minimum clip depth.
    /// @param[in] maxX Right edge in pixels.
    /// @param[in] maxY Bottom edge in pixels.
    /// @param[in] maxZ Maximum clip depth.
    void setViewport(
        gp::Float32 minX, gp::Float32 minY, gp::Float32 minZ, gp::Float32 maxX, gp::Float32 maxY, gp::Float32 maxZ
    ) noexcept;

    /// @brief Sets side-by-side stereo rasterizer viewports.
    /// @param[in] leftMinX Left-eye left edge.
    /// @param[in] rightMinX Right-eye left edge.
    /// @param[in] leftMinY Left-eye top edge.
    /// @param[in] rightMinY Right-eye top edge.
    /// @param[in] minZ Shared minimum clip depth.
    /// @param[in] leftMaxX Left-eye right edge.
    /// @param[in] rightMaxX Right-eye right edge.
    /// @param[in] leftMaxY Left-eye bottom edge.
    /// @param[in] rightMaxY Right-eye bottom edge.
    /// @param[in] maxZ Shared maximum clip depth.
    void setStereoViewport(
        gp::Float32 leftMinX,
        gp::Float32 rightMinX,
        gp::Float32 leftMinY,
        gp::Float32 rightMinY,
        gp::Float32 minZ,
        gp::Float32 leftMaxX,
        gp::Float32 rightMaxX,
        gp::Float32 leftMaxY,
        gp::Float32 rightMaxY,
        gp::Float32 maxZ
    ) noexcept;

    /// @brief Binds multiple rasterizer viewports simultaneously.
    /// @param[in] count Number of viewports in `data`.
    /// @param[in] data Array of `ViewportBounds`.
    void setMultipleViewports(gp::UInt32 count, const ViewportBounds* data) noexcept;

    /// @brief Enables or disables the hardware scissor test and configures its clip rectangle.
    /// @param[in] enable True to enable scissor clipping.
    /// @param[in] minX Left pixel bound.
    /// @param[in] minY Top pixel bound.
    /// @param[in] maxX Right pixel bound.
    /// @param[in] maxY Bottom pixel bound.
    void setScissorRect(bool enable, gp::UInt32 minX, gp::UInt32 minY, gp::UInt32 maxX, gp::UInt32 maxY) noexcept;

    /// @brief Sets dynamic depth bounds `[minDepth, maxDepth]`.
    /// @param[in] minDepth Minimum passing depth.
    /// @param[in] maxDepth Maximum passing depth.
    void setDepthBounds(gp::Float32 minDepth, gp::Float32 maxDepth) noexcept;

    /// @brief Sets per-draw Variable Rate Shading coarse pixel rate and combiner modes.
    /// @param[in] shadingRate Coarse pixel rate.
    /// @param[in] combiners Two-stage combiner array.
    void setShadingRate(pipeline::VRSShadingRate shadingRate, const pipeline::VRSRateCombiner (&combiners)[2]) noexcept;

    /// @brief Draws non-indexed primitives from bound vertex streams.
    /// @param[in] baseVertexIndex First vertex index.
    /// @param[in] numPrimitives Number of primitives.
    /// @param[in] numInstances Number of instances.
    void drawPrimitive(gp::UInt32 baseVertexIndex, gp::UInt32 numPrimitives, gp::UInt32 numInstances) noexcept;

    /// @brief Draws non-indexed primitives using arguments read from `argumentBuffer`.
    /// @param[in] argumentBuffer GPU indirect argument buffer.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    void drawPrimitiveIndirect(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept;

    /// @brief Draws indexed primitives using `indexBuffer`.
    /// @param[in] indexBuffer Bound index buffer.
    /// @param[in] baseVertexIndex Signed vertex offset.
    /// @param[in] firstInstance Starting instance ID.
    /// @param[in] numVertices Referenced vertex count hint.
    /// @param[in] startIndex First index read from `indexBuffer`.
    /// @param[in] numPrimitives Number of primitives.
    /// @param[in] numInstances Number of instances.
    void drawIndexedPrimitive(
        resource::Buffer* indexBuffer,
        gp::Int32 baseVertexIndex,
        gp::UInt32 firstInstance,
        gp::UInt32 numVertices,
        gp::UInt32 startIndex,
        gp::UInt32 numPrimitives,
        gp::UInt32 numInstances
    ) noexcept;

    /// @brief Draws indexed primitives using arguments read from `argumentBuffer`.
    /// @param[in] indexBuffer Bound index buffer.
    /// @param[in] argumentBuffer GPU indirect argument buffer.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    void drawIndexedPrimitiveIndirect(
        resource::Buffer* indexBuffer, resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset
    ) noexcept;

    /// @brief Issues a multi-draw indexed indirect batch.
    /// @param[in] indexBuffer Bound index buffer.
    /// @param[in] argumentBuffer GPU buffer storing indirect draw records.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    /// @param[in] countBuffer Optional GPU buffer storing the draw count.
    /// @param[in] countBufferOffset Byte offset into `countBuffer`.
    /// @param[in] maxDrawArguments Maximum indirect draws executed.
    /// @param[in] minMaterialIndex Minimum material sort index hint.
    /// @param[in] maxMaterialIndex Maximum material sort index hint.
    void multiDrawIndexedPrimitiveIndirect(
        resource::Buffer* indexBuffer,
        resource::Buffer* argumentBuffer,
        gp::UInt32 argumentOffset,
        resource::Buffer* countBuffer,
        gp::UInt32 countBufferOffset,
        gp::UInt32 maxDrawArguments,
        gp::UInt32 minMaterialIndex = 0u,
        gp::UInt32 maxMaterialIndex = 0u
    ) noexcept;

    /// @brief Dispatches a mesh-shader workgroup grid.
    /// @param[in] threadGroupCountX Thread groups along X.
    /// @param[in] threadGroupCountY Thread groups along Y.
    /// @param[in] threadGroupCountZ Thread groups along Z.
    void dispatchMeshShader(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept;

    /// @brief Dispatches an indirect mesh-shader workgroup grid.
    /// @param[in] argumentBuffer GPU indirect argument buffer.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    void dispatchIndirectMeshShader(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept;

    /// @brief Begins a graphics render pass and caches its attachment metadata for PSO setup.
    /// @param[in] inInfo Complete render pass descriptor.
    /// @param[in] name Diagnostic render pass name.
    void beginRenderPass(const RenderPassInfo& inInfo, const char* name) noexcept;

    /// @brief Ends the currently active graphics render pass.
    void endRenderPass() noexcept;

    /// @brief Advances to the next subpass inside an active render pass.
    void nextSubpass() noexcept;

    /// @brief Copies regions, mips, and slices from `sourceTexture` to `destTexture`.
    /// @param[in] sourceTexture Source texture.
    /// @param[in] destTexture Destination texture.
    /// @param[in] copyInfo Region, mip, and slice parameters.
    void copyTexture(
        resource::Texture* sourceTexture, resource::Texture* destTexture, const CopyTextureInfo& copyInfo
    ) noexcept;

    /// @brief Copies a byte region from `sourceBuffer` to `destBuffer`.
    /// @param[in] destBuffer Destination GPU buffer.
    /// @param[in] dstOffset Destination byte offset.
    /// @param[in] sourceBuffer Source GPU buffer.
    /// @param[in] srcOffset Source byte offset.
    /// @param[in] numBytes Number of bytes to copy.
    void copyBufferRegion(
        resource::Buffer* destBuffer,
        gp::UInt64 dstOffset,
        resource::Buffer* sourceBuffer,
        gp::UInt64 srcOffset,
        gp::UInt64 numBytes
    ) noexcept;

    /// @brief Resummarizes hierarchical depth metadata for `depthTexture`.
    /// @param[in] depthTexture Target depth texture.
    void resummarizeHTile(resource::Texture* depthTexture) noexcept;

    /// @brief Begins recording a hardware render query.
    /// @param[in] renderQuery Target query object.
    void beginRenderQuery(sync::RenderQuery* renderQuery) noexcept;

    /// @brief Ends recording a hardware render query.
    /// @param[in] renderQuery Target query object.
    void endRenderQuery(sync::RenderQuery* renderQuery) noexcept;
};

static_assert(
    sizeof(CommandList) == sizeof(CommandListBase), "CommandList must not add data members beyond CommandListBase."
);

}   // namespace gp::rhi::command
