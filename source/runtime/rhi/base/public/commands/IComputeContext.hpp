// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "commands/CommandForward.hpp"       // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/Breadcrumbs.hpp"
#include "diagnostics/DiagnosticsForward.hpp"
#include "maths/vector/Vector4.hpp"
#include "pipeline/PipelineForward.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include "sync/TransitionDesc.hpp"
#include "views/ViewForward.hpp"

namespace gp::rhi::validation
{
class Tracker;
}   // namespace gp::rhi::validation

namespace gp::rhi::command
{

/// @brief Opaque finalized backend command list returned by `DynamicRHI::finalizeContexts()`.
/// @details Backend subclasses store native command buffer handles (such as `ID3D12CommandList*` or
///          `VkCommandBuffer`) and are submitted to hardware queues through `DynamicRHI::submitCommandLists()`.
class GP_RHI_API IPlatformCommandList
{
public:
    /// @brief Virtual destructor allowing backend-owned cleanup on submission completion.
    virtual ~IPlatformCommandList() noexcept = default;

#if GP_WITH_RHI_BREADCRUMBS
    /// @brief Breadcrumb allocators kept alive until this platform command list completes execution/profiling.
    diagnostics::BreadcrumbAllocatorArray breadcrumbAllocators{};

    /// @brief Inclusive GPU breadcrumb node range recorded into this platform command list.
    diagnostics::BreadcrumbRange breadcrumbRange{};
#endif
};

/// @brief Base tag interface for backend-owned resource upload contexts.
/// @details Backends that batch buffer and texture staging uploads derive from this interface and return an instance
///          via `DynamicRHI::acquireUploadContext()`.
class GP_RHI_API IUploadContext
{
public:
    /// @brief Virtual destructor.
    virtual ~IUploadContext() noexcept = default;
};

/// @brief Low-level backend execution context recording compute dispatches, barriers, UAV clears, and resource binds.
/// @details Every RHI backend implements at least the derived `ICommandContext` for graphics + compute queues and may
///          optionally provide a dedicated `IComputeContext` for asynchronous compute queues. `CommandListBase` and
///          `ComputeCommandList` execute either directly into this interface (bypass mode) or replay recorded command
///          packets into it during `CommandListExecutor::executeList()`.
class GP_RHI_API IComputeContext
{
public:
    /// @brief Constructs a compute context bound to `GPUMask::gpu0()` with no active executing command list.
    IComputeContext() noexcept = default;

    IComputeContext(const IComputeContext&) = delete;
    IComputeContext& operator=(const IComputeContext&) = delete;
    IComputeContext(IComputeContext&&) = delete;
    IComputeContext& operator=(IComputeContext&&) = delete;

    /// @brief Virtual destructor.
    virtual ~IComputeContext() noexcept = default;

public:
    /// @brief Returns the execution pipeline served by this context (`AsyncCompute` by default).
    /// @return `sync::PipelineStage::AsyncCompute` on compute contexts or `sync::PipelineStage::Graphics` on graphics
    ///         contexts.
    [[nodiscard]] virtual sync::PipelineStage getPipeline() const noexcept
    {
        return sync::PipelineStage::AsyncCompute;
    }

    /// @brief Returns the lowest-level concrete context if this context is wrapped by a validation or replay layer.
    /// @return Non-null pointer to the underlying hardware context (defaults to `this`).
    [[nodiscard]] virtual IComputeContext& getLowestLevelContext() noexcept
    {
        return *this;
    }

    /// @brief Returns the outermost wrapper context (such as a validation proxy) if attached.
    /// @return Non-null reference to the highest-level context (defaults to `this`).
    [[nodiscard]] virtual IComputeContext& getHighestLevelContext() noexcept
    {
        return (m_wrappingContext != nullptr) ? *m_wrappingContext : *this;
    }

    /// @brief Links an outer wrapper context (such as `ValidationContext`) around this concrete hardware context.
    /// @param[in] wrappingContext Outer wrapper context pointer, or `nullptr` to detach.
    void linkToWrappingContext(IComputeContext* const wrappingContext) noexcept
    {
        m_wrappingContext = wrappingContext;
    }

    /// @brief Returns the attached outer wrapper context, or `nullptr` if unwrapped.
    /// @return Pointer to the outer wrapper context.
    [[nodiscard]] GP_FORCEINLINE_HINT IComputeContext* getWrappingContext() const noexcept
    {
        return m_wrappingContext;
    }

#if GP_ENABLE_RHI_VALIDATION
    /// @brief Attaches the RHI validation state tracker active on this context.
    /// @param[in] tracker Validation tracker pointer, or `nullptr`.
    void setValidationTracker(validation::Tracker* const tracker) noexcept
    {
        m_tracker = tracker;
    }

    /// @brief Returns the RHI validation state tracker attached to this context, or `nullptr`.
    /// @return Active `validation::Tracker*`.
    [[nodiscard]] GP_FORCEINLINE_HINT validation::Tracker* getValidationTracker() const noexcept
    {
        return m_tracker;
    }
#endif

    /// @brief Accumulates per-context draw and primitive counters for GPU profiler stats.
    /// @param[in] numDraws Number of draw calls to add.
    /// @param[in] numPrimitives Number of primitives rendered to add.
    GP_FORCEINLINE_HINT void recordDrawStats(const gp::UInt32 numDraws, const gp::UInt32 numPrimitives) noexcept
    {
        m_statDraws += numDraws;
        m_statPrimitives += numPrimitives;
    }

    /// @brief Accumulates per-context dispatch counters for GPU profiler stats.
    /// @param[in] numDispatches Number of compute/mesh dispatches to add.
    GP_FORCEINLINE_HINT void recordDispatchStats(const gp::UInt32 numDispatches = 1u) noexcept
    {
        m_statDispatches += numDispatches;
    }

    /// @brief Resets accumulated per-context draw, primitive, and dispatch stats.
    GP_FORCEINLINE_HINT void resetContextStats() noexcept
    {
        m_statDraws = 0u;
        m_statPrimitives = 0u;
        m_statDispatches = 0u;
    }

    /// @brief Returns the accumulated draw count recorded on this context.
    /// @return Draw count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getContextDrawCount() const noexcept
    {
        return m_statDraws;
    }

    /// @brief Returns the accumulated primitive count recorded on this context.
    /// @return Primitive count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getContextPrimitiveCount() const noexcept
    {
        return m_statPrimitives;
    }

    /// @brief Returns the accumulated dispatch count recorded on this context.
    /// @return Dispatch count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getContextDispatchCount() const noexcept
    {
        return m_statDispatches;
    }

    /// @brief Associates the `CommandListBase` currently replaying commands into this context.
    /// @param[in] cmdList Executing command list pointer, or `nullptr` when idle.
    virtual void setExecutingCommandList(CommandListBase* const cmdList) noexcept
    {
        m_executingCommandList = cmdList;
    }

    /// @brief Returns the `CommandListBase` currently executing on this context, or `nullptr` when idle.
    /// @return Active executing command list pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT CommandListBase* getExecutingCommandList() const noexcept
    {
        return m_executingCommandList;
    }

    /// @brief Binds the active compute pipeline state for subsequent compute dispatches.
    /// @param[in] computePipelineState Target compute pipeline state object.
    virtual void setComputePipelineState(pipeline::ComputePipelineState* computePipelineState) noexcept = 0;

    /// @brief Dispatches a compute thread group grid of dimensions `(threadGroupCountX, threadGroupCountY,
    ///        threadGroupCountZ)`.
    /// @param[in] threadGroupCountX Thread group count along X.
    /// @param[in] threadGroupCountY Thread group count along Y.
    /// @param[in] threadGroupCountZ Thread group count along Z.
    virtual void dispatchComputeShader(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept = 0;

    /// @brief Dispatches a compute shader whose thread group counts are read from `argumentBuffer` at `argumentOffset`.
    /// @param[in] argumentBuffer GPU buffer storing a `DispatchIndirectParameters` record.
    /// @param[in] argumentOffset Byte offset into `argumentBuffer`.
    virtual void
        dispatchIndirectComputeShader(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept = 0;

    /// @brief Hints how aggressively asynchronous compute work may compete with graphics work on the GPU.
    /// @param[in] budget Async compute occupancy hint.
    virtual void setAsyncComputeBudget(AsyncComputeBudget budget) noexcept
    {
        static_cast<void>(budget);
    }

    /// @brief Hints how synchronous compute work should balance occupancy against graphics work.
    /// @param[in] budget Synchronous compute occupancy hint.
    virtual void setComputeBudget(SyncComputeBudget budget) noexcept
    {
        static_cast<void>(budget);
    }

    /// @brief Hints that subsequent dispatches prefer low-latency scheduling over maximum throughput.
    /// @param[in] lowLatency True to request low-latency scheduling.
    virtual void setLowLatencyHint(const bool lowLatency) noexcept
    {
        static_cast<void>(lowLatency);
    }

    /// @brief Begins a batch of split resource transitions on this context's pipeline.
    /// @param[in] transitions Contiguous span of active `sync::Transition` pointers.
    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept = 0;

    /// @brief Completes a batch of split resource transitions on this context's pipeline.
    /// @param[in] transitions Contiguous span of active `sync::Transition` pointers.
    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept = 0;

    /// @brief Clears an unordered access view to the four-component floating-point vector `values`.
    /// @param[in] unorderedAccessView Target UAV to clear.
    /// @param[in] values Clear color/value vector `(x, y, z, w)`.
    virtual void clearUAVFloat(
        view::UnorderedAccessView* unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
    ) noexcept = 0;

    /// @brief Clears an unordered access view to the four-component unsigned integer vector `values`.
    /// @param[in] unorderedAccessView Target UAV to clear.
    /// @param[in] values Clear value vector `(x, y, z, w)`.
    virtual void
        clearUAVUint(view::UnorderedAccessView* unorderedAccessView, const resource::UIntVector4& values) noexcept = 0;

    /// @brief Updates inline root constants exposed directly to shaders without allocating a constant buffer.
    /// @param[in] constants Four 32-bit unsigned root constant values.
    virtual void setShaderRootConstants(const resource::UIntVector4& constants) noexcept
    {
        static_cast<void>(constants);
    }

    /// @brief Applies batched constant-buffer, resource, and bindless bindings to `shader`.
    /// @param[in] shader Target compute shader.
    /// @param[in] inParametersData Raw constant-data bytes referenced by `inParameters`.
    /// @param[in] inParameters Constant-buffer byte-range descriptors.
    /// @param[in] inResourceParameters Non-bindless shader resource bindings.
    /// @param[in] inBindlessParameters Bindless shader resource bindings.
    virtual void setShaderParameters(
        shader::ComputeShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept = 0;

    /// @brief Convenience overload forwarding a `BatchedShaderParameters` packet to `setShaderParameters()` and
    /// resetting it.
    /// @param[in] shader Target compute shader.
    /// @param[in,out] batchedParameters Accumulated shader parameter batch (reset after dispatch).
    void setBatchedShaderParameters(
        shader::ComputeShader* const shader, shader::BatchedShaderParameters& batchedParameters
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

    /// @brief Clears active SRV or UAV bindings on `shader`.
    /// @param[in] shader Target compute shader.
    /// @param[in] inUnbinds List of slots to unbind.
    virtual void setShaderUnbinds(
        shader::ComputeShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept = 0;

    /// @brief Binds a batch of static global uniform buffers to their registered static slots.
    /// @param[in] uniformBuffers Static uniform buffer binding set.
    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept = 0;

    /// @brief Binds a single uniform buffer to static slot `slot`.
    /// @param[in] slot Static slot index.
    /// @param[in] uniformBuffer Uniform buffer pointer, or `nullptr` to unbind.
    virtual void setStaticUniformBuffer(
        const resource::UniformBufferStaticSlot slot, resource::UniformBuffer* const uniformBuffer
    ) noexcept
    {
        shader::UniformBufferStaticBindings bindings{};
        bindings.addUniformBufferAtSlot(slot, uniformBuffer);
        setStaticUniformBuffers(bindings);
    }

    /// @brief Sets a dynamic byte offset applied when reading uniform block `srv`.
    /// @param[in] srv Shader resource view backing the uniform block.
    /// @param[in] offset Byte offset into `srv`.
    virtual void setUniformBlockOffset(view::ShaderResourceView* const srv, const gp::UInt32 offset) noexcept
    {
        static_cast<void>(srv);
        static_cast<void>(offset);
    }

    /// @brief Copies `numBytes` starting at `offset` from GPU `sourceBuffer` into CPU-readable
    /// `destinationStagingBuffer`.
    /// @param[in] sourceBuffer Source GPU buffer.
    /// @param[in] destinationStagingBuffer Destination staging buffer.
    /// @param[in] offset Byte offset in `sourceBuffer`.
    /// @param[in] numBytes Number of bytes to copy.
    virtual void copyToStagingBuffer(
        resource::Buffer* sourceBuffer,
        resource::StagingBuffer* destinationStagingBuffer,
        gp::UInt32 offset,
        gp::UInt32 numBytes
    ) noexcept = 0;

    /// @brief Enqueues a GPU signal operation on `fence` at the current point in the command stream.
    /// @param[in] fence Target GPU fence to signal.
    virtual void writeGPUFence(sync::GPUFence* fence) noexcept = 0;

    /// @brief Selects which GPU nodes execute subsequent commands recorded on this context.
    /// @param[in] gpuMask Target multi-GPU node mask.
    virtual void setGPUMask(const GPUMask gpuMask) noexcept
    {
        m_gpuMask = gpuMask;
    }

    /// @brief Returns the active GPU node mask for this context.
    /// @return Active `GPUMask`.
    [[nodiscard]] virtual GPUMask getGPUMask() const noexcept
    {
        return m_gpuMask;
    }

    /// @brief Resets cached backend state after external native commands ran on the underlying hardware queue.
    virtual void postExternalCommandsReset() noexcept
    {}

    /// @brief Permits overlapping UAV writes across subsequent dispatches/draws without implicit UAV barriers.
    virtual void beginUAVOverlap() noexcept
    {}

    /// @brief Ends global UAV overlap and restores implicit UAV synchronization between dispatches/draws.
    virtual void endUAVOverlap() noexcept
    {}

    /// @brief Permits overlapping UAV writes on the explicit set of `uavs` without implicit UAV barriers.
    /// @param[in] uavs Span of UAVs allowed to overlap across dispatches/draws.
    virtual void beginUAVOverlap(const gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept
    {
        static_cast<void>(uavs);
    }

    /// @brief Ends overlapping UAV writes on the explicit set of `uavs`.
    /// @param[in] uavs Span of UAVs exiting overlap mode.
    virtual void endUAVOverlap(const gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept
    {
        static_cast<void>(uavs);
    }

#if GP_WITH_RHI_BREADCRUMBS
    /// @brief Emits a GPU-side entry marker for `node` into the underlying hardware command buffer.
    /// @param[in] node Non-null breadcrumb node being entered.
    virtual void beginBreadcrumbGPU(diagnostics::BreadcrumbNode* const node) noexcept
    {
        static_cast<void>(node);
    }

    /// @brief Emits a GPU-side exit marker for `node` into the underlying hardware command buffer.
    /// @param[in] node Non-null breadcrumb node being exited.
    virtual void endBreadcrumbGPU(diagnostics::BreadcrumbNode* const node) noexcept
    {
        static_cast<void>(node);
    }
#endif

    /// @brief Reports whether GPU breadcrumb commands should be emitted into this context.
    /// @return True when GPU breadcrumb emission is active for this context.
    [[nodiscard]] virtual bool shouldEmitBreadcrumbs() const noexcept
    {
        return true;
    }

    /// @brief Updates tracked access and pipeline state from a `sync::TrackedAccessInfo` descriptor.
    /// @details Virtual so validation wrapper contexts can intercept and audit tracked-access transitions.
    /// @param[in] info Target resource, access mask, and pipeline stage mask.
    virtual void setTrackedAccess(const sync::TrackedAccessInfo& info) noexcept
    {
        if (info.resource != nullptr)
        {
            info.resource->setTrackedAccessFromContext(sync::TrackedAccess(info.access, info.pipelines));
        }
    }

    /// @brief Updates the tracked access and pipeline state stored on a `ViewableResource`.
    /// @param[in] resource Target viewable resource (ignored if null).
    /// @param[in] access New GPU resource access state.
    /// @param[in] pipelines Execution pipelines holding `access`.
    void setTrackedAccess(
        resource::ViewableResource* const resource,
        const sync::ResourceAccess access,
        const sync::PipelineStage pipelines
    ) noexcept
    {
        setTrackedAccess(sync::TrackedAccessInfo(resource, access, pipelines));
    }

    /// @brief Reads the tracked access state stored on `resource` from this executing context.
    /// @param[in] resource Target viewable resource (returns `ResourceAccess::Unknown` if null).
    /// @return Current tracked `sync::ResourceAccess`.
    [[nodiscard]] sync::ResourceAccess getTrackedAccess(const resource::ViewableResource* const resource) const noexcept
    {
        return (resource != nullptr) ? resource->getTrackedAccessForContext() : sync::ResourceAccess::Unknown;
    }

    /// @brief Reads the tracked pipeline mask stored on `resource` from this executing context.
    /// @param[in] resource Target viewable resource (returns `PipelineStage::Graphics` if null).
    /// @return Current tracked `sync::PipelineStage`.
    [[nodiscard]] sync::PipelineStage
        getTrackedPipelines(const resource::ViewableResource* const resource) const noexcept
    {
        return (resource != nullptr) ? resource->getTrackedPipelinesForContext() : sync::PipelineStage::Graphics;
    }

protected:
    /// @brief Command list currently replaying packets into this context, or `nullptr`.
    CommandListBase* m_executingCommandList{ nullptr };

    /// @brief Optional outer wrapper context (e.g., `ValidationContext` / `ValidationComputeContext`).
    IComputeContext* m_wrappingContext{ nullptr };

#if GP_ENABLE_RHI_VALIDATION
    /// @brief Optional RHI validation tracker bound to this context.
    validation::Tracker* m_tracker{ nullptr };
#endif

    /// @brief Per-context draw call counter accumulated for GPU stats.
    gp::UInt32 m_statDraws{ 0u };

    /// @brief Per-context primitive counter accumulated for GPU stats.
    gp::UInt32 m_statPrimitives{ 0u };

    /// @brief Per-context dispatch counter accumulated for GPU stats.
    gp::UInt32 m_statDispatches{ 0u };

    /// @brief Multi-GPU execution mask active on this context.
    GPUMask m_gpuMask{ GPUMask::gpu0() };
};

}   // namespace gp::rhi::command
