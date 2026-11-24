// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandPacket.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/IComputeContext.hpp"
#include "sync/GPUFence.hpp"
#include "sync/Transition.hpp"

namespace gp::rhi::command
{

void CommandBeginTransitions::execute(CommandListBase& cmdList) noexcept
{
    if (transitions.isEmpty())
    {
        return;
    }

    IComputeContext& ctx = cmdList.getComputeContext();
    ctx.beginTransitions(transitions);

    const sync::PipelineStage pipeline =
        (cmdList.getPipeline() != sync::PipelineStage::None) ? cmdList.getPipeline() : sync::PipelineStage::Graphics;
    for (const sync::Transition* const transition: transitions)
    {
        if (transition != nullptr)
        {
            sync::TransitionController::markBegin(transition, pipeline);
        }
    }
}

void CommandEndTransitions::execute(CommandListBase& cmdList) noexcept
{
    if (transitions.isEmpty())
    {
        return;
    }

    IComputeContext& ctx = cmdList.getComputeContext();
    ctx.endTransitions(transitions);

    const sync::PipelineStage pipeline =
        (cmdList.getPipeline() != sync::PipelineStage::None) ? cmdList.getPipeline() : sync::PipelineStage::Graphics;
    for (const sync::Transition* const transition: transitions)
    {
        if (transition != nullptr)
        {
            sync::TransitionController::markEnd(transition, pipeline);
        }
    }
}

CommandResourceTransition::~CommandResourceTransition() noexcept
{
    if (!executed && transition != nullptr)
    {
        const sync::PipelineStage resolved =
            (pipeline != sync::PipelineStage::None) ? pipeline : sync::PipelineStage::Graphics;
        sync::TransitionController::markBegin(transition, resolved);
        sync::TransitionController::markEnd(transition, resolved);
    }
}

void CommandResourceTransition::execute(CommandListBase& cmdList) noexcept
{
    executed = true;
    if (transition == nullptr)
    {
        return;
    }

    IComputeContext& ctx = cmdList.getComputeContext();
    const gp::ConstVectorView<const sync::Transition*> span(&transition, 1u);
    ctx.beginTransitions(span);
    ctx.endTransitions(span);

    const sync::PipelineStage resolved =
        (cmdList.getPipeline() != sync::PipelineStage::None) ? cmdList.getPipeline() : sync::PipelineStage::Graphics;
    sync::TransitionController::markBegin(transition, resolved);
    sync::TransitionController::markEnd(transition, resolved);
}

void CommandSetTrackedAccess::execute(CommandListBase& cmdList) noexcept
{
    IComputeContext& ctx = cmdList.getComputeContext();
    for (const sync::TrackedAccessInfo& info: infos)
    {
        ctx.setTrackedAccess(info);
    }
}

void CommandSetComputePipelineState::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setComputePipelineState(computePipelineState);
}

void CommandDispatchComputeShader::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().dispatchComputeShader(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void CommandDispatchIndirectComputeShader::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().dispatchIndirectComputeShader(argumentBuffer, argumentOffset);
}

void CommandSetAsyncComputeBudget::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setAsyncComputeBudget(budget);
}

void CommandSetComputeBudget::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setComputeBudget(budget);
}

void CommandSetLowLatencyHint::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setLowLatencyHint(lowLatency);
}

void CommandClearUAVFloat::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().clearUAVFloat(uav, values);
}

void CommandClearUAVUint::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().clearUAVUint(uav, values);
}

void CommandSetShaderRootConstants::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setShaderRootConstants(constants);
}

void CommandSetComputeShaderParameters::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setShaderParameters(
        shader, parametersData, parameters, resourceParameters, bindlessParameters
    );
}

void CommandSetGraphicsShaderParameters::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setShaderParameters(
        shader, parametersData, parameters, resourceParameters, bindlessParameters
    );
}

void CommandSetComputeShaderUnbinds::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setShaderUnbinds(shader, unbinds);
}

void CommandSetGraphicsShaderUnbinds::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setShaderUnbinds(shader, unbinds);
}

void CommandSetStaticUniformBuffers::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setStaticUniformBuffers(uniformBuffers);
}

void CommandSetStaticUniformBuffer::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setStaticUniformBuffer(slot, uniformBuffer);
}

void CommandSetUniformBlockOffset::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().setUniformBlockOffset(uniformBuffer, offset);
}

void CommandCopyToStagingBuffer::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().copyToStagingBuffer(sourceBuffer, destinationStagingBuffer, offset, numBytes);
}

CommandWriteGPUFence::CommandWriteGPUFence(sync::GPUFence* const inFence) noexcept
    : fence(inFence)
{
    if (fence != nullptr)
    {
        fence->incrementPendingWriteCommands();
    }
}

CommandWriteGPUFence::~CommandWriteGPUFence() noexcept
{
    if (!executed && fence != nullptr)
    {
        fence->decrementPendingWriteCommands();
    }
}

void CommandWriteGPUFence::execute(CommandListBase& cmdList) noexcept
{
    executed = true;
    cmdList.getComputeContext().writeGPUFence(fence);
    if (fence != nullptr)
    {
        fence->decrementPendingWriteCommands();
    }
}

void CommandSetGPUMask::execute(CommandListBase& cmdList) noexcept
{
    cmdList.m_persistentState.currentGPUMask = gpuMask;
    for (IComputeContext* const ctx: cmdList.m_contexts)
    {
        if (ctx != nullptr)
        {
            ctx->setGPUMask(gpuMask);
        }
    }
}

void CommandSwitchPipeline::execute(CommandListBase& cmdList) noexcept
{
    cmdList.m_activePipeline = targetPipeline;
    if (targetPipeline != sync::PipelineStage::None)
    {
        cmdList.activatePipelines(targetPipeline);
        cmdList.m_computeContext = cmdList.m_contexts[targetPipeline];
    }
}

void CommandPostExternalCommandsReset::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getComputeContext().postExternalCommandsReset();
}

void CommandSetGraphicsPipelineState::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setGraphicsPipelineState(graphicsState, stencilRef, applyAdditionalState);
}

void CommandSetStencilRef::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setStencilRef(stencilRef);
}

void CommandSetBlendFactor::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setBlendFactor(blendFactor);
}

void CommandSetStreamSource::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setStreamSource(streamIndex, vertexBuffer, offset);
}

void CommandSetStreamSourceSlot::execute(CommandListBase& cmdList) noexcept
{
    if (slot != nullptr)
    {
        cmdList.getContext().setStreamSource(streamIndex, slot->getBuffer(), slot->getOffset() + offset);
    }
    else
    {
        cmdList.getContext().setStreamSource(streamIndex, nullptr, offset);
    }
}

void CommandSetViewport::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setViewport(minX, minY, minZ, maxX, maxY, maxZ);
}

void CommandSetStereoViewport::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setStereoViewport(
        leftMinX, rightMinX, leftMinY, rightMinY, minZ, leftMaxX, rightMaxX, leftMaxY, rightMaxY, maxZ
    );
}

void CommandSetMultipleViewports::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setMultipleViewports(static_cast<gp::UInt32>(viewports.size()), viewports.data());
}

void CommandSetScissorRect::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setScissorRect(enable, minX, minY, maxX, maxY);
}

void CommandSetDepthBounds::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setDepthBounds(minDepth, maxDepth);
}

void CommandSetShadingRate::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().setShadingRate(shadingRate, combiners);
}

void CommandDrawPrimitive::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().drawPrimitive(baseVertexIndex, numPrimitives, numInstances);
}

void CommandDrawPrimitiveIndirect::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().drawPrimitiveIndirect(argumentBuffer, argumentOffset);
}

void CommandDrawIndexedPrimitive::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().drawIndexedPrimitive(
        indexBuffer, baseVertexIndex, firstInstance, numVertices, startIndex, numPrimitives, numInstances
    );
}

void CommandDrawIndexedPrimitiveIndirect::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().drawIndexedPrimitiveIndirect(indexBuffer, argumentBuffer, argumentOffset);
}

void CommandMultiDrawIndexedPrimitiveIndirect::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().multiDrawIndexedPrimitiveIndirect(
        indexBuffer,
        argumentBuffer,
        argumentOffset,
        countBuffer,
        countBufferOffset,
        maxDrawArguments,
        minMaterialIndex,
        maxMaterialIndex
    );
}

void CommandDispatchMeshShader::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().dispatchMeshShader(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void CommandDispatchIndirectMeshShader::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().dispatchIndirectMeshShader(argumentBuffer, argumentOffset);
}

void CommandBeginRenderPass::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().beginRenderPass(info, name);
}

void CommandEndRenderPass::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().endRenderPass();
}

void CommandNextSubpass::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().nextSubpass();
}

void CommandCopyTexture::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().copyTexture(sourceTexture, destTexture, copyInfo);
}

void CommandCopyBufferRegion::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().copyBufferRegion(destBuffer, dstOffset, sourceBuffer, srcOffset, numBytes);
}

void CommandResummarizeHTile::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().resummarizeHTile(depthTexture);
}

void CommandBeginRenderQuery::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().beginRenderQuery(renderQuery);
}

void CommandEndRenderQuery::execute(CommandListBase& cmdList) noexcept
{
    cmdList.getContext().endRenderQuery(renderQuery);
}

}   // namespace gp::rhi::command
