// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "validation/ValidationContext.hpp"
#include "commands/CommandEnums.hpp"
#include "core/RHIStrings.hpp"         // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"   // IWYU pragma: keep
#include "pipeline/ComputePipelineState.hpp"
#include "pipeline/GraphicsPipelineState.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/StagingBuffer.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/UniformBuffer.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GraphicsShader.hpp"
#include "shaders/Shader.hpp"
#include "sync/Transition.hpp"
#include "validation/ValidationRHI.hpp"
#include "validation/ValidationUtils.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include <cstdio>

namespace gp::rhi
{

namespace
{

[[nodiscard]] sync::ResourceAccess
    requiredSRVAccessForFrequency(const shader::ShaderFrequency frequency, const sync::PipelineStage pipeline) noexcept
{
    if (pipeline == sync::PipelineStage::AsyncCompute || frequency == shader::ShaderFrequency::Compute)
    {
        return sync::ResourceAccess::SRVCompute;
    }
    if (frequency == shader::ShaderFrequency::Pixel)
    {
        return sync::ResourceAccess::SRVGraphicsPixel;
    }
    return sync::ResourceAccess::SRVGraphicsNonPixel;
}

void validateUniformBufferResourceTable(
    validation::Tracker& tracker,
    const resource::UniformBuffer& ub,
    const sync::ResourceAccess srvAccess,
    const validation::UAVMode uavMode
) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    const gp::ConstVectorView<resource::ResourceRef> table = ub.getResourceTable();
    for (gp::USize idx = 0u; idx < table.size(); ++idx)
    {
        resource::Resource* const res = table[idx].get();
        if (res == nullptr)
        {
            continue;
        }

        const resource::ResourceType resType = res->getType();
        if (resType == resource::ResourceType::Texture || resType == resource::ResourceType::TextureReference)
        {
            resource::Texture* const tex = static_cast<resource::Texture*>(res);
            if (validation::Resource* const valRes = tex->getValidationTrackerResource(); valRes != nullptr)
            {
                tracker.assertAccess(valRes->getWholeResourceIdentity(), srvAccess);
            }
        }
        else if (resType == resource::ResourceType::ShaderResourceView)
        {
            view::ShaderResourceView* const srv = static_cast<view::ShaderResourceView*>(res);
            tracker.assertAccess(srv->getViewIdentity(), srvAccess);
        }
        else if (resType == resource::ResourceType::UnorderedAccessView)
        {
            view::UnorderedAccessView* const uav = static_cast<view::UnorderedAccessView*>(res);
            const sync::ResourceAccess uavAccess = (uavMode == validation::UAVMode::Graphics)
                                                       ? sync::ResourceAccess::UAVGraphics
                                                       : sync::ResourceAccess::UAVCompute;
            tracker.assertAccess(uav->getViewIdentity(), uavAccess);
        }
    }
#else
    static_cast<void>(tracker);
    static_cast<void>(ub);
    static_cast<void>(srvAccess);
    static_cast<void>(uavMode);
#endif
}

void validateShaderParametersCommon(
    const ValidationRHI& validationRHI,
    validation::Tracker& tracker,
    validation::StaticUniformBuffers& staticUBs,
    validation::BoundUniformBuffers& boundUBs,
    const shader::Shader* const targetShader,
    const gp::ConstVectorView<gp::UInt8> parametersData,
    const gp::ConstVectorView<shader::ShaderParameter> parameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> resourceParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> bindlessParameters,
    const validation::UAVMode uavMode
) noexcept
{
    if (targetShader == nullptr)
    {
        validation::reportValidationFailure("setShaderParameters called with a null Shader.");
        return;
    }

    // Validate constant byte slice bounds.
    for (const shader::ShaderParameter& param: parameters)
    {
        if ((static_cast<gp::USize>(param.byteOffset) + static_cast<gp::USize>(param.byteSize)) > parametersData.size())
        {
            validation::reportValidationFailure(
                "ShaderParameter [byteOffset, byteOffset + byteSize) exceeds parametersData size."
            );
        }
    }

    const shader::ShaderFrequency freq = targetShader->getFrequency();
    const sync::ResourceAccess srvAccess = requiredSRVAccessForFrequency(freq, tracker.getPipeline());
    const gp::ConstVectorView<gp::UInt32> expectedUBHashes =
        targetShader->getShaderResourceTable().resourceTableLayoutHashes;
#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
    const gp::ConstVectorView<shader::ShaderCodeValidationStride> strideChecks =
        targetShader->getCodeValidationStrides();
    const gp::ConstVectorView<shader::ShaderCodeValidationUBSize> ubSizeChecks =
        targetShader->getCodeValidationUBSizes();
#endif

    const auto validateResourceEntry = [&](const shader::ShaderParameterResource& entry, const bool isBindless) noexcept
    {
        switch (entry.type)
        {
        case shader::ShaderParameterResource::Type::Texture:
        {
            if (entry.resource != nullptr)
            {
                resource::Texture* const tex = static_cast<resource::Texture*>(entry.resource);
                if ((tex->getFlags() & resource::TextureCreateFlags::ShaderResource) ==
                    resource::TextureCreateFlags::None)
                {
                    char msg[256]{};
                    const gp::String texName = tex->getName().toString();
                    const int len = std::snprintf(
                        msg,
                        sizeof(msg),
                        "Texture '%s' bound as shader texture without TextureCreateFlags::ShaderResource.",
                        texName.cString()
                    );
                    if (len > 0)
                    {
                        validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
                    }
                }
#if GP_ENABLE_RHI_VALIDATION
                if (validation::Resource* const valRes = tex->getValidationTrackerResource(); valRes != nullptr)
                {
                    tracker.assertAccess(valRes->getWholeResourceIdentity(), srvAccess);
                }
#endif
            }
            break;
        }

        case shader::ShaderParameterResource::Type::ResourceView:
        {
            if (entry.resource != nullptr)
            {
                view::ShaderResourceView* const srv = static_cast<view::ShaderResourceView*>(entry.resource);
#if GP_ENABLE_RHI_VALIDATION
                const validation::ViewIdentity& viewId = srv->getViewIdentity();
                tracker.assertAccess(viewId, srvAccess);
    #if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
                if (!isBindless && srv->isBuffer() && viewId.stride > 0u)
                {
                    for (const shader::ShaderCodeValidationStride& check: strideChecks)
                    {
                        if (check.bindPoint == entry.index && check.stride != 0u && check.stride != viewId.stride)
                        {
                            char msg[256]{};
                            const int len = std::snprintf(
                                msg,
                                sizeof(msg),
                                "Shader '%.*s' slot %u expects buffer SRV stride %u, but bound SRV has stride %u.",
                                static_cast<int>(targetShader->getShaderName().size()),
                                targetShader->getShaderName().data(),
                                static_cast<gp::UInt32>(entry.index),
                                static_cast<gp::UInt32>(check.stride),
                                viewId.stride
                            );
                            if (len > 0)
                            {
                                validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
                            }
                        }
                    }
                }
    #endif
#endif
            }
            break;
        }

        case shader::ShaderParameterResource::Type::UnorderedAccessView:
        {
            view::UnorderedAccessView* const uav = static_cast<view::UnorderedAccessView*>(entry.resource);
            if (!isBindless)
            {
                tracker.assertUAV(uav, uavMode, entry.index);
            }
            else if (uav != nullptr)
            {
#if GP_ENABLE_RHI_VALIDATION
                const sync::ResourceAccess uavAccess = (uavMode == validation::UAVMode::Graphics)
                                                           ? sync::ResourceAccess::UAVGraphics
                                                           : sync::ResourceAccess::UAVCompute;
                tracker.assertAccess(uav->getViewIdentity(), uavAccess);
#endif
            }
            break;
        }

        case shader::ShaderParameterResource::Type::UniformBuffer:
        {
            resource::UniformBuffer* const ub = static_cast<resource::UniformBuffer*>(entry.resource);
            if (!isBindless && static_cast<gp::UInt32>(freq) < shader::kShaderFrequencyCount)
            {
                boundUBs.stages[static_cast<gp::UInt32>(freq)].bind(entry.index, ub);
            }

            if (ub != nullptr)
            {
                staticUBs.validateShaderUniformBuffer(ub);
#if GP_ENABLE_RHI_VALIDATION
                ub->getValidationResource().validateLifeTime(
                    validationRHI.getValidationFrameID(), ub->getLayout().getDebugName()
                );
#endif
                if (!isBindless && static_cast<gp::USize>(entry.index) < expectedUBHashes.size())
                {
                    const gp::UInt32 expectedHash = expectedUBHashes[entry.index];
                    const gp::UInt32 actualHash = ub->getLayout().getHash();
                    if (expectedHash != 0u && actualHash != expectedHash)
                    {
                        char msg[384]{};
                        const gp::StringView ubName = ub->getLayout().getDebugName();
                        const int len = std::snprintf(
                            msg,
                            sizeof(msg),
                            "Shader '%.*s' UB slot %u layout hash mismatch: expected 0x%08X, got 0x%08X ('%.*s').",
                            static_cast<int>(targetShader->getShaderName().size()),
                            targetShader->getShaderName().data(),
                            static_cast<gp::UInt32>(entry.index),
                            expectedHash,
                            actualHash,
                            static_cast<int>(ubName.size()),
                            ubName.data()
                        );
                        if (len > 0)
                        {
                            validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
                        }
                    }
                }
#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
                if (!isBindless)
                {
                    for (const shader::ShaderCodeValidationUBSize& szCheck: ubSizeChecks)
                    {
                        if (szCheck.bindPoint == entry.index && ub->getSize() < szCheck.size)
                        {
                            char msg[256]{};
                            const gp::StringView ubName = ub->getLayout().getDebugName();
                            const int len = std::snprintf(
                                msg,
                                sizeof(msg),
                                "UniformBuffer '%.*s' size (%u) is smaller than shader slot %u requirement (%u).",
                                static_cast<int>(ubName.size()),
                                ubName.data(),
                                ub->getSize(),
                                static_cast<gp::UInt32>(entry.index),
                                szCheck.size
                            );
                            if (len > 0)
                            {
                                validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
                            }
                        }
                    }
                }
#endif
                validateUniformBufferResourceTable(tracker, *ub, srvAccess, uavMode);
            }
            break;
        }

        case shader::ShaderParameterResource::Type::Sampler:
        case shader::ShaderParameterResource::Type::ResourceCollection:
        case shader::ShaderParameterResource::Type::DescriptorRange:
        default:
            break;
        }
    };

    for (const shader::ShaderParameterResource& entry: resourceParameters)
    {
        validateResourceEntry(entry, false);
    }
    for (const shader::ShaderParameterResource& entry: bindlessParameters)
    {
        validateResourceEntry(entry, true);
    }
}

void enqueueBeginTransitionsOps(
    validation::Tracker& tracker,
    const gp::ConstVectorView<const sync::Transition*> transitions,
    const bool insideRenderPass
) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    const sync::PipelineStage pipe = tracker.getPipeline();
    for (const sync::Transition* const transition: transitions)
    {
        if (transition == nullptr)
        {
            continue;
        }

        if (insideRenderPass && !transition->allowInRenderPass())
        {
            validation::reportValidationFailure(
                "beginTransitions called inside an active render pass without TransitionCreateFlags::AllowDuringRenderPass."
            );
        }

        const validation::TransitionResource& valData = transition->getValidationData();
        tracker.addOps(valData.pendingAliases);
        tracker.addOps(valData.pendingOperationsBegin);
        tracker.addOps(valData.pendingSignals[pipe]);
    }
#else
    static_cast<void>(tracker);
    static_cast<void>(transitions);
    static_cast<void>(insideRenderPass);
#endif
}

void enqueueEndTransitionsOps(
    validation::Tracker& tracker,
    const gp::ConstVectorView<const sync::Transition*> transitions,
    const bool insideRenderPass
) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    const sync::PipelineStage pipe = tracker.getPipeline();
    for (const sync::Transition* const transition: transitions)
    {
        if (transition == nullptr)
        {
            continue;
        }

        if (insideRenderPass && !transition->allowInRenderPass())
        {
            validation::reportValidationFailure(
                "endTransitions called inside an active render pass without TransitionCreateFlags::AllowDuringRenderPass."
            );
        }

        const validation::TransitionResource& valData = transition->getValidationData();
        tracker.addOps(valData.pendingWaits[pipe]);
        tracker.addOps(valData.pendingAliasingOverlaps);
        tracker.addOps(valData.pendingOperationsEnd);
    }
#else
    static_cast<void>(tracker);
    static_cast<void>(transitions);
    static_cast<void>(insideRenderPass);
#endif
}

}   // namespace

ValidationComputeContext::ValidationComputeContext(
    ValidationRHI& validationRHI, command::IComputeContext* const innerContext
) noexcept
    : m_rhi(validationRHI)
    , m_innerContext(innerContext)
    , m_stateTracker(sync::PipelineStage::AsyncCompute)
{
    GP_CHECK(m_innerContext != nullptr);
    m_innerContext->linkToWrappingContext(this);
#if GP_ENABLE_RHI_VALIDATION
    setValidationTracker(&m_stateTracker);
    m_innerContext->setValidationTracker(&m_stateTracker);
#endif
}

ValidationComputeContext::~ValidationComputeContext() noexcept
{
    if (m_innerContext != nullptr && m_innerContext->getWrappingContext() == this)
    {
        m_innerContext->linkToWrappingContext(nullptr);
#if GP_ENABLE_RHI_VALIDATION
        m_innerContext->setValidationTracker(nullptr);
#endif
    }
}

void ValidationComputeContext::resetContextState() noexcept
{
    m_boundComputePSO = nullptr;
    m_staticUniformBuffers.reset();
    m_boundUniformBuffers.resetAll();
    m_stateTracker.clearUAVState(validation::UAVMode::Compute);
}

void ValidationComputeContext::setExecutingCommandList(command::CommandListBase* const cmdList) noexcept
{
    command::IComputeContext::setExecutingCommandList(cmdList);
    m_innerContext->setExecutingCommandList(cmdList);
}

void ValidationComputeContext::setComputePipelineState(
    pipeline::ComputePipelineState* const computePipelineState
) noexcept
{
    if (computePipelineState == nullptr)
    {
        validation::reportValidationFailure("setComputePipelineState called with a null ComputePipelineState.");
        return;
    }
    m_boundComputePSO = computePipelineState;
    m_boundUniformBuffers.stages[static_cast<gp::UInt32>(shader::ShaderFrequency::Compute)].reset();
    m_stateTracker.clearUAVState(validation::UAVMode::Compute);
    m_innerContext->setComputePipelineState(computePipelineState);
}

void ValidationComputeContext::dispatchComputeShader(
    const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
) noexcept
{
    if (m_boundComputePSO == nullptr)
    {
        validation::reportValidationFailure("dispatchComputeShader called without a bound ComputePipelineState.");
    }
    ValidationRHI::validateThreadGroupCount(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
    m_stateTracker.dispatchOrDrawBarrier(validation::UAVMode::Compute);
    recordDispatchStats(1u);
    m_innerContext->dispatchComputeShader(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void ValidationComputeContext::dispatchIndirectComputeShader(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    if (m_boundComputePSO == nullptr)
    {
        validation::reportValidationFailure(
            "dispatchIndirectComputeShader called without a bound ComputePipelineState."
        );
    }
    ValidationRHI::validateDispatchIndirectArgsBuffer(argumentBuffer, argumentOffset);
#if GP_ENABLE_RHI_VALIDATION
    if (argumentBuffer != nullptr && argumentBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            argumentBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
            sync::ResourceAccess::IndirectArgs
        );
    }
#endif
    m_stateTracker.dispatchOrDrawBarrier(validation::UAVMode::Compute);
    recordDispatchStats(1u);
    m_innerContext->dispatchIndirectComputeShader(argumentBuffer, argumentOffset);
}

void ValidationComputeContext::setAsyncComputeBudget(const command::AsyncComputeBudget budget) noexcept
{
    m_innerContext->setAsyncComputeBudget(budget);
}

void ValidationComputeContext::setComputeBudget(const command::SyncComputeBudget budget) noexcept
{
    m_innerContext->setComputeBudget(budget);
}

void ValidationComputeContext::setLowLatencyHint(const bool lowLatency) noexcept
{
    m_innerContext->setLowLatencyHint(lowLatency);
}

void ValidationComputeContext::beginTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    enqueueBeginTransitionsOps(m_stateTracker, transitions, false);
    m_innerContext->beginTransitions(transitions);
}

void ValidationComputeContext::endTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    enqueueEndTransitionsOps(m_stateTracker, transitions, false);
    m_innerContext->endTransitions(transitions);
}

void ValidationComputeContext::clearUAVFloat(
    view::UnorderedAccessView* const unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
) noexcept
{
    if (unorderedAccessView == nullptr)
    {
        validation::reportValidationFailure("clearUAVFloat called with a null UnorderedAccessView.");
        return;
    }
#if GP_ENABLE_RHI_VALIDATION
    m_stateTracker.assertAccess(unorderedAccessView->getViewIdentity(), sync::ResourceAccess::UAVCompute);
#endif
    m_innerContext->clearUAVFloat(unorderedAccessView, values);
}

void ValidationComputeContext::clearUAVUint(
    view::UnorderedAccessView* const unorderedAccessView, const resource::UIntVector4& values
) noexcept
{
    if (unorderedAccessView == nullptr)
    {
        validation::reportValidationFailure("clearUAVUint called with a null UnorderedAccessView.");
        return;
    }
#if GP_ENABLE_RHI_VALIDATION
    m_stateTracker.assertAccess(unorderedAccessView->getViewIdentity(), sync::ResourceAccess::UAVCompute);
#endif
    m_innerContext->clearUAVUint(unorderedAccessView, values);
}

void ValidationComputeContext::setShaderRootConstants(const resource::UIntVector4& constants) noexcept
{
    m_innerContext->setShaderRootConstants(constants);
}

void ValidationComputeContext::setShaderParameters(
    shader::ComputeShader* const shader,
    const gp::ConstVectorView<gp::UInt8> inParametersData,
    const gp::ConstVectorView<shader::ShaderParameter> inParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
) noexcept
{
    if (m_boundComputePSO == nullptr)
    {
        validation::reportValidationFailure("Compute setShaderParameters called before setComputePipelineState.");
    }

    validateShaderParametersCommon(
        m_rhi,
        m_stateTracker,
        m_staticUniformBuffers,
        m_boundUniformBuffers,
        shader,
        inParametersData,
        inParameters,
        inResourceParameters,
        inBindlessParameters,
        validation::UAVMode::Compute
    );

    m_innerContext->setShaderParameters(
        shader, inParametersData, inParameters, inResourceParameters, inBindlessParameters
    );
}

void ValidationComputeContext::setShaderUnbinds(
    shader::ComputeShader* const shader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
) noexcept
{
    for (const shader::ShaderParameterUnbind& unbind: inUnbinds)
    {
        if (unbind.type == shader::ShaderParameterUnbind::Type::UnorderedAccessView)
        {
            m_stateTracker.assertUAV(nullptr, validation::UAVMode::Compute, unbind.index);
        }
    }
    m_innerContext->setShaderUnbinds(shader, inUnbinds);
}

void ValidationComputeContext::setStaticUniformBuffers(
    const shader::UniformBufferStaticBindings& uniformBuffers
) noexcept
{
    for (gp::UInt32 i = 0u; i < uniformBuffers.getUniformBufferCount(); ++i)
    {
        const resource::UniformBufferStaticSlot slot = uniformBuffers.getSlot(i);
        resource::UniformBuffer* const ub = uniformBuffers.getUniformBuffer(i);
        if (slot < kMaxUniformBufferStaticSlots)
        {
            m_staticUniformBuffers.bindings[slot] = ub;
        }
#if GP_ENABLE_RHI_VALIDATION
        if (ub != nullptr)
        {
            ub->getValidationResource().validateLifeTime(m_rhi.getValidationFrameID(), ub->getLayout().getDebugName());
        }
#endif
    }
    m_innerContext->setStaticUniformBuffers(uniformBuffers);
}

void ValidationComputeContext::setStaticUniformBuffer(
    const resource::UniformBufferStaticSlot slot, resource::UniformBuffer* const uniformBuffer
) noexcept
{
    if (slot < kMaxUniformBufferStaticSlots)
    {
        m_staticUniformBuffers.bindings[slot] = uniformBuffer;
    }
#if GP_ENABLE_RHI_VALIDATION
    if (uniformBuffer != nullptr)
    {
        uniformBuffer->getValidationResource().validateLifeTime(
            m_rhi.getValidationFrameID(), uniformBuffer->getLayout().getDebugName()
        );
    }
#endif
    m_innerContext->setStaticUniformBuffer(slot, uniformBuffer);
}

void ValidationComputeContext::setUniformBlockOffset(
    view::ShaderResourceView* const srv, const gp::UInt32 offset
) noexcept
{
    if (srv == nullptr)
    {
        validation::reportValidationFailure("setUniformBlockOffset called with a null ShaderResourceView.");
        return;
    }
    m_innerContext->setUniformBlockOffset(srv, offset);
}

void ValidationComputeContext::copyToStagingBuffer(
    resource::Buffer* const sourceBuffer,
    resource::StagingBuffer* const destinationStagingBuffer,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    if (sourceBuffer == nullptr || destinationStagingBuffer == nullptr)
    {
        validation::reportValidationFailure("copyToStagingBuffer called with a null sourceBuffer or stagingBuffer.");
        return;
    }
    if ((static_cast<gp::UInt64>(offset) + static_cast<gp::UInt64>(numBytes)) >
        static_cast<gp::UInt64>(sourceBuffer->getSize()))
    {
        validation::reportValidationFailure(
            "copyToStagingBuffer [offset, offset + numBytes) exceeds sourceBuffer size."
        );
    }
#if GP_ENABLE_RHI_VALIDATION
    if (validation::g_rhiValidateBufferSourceCopy && sourceBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            sourceBuffer->getValidationTrackerResource()->getWholeResourceIdentity(), sync::ResourceAccess::CopySrc
        );
    }
#endif
    m_innerContext->copyToStagingBuffer(sourceBuffer, destinationStagingBuffer, offset, numBytes);
}

void ValidationComputeContext::writeGPUFence(sync::GPUFence* const fence) noexcept
{
    if (fence == nullptr)
    {
        validation::reportValidationFailure("writeGPUFence called with a null GPUFence.");
        return;
    }
    m_innerContext->writeGPUFence(fence);
}

void ValidationComputeContext::setGPUMask(const GPUMask gpuMask) noexcept
{
    command::IComputeContext::setGPUMask(gpuMask);
    m_innerContext->setGPUMask(gpuMask);
}

void ValidationComputeContext::postExternalCommandsReset() noexcept
{
    resetContextState();
    m_innerContext->postExternalCommandsReset();
}

void ValidationComputeContext::setTrackedAccess(const sync::TrackedAccessInfo& info) noexcept
{
    command::IComputeContext::setTrackedAccess(info);
#if GP_ENABLE_RHI_VALIDATION
    if (info.resource != nullptr && info.resource->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.setTrackedAccess(
            info.resource->getValidationTrackerResource()->getWholeResourceIdentity(), info.access, info.pipelines
        );
    }
#endif
    m_innerContext->setTrackedAccess(info);
}

void ValidationComputeContext::beginUAVOverlap() noexcept
{
    m_stateTracker.allUAVsOverlap(true);
    m_innerContext->beginUAVOverlap();
}

void ValidationComputeContext::endUAVOverlap() noexcept
{
    m_stateTracker.allUAVsOverlap(false);
    m_innerContext->endUAVOverlap();
}

void ValidationComputeContext::beginUAVOverlap(const gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    for (view::UnorderedAccessView* const uav: uavs)
    {
        if (uav != nullptr)
        {
            m_stateTracker.specificUAVOverlap(uav->getViewIdentity(), true);
        }
    }
#endif
    m_innerContext->beginUAVOverlap(uavs);
}

void ValidationComputeContext::endUAVOverlap(const gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    for (view::UnorderedAccessView* const uav: uavs)
    {
        if (uav != nullptr)
        {
            m_stateTracker.specificUAVOverlap(uav->getViewIdentity(), false);
        }
    }
#endif
    m_innerContext->endUAVOverlap(uavs);
}

#if GP_WITH_RHI_BREADCRUMBS
void ValidationComputeContext::beginBreadcrumbGPU(diagnostics::BreadcrumbNode* const node) noexcept
{
    m_innerContext->beginBreadcrumbGPU(node);
}

void ValidationComputeContext::endBreadcrumbGPU(diagnostics::BreadcrumbNode* const node) noexcept
{
    m_innerContext->endBreadcrumbGPU(node);
}
#endif

ValidationContext::ValidationContext(
    ValidationRHI& validationRHI, command::ICommandContext* const innerContext
) noexcept
    : m_rhi(validationRHI)
    , m_innerContext(innerContext)
    , m_stateTracker(sync::PipelineStage::Graphics)
{
    GP_CHECK(m_innerContext != nullptr);
    m_innerContext->linkToWrappingContext(this);
#if GP_ENABLE_RHI_VALIDATION
    setValidationTracker(&m_stateTracker);
    m_innerContext->setValidationTracker(&m_stateTracker);
#endif
}

ValidationContext::~ValidationContext() noexcept
{
    if (m_innerContext != nullptr && m_innerContext->getWrappingContext() == this)
    {
        m_innerContext->linkToWrappingContext(nullptr);
#if GP_ENABLE_RHI_VALIDATION
        m_innerContext->setValidationTracker(nullptr);
#endif
    }
}

void ValidationContext::resetContextState() noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure(
            "Command context finalized while a render pass was still open (missing endRenderPass)."
        );
        m_insideRenderPass = false;
    }

    m_boundGraphicsPSO = nullptr;
    m_boundComputePSO = nullptr;
    m_staticUniformBuffers.reset();
    m_boundUniformBuffers.resetAll();
    m_stateTracker.clearUAVState(validation::UAVMode::Graphics);
    m_stateTracker.clearUAVState(validation::UAVMode::Compute);
    for (gp::UInt32 i = 0u; i < kMaxVertexElementCount; ++i)
    {
        m_vertexStreams[i] = VertexStreamBinding{};
    }
}

void ValidationContext::setExecutingCommandList(command::CommandListBase* const cmdList) noexcept
{
    command::IComputeContext::setExecutingCommandList(cmdList);
    m_innerContext->setExecutingCommandList(cmdList);
}

void ValidationContext::setComputePipelineState(pipeline::ComputePipelineState* const computePipelineState) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("setComputePipelineState is forbidden inside an active render pass.");
    }
    if (computePipelineState == nullptr)
    {
        validation::reportValidationFailure("setComputePipelineState called with a null ComputePipelineState.");
        return;
    }
    m_boundComputePSO = computePipelineState;
    m_boundUniformBuffers.stages[static_cast<gp::UInt32>(shader::ShaderFrequency::Compute)].reset();
    m_stateTracker.clearUAVState(validation::UAVMode::Compute);
    m_innerContext->setComputePipelineState(computePipelineState);
}

void ValidationContext::dispatchComputeShader(
    const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("dispatchComputeShader is forbidden inside an active render pass.");
    }
    if (m_boundComputePSO == nullptr)
    {
        validation::reportValidationFailure("dispatchComputeShader called without a bound ComputePipelineState.");
    }
    ValidationRHI::validateThreadGroupCount(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
    m_stateTracker.dispatchOrDrawBarrier(validation::UAVMode::Compute);
    recordDispatchStats(1u);
    m_innerContext->dispatchComputeShader(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void ValidationContext::dispatchIndirectComputeShader(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("dispatchIndirectComputeShader is forbidden inside an active render pass.");
    }
    if (m_boundComputePSO == nullptr)
    {
        validation::reportValidationFailure(
            "dispatchIndirectComputeShader called without a bound ComputePipelineState."
        );
    }
    ValidationRHI::validateDispatchIndirectArgsBuffer(argumentBuffer, argumentOffset);
#if GP_ENABLE_RHI_VALIDATION
    if (argumentBuffer != nullptr && argumentBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            argumentBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
            sync::ResourceAccess::IndirectArgs
        );
    }
#endif
    m_stateTracker.dispatchOrDrawBarrier(validation::UAVMode::Compute);
    recordDispatchStats(1u);
    m_innerContext->dispatchIndirectComputeShader(argumentBuffer, argumentOffset);
}

void ValidationContext::setAsyncComputeBudget(const command::AsyncComputeBudget budget) noexcept
{
    m_innerContext->setAsyncComputeBudget(budget);
}

void ValidationContext::setComputeBudget(const command::SyncComputeBudget budget) noexcept
{
    m_innerContext->setComputeBudget(budget);
}

void ValidationContext::setLowLatencyHint(const bool lowLatency) noexcept
{
    m_innerContext->setLowLatencyHint(lowLatency);
}

void ValidationContext::beginTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    enqueueBeginTransitionsOps(m_stateTracker, transitions, m_insideRenderPass);
    m_innerContext->beginTransitions(transitions);
}

void ValidationContext::endTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    enqueueEndTransitionsOps(m_stateTracker, transitions, m_insideRenderPass);
    m_innerContext->endTransitions(transitions);
}

void ValidationContext::clearUAVFloat(
    view::UnorderedAccessView* const unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("clearUAVFloat is forbidden inside an active render pass.");
    }
    if (unorderedAccessView == nullptr)
    {
        validation::reportValidationFailure("clearUAVFloat called with a null UnorderedAccessView.");
        return;
    }
#if GP_ENABLE_RHI_VALIDATION
    m_stateTracker.assertAccess(unorderedAccessView->getViewIdentity(), sync::ResourceAccess::UAVCompute);
#endif
    m_innerContext->clearUAVFloat(unorderedAccessView, values);
}

void ValidationContext::clearUAVUint(
    view::UnorderedAccessView* const unorderedAccessView, const resource::UIntVector4& values
) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("clearUAVUint is forbidden inside an active render pass.");
    }
    if (unorderedAccessView == nullptr)
    {
        validation::reportValidationFailure("clearUAVUint called with a null UnorderedAccessView.");
        return;
    }
#if GP_ENABLE_RHI_VALIDATION
    m_stateTracker.assertAccess(unorderedAccessView->getViewIdentity(), sync::ResourceAccess::UAVCompute);
#endif
    m_innerContext->clearUAVUint(unorderedAccessView, values);
}

void ValidationContext::setShaderRootConstants(const resource::UIntVector4& constants) noexcept
{
    m_innerContext->setShaderRootConstants(constants);
}

void ValidationContext::setShaderParameters(
    shader::ComputeShader* const shader,
    const gp::ConstVectorView<gp::UInt8> inParametersData,
    const gp::ConstVectorView<shader::ShaderParameter> inParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
) noexcept
{
    if (m_boundComputePSO == nullptr)
    {
        validation::reportValidationFailure("Compute setShaderParameters called before setComputePipelineState.");
    }

    validateShaderParametersCommon(
        m_rhi,
        m_stateTracker,
        m_staticUniformBuffers,
        m_boundUniformBuffers,
        shader,
        inParametersData,
        inParameters,
        inResourceParameters,
        inBindlessParameters,
        validation::UAVMode::Compute
    );

    m_innerContext->setShaderParameters(
        shader, inParametersData, inParameters, inResourceParameters, inBindlessParameters
    );
}

void ValidationContext::setShaderParameters(
    shader::GraphicsShader* const shader,
    const gp::ConstVectorView<gp::UInt8> inParametersData,
    const gp::ConstVectorView<shader::ShaderParameter> inParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
) noexcept
{
    if (!m_insideRenderPass)
    {
        validation::reportValidationFailure("Graphics setShaderParameters called outside an active render pass.");
    }
    if (m_boundGraphicsPSO == nullptr)
    {
        validation::reportValidationFailure("Graphics setShaderParameters called before setGraphicsPipelineState.");
    }
    else if (shader != nullptr && m_boundGraphicsPSO->getShader(shader->getFrequency()) != shader)
    {
        validation::reportValidationFailure(
            "Graphics setShaderParameters called with a shader that does not match the bound GraphicsPipelineState."
        );
    }

    validateShaderParametersCommon(
        m_rhi,
        m_stateTracker,
        m_staticUniformBuffers,
        m_boundUniformBuffers,
        shader,
        inParametersData,
        inParameters,
        inResourceParameters,
        inBindlessParameters,
        validation::UAVMode::Graphics
    );

    m_innerContext->setShaderParameters(
        shader, inParametersData, inParameters, inResourceParameters, inBindlessParameters
    );
}

void ValidationContext::setShaderUnbinds(
    shader::ComputeShader* const shader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
) noexcept
{
    for (const shader::ShaderParameterUnbind& unbind: inUnbinds)
    {
        if (unbind.type == shader::ShaderParameterUnbind::Type::UnorderedAccessView)
        {
            m_stateTracker.assertUAV(nullptr, validation::UAVMode::Compute, unbind.index);
        }
    }
    m_innerContext->setShaderUnbinds(shader, inUnbinds);
}

void ValidationContext::setShaderUnbinds(
    shader::GraphicsShader* const shader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
) noexcept
{
    for (const shader::ShaderParameterUnbind& unbind: inUnbinds)
    {
        if (unbind.type == shader::ShaderParameterUnbind::Type::UnorderedAccessView)
        {
            m_stateTracker.assertUAV(nullptr, validation::UAVMode::Graphics, unbind.index);
        }
    }
    m_innerContext->setShaderUnbinds(shader, inUnbinds);
}

void ValidationContext::setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("setStaticUniformBuffers must be called outside of an active render pass.");
    }

    for (gp::UInt32 i = 0u; i < uniformBuffers.getUniformBufferCount(); ++i)
    {
        const resource::UniformBufferStaticSlot slot = uniformBuffers.getSlot(i);
        resource::UniformBuffer* const ub = uniformBuffers.getUniformBuffer(i);
        if (slot < kMaxUniformBufferStaticSlots)
        {
            m_staticUniformBuffers.bindings[slot] = ub;
        }
#if GP_ENABLE_RHI_VALIDATION
        if (ub != nullptr)
        {
            ub->getValidationResource().validateLifeTime(m_rhi.getValidationFrameID(), ub->getLayout().getDebugName());
        }
#endif
    }
    m_innerContext->setStaticUniformBuffers(uniformBuffers);
}

void ValidationContext::setStaticUniformBuffer(
    const resource::UniformBufferStaticSlot slot, resource::UniformBuffer* const uniformBuffer
) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("setStaticUniformBuffer must be called outside of an active render pass.");
    }
    if (slot < kMaxUniformBufferStaticSlots)
    {
        m_staticUniformBuffers.bindings[slot] = uniformBuffer;
    }
#if GP_ENABLE_RHI_VALIDATION
    if (uniformBuffer != nullptr)
    {
        uniformBuffer->getValidationResource().validateLifeTime(
            m_rhi.getValidationFrameID(), uniformBuffer->getLayout().getDebugName()
        );
    }
#endif
    m_innerContext->setStaticUniformBuffer(slot, uniformBuffer);
}

void ValidationContext::setUniformBlockOffset(view::ShaderResourceView* const srv, const gp::UInt32 offset) noexcept
{
    if (srv == nullptr)
    {
        validation::reportValidationFailure("setUniformBlockOffset called with a null ShaderResourceView.");
        return;
    }
    m_innerContext->setUniformBlockOffset(srv, offset);
}

void ValidationContext::copyToStagingBuffer(
    resource::Buffer* const sourceBuffer,
    resource::StagingBuffer* const destinationStagingBuffer,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("copyToStagingBuffer is forbidden inside an active render pass.");
    }
    if (sourceBuffer == nullptr || destinationStagingBuffer == nullptr)
    {
        validation::reportValidationFailure("copyToStagingBuffer called with a null sourceBuffer or stagingBuffer.");
        return;
    }
    if ((static_cast<gp::UInt64>(offset) + static_cast<gp::UInt64>(numBytes)) >
        static_cast<gp::UInt64>(sourceBuffer->getSize()))
    {
        validation::reportValidationFailure(
            "copyToStagingBuffer [offset, offset + numBytes) exceeds sourceBuffer size."
        );
    }
#if GP_ENABLE_RHI_VALIDATION
    if (validation::g_rhiValidateBufferSourceCopy && sourceBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            sourceBuffer->getValidationTrackerResource()->getWholeResourceIdentity(), sync::ResourceAccess::CopySrc
        );
    }
#endif
    m_innerContext->copyToStagingBuffer(sourceBuffer, destinationStagingBuffer, offset, numBytes);
}

void ValidationContext::writeGPUFence(sync::GPUFence* const fence) noexcept
{
    if (fence == nullptr)
    {
        validation::reportValidationFailure("writeGPUFence called with a null GPUFence.");
        return;
    }
    m_innerContext->writeGPUFence(fence);
}

void ValidationContext::setGPUMask(const GPUMask gpuMask) noexcept
{
    command::IComputeContext::setGPUMask(gpuMask);
    m_innerContext->setGPUMask(gpuMask);
}

void ValidationContext::postExternalCommandsReset() noexcept
{
    resetContextState();
    m_innerContext->postExternalCommandsReset();
}

void ValidationContext::setTrackedAccess(const sync::TrackedAccessInfo& info) noexcept
{
    command::IComputeContext::setTrackedAccess(info);
#if GP_ENABLE_RHI_VALIDATION
    if (info.resource != nullptr && info.resource->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.setTrackedAccess(
            info.resource->getValidationTrackerResource()->getWholeResourceIdentity(), info.access, info.pipelines
        );
    }
#endif
    m_innerContext->setTrackedAccess(info);
}

void ValidationContext::beginUAVOverlap() noexcept
{
    m_stateTracker.allUAVsOverlap(true);
    m_innerContext->beginUAVOverlap();
}

void ValidationContext::endUAVOverlap() noexcept
{
    m_stateTracker.allUAVsOverlap(false);
    m_innerContext->endUAVOverlap();
}

void ValidationContext::beginUAVOverlap(const gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    for (view::UnorderedAccessView* const uav: uavs)
    {
        if (uav != nullptr)
        {
            m_stateTracker.specificUAVOverlap(uav->getViewIdentity(), true);
        }
    }
#endif
    m_innerContext->beginUAVOverlap(uavs);
}

void ValidationContext::endUAVOverlap(const gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    for (view::UnorderedAccessView* const uav: uavs)
    {
        if (uav != nullptr)
        {
            m_stateTracker.specificUAVOverlap(uav->getViewIdentity(), false);
        }
    }
#endif
    m_innerContext->endUAVOverlap(uavs);
}

#if GP_WITH_RHI_BREADCRUMBS
void ValidationContext::beginBreadcrumbGPU(diagnostics::BreadcrumbNode* const node) noexcept
{
    m_innerContext->beginBreadcrumbGPU(node);
}

void ValidationContext::endBreadcrumbGPU(diagnostics::BreadcrumbNode* const node) noexcept
{
    m_innerContext->endBreadcrumbGPU(node);
}
#endif

void ValidationContext::setMultipleViewports(const gp::UInt32 count, const command::ViewportBounds* const data) noexcept
{
    if (count > 0u && data == nullptr)
    {
        validation::reportValidationFailure("setMultipleViewports called with count > 0 and null data.");
        return;
    }
    m_innerContext->setMultipleViewports(count, data);
}

void ValidationContext::resummarizeHTile(resource::Texture* const depthTexture) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("resummarizeHTile is forbidden inside an active render pass.");
    }
    if (depthTexture == nullptr)
    {
        validation::reportValidationFailure("resummarizeHTile called with a null depthTexture.");
        return;
    }
#if GP_ENABLE_RHI_VALIDATION
    if (validation::Resource* const valRes = depthTexture->getValidationTrackerResource(); valRes != nullptr)
    {
        m_stateTracker.assertAccess(valRes->getWholeResourceIdentity(), sync::ResourceAccess::DSVWrite);
    }
#endif
    m_innerContext->resummarizeHTile(depthTexture);
}

void ValidationContext::beginRenderQuery(sync::RenderQuery* const renderQuery) noexcept
{
    if (renderQuery == nullptr)
    {
        validation::reportValidationFailure("beginRenderQuery called with a null RenderQuery.");
        return;
    }
    m_innerContext->beginRenderQuery(renderQuery);
}

void ValidationContext::endRenderQuery(sync::RenderQuery* const renderQuery) noexcept
{
    if (renderQuery == nullptr)
    {
        validation::reportValidationFailure("endRenderQuery called with a null RenderQuery.");
        return;
    }
    m_innerContext->endRenderQuery(renderQuery);
}

void ValidationContext::setStreamSource(
    const gp::UInt32 streamIndex, resource::Buffer* const vertexBuffer, const gp::UInt32 offset
) noexcept
{
    if (!m_insideRenderPass)
    {
        validation::reportValidationFailure("setStreamSource called outside an active render pass.");
    }
    if (streamIndex >= kMaxVertexElementCount)
    {
        validation::reportValidationFailure("setStreamSource streamIndex exceeds kMaxVertexElementCount.");
        return;
    }

    if (vertexBuffer != nullptr)
    {
        if ((vertexBuffer->getUsage() & resource::BufferUsageFlags::VertexBuffer) == resource::BufferUsageFlags::None)
        {
            char msg[256]{};
            const gp::String vbName = vertexBuffer->getName().toString();
            const int len = std::snprintf(
                msg,
                sizeof(msg),
                "Buffer '%s' bound via setStreamSource without BufferUsageFlags::VertexBuffer.",
                vbName.cString()
            );
            if (len > 0)
            {
                validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
            }
        }
        if (offset >= vertexBuffer->getSize() && vertexBuffer->getSize() > 0u)
        {
            validation::reportValidationFailure("setStreamSource offset exceeds vertexBuffer size.");
        }
    }

    m_vertexStreams[streamIndex] = VertexStreamBinding{ vertexBuffer, offset };
    m_innerContext->setStreamSource(streamIndex, vertexBuffer, offset);
}

void ValidationContext::setViewport(
    const gp::Float32 minX,
    const gp::Float32 minY,
    const gp::Float32 minZ,
    const gp::Float32 maxX,
    const gp::Float32 maxY,
    const gp::Float32 maxZ
) noexcept
{
    m_innerContext->setViewport(minX, minY, minZ, maxX, maxY, maxZ);
}

void ValidationContext::setStereoViewport(
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
    m_innerContext->setStereoViewport(
        leftMinX, rightMinX, leftMinY, rightMinY, minZ, leftMaxX, rightMaxX, leftMaxY, rightMaxY, maxZ
    );
}

void ValidationContext::setScissorRect(
    const bool enable, const gp::UInt32 minX, const gp::UInt32 minY, const gp::UInt32 maxX, const gp::UInt32 maxY
) noexcept
{
    if (enable && (minX > maxX || minY > maxY))
    {
        validation::reportValidationFailure("setScissorRect called with inverted rectangle bounds (min > max).");
    }
    m_innerContext->setScissorRect(enable, minX, minY, maxX, maxY);
}

void ValidationContext::setGraphicsPipelineState(
    pipeline::GraphicsPipelineState* const graphicsState, const gp::UInt32 stencilRef, const bool applyAdditionalState
) noexcept
{
    if (!m_insideRenderPass)
    {
        validation::reportValidationFailure("setGraphicsPipelineState called outside an active render pass.");
    }
    if (graphicsState == nullptr)
    {
        validation::reportValidationFailure("setGraphicsPipelineState called with a null GraphicsPipelineState.");
        return;
    }

#if GP_ENABLE_RHI_VALIDATION
    const resource::ExclusiveDepthStencil passDS = m_renderPassInfo.depthStencilRenderTarget.exclusiveDepthStencil;
    if (!graphicsState->dsMode.isValid(passDS))
    {
        validation::reportValidationFailure(
            "setGraphicsPipelineState: PSO depth-stencil read/write state is incompatible with the active render pass DSV access mode."
        );
    }
#endif

    m_boundGraphicsPSO = graphicsState;
    m_boundUniformBuffers.resetGraphicsStages();
    m_stateTracker.clearUAVState(validation::UAVMode::Graphics);
    m_innerContext->setGraphicsPipelineState(graphicsState, stencilRef, applyAdditionalState);
}

void ValidationContext::setStencilRef(const gp::UInt32 stencilRef) noexcept
{
    m_innerContext->setStencilRef(stencilRef);
}

void ValidationContext::setBlendFactor(const gp::math::LinearColor& blendFactor) noexcept
{
    m_innerContext->setBlendFactor(blendFactor);
}

void ValidationContext::validateDrawing() noexcept
{
    if (!m_insideRenderPass)
    {
        validation::reportValidationFailure("Draw/mesh-dispatch command issued outside an active render pass.");
    }
    if (m_boundGraphicsPSO == nullptr)
    {
        validation::reportValidationFailure("Draw/mesh-dispatch command issued without a bound GraphicsPipelineState.");
    }

#if GP_ENABLE_RHI_VALIDATION
    for (gp::UInt32 slot = 0u; slot < kMaxVertexElementCount; ++slot)
    {
        resource::Buffer* const vb = m_vertexStreams[slot].buffer;
        if (vb != nullptr && vb->getValidationTrackerResource() != nullptr)
        {
            m_stateTracker.assertAccess(
                vb->getValidationTrackerResource()->getWholeResourceIdentity(),
                sync::ResourceAccess::VertexOrIndexBuffer
            );
        }
    }
#endif

    m_stateTracker.dispatchOrDrawBarrier(validation::UAVMode::Graphics);
}

void ValidationContext::drawPrimitive(
    const gp::UInt32 baseVertexIndex, const gp::UInt32 numPrimitives, const gp::UInt32 numInstances
) noexcept
{
    validateDrawing();
    recordDrawStats(1u, numPrimitives * numInstances);
    m_innerContext->drawPrimitive(baseVertexIndex, numPrimitives, numInstances);
}

void ValidationContext::drawPrimitiveIndirect(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    validateDrawing();
    ValidationRHI::validateIndirectArgsBuffer(
        argumentBuffer,
        argumentOffset,
        static_cast<gp::UInt32>(sizeof(command::DrawIndirectParameters)),
        static_cast<gp::UInt32>(sizeof(command::DrawIndirectParameters))
    );
#if GP_ENABLE_RHI_VALIDATION
    if (argumentBuffer != nullptr && argumentBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            argumentBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
            sync::ResourceAccess::IndirectArgs
        );
    }
#endif
    recordDrawStats(1u, 0u);
    m_innerContext->drawPrimitiveIndirect(argumentBuffer, argumentOffset);
}

void ValidationContext::drawIndexedPrimitive(
    resource::Buffer* const indexBuffer,
    const gp::Int32 baseVertexIndex,
    const gp::UInt32 firstInstance,
    const gp::UInt32 numVertices,
    const gp::UInt32 startIndex,
    const gp::UInt32 numPrimitives,
    const gp::UInt32 numInstances
) noexcept
{
    validateDrawing();
    if (indexBuffer == nullptr)
    {
        validation::reportValidationFailure("drawIndexedPrimitive called with a null indexBuffer.");
    }
    else
    {
        if ((indexBuffer->getUsage() & resource::BufferUsageFlags::IndexBuffer) == resource::BufferUsageFlags::None)
        {
            validation::reportValidationFailure(
                "drawIndexedPrimitive indexBuffer is missing BufferUsageFlags::IndexBuffer."
            );
        }
#if GP_ENABLE_RHI_VALIDATION
        if (indexBuffer->getValidationTrackerResource() != nullptr)
        {
            m_stateTracker.assertAccess(
                indexBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
                sync::ResourceAccess::VertexOrIndexBuffer
            );
        }
#endif
    }

    recordDrawStats(1u, numPrimitives * numInstances);
    m_innerContext->drawIndexedPrimitive(
        indexBuffer, baseVertexIndex, firstInstance, numVertices, startIndex, numPrimitives, numInstances
    );
}

void ValidationContext::drawIndexedPrimitiveIndirect(
    resource::Buffer* const indexBuffer, resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    validateDrawing();
    if (indexBuffer == nullptr)
    {
        validation::reportValidationFailure("drawIndexedPrimitiveIndirect called with a null indexBuffer.");
    }
    else
    {
        if ((indexBuffer->getUsage() & resource::BufferUsageFlags::IndexBuffer) == resource::BufferUsageFlags::None)
        {
            validation::reportValidationFailure(
                "drawIndexedPrimitiveIndirect indexBuffer is missing BufferUsageFlags::IndexBuffer."
            );
        }
#if GP_ENABLE_RHI_VALIDATION
        if (indexBuffer->getValidationTrackerResource() != nullptr)
        {
            m_stateTracker.assertAccess(
                indexBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
                sync::ResourceAccess::VertexOrIndexBuffer
            );
        }
#endif
    }

    ValidationRHI::validateIndirectArgsBuffer(
        argumentBuffer,
        argumentOffset,
        static_cast<gp::UInt32>(sizeof(command::DrawIndexedIndirectParameters)),
        static_cast<gp::UInt32>(sizeof(command::DrawIndexedIndirectParameters))
    );
#if GP_ENABLE_RHI_VALIDATION
    if (argumentBuffer != nullptr && argumentBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            argumentBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
            sync::ResourceAccess::IndirectArgs
        );
    }
#endif
    recordDrawStats(1u, 0u);
    m_innerContext->drawIndexedPrimitiveIndirect(indexBuffer, argumentBuffer, argumentOffset);
}

void ValidationContext::multiDrawIndexedPrimitiveIndirect(
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
    validateDrawing();
    if (indexBuffer == nullptr)
    {
        validation::reportValidationFailure("multiDrawIndexedPrimitiveIndirect called with a null indexBuffer.");
    }
    else
    {
        if ((indexBuffer->getUsage() & resource::BufferUsageFlags::IndexBuffer) == resource::BufferUsageFlags::None)
        {
            validation::reportValidationFailure(
                "multiDrawIndexedPrimitiveIndirect indexBuffer is missing BufferUsageFlags::IndexBuffer."
            );
        }
#if GP_ENABLE_RHI_VALIDATION
        if (indexBuffer->getValidationTrackerResource() != nullptr)
        {
            m_stateTracker.assertAccess(
                indexBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
                sync::ResourceAccess::VertexOrIndexBuffer
            );
        }
#endif
    }

    if (maxDrawArguments > 0u)
    {
        ValidationRHI::validateIndirectArgsBuffer(
            argumentBuffer,
            argumentOffset,
            maxDrawArguments * static_cast<gp::UInt32>(sizeof(command::DrawIndexedIndirectParameters)),
            static_cast<gp::UInt32>(sizeof(command::DrawIndexedIndirectParameters))
        );
    }
#if GP_ENABLE_RHI_VALIDATION
    if (argumentBuffer != nullptr && argumentBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            argumentBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
            sync::ResourceAccess::IndirectArgs
        );
    }
#endif

    if (countBuffer != nullptr)
    {
        ValidationRHI::validateIndirectArgsBuffer(
            countBuffer, countBufferOffset, static_cast<gp::UInt32>(sizeof(gp::UInt32)), sizeof(gp::UInt32)
        );
#if GP_ENABLE_RHI_VALIDATION
        if (countBuffer->getValidationTrackerResource() != nullptr)
        {
            m_stateTracker.assertAccess(
                countBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
                sync::ResourceAccess::IndirectArgs
            );
        }
#endif
    }

    recordDrawStats(1u, 0u);
    m_innerContext->multiDrawIndexedPrimitiveIndirect(
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

void ValidationContext::dispatchMeshShader(
    const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
) noexcept
{
    validateDrawing();
    ValidationRHI::validateThreadGroupCount(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
    recordDispatchStats(1u);
    m_innerContext->dispatchMeshShader(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void ValidationContext::dispatchIndirectMeshShader(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    validateDrawing();
    ValidationRHI::validateDispatchIndirectArgsBuffer(argumentBuffer, argumentOffset);
#if GP_ENABLE_RHI_VALIDATION
    if (argumentBuffer != nullptr && argumentBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            argumentBuffer->getValidationTrackerResource()->getWholeResourceIdentity(),
            sync::ResourceAccess::IndirectArgs
        );
    }
#endif
    recordDispatchStats(1u);
    m_innerContext->dispatchIndirectMeshShader(argumentBuffer, argumentOffset);
}

void ValidationContext::setDepthBounds(const gp::Float32 minDepth, const gp::Float32 maxDepth) noexcept
{
    if (minDepth > maxDepth)
    {
        validation::reportValidationFailure("setDepthBounds called with minDepth > maxDepth.");
    }
    m_innerContext->setDepthBounds(minDepth, maxDepth);
}

void ValidationContext::setShadingRate(
    const pipeline::VRSShadingRate shadingRate, const pipeline::VRSRateCombiner combiner
) noexcept
{
    m_innerContext->setShadingRate(shadingRate, combiner);
}

void ValidationContext::beginRenderPass(const command::RenderPassInfo& inInfo, const char* const name) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("Nested beginRenderPass detected before ending the active render pass.");
    }

    if (!inInfo.validate())
    {
        validation::reportValidationFailure(
            "RenderPassInfo::validate() failed in beginRenderPass (non-contiguous RTs, sample count mismatch, invalid resolve, or out-of-bounds accessRect)."
        );
    }

    m_insideRenderPass = true;
    m_staticUniformBuffers.inRenderPass = true;
    m_boundGraphicsPSO = nullptr;
    m_renderPassInfo = inInfo;

    gp::Int32 commonWidth = -1;
    gp::Int32 commonHeight = -1;

    // Validate color attachments & resolve targets.
    const gp::UInt32 numColor = inInfo.getNumColorRenderTargets();
    for (gp::UInt32 i = 0u; i < numColor; ++i)
    {
        const command::RenderPassInfo::ColorEntry& entry = inInfo.colorRenderTargets[i];
        resource::Texture* const rt = entry.renderTarget;
        if (rt == nullptr)
        {
            continue;
        }

        if ((rt->getFlags() & resource::TextureCreateFlags::RenderTargetable) == resource::TextureCreateFlags::None)
        {
            char msg[256]{};
            const gp::String rtName = rt->getName().toString();
            const int len = std::snprintf(
                msg,
                sizeof(msg),
                "Render target '%s' at slot %u lacks TextureCreateFlags::RenderTargetable.",
                rtName.cString(),
                i
            );
            if (len > 0)
            {
                validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
            }
        }

        if (entry.mipIndex >= rt->getDesc().numMips)
        {
            validation::reportValidationFailure("RenderPassInfo color attachment mipIndex exceeds texture numMips.");
            continue;
        }

        const resource::IntVector mipDim = rt->getMipDimensions(entry.mipIndex);
        if (commonWidth < 0)
        {
            commonWidth = mipDim.x;
            commonHeight = mipDim.y;
        }
        else if (mipDim.x != commonWidth || mipDim.y != commonHeight)
        {
            validation::reportValidationFailure(
                "RenderPassInfo attachments have mismatched mip dimensions across slots."
            );
        }

#if GP_ENABLE_RHI_VALIDATION
        if (validation::Resource* const valRes = rt->getValidationTrackerResource(); valRes != nullptr)
        {
            validation::ResourceIdentity id{};
            id.resource = valRes;
            id.subresourceRange.mipIndex = entry.mipIndex;
            id.subresourceRange.numMips = 1u;
            id.subresourceRange.arraySlice = (entry.arraySlice < 0) ? 0u : static_cast<gp::UInt16>(entry.arraySlice);
            id.subresourceRange.numArraySlices = (entry.arraySlice < 0) ? valRes->getNumArraySlices() : 1u;
            id.subresourceRange.planeIndex = 0u;
            id.subresourceRange.numPlanes = 1u;
            m_stateTracker.assertAccess(id, sync::ResourceAccess::RTV);
        }

        if (entry.resolveTarget != nullptr && entry.resolveTarget != rt)
        {
            if (validation::Resource* const resolveVal = entry.resolveTarget->getValidationTrackerResource();
                resolveVal != nullptr)
            {
                m_stateTracker.assertAccess(resolveVal->getWholeResourceIdentity(), sync::ResourceAccess::ResolveDst);
            }
        }
#endif
    }

    // Validate depth-stencil attachment.
    if (resource::Texture* const ds = inInfo.depthStencilRenderTarget.depthStencilTarget; ds != nullptr)
    {
        if ((ds->getFlags() & resource::TextureCreateFlags::DepthStencilTargetable) ==
            resource::TextureCreateFlags::None)
        {
            char msg[256]{};
            const gp::String dsName = ds->getName().toString();
            const int len = std::snprintf(
                msg,
                sizeof(msg),
                "Depth-stencil target '%s' lacks TextureCreateFlags::DepthStencilTargetable.",
                dsName.cString()
            );
            if (len > 0)
            {
                validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
            }
        }

        const resource::IntVector dsDim = ds->getMipDimensions(0u);
        if (commonWidth >= 0 && (dsDim.x < commonWidth || dsDim.y < commonHeight))
        {
            validation::reportValidationFailure(
                "RenderPassInfo depth-stencil attachment is smaller than color attachment extent."
            );
        }

#if GP_ENABLE_RHI_VALIDATION
        if (validation::Resource* const dsVal = ds->getValidationTrackerResource(); dsVal != nullptr)
        {
            const resource::ExclusiveDepthStencil dsAccess = inInfo.depthStencilRenderTarget.exclusiveDepthStencil;
            dsAccess.enumerateSubresources(
                [this, dsVal](const sync::ResourceAccess planeAccess, const gp::UInt16 planeSlice) noexcept -> void
            {
                if (planeSlice == sync::SubresourceRange::kAllSubresources)
                {
                    m_stateTracker.assertAccess(dsVal->getWholeResourceIdentity(), planeAccess);
                }
                else
                {
                    const validation::ResourcePlane valPlane =
                        (planeSlice == sync::SubresourceRange::kStencilPlaneSlice) ? validation::ResourcePlane::Stencil
                                                                                   : validation::ResourcePlane::Common;
                    m_stateTracker.assertAccess(dsVal->getPlaneIdentity(valPlane), planeAccess);
                }
            }
            );
        }
#endif
    }

    // Validate VRS shading rate attachment.
    if (inInfo.shadingRateTexture.isValid())
    {
        resource::Texture* const vrs = inInfo.shadingRateTexture.get();
#if GP_ENABLE_RHI_VALIDATION
        if (validation::Resource* const vrsVal = vrs->getValidationTrackerResource(); vrsVal != nullptr)
        {
            m_stateTracker.assertAccess(vrsVal->getWholeResourceIdentity(), sync::ResourceAccess::ShadingRateSource);
        }
#endif
    }

    m_innerContext->beginRenderPass(inInfo, name);
}

void ValidationContext::endRenderPass() noexcept
{
    if (!m_insideRenderPass)
    {
        validation::reportValidationFailure("endRenderPass called when no render pass is active.");
        return;
    }

    m_insideRenderPass = false;
    m_staticUniformBuffers.inRenderPass = false;
    m_boundGraphicsPSO = nullptr;
    m_stateTracker.clearUAVState(validation::UAVMode::Graphics);
    for (gp::UInt32 i = 0u; i < kMaxVertexElementCount; ++i)
    {
        m_vertexStreams[i] = VertexStreamBinding{};
    }

    m_innerContext->endRenderPass();
}

void ValidationContext::nextSubpass() noexcept
{
    if (!m_insideRenderPass)
    {
        validation::reportValidationFailure("nextSubpass called outside an active render pass.");
    }
    m_innerContext->nextSubpass();
}

void ValidationContext::copyTexture(
    resource::Texture* const sourceTexture,
    resource::Texture* const destTexture,
    const command::CopyTextureInfo& copyInfo
) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("copyTexture is forbidden inside an active render pass.");
    }
    validation::ValidationUtils::validateCopyTexture(sourceTexture, destTexture, copyInfo);

#if GP_ENABLE_RHI_VALIDATION
    if (sourceTexture != nullptr && sourceTexture->getValidationTrackerResource() != nullptr)
    {
        validation::Resource* const srcVal = sourceTexture->getValidationTrackerResource();
        validation::ResourceIdentity srcId{};
        srcId.resource = srcVal;
        srcId.subresourceRange.mipIndex = static_cast<gp::UInt16>(copyInfo.sourceMipIndex);
        srcId.subresourceRange.numMips = static_cast<gp::UInt16>(copyInfo.numMips);
        srcId.subresourceRange.arraySlice = static_cast<gp::UInt16>(copyInfo.sourceSliceIndex);
        srcId.subresourceRange.numArraySlices = static_cast<gp::UInt16>(copyInfo.numSlices);
        srcId.subresourceRange.planeIndex = 0u;
        srcId.subresourceRange.numPlanes = srcVal->getNumPlanes();
        m_stateTracker.assertAccess(srcId, sync::ResourceAccess::CopySrc);
    }

    if (destTexture != nullptr && destTexture->getValidationTrackerResource() != nullptr)
    {
        validation::Resource* const dstVal = destTexture->getValidationTrackerResource();
        validation::ResourceIdentity dstId{};
        dstId.resource = dstVal;
        dstId.subresourceRange.mipIndex = static_cast<gp::UInt16>(copyInfo.destMipIndex);
        dstId.subresourceRange.numMips = static_cast<gp::UInt16>(copyInfo.numMips);
        dstId.subresourceRange.arraySlice = static_cast<gp::UInt16>(copyInfo.destSliceIndex);
        dstId.subresourceRange.numArraySlices = static_cast<gp::UInt16>(copyInfo.numSlices);
        dstId.subresourceRange.planeIndex = 0u;
        dstId.subresourceRange.numPlanes = dstVal->getNumPlanes();
        m_stateTracker.assertAccess(dstId, sync::ResourceAccess::CopyDest);
    }
#endif

    m_innerContext->copyTexture(sourceTexture, destTexture, copyInfo);
}

void ValidationContext::copyBufferRegion(
    resource::Buffer* const destBuffer,
    const gp::UInt64 dstOffset,
    resource::Buffer* const sourceBuffer,
    const gp::UInt64 srcOffset,
    const gp::UInt64 numBytes
) noexcept
{
    if (m_insideRenderPass)
    {
        validation::reportValidationFailure("copyBufferRegion is forbidden inside an active render pass.");
    }
    validation::ValidationUtils::validateCopyBufferRegion(destBuffer, dstOffset, sourceBuffer, srcOffset, numBytes);

#if GP_ENABLE_RHI_VALIDATION
    if (validation::g_rhiValidateBufferSourceCopy && sourceBuffer != nullptr &&
        sourceBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            sourceBuffer->getValidationTrackerResource()->getWholeResourceIdentity(), sync::ResourceAccess::CopySrc
        );
    }
    if (destBuffer != nullptr && destBuffer->getValidationTrackerResource() != nullptr)
    {
        m_stateTracker.assertAccess(
            destBuffer->getValidationTrackerResource()->getWholeResourceIdentity(), sync::ResourceAccess::CopyDest
        );
    }
#endif

    m_innerContext->copyBufferRegion(destBuffer, dstOffset, sourceBuffer, srcOffset, numBytes);
}

}   // namespace gp::rhi
