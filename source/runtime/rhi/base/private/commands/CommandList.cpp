// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandList.hpp"
#include "commands/CommandPacket.hpp"
#include "commands/ICommandContext.hpp"
#include "device/DynamicRHI.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"

namespace gp::rhi::command
{

void CommandList::applyCachedRenderTargets(pipeline::GraphicsPipelineStateInitializer& psoInit) const noexcept
{
    psoInit.applyRenderTargetsInfo(m_persistentState.cachedRenderTargetsInfo);
}

void CommandList::setGraphicsPipelineState(
    pipeline::GraphicsPipelineState* const graphicsState, const gp::UInt32 stencilRef, const bool applyAdditionalState
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().setGraphicsPipelineState(graphicsState, stencilRef, applyAdditionalState);
        return;
    }
    emplaceCommand<CommandSetGraphicsPipelineState>(graphicsState, stencilRef, applyAdditionalState);
}

void CommandList::setBatchedShaderParameters(
    shader::GraphicsShader* const shader, shader::BatchedShaderParameters& batchedParameters
) noexcept
{
    if (!batchedParameters.hasParameters())
    {
        return;
    }

    if (isBottomOfPipe())
    {
        getContext().setBatchedShaderParameters(shader, batchedParameters);
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

    emplaceCommand<CommandSetGraphicsShaderParameters>(shader, dataCopy, paramsCopy, resourcesCopy, bindlessCopy);
    batchedParameters.reset();
}

void CommandList::setBatchedShaderUnbinds(
    shader::GraphicsShader* const shader, shader::BatchedShaderUnbinds& batchedUnbinds
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
        getContext().setShaderUnbinds(shader, batchedUnbinds.unbinds);
        batchedUnbinds.reset();
        return;
    }

    const gp::ConstVectorView<shader::ShaderParameterUnbind> unbindsCopy =
        allocArray(gp::ConstVectorView<shader::ShaderParameterUnbind>(batchedUnbinds.unbinds));
    emplaceCommand<CommandSetGraphicsShaderUnbinds>(shader, unbindsCopy);
    batchedUnbinds.reset();
}

void CommandList::setStencilRef(const gp::UInt32 stencilRef) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().setStencilRef(stencilRef);
        return;
    }
    emplaceCommand<CommandSetStencilRef>(stencilRef);
}

void CommandList::setBlendFactor(const gp::math::LinearColor& blendFactor) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().setBlendFactor(blendFactor);
        return;
    }
    emplaceCommand<CommandSetBlendFactor>(blendFactor);
}

void CommandList::setStreamSource(
    const gp::UInt32 streamIndex, resource::Buffer* const vertexBuffer, const gp::UInt32 offset
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().setStreamSource(streamIndex, vertexBuffer, offset);
        return;
    }
    emplaceCommand<CommandSetStreamSource>(streamIndex, vertexBuffer, offset);
}

void CommandList::setStreamSourceSlot(
    const gp::UInt32 streamIndex, resource::StreamSourceSlot* const slot, const gp::UInt32 offset
) noexcept
{
    if (isBottomOfPipe())
    {
        if (slot != nullptr)
        {
            getContext().setStreamSource(streamIndex, slot->getBuffer(), slot->getOffset() + offset);
        }
        else
        {
            getContext().setStreamSource(streamIndex, nullptr, offset);
        }
        return;
    }
    emplaceCommand<CommandSetStreamSourceSlot>(streamIndex, slot, offset);
}

void CommandList::setViewport(
    const gp::Float32 minX,
    const gp::Float32 minY,
    const gp::Float32 minZ,
    const gp::Float32 maxX,
    const gp::Float32 maxY,
    const gp::Float32 maxZ
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().setViewport(minX, minY, minZ, maxX, maxY, maxZ);
        return;
    }
    emplaceCommand<CommandSetViewport>(minX, minY, minZ, maxX, maxY, maxZ);
}

void CommandList::setStereoViewport(
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
    if (isBottomOfPipe())
    {
        getContext().setStereoViewport(
            leftMinX, rightMinX, leftMinY, rightMinY, minZ, leftMaxX, rightMaxX, leftMaxY, rightMaxY, maxZ
        );
        return;
    }
    emplaceCommand<CommandSetStereoViewport>(
        leftMinX, rightMinX, leftMinY, rightMinY, minZ, leftMaxX, rightMaxX, leftMaxY, rightMaxY, maxZ
    );
}

void CommandList::setMultipleViewports(const gp::UInt32 count, const ViewportBounds* const data) noexcept
{
    if (count == 0u || data == nullptr)
    {
        return;
    }

    if (isBottomOfPipe())
    {
        getContext().setMultipleViewports(count, data);
        return;
    }
    emplaceCommand<CommandSetMultipleViewports>(allocArray(gp::ConstVectorView<ViewportBounds>(data, count)));
}

void CommandList::setScissorRect(
    const bool enable, const gp::UInt32 minX, const gp::UInt32 minY, const gp::UInt32 maxX, const gp::UInt32 maxY
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().setScissorRect(enable, minX, minY, maxX, maxY);
        return;
    }
    emplaceCommand<CommandSetScissorRect>(enable, minX, minY, maxX, maxY);
}

void CommandList::setDepthBounds(const gp::Float32 minDepth, const gp::Float32 maxDepth) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().setDepthBounds(minDepth, maxDepth);
        return;
    }
    emplaceCommand<CommandSetDepthBounds>(minDepth, maxDepth);
}

void CommandList::setShadingRate(
    const pipeline::VRSShadingRate shadingRate, const pipeline::VRSRateCombiner (&combiners)[2]
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().setShadingRate(shadingRate, combiners);
        return;
    }
    emplaceCommand<CommandSetShadingRate>(shadingRate, combiners);
}

void CommandList::drawPrimitive(
    const gp::UInt32 baseVertexIndex, const gp::UInt32 numPrimitives, const gp::UInt32 numInstances
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().drawPrimitive(baseVertexIndex, numPrimitives, numInstances);
        return;
    }
    emplaceCommand<CommandDrawPrimitive>(baseVertexIndex, numPrimitives, numInstances);
}

void CommandList::drawPrimitiveIndirect(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().drawPrimitiveIndirect(argumentBuffer, argumentOffset);
        return;
    }
    emplaceCommand<CommandDrawPrimitiveIndirect>(argumentBuffer, argumentOffset);
}

void CommandList::drawIndexedPrimitive(
    resource::Buffer* const indexBuffer,
    const gp::Int32 baseVertexIndex,
    const gp::UInt32 firstInstance,
    const gp::UInt32 numVertices,
    const gp::UInt32 startIndex,
    const gp::UInt32 numPrimitives,
    const gp::UInt32 numInstances
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().drawIndexedPrimitive(
            indexBuffer, baseVertexIndex, firstInstance, numVertices, startIndex, numPrimitives, numInstances
        );
        return;
    }
    emplaceCommand<CommandDrawIndexedPrimitive>(
        indexBuffer, baseVertexIndex, firstInstance, numVertices, startIndex, numPrimitives, numInstances
    );
}

void CommandList::drawIndexedPrimitiveIndirect(
    resource::Buffer* const indexBuffer, resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().drawIndexedPrimitiveIndirect(indexBuffer, argumentBuffer, argumentOffset);
        return;
    }
    emplaceCommand<CommandDrawIndexedPrimitiveIndirect>(indexBuffer, argumentBuffer, argumentOffset);
}

void CommandList::multiDrawIndexedPrimitiveIndirect(
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
    if (isBottomOfPipe())
    {
        getContext().multiDrawIndexedPrimitiveIndirect(
            indexBuffer,
            argumentBuffer,
            argumentOffset,
            countBuffer,
            countBufferOffset,
            maxDrawArguments,
            minMaterialIndex,
            maxMaterialIndex
        );
        return;
    }
    emplaceCommand<CommandMultiDrawIndexedPrimitiveIndirect>(
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

void CommandList::dispatchMeshShader(
    const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().dispatchMeshShader(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
        return;
    }
    emplaceCommand<CommandDispatchMeshShader>(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void CommandList::dispatchIndirectMeshShader(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().dispatchIndirectMeshShader(argumentBuffer, argumentOffset);
        return;
    }
    emplaceCommand<CommandDispatchIndirectMeshShader>(argumentBuffer, argumentOffset);
}

void CommandList::beginRenderPass(const RenderPassInfo& inInfo, const char* const name) noexcept
{
    GP_CHECKF(!m_persistentState.insideRenderPass, "Nested CommandList::beginRenderPass() is not permitted.");
    GP_CHECKF(inInfo.validate(), "CommandList::beginRenderPass() received an invalid RenderPassInfo.");
    m_persistentState.insideRenderPass = true;
    m_persistentState.cachedRenderTargetsInfo = inInfo.extractRenderTargetsInfo();

    if (isBottomOfPipe())
    {
        getContext().beginRenderPass(inInfo, name);
        return;
    }
    const char* const copiedName = (name != nullptr) ? allocString(gp::StringView(name)) : "";
    emplaceCommand<CommandBeginRenderPass>(inInfo, copiedName);
}

void CommandList::endRenderPass() noexcept
{
    GP_CHECKF(m_persistentState.insideRenderPass, "CommandList::endRenderPass() called without matching begin.");
    m_persistentState.insideRenderPass = false;
    m_persistentState.cachedRenderTargetsInfo = pipeline::GraphicsPipelineRenderTargetsInfo{};

    if (isBottomOfPipe())
    {
        getContext().endRenderPass();
        return;
    }
    emplaceCommand<CommandEndRenderPass>();
}

void CommandList::nextSubpass() noexcept
{
    GP_CHECKF(m_persistentState.insideRenderPass, "CommandList::nextSubpass() called outside an active render pass.");
    ++m_persistentState.cachedRenderTargetsInfo.subpassIndex;
    if (isBottomOfPipe())
    {
        getContext().nextSubpass();
        return;
    }
    emplaceCommand<CommandNextSubpass>();
}

void CommandList::copyTexture(
    resource::Texture* const sourceTexture, resource::Texture* const destTexture, const CopyTextureInfo& copyInfo
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().copyTexture(sourceTexture, destTexture, copyInfo);
        return;
    }
    emplaceCommand<CommandCopyTexture>(sourceTexture, destTexture, copyInfo);
}

void CommandList::copyBufferRegion(
    resource::Buffer* const destBuffer,
    const gp::UInt64 dstOffset,
    resource::Buffer* const sourceBuffer,
    const gp::UInt64 srcOffset,
    const gp::UInt64 numBytes
) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().copyBufferRegion(destBuffer, dstOffset, sourceBuffer, srcOffset, numBytes);
        return;
    }
    emplaceCommand<CommandCopyBufferRegion>(destBuffer, dstOffset, sourceBuffer, srcOffset, numBytes);
}

void CommandList::resummarizeHTile(resource::Texture* const depthTexture) noexcept
{
    if (isBottomOfPipe())
    {
        getContext().resummarizeHTile(depthTexture);
        return;
    }
    emplaceCommand<CommandResummarizeHTile>(depthTexture);
}

void CommandList::beginRenderQuery(sync::RenderQuery* const renderQuery) noexcept
{
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->beginRenderQueryTopOfPipe(*this, renderQuery);
        return;
    }

    if (isBottomOfPipe())
    {
        getContext().beginRenderQuery(renderQuery);
        return;
    }
    emplaceCommand<CommandBeginRenderQuery>(renderQuery);
}

void CommandList::endRenderQuery(sync::RenderQuery* const renderQuery) noexcept
{
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->endRenderQueryTopOfPipe(*this, renderQuery);
        return;
    }

    if (isBottomOfPipe())
    {
        getContext().endRenderQuery(renderQuery);
        return;
    }
    emplaceCommand<CommandEndRenderQuery>(renderQuery);
}

}   // namespace gp::rhi::command
