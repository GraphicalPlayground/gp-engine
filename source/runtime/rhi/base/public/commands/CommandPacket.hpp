// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "commands/CommandForward.hpp"       // IWYU pragma: keep
#include "commands/CommandTypes.hpp"
#include "commands/ICommandContext.hpp"      // IWYU pragma: keep
#include "commands/RenderPassInfo.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/LinearColor.hpp"
#include "maths/vector/Vector4.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include "sync/Transition.hpp"
#include "sync/TransitionDesc.hpp"
#include "views/ViewForward.hpp"
#include <utility>

namespace gp::rhi::command
{

/// @brief Intrusive base packet stored inside a `CommandListBase` linear arena stream.
/// @details Omits a virtual destructor intentionally; each concrete command packet invokes its own non-virtual
///          destructor inside `executeAndDestruct()` right after replaying into the active backend context.
struct CommandBase
{
    /// @brief Intrusive singly-linked pointer to the next recorded command packet.
    CommandBase* next{ nullptr };

    /// @brief Replays this command against `cmdList` and destroys the concrete command payload in place.
    /// @param[in,out] cmdList Executing command list supplying the target backend context.
    virtual void executeAndDestruct(CommandListBase& cmdList) noexcept = 0;

    /// @brief Destroys the concrete command payload in place without executing it.
    /// @details Used by `CommandListBase::reset()` when an unexecuted command stream is discarded so captured
    ///          reference-counted handles inside lambda packets are released cleanly.
    virtual void destruct() noexcept = 0;

protected:
    /// @brief Protected non-virtual destructor; packets are destroyed via `executeAndDestruct()` or `destruct()`.
    ~CommandBase() noexcept = default;
};

/// @brief CRTP helper dispatching `executeAndDestruct()` to `InCmdType::execute()` followed by `~InCmdType()`.
/// @tparam InCmdType Concrete command packet type providing `void execute(CommandListBase&) noexcept`.
template <typename InCmdType>
struct Command : public CommandBase
{
    /// @copydoc CommandBase::executeAndDestruct()
    virtual void executeAndDestruct(CommandListBase& cmdList) noexcept final override
    {
        InCmdType* const cmd = static_cast<InCmdType*>(this);
        cmd->execute(cmdList);
        cmd->~InCmdType();
    }

    /// @copydoc CommandBase::destruct()
    virtual void destruct() noexcept final override
    {
        static_cast<InCmdType*>(this)->~InCmdType();
    }

protected:
    /// @brief Protected non-virtual destructor; packets are destroyed via `executeAndDestruct()` or `destruct()`.
    ~Command() noexcept = default;
};

/// @brief Recorded command packet invoking an arbitrary callable with `InCmdListType&` on replay.
/// @tparam InCmdListType Command list reference type passed to `lambda` (`CommandListBase`, `ComputeCommandList`,
///         `CommandList`, or `CommandListImmediate`).
/// @tparam InLambdaType Movable callable invocable with `(InCmdListType&)`.
template <typename InCmdListType, typename InLambdaType>
struct LambdaCommand final : public Command<LambdaCommand<InCmdListType, InLambdaType>>
{
    /// @brief Stored callable invoked once during command list replay.
    InLambdaType lambda;

    /// @brief Constructs a lambda command packet taking ownership of `inLambda`.
    /// @param[in,out] inLambda Callable moved into the command stream arena.
    explicit LambdaCommand(InLambdaType&& inLambda) noexcept
        : lambda(std::move(inLambda))
    {}

    /// @brief Invokes `lambda` with `cmdList` cast to `InCmdListType&`.
    /// @param[in,out] cmdList Executing command list.
    void execute(CommandListBase& cmdList) noexcept
    {
        lambda(static_cast<InCmdListType&>(cmdList));
    }
};

/// @brief Recorded command packet invoking a callable receiving every active `IComputeContext*` in a `PipelineArray`.
/// @tparam InLambdaType Movable callable invocable with `(sync::PipelineArray<IComputeContext*>&)`.
template <typename InLambdaType>
struct LambdaCommandMultiPipe final : public Command<LambdaCommandMultiPipe<InLambdaType>>
{
    /// @brief Stored multi-pipe callable invoked once during replay.
    InLambdaType lambda;

    /// @brief Constructs a multi-pipe lambda packet taking ownership of `inLambda`.
    /// @param[in,out] inLambda Callable moved into the command stream arena.
    explicit LambdaCommandMultiPipe(InLambdaType&& inLambda) noexcept
        : lambda(std::move(inLambda))
    {}

    /// @brief Invokes `lambda` with the executing command list's active contexts.
    /// @param[in,out] cmdList Executing command list.
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet executing `IComputeContext::beginTransitions()` and marking source pipeline completion.
struct GP_RHI_API CommandBeginTransitions final : public Command<CommandBeginTransitions>
{
    /// @brief Contiguous span of transition pointers stored in the command list arena.
    gp::ConstVectorView<const sync::Transition*> transitions{};

    /// @brief Constructs a begin-transitions packet over arena-owned `inTransitions`.
    /// @param[in] inTransitions Span of transition pointers.
    explicit CommandBeginTransitions(const gp::ConstVectorView<const sync::Transition*> inTransitions) noexcept
        : transitions(inTransitions)
    {}

    /// @brief Replays `beginTransitions()` on the active compute context and retires source pipeline bits.
    /// @param[in,out] cmdList Executing command list.
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet executing `IComputeContext::endTransitions()` and marking destination pipeline completion.
struct GP_RHI_API CommandEndTransitions final : public Command<CommandEndTransitions>
{
    /// @brief Contiguous span of transition pointers stored in the command list arena.
    gp::ConstVectorView<const sync::Transition*> transitions{};

    /// @brief Constructs an end-transitions packet over arena-owned `inTransitions`.
    /// @param[in] inTransitions Span of transition pointers.
    explicit CommandEndTransitions(const gp::ConstVectorView<const sync::Transition*> inTransitions) noexcept
        : transitions(inTransitions)
    {}

    /// @brief Replays `endTransitions()` on the active compute context and retires destination pipeline bits.
    /// @param[in,out] cmdList Executing command list.
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet executing a full begin+end transition atomically on a single pipeline.
struct GP_RHI_API CommandResourceTransition final : public Command<CommandResourceTransition>
{
    /// @brief Single transition object allocated for this immediate barrier.
    const sync::Transition* transition{ nullptr };

    /// @brief Pipeline stage bitmask to retire if this packet is discarded before `execute()`.
    sync::PipelineStage pipeline{ sync::PipelineStage::Graphics };

    /// @brief Tracks whether `execute()` ran so `~CommandResourceTransition()` frees unexecuted transitions.
    bool executed{ false };

    /// @brief Constructs an immediate single-pipeline transition packet.
    /// @param[in] inTransition Transition object to begin and end.
    /// @param[in] inPipeline Active pipeline stage owning this transition.
    explicit CommandResourceTransition(
        const sync::Transition* const inTransition, const sync::PipelineStage inPipeline = sync::PipelineStage::Graphics
    ) noexcept
        : transition(inTransition)
        , pipeline(inPipeline)
    {}

    /// @brief Releases `transition` if the command list was reset before replaying this packet.
    ~CommandResourceTransition() noexcept;

    /// @brief Replays begin and end transitions on the active context and retires the transition.
    /// @param[in,out] cmdList Executing command list.
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet updating `ViewableResource` tracked access and pipeline state on replay.
struct GP_RHI_API CommandSetTrackedAccess final : public Command<CommandSetTrackedAccess>
{
    /// @brief Span of tracked access updates stored in the command list arena.
    gp::ConstVectorView<sync::TrackedAccessInfo> infos{};

    /// @brief Constructs a tracked-access update packet.
    /// @param[in] inInfos Arena-backed span of tracked access updates.
    explicit CommandSetTrackedAccess(const gp::ConstVectorView<sync::TrackedAccessInfo> inInfos) noexcept
        : infos(inInfos)
    {}

    /// @brief Applies every tracked access update through `IComputeContext::setTrackedAccess()`.
    /// @param[in,out] cmdList Executing command list.
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet binding a compute pipeline state.
struct GP_RHI_API CommandSetComputePipelineState final : public Command<CommandSetComputePipelineState>
{
    pipeline::ComputePipelineState* computePipelineState{ nullptr };

    explicit CommandSetComputePipelineState(pipeline::ComputePipelineState* const inPSO) noexcept
        : computePipelineState(inPSO)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet dispatching a compute thread group grid.
struct GP_RHI_API CommandDispatchComputeShader final : public Command<CommandDispatchComputeShader>
{
    gp::UInt32 threadGroupCountX{ 0u };
    gp::UInt32 threadGroupCountY{ 0u };
    gp::UInt32 threadGroupCountZ{ 0u };

    CommandDispatchComputeShader(const gp::UInt32 x, const gp::UInt32 y, const gp::UInt32 z) noexcept
        : threadGroupCountX(x)
        , threadGroupCountY(y)
        , threadGroupCountZ(z)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet dispatching an indirect compute grid.
struct GP_RHI_API CommandDispatchIndirectComputeShader final : public Command<CommandDispatchIndirectComputeShader>
{
    resource::Buffer* argumentBuffer{ nullptr };
    gp::UInt32 argumentOffset{ 0u };

    CommandDispatchIndirectComputeShader(resource::Buffer* const inBuffer, const gp::UInt32 inOffset) noexcept
        : argumentBuffer(inBuffer)
        , argumentOffset(inOffset)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet setting the async compute occupancy budget hint.
struct GP_RHI_API CommandSetAsyncComputeBudget final : public Command<CommandSetAsyncComputeBudget>
{
    AsyncComputeBudget budget{ AsyncComputeBudget::Balanced };

    explicit CommandSetAsyncComputeBudget(const AsyncComputeBudget inBudget) noexcept
        : budget(inBudget)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet setting the synchronous compute occupancy budget hint.
struct GP_RHI_API CommandSetComputeBudget final : public Command<CommandSetComputeBudget>
{
    SyncComputeBudget budget{ SyncComputeBudget::Balanced };

    explicit CommandSetComputeBudget(const SyncComputeBudget inBudget) noexcept
        : budget(inBudget)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet toggling the low-latency compute scheduling hint.
struct GP_RHI_API CommandSetLowLatencyHint final : public Command<CommandSetLowLatencyHint>
{
    bool lowLatency{ false };

    explicit CommandSetLowLatencyHint(const bool inLowLatency) noexcept
        : lowLatency(inLowLatency)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet clearing a UAV to a float4 value.
struct GP_RHI_API CommandClearUAVFloat final : public Command<CommandClearUAVFloat>
{
    view::UnorderedAccessView* uav{ nullptr };
    gp::math::Vector4<gp::Float32> values{};

    CommandClearUAVFloat(
        view::UnorderedAccessView* const inUav, const gp::math::Vector4<gp::Float32>& inValues
    ) noexcept
        : uav(inUav)
        , values(inValues)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet clearing a UAV to a uint4 value.
struct GP_RHI_API CommandClearUAVUint final : public Command<CommandClearUAVUint>
{
    view::UnorderedAccessView* uav{ nullptr };
    resource::UIntVector4 values{};

    CommandClearUAVUint(view::UnorderedAccessView* const inUav, const resource::UIntVector4& inValues) noexcept
        : uav(inUav)
        , values(inValues)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet updating inline shader root constants.
struct GP_RHI_API CommandSetShaderRootConstants final : public Command<CommandSetShaderRootConstants>
{
    resource::UIntVector4 constants{};

    explicit CommandSetShaderRootConstants(const resource::UIntVector4& inConstants) noexcept
        : constants(inConstants)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet applying batched shader parameters to a compute shader.
struct GP_RHI_API CommandSetComputeShaderParameters final : public Command<CommandSetComputeShaderParameters>
{
    shader::ComputeShader* shader{ nullptr };
    gp::ConstVectorView<gp::UInt8> parametersData{};
    gp::ConstVectorView<shader::ShaderParameter> parameters{};
    gp::ConstVectorView<shader::ShaderParameterResource> resourceParameters{};
    gp::ConstVectorView<shader::ShaderParameterResource> bindlessParameters{};

    CommandSetComputeShaderParameters(
        shader::ComputeShader* const inShader,
        const gp::ConstVectorView<gp::UInt8> inData,
        const gp::ConstVectorView<shader::ShaderParameter> inParams,
        const gp::ConstVectorView<shader::ShaderParameterResource> inResources,
        const gp::ConstVectorView<shader::ShaderParameterResource> inBindless
    ) noexcept
        : shader(inShader)
        , parametersData(inData)
        , parameters(inParams)
        , resourceParameters(inResources)
        , bindlessParameters(inBindless)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet applying batched shader parameters to a graphics shader.
struct GP_RHI_API CommandSetGraphicsShaderParameters final : public Command<CommandSetGraphicsShaderParameters>
{
    shader::GraphicsShader* shader{ nullptr };
    gp::ConstVectorView<gp::UInt8> parametersData{};
    gp::ConstVectorView<shader::ShaderParameter> parameters{};
    gp::ConstVectorView<shader::ShaderParameterResource> resourceParameters{};
    gp::ConstVectorView<shader::ShaderParameterResource> bindlessParameters{};

    CommandSetGraphicsShaderParameters(
        shader::GraphicsShader* const inShader,
        const gp::ConstVectorView<gp::UInt8> inData,
        const gp::ConstVectorView<shader::ShaderParameter> inParams,
        const gp::ConstVectorView<shader::ShaderParameterResource> inResources,
        const gp::ConstVectorView<shader::ShaderParameterResource> inBindless
    ) noexcept
        : shader(inShader)
        , parametersData(inData)
        , parameters(inParams)
        , resourceParameters(inResources)
        , bindlessParameters(inBindless)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet unbinding shader SRV/UAV slots on a compute shader.
struct GP_RHI_API CommandSetComputeShaderUnbinds final : public Command<CommandSetComputeShaderUnbinds>
{
    shader::ComputeShader* shader{ nullptr };
    gp::ConstVectorView<shader::ShaderParameterUnbind> unbinds{};

    CommandSetComputeShaderUnbinds(
        shader::ComputeShader* const inShader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept
        : shader(inShader)
        , unbinds(inUnbinds)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet unbinding shader SRV/UAV slots on a graphics shader.
struct GP_RHI_API CommandSetGraphicsShaderUnbinds final : public Command<CommandSetGraphicsShaderUnbinds>
{
    shader::GraphicsShader* shader{ nullptr };
    gp::ConstVectorView<shader::ShaderParameterUnbind> unbinds{};

    CommandSetGraphicsShaderUnbinds(
        shader::GraphicsShader* const inShader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept
        : shader(inShader)
        , unbinds(inUnbinds)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet binding a batch of static global uniform buffers.
struct GP_RHI_API CommandSetStaticUniformBuffers final : public Command<CommandSetStaticUniformBuffers>
{
    shader::UniformBufferStaticBindings uniformBuffers{};

    explicit CommandSetStaticUniformBuffers(const shader::UniformBufferStaticBindings& inBindings) noexcept
        : uniformBuffers(inBindings)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet binding a single static uniform buffer slot.
struct GP_RHI_API CommandSetStaticUniformBuffer final : public Command<CommandSetStaticUniformBuffer>
{
    resource::UniformBufferStaticSlot slot{ kMaxUniformBufferStaticSlots };
    resource::UniformBuffer* uniformBuffer{ nullptr };

    CommandSetStaticUniformBuffer(
        const resource::UniformBufferStaticSlot inSlot, resource::UniformBuffer* const inBuffer
    ) noexcept
        : slot(inSlot)
        , uniformBuffer(inBuffer)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet setting a uniform block dynamic offset.
struct GP_RHI_API CommandSetUniformBlockOffset final : public Command<CommandSetUniformBlockOffset>
{
    resource::UniformBuffer* uniformBuffer{ nullptr };
    gp::UInt32 offset{ 0u };

    CommandSetUniformBlockOffset(resource::UniformBuffer* const inBuffer, const gp::UInt32 inOffset) noexcept
        : uniformBuffer(inBuffer)
        , offset(inOffset)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet copying GPU buffer data into a CPU-readable staging buffer.
struct GP_RHI_API CommandCopyToStagingBuffer final : public Command<CommandCopyToStagingBuffer>
{
    resource::Buffer* sourceBuffer{ nullptr };
    resource::StagingBuffer* destinationStagingBuffer{ nullptr };
    gp::UInt32 offset{ 0u };
    gp::UInt32 numBytes{ 0u };

    CommandCopyToStagingBuffer(
        resource::Buffer* const inSrc,
        resource::StagingBuffer* const inDst,
        const gp::UInt32 inOffset,
        const gp::UInt32 inNumBytes
    ) noexcept
        : sourceBuffer(inSrc)
        , destinationStagingBuffer(inDst)
        , offset(inOffset)
        , numBytes(inNumBytes)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet signaling a GPU fence.
struct GP_RHI_API CommandWriteGPUFence final : public Command<CommandWriteGPUFence>
{
    /// @brief Target GPU fence to signal.
    sync::GPUFence* fence{ nullptr };

    /// @brief Tracks whether `execute()` ran so `~CommandWriteGPUFence()` decrements pending writes on discard.
    bool executed{ false };

    /// @brief Constructs a fence-write packet and increments `inFence`'s pending write count.
    /// @param[in] inFence Target GPU fence.
    explicit CommandWriteGPUFence(sync::GPUFence* const inFence) noexcept;

    /// @brief Decrements `fence`'s pending write count if discarded before `execute()`.
    ~CommandWriteGPUFence() noexcept;

    /// @brief Signals `fence` on the active compute context and decrements its pending write count.
    /// @param[in,out] cmdList Executing command list.
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet changing the active multi-GPU node mask on the backend context.
struct GP_RHI_API CommandSetGPUMask final : public Command<CommandSetGPUMask>
{
    GPUMask gpuMask{ GPUMask::gpu0() };

    explicit CommandSetGPUMask(const GPUMask inMask) noexcept
        : gpuMask(inMask)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet switching the active execution pipeline inside a command stream.
struct GP_RHI_API CommandSwitchPipeline final : public Command<CommandSwitchPipeline>
{
    sync::PipelineStage targetPipeline{ sync::PipelineStage::Graphics };

    explicit CommandSwitchPipeline(const sync::PipelineStage inPipeline) noexcept
        : targetPipeline(inPipeline)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet notifying the backend context after external native queue work.
struct GP_RHI_API CommandPostExternalCommandsReset final : public Command<CommandPostExternalCommandsReset>
{
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet binding a compiled graphics pipeline state object.
struct GP_RHI_API CommandSetGraphicsPipelineState final : public Command<CommandSetGraphicsPipelineState>
{
    pipeline::GraphicsPipelineState* graphicsState{ nullptr };
    gp::UInt32 stencilRef{ 0u };
    bool applyAdditionalState{ true };

    CommandSetGraphicsPipelineState(
        pipeline::GraphicsPipelineState* const inState, const gp::UInt32 inStencilRef, const bool inApplyAdditionalState
    ) noexcept
        : graphicsState(inState)
        , stencilRef(inStencilRef)
        , applyAdditionalState(inApplyAdditionalState)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet updating the dynamic stencil reference value.
struct GP_RHI_API CommandSetStencilRef final : public Command<CommandSetStencilRef>
{
    gp::UInt32 stencilRef{ 0u };

    explicit CommandSetStencilRef(const gp::UInt32 inStencilRef) noexcept
        : stencilRef(inStencilRef)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet updating the dynamic constant blend factor color.
struct GP_RHI_API CommandSetBlendFactor final : public Command<CommandSetBlendFactor>
{
    gp::math::LinearColor blendFactor{};

    explicit CommandSetBlendFactor(const gp::math::LinearColor& inBlendFactor) noexcept
        : blendFactor(inBlendFactor)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet binding a vertex buffer stream slot.
struct GP_RHI_API CommandSetStreamSource final : public Command<CommandSetStreamSource>
{
    gp::UInt32 streamIndex{ 0u };
    resource::Buffer* vertexBuffer{ nullptr };
    gp::UInt32 offset{ 0u };

    CommandSetStreamSource(
        const gp::UInt32 inStreamIndex, resource::Buffer* const inVertexBuffer, const gp::UInt32 inOffset
    ) noexcept
        : streamIndex(inStreamIndex)
        , vertexBuffer(inVertexBuffer)
        , offset(inOffset)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet binding a stream source slot indirection object.
struct GP_RHI_API CommandSetStreamSourceSlot final : public Command<CommandSetStreamSourceSlot>
{
    gp::UInt32 streamIndex{ 0u };
    resource::StreamSourceSlot* slot{ nullptr };
    gp::UInt32 offset{ 0u };

    CommandSetStreamSourceSlot(
        const gp::UInt32 inStreamIndex, resource::StreamSourceSlot* const inSlot, const gp::UInt32 inOffset
    ) noexcept
        : streamIndex(inStreamIndex)
        , slot(inSlot)
        , offset(inOffset)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet setting the primary rasterizer viewport.
struct GP_RHI_API CommandSetViewport final : public Command<CommandSetViewport>
{
    gp::Float32 minX{ 0.0f };
    gp::Float32 minY{ 0.0f };
    gp::Float32 minZ{ 0.0f };
    gp::Float32 maxX{ 0.0f };
    gp::Float32 maxY{ 0.0f };
    gp::Float32 maxZ{ 1.0f };

    CommandSetViewport(
        const gp::Float32 inMinX,
        const gp::Float32 inMinY,
        const gp::Float32 inMinZ,
        const gp::Float32 inMaxX,
        const gp::Float32 inMaxY,
        const gp::Float32 inMaxZ
    ) noexcept
        : minX(inMinX)
        , minY(inMinY)
        , minZ(inMinZ)
        , maxX(inMaxX)
        , maxY(inMaxY)
        , maxZ(inMaxZ)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet setting side-by-side stereo viewports.
struct GP_RHI_API CommandSetStereoViewport final : public Command<CommandSetStereoViewport>
{
    gp::Float32 leftMinX{ 0.0f };
    gp::Float32 rightMinX{ 0.0f };
    gp::Float32 leftMinY{ 0.0f };
    gp::Float32 rightMinY{ 0.0f };
    gp::Float32 minZ{ 0.0f };
    gp::Float32 leftMaxX{ 0.0f };
    gp::Float32 rightMaxX{ 0.0f };
    gp::Float32 leftMaxY{ 0.0f };
    gp::Float32 rightMaxY{ 0.0f };
    gp::Float32 maxZ{ 1.0f };

    CommandSetStereoViewport(
        const gp::Float32 inLeftMinX,
        const gp::Float32 inRightMinX,
        const gp::Float32 inLeftMinY,
        const gp::Float32 inRightMinY,
        const gp::Float32 inMinZ,
        const gp::Float32 inLeftMaxX,
        const gp::Float32 inRightMaxX,
        const gp::Float32 inLeftMaxY,
        const gp::Float32 inRightMaxY,
        const gp::Float32 inMaxZ
    ) noexcept
        : leftMinX(inLeftMinX)
        , rightMinX(inRightMinX)
        , leftMinY(inLeftMinY)
        , rightMinY(inRightMinY)
        , minZ(inMinZ)
        , leftMaxX(inLeftMaxX)
        , rightMaxX(inRightMaxX)
        , leftMaxY(inLeftMaxY)
        , rightMaxY(inRightMaxY)
        , maxZ(inMaxZ)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet binding multiple rasterizer viewports.
struct GP_RHI_API CommandSetMultipleViewports final : public Command<CommandSetMultipleViewports>
{
    gp::ConstVectorView<ViewportBounds> viewports{};

    explicit CommandSetMultipleViewports(const gp::ConstVectorView<ViewportBounds> inViewports) noexcept
        : viewports(inViewports)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet setting the hardware scissor rectangle.
struct GP_RHI_API CommandSetScissorRect final : public Command<CommandSetScissorRect>
{
    bool enable{ false };
    gp::UInt32 minX{ 0u };
    gp::UInt32 minY{ 0u };
    gp::UInt32 maxX{ 0u };
    gp::UInt32 maxY{ 0u };

    CommandSetScissorRect(
        const bool inEnable,
        const gp::UInt32 inMinX,
        const gp::UInt32 inMinY,
        const gp::UInt32 inMaxX,
        const gp::UInt32 inMaxY
    ) noexcept
        : enable(inEnable)
        , minX(inMinX)
        , minY(inMinY)
        , maxX(inMaxX)
        , maxY(inMaxY)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet setting dynamic depth bounds.
struct GP_RHI_API CommandSetDepthBounds final : public Command<CommandSetDepthBounds>
{
    gp::Float32 minDepth{ 0.0f };
    gp::Float32 maxDepth{ 1.0f };

    CommandSetDepthBounds(const gp::Float32 inMinDepth, const gp::Float32 inMaxDepth) noexcept
        : minDepth(inMinDepth)
        , maxDepth(inMaxDepth)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet setting per-draw Variable Rate Shading rate and combiners.
struct GP_RHI_API CommandSetShadingRate final : public Command<CommandSetShadingRate>
{
    pipeline::VRSShadingRate shadingRate{ pipeline::VRSShadingRate::Rate1x1 };
    pipeline::VRSRateCombiner combiners[2]{ pipeline::VRSRateCombiner::Passthrough,
                                            pipeline::VRSRateCombiner::Passthrough };

    CommandSetShadingRate(
        const pipeline::VRSShadingRate inRate, const pipeline::VRSRateCombiner (&inCombiners)[2]
    ) noexcept
        : shadingRate(inRate)
    {
        combiners[0] = inCombiners[0];
        combiners[1] = inCombiners[1];
    }

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet issuing a non-indexed draw call.
struct GP_RHI_API CommandDrawPrimitive final : public Command<CommandDrawPrimitive>
{
    gp::UInt32 baseVertexIndex{ 0u };
    gp::UInt32 numPrimitives{ 0u };
    gp::UInt32 numInstances{ 1u };

    CommandDrawPrimitive(
        const gp::UInt32 inBaseVertexIndex, const gp::UInt32 inNumPrimitives, const gp::UInt32 inNumInstances
    ) noexcept
        : baseVertexIndex(inBaseVertexIndex)
        , numPrimitives(inNumPrimitives)
        , numInstances(inNumInstances)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet issuing a non-indexed indirect draw call.
struct GP_RHI_API CommandDrawPrimitiveIndirect final : public Command<CommandDrawPrimitiveIndirect>
{
    resource::Buffer* argumentBuffer{ nullptr };
    gp::UInt32 argumentOffset{ 0u };

    CommandDrawPrimitiveIndirect(resource::Buffer* const inBuffer, const gp::UInt32 inOffset) noexcept
        : argumentBuffer(inBuffer)
        , argumentOffset(inOffset)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet issuing an indexed draw call.
struct GP_RHI_API CommandDrawIndexedPrimitive final : public Command<CommandDrawIndexedPrimitive>
{
    resource::Buffer* indexBuffer{ nullptr };
    gp::Int32 baseVertexIndex{ 0 };
    gp::UInt32 firstInstance{ 0u };
    gp::UInt32 numVertices{ 0u };
    gp::UInt32 startIndex{ 0u };
    gp::UInt32 numPrimitives{ 0u };
    gp::UInt32 numInstances{ 1u };

    CommandDrawIndexedPrimitive(
        resource::Buffer* const inIndexBuffer,
        const gp::Int32 inBaseVertexIndex,
        const gp::UInt32 inFirstInstance,
        const gp::UInt32 inNumVertices,
        const gp::UInt32 inStartIndex,
        const gp::UInt32 inNumPrimitives,
        const gp::UInt32 inNumInstances
    ) noexcept
        : indexBuffer(inIndexBuffer)
        , baseVertexIndex(inBaseVertexIndex)
        , firstInstance(inFirstInstance)
        , numVertices(inNumVertices)
        , startIndex(inStartIndex)
        , numPrimitives(inNumPrimitives)
        , numInstances(inNumInstances)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet issuing an indexed indirect draw call.
struct GP_RHI_API CommandDrawIndexedPrimitiveIndirect final : public Command<CommandDrawIndexedPrimitiveIndirect>
{
    resource::Buffer* indexBuffer{ nullptr };
    resource::Buffer* argumentBuffer{ nullptr };
    gp::UInt32 argumentOffset{ 0u };

    CommandDrawIndexedPrimitiveIndirect(
        resource::Buffer* const inIndexBuffer, resource::Buffer* const inArgBuffer, const gp::UInt32 inArgOffset
    ) noexcept
        : indexBuffer(inIndexBuffer)
        , argumentBuffer(inArgBuffer)
        , argumentOffset(inArgOffset)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet issuing a multi-draw indexed indirect call.
struct GP_RHI_API CommandMultiDrawIndexedPrimitiveIndirect final
    : public Command<CommandMultiDrawIndexedPrimitiveIndirect>
{
    resource::Buffer* indexBuffer{ nullptr };
    resource::Buffer* argumentBuffer{ nullptr };
    gp::UInt32 argumentOffset{ 0u };
    resource::Buffer* countBuffer{ nullptr };
    gp::UInt32 countBufferOffset{ 0u };
    gp::UInt32 maxDrawArguments{ 0u };
    gp::UInt32 minMaterialIndex{ 0u };
    gp::UInt32 maxMaterialIndex{ 0u };

    CommandMultiDrawIndexedPrimitiveIndirect(
        resource::Buffer* const inIndexBuffer,
        resource::Buffer* const inArgumentBuffer,
        const gp::UInt32 inArgumentOffset,
        resource::Buffer* const inCountBuffer,
        const gp::UInt32 inCountBufferOffset,
        const gp::UInt32 inMaxDrawArguments,
        const gp::UInt32 inMinMaterialIndex,
        const gp::UInt32 inMaxMaterialIndex
    ) noexcept
        : indexBuffer(inIndexBuffer)
        , argumentBuffer(inArgumentBuffer)
        , argumentOffset(inArgumentOffset)
        , countBuffer(inCountBuffer)
        , countBufferOffset(inCountBufferOffset)
        , maxDrawArguments(inMaxDrawArguments)
        , minMaterialIndex(inMinMaterialIndex)
        , maxMaterialIndex(inMaxMaterialIndex)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet dispatching a mesh shader grid.
struct GP_RHI_API CommandDispatchMeshShader final : public Command<CommandDispatchMeshShader>
{
    gp::UInt32 threadGroupCountX{ 0u };
    gp::UInt32 threadGroupCountY{ 0u };
    gp::UInt32 threadGroupCountZ{ 0u };

    CommandDispatchMeshShader(const gp::UInt32 x, const gp::UInt32 y, const gp::UInt32 z) noexcept
        : threadGroupCountX(x)
        , threadGroupCountY(y)
        , threadGroupCountZ(z)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet dispatching an indirect mesh shader grid.
struct GP_RHI_API CommandDispatchIndirectMeshShader final : public Command<CommandDispatchIndirectMeshShader>
{
    resource::Buffer* argumentBuffer{ nullptr };
    gp::UInt32 argumentOffset{ 0u };

    CommandDispatchIndirectMeshShader(resource::Buffer* const inBuffer, const gp::UInt32 inOffset) noexcept
        : argumentBuffer(inBuffer)
        , argumentOffset(inOffset)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet beginning a graphics render pass.
struct GP_RHI_API CommandBeginRenderPass final : public Command<CommandBeginRenderPass>
{
    RenderPassInfo info{};
    const char* name{ nullptr };

    CommandBeginRenderPass(const RenderPassInfo& inInfo, const char* const inName) noexcept
        : info(inInfo)
        , name(inName)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet ending the active graphics render pass.
struct GP_RHI_API CommandEndRenderPass final : public Command<CommandEndRenderPass>
{
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet advancing to the next render subpass.
struct GP_RHI_API CommandNextSubpass final : public Command<CommandNextSubpass>
{
    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet copying a region between two GPU textures.
struct GP_RHI_API CommandCopyTexture final : public Command<CommandCopyTexture>
{
    resource::Texture* sourceTexture{ nullptr };
    resource::Texture* destTexture{ nullptr };
    CopyTextureInfo copyInfo{};

    CommandCopyTexture(
        resource::Texture* const inSource, resource::Texture* const inDest, const CopyTextureInfo& inInfo
    ) noexcept
        : sourceTexture(inSource)
        , destTexture(inDest)
        , copyInfo(inInfo)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet copying a byte range between two GPU buffers.
struct GP_RHI_API CommandCopyBufferRegion final : public Command<CommandCopyBufferRegion>
{
    resource::Buffer* destBuffer{ nullptr };
    gp::UInt64 dstOffset{ 0u };
    resource::Buffer* sourceBuffer{ nullptr };
    gp::UInt64 srcOffset{ 0u };
    gp::UInt64 numBytes{ 0u };

    CommandCopyBufferRegion(
        resource::Buffer* const inDest,
        const gp::UInt64 inDstOffset,
        resource::Buffer* const inSource,
        const gp::UInt64 inSrcOffset,
        const gp::UInt64 inNumBytes
    ) noexcept
        : destBuffer(inDest)
        , dstOffset(inDstOffset)
        , sourceBuffer(inSource)
        , srcOffset(inSrcOffset)
        , numBytes(inNumBytes)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet resummarizing hierarchical depth metadata for a depth texture.
struct GP_RHI_API CommandResummarizeHTile final : public Command<CommandResummarizeHTile>
{
    resource::Texture* depthTexture{ nullptr };

    explicit CommandResummarizeHTile(resource::Texture* const inTexture) noexcept
        : depthTexture(inTexture)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet beginning a hardware render query.
struct GP_RHI_API CommandBeginRenderQuery final : public Command<CommandBeginRenderQuery>
{
    sync::RenderQuery* renderQuery{ nullptr };

    explicit CommandBeginRenderQuery(sync::RenderQuery* const inQuery) noexcept
        : renderQuery(inQuery)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

/// @brief Recorded packet ending a hardware render query.
struct GP_RHI_API CommandEndRenderQuery final : public Command<CommandEndRenderQuery>
{
    sync::RenderQuery* renderQuery{ nullptr };

    explicit CommandEndRenderQuery(sync::RenderQuery* const inQuery) noexcept
        : renderQuery(inQuery)
    {}

    void execute(CommandListBase& cmdList) noexcept;
};

}   // namespace gp::rhi::command
