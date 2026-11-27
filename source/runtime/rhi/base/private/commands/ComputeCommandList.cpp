// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/ComputeCommandList.hpp"
#include "commands/CommandPacket.hpp"
#include "commands/IComputeContext.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "device/DynamicRHI.hpp"
#include "sync/Transition.hpp"

namespace gp::rhi::command
{

void ComputeCommandList::setComputePipelineState(pipeline::ComputePipelineState* const computePipelineState) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().setComputePipelineState(computePipelineState);
        return;
    }
    emplaceCommand<CommandSetComputePipelineState>(computePipelineState);
}

void ComputeCommandList::dispatchComputeShader(
    const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().dispatchComputeShader(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
        return;
    }
    emplaceCommand<CommandDispatchComputeShader>(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void ComputeCommandList::dispatchIndirectComputeShader(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().dispatchIndirectComputeShader(argumentBuffer, argumentOffset);
        return;
    }
    emplaceCommand<CommandDispatchIndirectComputeShader>(argumentBuffer, argumentOffset);
}

void ComputeCommandList::setAsyncComputeBudget(const AsyncComputeBudget budget) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().setAsyncComputeBudget(budget);
        return;
    }
    emplaceCommand<CommandSetAsyncComputeBudget>(budget);
}

void ComputeCommandList::setComputeBudget(const SyncComputeBudget budget) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().setComputeBudget(budget);
        return;
    }
    emplaceCommand<CommandSetComputeBudget>(budget);
}

void ComputeCommandList::setLowLatencyHint(const bool lowLatency) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().setLowLatencyHint(lowLatency);
        return;
    }
    emplaceCommand<CommandSetLowLatencyHint>(lowLatency);
}

void ComputeCommandList::beginTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    if (transitions.isEmpty())
    {
        return;
    }

    if (isBottomOfPipe())
    {
        getComputeContext().beginTransitions(transitions);
        const sync::PipelineStage pipeline =
            (m_activePipeline != sync::PipelineStage::None) ? m_activePipeline : sync::PipelineStage::Graphics;
        for (const sync::Transition* const transition: transitions)
        {
            if (transition != nullptr)
            {
                sync::TransitionController::markBegin(transition, pipeline);
            }
        }
        return;
    }
    emplaceCommand<CommandBeginTransitions>(allocArray(transitions));
}

void ComputeCommandList::endTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    if (transitions.isEmpty())
    {
        return;
    }

    if (isBottomOfPipe())
    {
        getComputeContext().endTransitions(transitions);
        const sync::PipelineStage pipeline =
            (m_activePipeline != sync::PipelineStage::None) ? m_activePipeline : sync::PipelineStage::Graphics;
        for (const sync::Transition* const transition: transitions)
        {
            if (transition != nullptr)
            {
                sync::TransitionController::markEnd(transition, pipeline);
            }
        }
        return;
    }
    emplaceCommand<CommandEndTransitions>(allocArray(transitions));
}

void ComputeCommandList::beginTransition(const sync::Transition* const transition) noexcept
{
    if (transition == nullptr)
    {
        return;
    }
    beginTransitions(gp::ConstVectorView<const sync::Transition*>(&transition, 1u));
}

void ComputeCommandList::endTransition(const sync::Transition* const transition) noexcept
{
    if (transition == nullptr)
    {
        return;
    }
    endTransitions(gp::ConstVectorView<const sync::Transition*>(&transition, 1u));
}

void ComputeCommandList::transition(
    const gp::ConstVectorView<sync::TransitionInfo> infos, const sync::TransitionCreateFlags flags
) noexcept
{
    const sync::PipelineStage pipeline =
        (m_activePipeline != sync::PipelineStage::None) ? m_activePipeline : sync::PipelineStage::Graphics;
    transitionInternal(infos, pipeline, pipeline, flags);
    setTrackedAccess(infos, pipeline);
}

void ComputeCommandList::transition(const sync::TransitionInfo& info, const sync::TransitionCreateFlags flags) noexcept
{
    transition(gp::ConstVectorView<sync::TransitionInfo>(&info, 1u), flags);
}

void ComputeCommandList::transition(
    const gp::ConstVectorView<sync::TransitionInfo> infos,
    const sync::PipelineStage srcPipelines,
    const sync::PipelineStage dstPipelines,
    const sync::TransitionCreateFlags flags
) noexcept
{
    transitionInternal(infos, srcPipelines, dstPipelines, flags);
    setTrackedAccess(infos, dstPipelines);
}

void ComputeCommandList::setTrackedAccess(const gp::ConstVectorView<sync::TrackedAccessInfo> infos) noexcept
{
    if (infos.isEmpty())
    {
        return;
    }

    if (isBottomOfPipe())
    {
        IComputeContext& ctx = getComputeContext();
        for (const sync::TrackedAccessInfo& info: infos)
        {
            ctx.setTrackedAccess(info);
        }
        return;
    }
    emplaceCommand<CommandSetTrackedAccess>(allocArray(infos));
}

void ComputeCommandList::setTrackedAccess(
    const gp::ConstVectorView<sync::TransitionInfo> infos, const sync::PipelineStage pipelines
) noexcept
{
    if (infos.isEmpty())
    {
        return;
    }

    gp::Vector<sync::TrackedAccessInfo> tracked{};
    tracked.reserve(infos.size());
    for (const sync::TransitionInfo& info: infos)
    {
        if (info.resource != nullptr && info.accessAfter != sync::ResourceAccess::Unknown &&
            (info.type == sync::TransitionResourceType::Texture || info.type == sync::TransitionResourceType::Buffer))
        {
            tracked.emplaceBack(static_cast<resource::ViewableResource*>(info.resource), info.accessAfter, pipelines);
        }
    }

    if (!tracked.isEmpty())
    {
        setTrackedAccess(gp::ConstVectorView<sync::TrackedAccessInfo>(tracked.data(), tracked.size()));
    }
}

void ComputeCommandList::clearUAVFloat(
    view::UnorderedAccessView* const unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().clearUAVFloat(unorderedAccessView, values);
        return;
    }
    emplaceCommand<CommandClearUAVFloat>(unorderedAccessView, values);
}

void ComputeCommandList::clearUAVUint(
    view::UnorderedAccessView* const unorderedAccessView, const resource::UIntVector4& values
) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().clearUAVUint(unorderedAccessView, values);
        return;
    }
    emplaceCommand<CommandClearUAVUint>(unorderedAccessView, values);
}

void ComputeCommandList::setShaderRootConstants(const resource::UIntVector4& constants) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().setShaderRootConstants(constants);
        return;
    }
    emplaceCommand<CommandSetShaderRootConstants>(constants);
}

void ComputeCommandList::setBatchedShaderParameters(
    shader::ComputeShader* const shader, shader::BatchedShaderParameters& batchedParameters
) noexcept
{
    if (!batchedParameters.hasParameters())
    {
        return;
    }

    if (isBottomOfPipe())
    {
        getComputeContext().setBatchedShaderParameters(shader, batchedParameters);
        batchedParameters.reset();
        return;
    }

    const gp::ConstVectorView<gp::UInt8> dataCopy =
        allocArray(gp::ConstVectorView<gp::UInt8>(batchedParameters.parametersData));
    const gp::ConstVectorView<shader::ShaderParameter> paramsCopy =
        allocArray(gp::ConstVectorView<shader::ShaderParameter>(batchedParameters.parameters));
    const gp::ConstVectorView<shader::ShaderParameterResource> resourcesCopy =
        allocArray(gp::ConstVectorView<shader::ShaderParameterResource>(batchedParameters.resourceParameters));
    const gp::ConstVectorView<shader::ShaderParameterResource> bindlessCopy =
        allocArray(gp::ConstVectorView<shader::ShaderParameterResource>(batchedParameters.bindlessParameters));

    emplaceCommand<CommandSetComputeShaderParameters>(shader, dataCopy, paramsCopy, resourcesCopy, bindlessCopy);
    batchedParameters.reset();
}

void ComputeCommandList::setBatchedShaderUnbinds(
    shader::ComputeShader* const shader, shader::BatchedShaderUnbinds& batchedUnbinds
) noexcept
{
    if (!batchedUnbinds.hasParameters())
    {
        return;
    }

    if (!needsShaderUnbinds())
    {
        batchedUnbinds.reset();
        return;
    }

    if (isBottomOfPipe())
    {
        getComputeContext().setShaderUnbinds(shader, batchedUnbinds.unbinds);
        batchedUnbinds.reset();
        return;
    }

    const gp::ConstVectorView<shader::ShaderParameterUnbind> unbindsCopy =
        allocArray(gp::ConstVectorView<shader::ShaderParameterUnbind>(batchedUnbinds.unbinds));
    emplaceCommand<CommandSetComputeShaderUnbinds>(shader, unbindsCopy);
    batchedUnbinds.reset();
}

void ComputeCommandList::setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept
{
    m_persistentState.staticUniformBuffers = uniformBuffers;
    if (isBottomOfPipe())
    {
        getComputeContext().setStaticUniformBuffers(uniformBuffers);
        return;
    }
    emplaceCommand<CommandSetStaticUniformBuffers>(uniformBuffers);
}

void ComputeCommandList::setStaticUniformBuffer(
    const resource::UniformBufferStaticSlot slot, resource::UniformBuffer* const uniformBuffer
) noexcept
{
    m_persistentState.staticUniformBuffers.addUniformBufferAtSlot(slot, uniformBuffer);
    if (isBottomOfPipe())
    {
        getComputeContext().setStaticUniformBuffer(slot, uniformBuffer);
        return;
    }
    emplaceCommand<CommandSetStaticUniformBuffer>(slot, uniformBuffer);
}

void ComputeCommandList::setUniformBlockOffset(view::ShaderResourceView* const srv, const gp::UInt32 offset) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().setUniformBlockOffset(srv, offset);
        return;
    }
    emplaceCommand<CommandSetUniformBlockOffset>(srv, offset);
}

void ComputeCommandList::copyToStagingBuffer(
    resource::Buffer* const sourceBuffer,
    resource::StagingBuffer* const destinationStagingBuffer,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().copyToStagingBuffer(sourceBuffer, destinationStagingBuffer, offset, numBytes);
        return;
    }
    emplaceCommand<CommandCopyToStagingBuffer>(sourceBuffer, destinationStagingBuffer, offset, numBytes);
}

void ComputeCommandList::writeGPUFence(sync::GPUFence* const fence) noexcept
{
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->writeGPUFenceTopOfPipe(*this, fence);
        return;
    }

    if (isBottomOfPipe())
    {
        getComputeContext().writeGPUFence(fence);
        return;
    }

    emplaceCommand<CommandWriteGPUFence>(fence);
}

void ComputeCommandList::setGPUMask(const GPUMask gpuMask) noexcept
{
    if (m_persistentState.currentGPUMask == gpuMask)
    {
        return;
    }

    m_persistentState.currentGPUMask = gpuMask;
    if (isBottomOfPipe())
    {
        for (IComputeContext* const ctx: m_contexts)
        {
            if (ctx != nullptr)
            {
                ctx->setGPUMask(gpuMask);
            }
        }
        return;
    }
    emplaceCommand<CommandSetGPUMask>(gpuMask);
}

void ComputeCommandList::postExternalCommandsReset() noexcept
{
    if (isBottomOfPipe())
    {
        getComputeContext().postExternalCommandsReset();
        return;
    }
    emplaceCommand<CommandPostExternalCommandsReset>();
}

}   // namespace gp::rhi::command
