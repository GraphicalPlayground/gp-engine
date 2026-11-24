// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "commands/CommandListBase.hpp"
#include "commands/CommandPacket.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/vector/Vector4.hpp"
#include "pipeline/PipelineForward.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include "sync/TransitionDesc.hpp"
#include "views/ViewForward.hpp"
#include <type_traits>
#include <utility>

namespace gp::rhi::command
{

/// @brief Command list interface exposing compute dispatches, barriers, UAV clears, fences, and shader parameters.
/// @details Adds zero data members on top of `CommandListBase`. Every method dispatches directly to
///          `getComputeContext()` when `bypass()` or `isExecuting()` is true, or appends a typed command packet into
///          `CommandListBase`'s linear arena when recording asynchronously.
class GP_RHI_API ComputeCommandList : public CommandListBase
{
public:
    /// @brief Constructs a compute command list bound to `gpuMask`.
    /// @param[in] gpuMask Initial multi-GPU execution mask.
    /// @param[in] isImmediateList True only for `CommandListImmediate`.
    explicit ComputeCommandList(const GPUMask gpuMask = GPUMask::all(), const bool isImmediateList = false) noexcept
        : CommandListBase(gpuMask, isImmediateList)
    {}

    ComputeCommandList(const ComputeCommandList&) = delete;
    ComputeCommandList& operator=(const ComputeCommandList&) = delete;
    ComputeCommandList(ComputeCommandList&&) = delete;
    ComputeCommandList& operator=(ComputeCommandList&&) = delete;

public:
    /// @brief Appends a lambda receiving `ComputeCommandList&` on replay (or executes immediately in bypass mode).
    /// @tparam InLambdaType Movable callable invocable with `(ComputeCommandList&)`.
    /// @param[in,out] inLambda Callable to execute.
    template <typename InLambdaType>
    void enqueueLambda(InLambdaType&& inLambda) noexcept
    {
        if (isBottomOfPipe())
        {
            inLambda(*this);
            return;
        }
        using CmdType = LambdaCommand<ComputeCommandList, std::decay_t<InLambdaType>>;
        emplaceCommand<CmdType>(std::forward<InLambdaType>(inLambda));
    }

    /// @brief Binds a compute pipeline state object for subsequent dispatches.
    /// @param[in] computePipelineState Target compute PSO.
    void setComputePipelineState(pipeline::ComputePipelineState* computePipelineState) noexcept;

    /// @brief Dispatches a compute thread group grid `(threadGroupCountX, threadGroupCountY, threadGroupCountZ)`.
    /// @param[in] threadGroupCountX Thread group count along X.
    /// @param[in] threadGroupCountY Thread group count along Y.
    /// @param[in] threadGroupCountZ Thread group count along Z.
    void dispatchComputeShader(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept;

    /// @brief Dispatches an indirect compute grid using arguments in `argumentBuffer` at `argumentOffset`.
    /// @param[in] argumentBuffer GPU buffer storing `DispatchIndirectParameters`.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    void dispatchIndirectComputeShader(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept;

    /// @brief Sets the async compute occupancy budget hint.
    /// @param[in] budget Async compute occupancy hint.
    void setAsyncComputeBudget(AsyncComputeBudget budget) noexcept;

    /// @brief Sets the synchronous compute occupancy budget hint.
    /// @param[in] budget Synchronous compute occupancy hint.
    void setComputeBudget(SyncComputeBudget budget) noexcept;

    /// @brief Sets the low-latency compute scheduling hint.
    /// @param[in] lowLatency True to request low-latency scheduling.
    void setLowLatencyHint(bool lowLatency) noexcept;

    /// @brief Begins a batch of split transitions on the active pipeline.
    /// @param[in] transitions Span of active `sync::Transition` pointers.
    void beginTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept;

    /// @brief Completes a batch of split transitions on the active pipeline.
    /// @param[in] transitions Span of active `sync::Transition` pointers.
    void endTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept;

    /// @brief Begins a single split transition on the active pipeline.
    /// @param[in] transition Active `sync::Transition` pointer.
    void beginTransition(const sync::Transition* transition) noexcept;

    /// @brief Completes a single split transition on the active pipeline.
    /// @param[in] transition Active `sync::Transition` pointer.
    void endTransition(const sync::Transition* transition) noexcept;

    /// @brief Executes an immediate single-pipeline barrier batch for `infos` on the active pipeline.
    /// @param[in] infos Per-resource transition descriptors.
    /// @param[in] flags Optional transition creation flags.
    void transition(
        gp::ConstVectorView<sync::TransitionInfo> infos,
        sync::TransitionCreateFlags flags = sync::TransitionCreateFlags::None
    ) noexcept;

    /// @brief Executes an immediate single-resource barrier on the active pipeline.
    /// @param[in] info Single resource transition descriptor.
    /// @param[in] flags Optional transition creation flags.
    void transition(
        const sync::TransitionInfo& info, sync::TransitionCreateFlags flags = sync::TransitionCreateFlags::None
    ) noexcept;

    /// @brief Executes an immediate transition batch across explicit source and destination pipelines.
    /// @param[in] infos Per-resource transition descriptors.
    /// @param[in] srcPipelines Source execution pipelines.
    /// @param[in] dstPipelines Destination execution pipelines.
    /// @param[in] flags Optional transition creation flags.
    void transition(
        gp::ConstVectorView<sync::TransitionInfo> infos,
        sync::PipelineStage srcPipelines,
        sync::PipelineStage dstPipelines,
        sync::TransitionCreateFlags flags = sync::TransitionCreateFlags::None
    ) noexcept;

    /// @brief Updates tracked access state on one or more viewable resources on the RHI timeline.
    /// @param[in] infos Tracked access update records.
    void setTrackedAccess(gp::ConstVectorView<sync::TrackedAccessInfo> infos) noexcept;

    /// @brief Updates tracked access state from transition descriptors targeting `pipelines`.
    /// @param[in] infos Per-resource transition descriptors whose `accessAfter` state becomes active.
    /// @param[in] pipelines Pipeline stages holding the post-transition access.
    void setTrackedAccess(gp::ConstVectorView<sync::TransitionInfo> infos, sync::PipelineStage pipelines) noexcept;

    /// @brief Clears a UAV to a four-component floating-point vector.
    /// @param[in] unorderedAccessView Target UAV.
    /// @param[in] values Clear value vector.
    void clearUAVFloat(
        view::UnorderedAccessView* unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
    ) noexcept;

    /// @brief Clears a UAV to a four-component unsigned integer vector.
    /// @param[in] unorderedAccessView Target UAV.
    /// @param[in] values Clear value vector.
    void clearUAVUint(view::UnorderedAccessView* unorderedAccessView, const resource::UIntVector4& values) noexcept;

    /// @brief Sets inline 4x32-bit shader root constants.
    /// @param[in] constants Root constant values.
    void setShaderRootConstants(const resource::UIntVector4& constants) noexcept;

    /// @brief Applies batched constant and resource bindings to `shader` and resets `batchedParameters`.
    /// @param[in] shader Target compute shader.
    /// @param[in,out] batchedParameters Parameter batch consumed and reset by this call.
    void setBatchedShaderParameters(
        shader::ComputeShader* shader, shader::BatchedShaderParameters& batchedParameters
    ) noexcept;

    /// @brief Clears active SRV/UAV slots on `shader` and resets `batchedUnbinds`.
    /// @param[in] shader Target compute shader.
    /// @param[in,out] batchedUnbinds Unbind batch consumed and reset by this call.
    void setBatchedShaderUnbinds(shader::ComputeShader* shader, shader::BatchedShaderUnbinds& batchedUnbinds) noexcept;

    /// @brief Binds a set of global static uniform buffers.
    /// @param[in] uniformBuffers Static uniform buffer binding set.
    void setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept;

    /// @brief Binds a single global static uniform buffer slot.
    /// @param[in] slot Static slot index.
    /// @param[in] uniformBuffer Uniform buffer pointer, or `nullptr` to clear.
    void
        setStaticUniformBuffer(resource::UniformBufferStaticSlot slot, resource::UniformBuffer* uniformBuffer) noexcept;

    /// @brief Sets a dynamic byte offset on `uniformBuffer`.
    /// @param[in] uniformBuffer Target uniform buffer.
    /// @param[in] offset Byte offset.
    void setUniformBlockOffset(resource::UniformBuffer* uniformBuffer, gp::UInt32 offset) noexcept;

    /// @brief Copies GPU buffer bytes into a CPU-readable staging buffer.
    /// @param[in] sourceBuffer Source GPU buffer.
    /// @param[in] destinationStagingBuffer Destination staging buffer.
    /// @param[in] offset Byte offset in `sourceBuffer`.
    /// @param[in] numBytes Number of bytes to copy.
    void copyToStagingBuffer(
        resource::Buffer* sourceBuffer,
        resource::StagingBuffer* destinationStagingBuffer,
        gp::UInt32 offset,
        gp::UInt32 numBytes
    ) noexcept;

    /// @brief Signals a GPU fence at the current point in the command stream.
    /// @param[in] fence Target GPU fence.
    void writeGPUFence(sync::GPUFence* fence) noexcept;

    /// @brief Changes the active multi-GPU node mask for subsequent commands on this command list.
    /// @param[in] gpuMask Target multi-GPU node mask.
    void setGPUMask(GPUMask gpuMask) noexcept;

    /// @brief Notifies the backend context to invalidate cached state after external native commands.
    void postExternalCommandsReset() noexcept;
};

static_assert(
    sizeof(ComputeCommandList) == sizeof(CommandListBase),
    "ComputeCommandList must not add data members beyond CommandListBase."
);

}   // namespace gp::rhi::command
