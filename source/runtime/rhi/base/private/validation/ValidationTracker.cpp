// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"
#include "core/RHIStrings.hpp"
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/GlobalMemory.hpp"
#include "memory/MemoryOrder.hpp"
#include "resources/Buffer.hpp"
#include "resources/Texture.hpp"   // IWYU pragma: keep
#include "resources/UniformBuffer.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPixelFormat.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "validation/ValidationEnums.hpp"
#include "validation/ValidationResource.hpp"
#include "validation/ValidationTypes.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include <cstdio>

namespace gp::rhi::validation
{

bool g_rhiValidationEnabled = false;
bool g_rhiValidateBufferSourceCopy = true;
bool g_rhiValidationBreakOnError = false;
bool g_rhiValidationDeduplicateErrors = false;

namespace
{

struct ValidationFailureState
{
    gp::thread::Mutex mutex{};
    ValidationFailureFn callback{ nullptr };
    void* callbackUserData{ nullptr };
    gp::thread::Atomic<gp::UInt32> count{ 0u };
    gp::String lastMessage{};
    gp::Vector<gp::String> seenMessages{};

    [[nodiscard]] bool hasSeenMessage(const gp::StringView candidate) const noexcept
    {
        for (const gp::String& existing: seenMessages)
        {
            if (existing.view() == candidate)
            {
                return true;
            }
        }
        return false;
    }
};

ValidationFailureState& getFailureState() noexcept
{
    static ValidationFailureState s_state{};
    return s_state;
}

struct PipelineReplayQueue
{
    gp::Vector<Operation> pending{};
    gp::USize readIndex{ 0u };
    bool allowAllUAVsOverlap{ false };
};

struct GlobalTrackerQueues
{
    gp::thread::Mutex mutex{};
    sync::PipelineArray<PipelineReplayQueue> queues{};
};

GlobalTrackerQueues& getGlobalTrackerQueues() noexcept
{
    static GlobalTrackerQueues s_queues{};
    return s_queues;
}

void formatSubresourceSuffix(char* const outBuf, const gp::USize cap, const SubresourceIndex& index) noexcept
{
    if (index.isWholeResource())
    {
        std::snprintf(outBuf, cap, "WholeResource");
    }
    else
    {
        std::snprintf(outBuf, cap, "Mip=%d, Slice=%d, Plane=%d", index.mipIndex, index.arraySlice, index.planeIndex);
    }
}

[[nodiscard]] gp::UInt8 mapTexturePlaneToValidationPlane(const resource::TexturePlane plane) noexcept
{
    switch (plane)
    {
    case resource::TexturePlane::Stencil:
    case resource::TexturePlane::CMask:
        return static_cast<gp::UInt8>(ResourcePlane::Stencil);

    case resource::TexturePlane::HTile:
        return static_cast<gp::UInt8>(ResourcePlane::HTile);

    case resource::TexturePlane::Primary:
    case resource::TexturePlane::PrimaryCompressed:
    case resource::TexturePlane::Depth:
    case resource::TexturePlane::FMask:
    default:
        return static_cast<gp::UInt8>(ResourcePlane::Common);
    }
}

}   // namespace

void setValidationFailureCallback(const ValidationFailureFn callback, void* const userData) noexcept
{
    ValidationFailureState& state = getFailureState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    state.callback = callback;
    state.callbackUserData = userData;
}

void reportValidationFailure(const gp::StringView message) noexcept
{
    ValidationFailureState& state = getFailureState();
    ValidationFailureFn cb = nullptr;
    void* userData = nullptr;
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
        if (g_rhiValidationDeduplicateErrors)
        {
            if (state.hasSeenMessage(message))
            {
                return;
            }
            state.seenMessages.emplaceBack(gp::String(message));
        }
        state.lastMessage = gp::String(message);
        cb = state.callback;
        userData = state.callbackUserData;
    }

    state.count.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
    GP_LOG_ERROR(::gp::LogRHI, "[RHIValidation] {}", message);

    if (cb != nullptr)
    {
        cb(message, userData);
    }

    if (g_rhiValidationBreakOnError)
    {
        GP_PLATFORM_BREAK();
    }
}

gp::UInt32 getValidationFailureCount() noexcept
{
    return getFailureState().count.load(gp::memory::MemoryOrder::Acquire);
}

gp::String getLastValidationFailureMessage() noexcept
{
    ValidationFailureState& state = getFailureState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    return state.lastMessage;
}

void clearValidationFailures() noexcept
{
    ValidationFailureState& state = getFailureState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    state.count.store(0u, gp::memory::MemoryOrder::Release);
    state.lastMessage.clear();
    state.seenMessages.clear();
}

gp::String State::toString() const noexcept
{
    gp::String out{};
    out.append("Access: ");
    out.append(getRHIAccessName(access).view());
    out.append(", Pipelines: ");
    out.append(getRHIPipelineName(pipelines).view());
    return out;
}

ViewIdentity::ViewIdentity(resource::ViewableResource* const targetResource, const view::ViewDesc& viewDesc) noexcept
{
    if (targetResource == nullptr)
    {
        return;
    }

#if GP_ENABLE_RHI_VALIDATION
    resource = targetResource->getValidationTrackerResource();
#endif
    if (resource == nullptr || !resource->isBarrierTrackingInitialized())
    {
        return;
    }

    if (viewDesc.isBuffer())
    {
        const resource::Buffer* const buf = static_cast<const resource::Buffer*>(targetResource);
        const resource::BufferDesc& desc = buf->getDesc();
        if (desc.isNull())
        {
            resource = nullptr;
            return;
        }

        subresourceRange = resource->getWholeResourceIdentity().subresourceRange;
        const view::ViewDesc::Buffer& bufView = viewDesc.isSRV()
                                                    ? static_cast<const view::ViewDesc::Buffer&>(viewDesc.buffer.srv)
                                                    : static_cast<const view::ViewDesc::Buffer&>(viewDesc.buffer.uav);
        switch (bufView.bufferType)
        {
        case view::BufferViewType::Typed:
            stride = isValidPixelFormat(bufView.format) ? getPixelFormatInfo(bufView.format).blockBytes : 0u;
            break;
        case view::BufferViewType::Structured:
            stride = (bufView.range.stride != 0u) ? bufView.range.stride : desc.stride;
            break;
        case view::BufferViewType::Raw:
            stride = static_cast<gp::UInt32>(sizeof(gp::UInt32));
            break;
        case view::BufferViewType::AccelerationStructure:
            stride = 1u;
            break;
        default:
            stride = 0u;
            break;
        }
    }
    else
    {
        const view::ViewDesc::Texture& texView =
            viewDesc.isSRV() ? static_cast<const view::ViewDesc::Texture&>(viewDesc.texture.srv)
                             : static_cast<const view::ViewDesc::Texture&>(viewDesc.texture.uav);
        const gp::UInt8 valPlane = mapTexturePlaneToValidationPlane(texView.plane);
        const bool isCubeView = (texView.dimension == view::TextureViewDimension::TextureCube) ||
                                (texView.dimension == view::TextureViewDimension::TextureCubeArray);
        const gp::UInt16 sliceMultiplier = isCubeView ? static_cast<gp::UInt16>(resource::kCubeFaceCount) : 1u;

        const gp::UInt16 totalMips = resource->getNumMips();
        const gp::UInt16 firstMip = gp::math::min<gp::UInt16>(
            static_cast<gp::UInt16>(texView.mipRange.first), static_cast<gp::UInt16>(totalMips - 1u)
        );
        const gp::UInt16 requestedMips =
            viewDesc.isUAV() ? 1u
                             : ((texView.mipRange.num == 0u) ? static_cast<gp::UInt16>(totalMips - firstMip)
                                                             : static_cast<gp::UInt16>(texView.mipRange.num));
        const gp::UInt16 numMips =
            gp::math::min<gp::UInt16>(requestedMips, static_cast<gp::UInt16>(totalMips - firstMip));

        const gp::UInt16 totalSlices = resource->getNumArraySlices();
        const gp::UInt16 firstSlice = gp::math::min<gp::UInt16>(
            static_cast<gp::UInt16>(texView.arrayRange.first * sliceMultiplier),
            static_cast<gp::UInt16>(totalSlices - 1u)
        );
        const gp::UInt16 requestedSlices = (texView.arrayRange.num == 0u)
                                               ? static_cast<gp::UInt16>(totalSlices - firstSlice)
                                               : static_cast<gp::UInt16>(texView.arrayRange.num * sliceMultiplier);
        const gp::UInt16 numSlices =
            gp::math::min<gp::UInt16>(requestedSlices, static_cast<gp::UInt16>(totalSlices - firstSlice));

        subresourceRange.mipIndex = firstMip;
        subresourceRange.numMips = gp::math::max<gp::UInt16>(1u, numMips);
        subresourceRange.arraySlice = firstSlice;
        subresourceRange.numArraySlices = gp::math::max<gp::UInt16>(1u, numSlices);
        subresourceRange.planeIndex =
            gp::math::min<gp::UInt8>(valPlane, static_cast<gp::UInt8>(resource->getNumPlanes() - 1u));
        subresourceRange.numPlanes = 1u;
    }
}

void StaticUniformBuffers::reset() noexcept
{
    for (gp::UInt32 i = 0u; i < kMaxUniformBufferStaticSlots; ++i)
    {
        bindings[i] = nullptr;
    }
    inRenderPass = false;
}

void StaticUniformBuffers::validateShaderUniformBuffer(
    const resource::UniformBuffer* const uniformBuffer
) const noexcept
{
    if (uniformBuffer == nullptr)
    {
        return;
    }

    const resource::UniformBufferLayout& layout = uniformBuffer->getLayout();
    if (inRenderPass && resource::isUniformBufferStaticSlotValid(layout.getStaticSlot()))
    {
        char msg[256]{};
        const gp::StringView layoutName = layout.getDebugName();
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Uniform buffer '%.*s' has global static slot %u and should be bound via setStaticUniformBuffers outside render passes.",
            static_cast<int>(layoutName.size()),
            layoutName.data(),
            static_cast<gp::UInt32>(layout.getStaticSlot())
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
}

void StageBoundUniformBuffers::reset() noexcept
{
    for (gp::UInt32 i = 0u; i < kMaxBoundSlots; ++i)
    {
        buffers[i] = nullptr;
    }
    boundMask = 0u;
}

void StageBoundUniformBuffers::bind(const gp::UInt32 slotIndex, resource::UniformBuffer* const buffer) noexcept
{
    if (slotIndex >= kMaxBoundSlots)
    {
        return;
    }

    buffers[slotIndex] = buffer;
    if (buffer != nullptr)
    {
        boundMask |= (1u << slotIndex);
    }
    else
    {
        boundMask &= ~(1u << slotIndex);
    }
}

void BoundUniformBuffers::resetAll() noexcept
{
    for (gp::UInt32 i = 0u; i < shader::kShaderFrequencyCount; ++i)
    {
        stages[i].reset();
    }
}

void BoundUniformBuffers::resetGraphicsStages() noexcept
{
    stages[static_cast<gp::UInt32>(shader::ShaderFrequency::Vertex)].reset();
    stages[static_cast<gp::UInt32>(shader::ShaderFrequency::Mesh)].reset();
    stages[static_cast<gp::UInt32>(shader::ShaderFrequency::Amplification)].reset();
    stages[static_cast<gp::UInt32>(shader::ShaderFrequency::Pixel)].reset();
    stages[static_cast<gp::UInt32>(shader::ShaderFrequency::Geometry)].reset();
}

SubresourceState::SubresourceState(const sync::ResourceAccess initialAccess) noexcept
{
    const State initial(initialAccess, sync::PipelineStage::Graphics);
    for (PipelineState& pipeState: pipelines)
    {
        pipeState.previous = initial;
        pipeState.current = initial;
    }
}

void SubresourceState::beginTransition(
    Resource& resource,
    const SubresourceIndex subresourceIndex,
    const State& stateBefore,
    const State& stateAfter,
    const sync::ResourceTransitionFlags newFlags,
    const sync::PipelineStage executePipeline
) noexcept
{
    PipelineState& pipe = pipelines[executePipeline];
    char subStr[64]{};
    char msg[512]{};

    if (!resource.getTransientState().isAcquired())
    {
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): cannot begin transition on an unacquired or discarded transient resource.",
            resource.getDebugName(),
            subStr
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    if (pipe.isTransitioning)
    {
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): duplicate beginTransitions while another split transition is already in progress on pipeline %s.",
            resource.getDebugName(),
            subStr,
            getRHIPipelineName(executePipeline).cString()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    if (pipe.explicitUAVOverlap)
    {
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): cannot transition resource while an explicit beginUAVOverlap scope is still open.",
            resource.getDebugName(),
            subStr
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }

    if (stateBefore.access != sync::ResourceAccess::Unknown && stateBefore.access != sync::ResourceAccess::Discard)
    {
        const bool accessMatches = (pipe.current.access == stateBefore.access);
        const bool pipeMatches = ((pipe.current.pipelines & stateBefore.pipelines) != sync::PipelineStage::None) ||
                                 (pipe.current.access == sync::ResourceAccess::Unknown);

        if (!accessMatches || !pipeMatches)
        {
            formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
            const gp::String currStr = pipe.current.toString();
            const gp::String expectedStr = stateBefore.toString();
            const int len = std::snprintf(
                msg,
                sizeof(msg),
                "Resource '%s' (%s): beginTransitions state mismatch on pipeline %s. Tracked=[%s], ExpectedBefore=[%s].",
                resource.getDebugName(),
                subStr,
                getRHIPipelineName(executePipeline).cString(),
                currStr.cString(),
                expectedStr.cString()
            );
            if (len > 0)
            {
                reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
            }
        }
    }

    pipe.previous = pipe.current;
    pipe.current = stateAfter;
    pipe.flags = newFlags;
    pipe.isTransitioning = true;
    pipe.usedWithAllUAVsOverlap = false;
    pipe.usedWithExplicitUAVOverlap = false;

    // Mirror target state to destination pipelines when transitioning across queues.
    const sync::PipelineStage kAllStages[sync::kPipelineStageCount] = { sync::PipelineStage::Graphics,
                                                                        sync::PipelineStage::AsyncCompute };
    for (const sync::PipelineStage dstStage: kAllStages)
    {
        if (dstStage != executePipeline && (stateAfter.pipelines & dstStage) != sync::PipelineStage::None)
        {
            PipelineState& dstPipe = pipelines[dstStage];
            dstPipe.previous = pipe.previous;
            dstPipe.current = stateAfter;
            dstPipe.flags = newFlags;
            dstPipe.isTransitioning = true;
        }
    }
}

void SubresourceState::endTransition(
    Resource& resource,
    const SubresourceIndex subresourceIndex,
    const State& stateBefore,
    const State& stateAfter,
    const sync::PipelineStage executePipeline
) noexcept
{
    static_cast<void>(stateBefore);
    PipelineState& pipe = pipelines[executePipeline];
    char subStr[64]{};
    char msg[512]{};

    if (!pipe.isTransitioning)
    {
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): endTransitions called without a matching beginTransitions on pipeline %s.",
            resource.getDebugName(),
            subStr,
            getRHIPipelineName(executePipeline).cString()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    pipe.current = stateAfter;
    pipe.isTransitioning = false;
}

void SubresourceState::assertAccess(
    Resource& resource,
    const SubresourceIndex subresourceIndex,
    const State& requiredState,
    const bool allowAllUAVsOverlap
) noexcept
{
    PipelineState& pipe = pipelines[requiredState.pipelines];
    char subStr[64]{};
    char msg[512]{};

    if (!resource.getTransientState().isAcquired())
    {
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): accessed on GPU while transient state is not Acquired.",
            resource.getDebugName(),
            subStr
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    if (pipe.isTransitioning)
    {
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): accessed on pipeline %s while an unfinished split transition is still in progress.",
            resource.getDebugName(),
            subStr,
            getRHIPipelineName(requiredState.pipelines).cString()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    const bool hasAccess = (pipe.current.access & requiredState.access) == requiredState.access;
    const bool hasPipeline = (pipe.current.pipelines & requiredState.pipelines) == requiredState.pipelines;
    if (!hasAccess || !hasPipeline)
    {
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const gp::String currStr = pipe.current.toString();
        const gp::String reqStr = requiredState.toString();
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): missing barrier before GPU access. Current=[%s], Required=[%s].",
            resource.getDebugName(),
            subStr,
            currStr.cString(),
            reqStr.cString()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    // Check unbarriered back-to-back UAV writes when UAV overlap is not enabled!
    if ((requiredState.access & sync::ResourceAccess::UAVMask) != sync::ResourceAccess::None)
    {
        const bool overlapActive = allowAllUAVsOverlap || pipe.allUAVsOverlap || pipe.explicitUAVOverlap;
        if (!overlapActive && (pipe.usedWithAllUAVsOverlap || pipe.usedWithExplicitUAVOverlap))
        {
            formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
            const int len = std::snprintf(
                msg,
                sizeof(msg),
                "Resource '%s' (%s): UAV accessed after UAV overlap ended without an intervening UAV barrier.",
                resource.getDebugName(),
                subStr
            );
            if (len > 0)
            {
                reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
            }
        }

        if (allowAllUAVsOverlap || pipe.allUAVsOverlap)
        {
            pipe.usedWithAllUAVsOverlap = true;
        }
        if (pipe.explicitUAVOverlap)
        {
            pipe.usedWithExplicitUAVOverlap = true;
        }
    }
}

void SubresourceState::assertTracked(
    Resource& resource,
    const SubresourceIndex subresourceIndex,
    const sync::ResourceAccess expectedAccess,
    const sync::PipelineStage executePipeline
) const noexcept
{
    const PipelineState& pipe = pipelines[executePipeline];
    if (expectedAccess != sync::ResourceAccess::Unknown && pipe.current.access != expectedAccess)
    {
        char subStr[64]{};
        char msg[512]{};
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): tracked access mismatch on pipeline %s. Actual=%s, Expected=%s.",
            resource.getDebugName(),
            subStr,
            getRHIPipelineName(executePipeline).cString(),
            getRHIAccessName(pipe.current.access).cString(),
            getRHIAccessName(expectedAccess).cString()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
}

void SubresourceState::setTracked(
    Resource& resource,
    const SubresourceIndex subresourceIndex,
    const sync::ResourceAccess trackedAccess,
    const sync::PipelineStage trackedPipelines
) noexcept
{
    static_cast<void>(resource);
    static_cast<void>(subresourceIndex);
    const sync::PipelineStage resolvedPipes =
        (trackedPipelines != sync::PipelineStage::None) ? trackedPipelines : sync::PipelineStage::Graphics;
    const State updated(trackedAccess, resolvedPipes);
    for (PipelineState& pipeState: pipelines)
    {
        if (!pipeState.isTransitioning)
        {
            pipeState.current = updated;
        }
    }
}

void SubresourceState::beginUAVOverlap(
    Resource& resource, const SubresourceIndex subresourceIndex, const sync::PipelineStage pipeline
) noexcept
{
    PipelineState& pipe = pipelines[pipeline];
    if (pipe.explicitUAVOverlap)
    {
        char subStr[64]{};
        char msg[256]{};
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): duplicate beginUAVOverlap on resource.",
            resource.getDebugName(),
            subStr
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
    pipe.explicitUAVOverlap = true;
}

void SubresourceState::endUAVOverlap(
    Resource& resource, const SubresourceIndex subresourceIndex, const sync::PipelineStage pipeline
) noexcept
{
    PipelineState& pipe = pipelines[pipeline];
    if (!pipe.explicitUAVOverlap)
    {
        char subStr[64]{};
        char msg[256]{};
        formatSubresourceSuffix(subStr, sizeof(subStr), subresourceIndex);
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s' (%s): endUAVOverlap called without matching beginUAVOverlap.",
            resource.getDebugName(),
            subStr
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
    pipe.explicitUAVOverlap = false;
}

void TransientState::acquire(Resource& resource, const sync::PipelineStage pipeline) noexcept
{
    static_cast<void>(pipeline);
    char msg[384]{};
    if (!isTransient)
    {
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s': TransientAliasingInfo::acquire issued for a non-transient resource.",
            resource.getDebugName()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    if (status == TransientStatus::Acquired)
    {
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Transient resource '%s': duplicate Acquire without an intervening Discard.",
            resource.getDebugName()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    status = TransientStatus::Acquired;
}

void TransientState::discardBegin(Resource& resource, const sync::PipelineStage pipeline) noexcept
{
    static_cast<void>(pipeline);
    char msg[384]{};
    if (!isTransient)
    {
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Resource '%s': TransientAliasingInfo::discard issued for a non-transient resource.",
            resource.getDebugName()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    if (status != TransientStatus::Acquired)
    {
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Transient resource '%s': Discard issued when resource is not currently Acquired.",
            resource.getDebugName()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    status = TransientStatus::Discarding;
}

void TransientState::discardEnd(Resource& resource, const sync::PipelineStage pipeline) noexcept
{
    static_cast<void>(pipeline);
    if (!isTransient)
    {
        return;
    }

    if (status != TransientStatus::Discarding)
    {
        char msg[384]{};
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Transient resource '%s': endTransitions Discard completed without a matching beginTransitions Discard.",
            resource.getDebugName()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    status = TransientStatus::Discarded;
}

void TransientState::aliasingOverlap(
    const Resource& overlappedResource, const Resource& acquiringResource
) const noexcept
{
    char msg[512]{};
    if (!overlappedResource.m_transientState.isTransient)
    {
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Transient acquire of '%s' specified non-transient overlap resource '%s'.",
            acquiringResource.getDebugName(),
            overlappedResource.getDebugName()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    if (overlappedResource.m_transientState.status != TransientStatus::Discarded)
    {
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Transient acquire of '%s' overlaps prior transient resource '%s' before '%s' finished its Discard transition.",
            acquiringResource.getDebugName(),
            overlappedResource.getDebugName(),
            overlappedResource.getDebugName()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
}

void Resource::initBarrierTracking(
    const gp::UInt32 numMips,
    const gp::UInt32 numArraySlices,
    const gp::UInt32 numPlanes,
    const sync::ResourceAccess initialAccess,
    const gp::StringView debugName,
    const bool isTransient
) noexcept
{
    m_numMips = static_cast<gp::UInt16>(gp::math::max<gp::UInt32>(1u, numMips));
    m_numArraySlices = static_cast<gp::UInt16>(gp::math::max<gp::UInt32>(1u, numArraySlices));
    m_numPlanes = static_cast<gp::UInt8>(gp::math::max<gp::UInt32>(1u, numPlanes));
    m_wholeResourceState = SubresourceState(initialAccess);
    m_subresourceStates.clear();
    m_transientState.setTransient(isTransient);
    if (!debugName.isEmpty())
    {
        m_debugName = gp::String(debugName);
    }
}

ResourceIdentity Resource::getWholeResourceIdentity() noexcept
{
    ResourceIdentity id{};
    id.resource = this;
    id.subresourceRange = SubresourceRange(0u, m_numMips, 0u, m_numArraySlices, 0u, m_numPlanes);
    return id;
}

ResourceIdentity Resource::getPlaneIdentity(const ResourcePlane plane) noexcept
{
    const gp::UInt8 clampedPlane =
        gp::math::min<gp::UInt8>(static_cast<gp::UInt8>(plane), static_cast<gp::UInt8>(m_numPlanes - 1u));
    ResourceIdentity id{};
    id.resource = this;
    id.subresourceRange = SubresourceRange(0u, m_numMips, 0u, m_numArraySlices, clampedPlane, 1u);
    return id;
}

void Resource::setDebugName(const gp::StringView debugName) noexcept
{
    m_debugName = gp::String(debugName);
}

void Resource::takeOwnership(Resource& other) noexcept
{
    m_debugName = other.m_debugName;
    m_wholeResourceState = other.m_wholeResourceState;
    m_subresourceStates = other.m_subresourceStates;
    m_transientState = other.m_transientState;
    m_numMips = other.m_numMips;
    m_numArraySlices = other.m_numArraySlices;
    m_numPlanes = other.m_numPlanes;
}

void Resource::releaseOwnership() noexcept
{
    m_wholeResourceState = SubresourceState(sync::ResourceAccess::Unknown);
    m_subresourceStates.clear();
    m_transientState = TransientState{};
    m_numMips = 0u;
    m_numArraySlices = 0u;
    m_numPlanes = 0u;
}

template <typename InFunc>
void Resource::enumerateSubresources(
    const SubresourceRange& range, InFunc&& callback, const bool isBeginTransition
) noexcept
{
    if (!isBarrierTrackingInitialized())
    {
        return;
    }

    const bool wholeRange = isWholeResourceRange(range);
    if (wholeRange && m_subresourceStates.isEmpty())
    {
        callback(m_wholeResourceState, SubresourceIndex{});
        return;
    }

    if (m_subresourceStates.isEmpty())
    {
        const gp::USize totalSubresources = static_cast<gp::USize>(m_numMips) *
                                            static_cast<gp::USize>(m_numArraySlices) *
                                            static_cast<gp::USize>(m_numPlanes);
        m_subresourceStates.reserve(totalSubresources);
        for (gp::USize i = 0u; i < totalSubresources; ++i)
        {
            m_subresourceStates.emplaceBack(m_wholeResourceState);
        }
    }

    const gp::UInt16 endMip =
        gp::math::min<gp::UInt16>(static_cast<gp::UInt16>(range.mipIndex + range.numMips), m_numMips);
    const gp::UInt16 endSlice =
        gp::math::min<gp::UInt16>(static_cast<gp::UInt16>(range.arraySlice + range.numArraySlices), m_numArraySlices);
    const gp::UInt8 endPlane =
        gp::math::min<gp::UInt8>(static_cast<gp::UInt8>(range.planeIndex + range.numPlanes), m_numPlanes);

    for (gp::UInt8 plane = range.planeIndex; plane < endPlane; ++plane)
    {
        for (gp::UInt16 slice = range.arraySlice; slice < endSlice; ++slice)
        {
            for (gp::UInt16 mip = range.mipIndex; mip < endMip; ++mip)
            {
                const gp::USize flatIdx = (static_cast<gp::USize>(plane) * static_cast<gp::USize>(m_numArraySlices) +
                                           static_cast<gp::USize>(slice)) *
                                              static_cast<gp::USize>(m_numMips) +
                                          static_cast<gp::USize>(mip);
                callback(
                    m_subresourceStates[flatIdx],
                    SubresourceIndex(
                        static_cast<gp::Int32>(mip), static_cast<gp::Int32>(slice), static_cast<gp::Int32>(plane)
                    )
                );
            }
        }
    }

    // Collapse back to inline whole-resource storage once a whole-resource transition unifies all subresources.
    if (wholeRange && !isBeginTransition && !m_subresourceStates.isEmpty())
    {
        const SubresourceState& firstState = m_subresourceStates[0u];
        bool allUniform = true;
        for (gp::USize i = 1u; i < m_subresourceStates.size(); ++i)
        {
            const SubresourceState& candidate = m_subresourceStates[i];
            for (gp::UInt32 p = 0u; p < sync::kPipelineStageCount; ++p)
            {
                const sync::PipelineStage stage =
                    (p == 0u) ? sync::PipelineStage::Graphics : sync::PipelineStage::AsyncCompute;
                if (!(candidate.pipelines[stage].current == firstState.pipelines[stage].current) ||
                    candidate.pipelines[stage].isTransitioning != firstState.pipelines[stage].isTransitioning)
                {
                    allUniform = false;
                    break;
                }
            }
            if (!allUniform)
            {
                break;
            }
        }

        if (allUniform)
        {
            m_wholeResourceState = firstState;
            m_subresourceStates.clear();
        }
    }
}

void UniformBufferResource::initLifetimeTracking(
    const gp::UInt64 frameID, const resource::UniformBufferUsage inUsage
) noexcept
{
    allocatedFrameID = frameID;
    usage = inUsage;
    initialized = true;
}

void UniformBufferResource::updateAllocation(const gp::UInt64 frameID) noexcept
{
    allocatedFrameID = frameID;
    initialized = true;
}

void UniformBufferResource::validateLifeTime(
    const gp::UInt64 currentFrameID, const gp::StringView layoutName
) const noexcept
{
    if (!initialized || usage == resource::UniformBufferUsage::MultiFrame)
    {
        return;
    }

    if (allocatedFrameID != currentFrameID)
    {
        char msg[384]{};
        const int nameLen = static_cast<int>(layoutName.size());
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "UniformBuffer '%.*s' created with %s usage on frame %llu was bound on later frame %llu!",
            nameLen > 0 ? nameLen : 9,
            nameLen > 0 ? layoutName.data() : "<unnamed>",
            usage == resource::UniformBufferUsage::SingleDraw ? "SingleDraw" : "SingleFrame",
            static_cast<unsigned long long>(allocatedFrameID),
            static_cast<unsigned long long>(currentFrameID)
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
}

void OperationsList::append(const OperationsList& other) noexcept
{
    if (other.m_ops.isEmpty())
    {
        return;
    }
    m_ops.reserve(m_ops.size() + other.m_ops.size());
    for (const Operation& op: other.m_ops)
    {
        m_ops.emplaceBack(op);
    }
}

void OperationsList::appendMove(OperationsList&& other) noexcept
{
    if (other.m_ops.isEmpty())
    {
        return;
    }
    if (m_ops.isEmpty())
    {
        m_ops = static_cast<gp::Vector<Operation>&&>(other.m_ops);
        return;
    }
    m_ops.reserve(m_ops.size() + other.m_ops.size());
    for (Operation& op: other.m_ops)
    {
        m_ops.emplaceBack(static_cast<Operation&&>(op));
    }
    other.m_ops.clear();
}

Tracker::Tracker(const sync::PipelineStage pipeline) noexcept
    : m_pipeline(pipeline)
{}

void Tracker::rename(Resource* const resource, const gp::StringView newName) noexcept
{
    if (resource == nullptr)
    {
        return;
    }
    Operation op{};
    op.type = OpType::Rename;
    op.identity.resource = resource;
    op.renameText = gp::String(newName);
    m_currentList.add(static_cast<Operation&&>(op));
}

void Tracker::addOps(const OperationsList& list) noexcept
{
    m_currentList.append(list);
}

void Tracker::assertAccess(const ResourceIdentity& identity, const sync::ResourceAccess requiredAccess) noexcept
{
    if (identity.resource == nullptr || !identity.resource->isBarrierTrackingInitialized())
    {
        return;
    }
    Operation op{};
    op.type = OpType::Assert;
    op.identity = identity;
    op.stateAfter = State(requiredAccess, m_pipeline);
    m_currentList.add(static_cast<Operation&&>(op));
}

void Tracker::assertUAV(view::UnorderedAccessView* const uav, const UAVMode mode, const gp::UInt32 slot) noexcept
{
    if (slot >= kMaxSimultaneousUAVs)
    {
        return;
    }

    BoundUAVSlot& entry = m_boundUAVs[static_cast<gp::UInt32>(mode)][slot];
    if (uav == nullptr)
    {
        entry.bound = false;
        entry.identity = ViewIdentity{};
        return;
    }

#if GP_ENABLE_RHI_VALIDATION
    entry.identity = uav->getViewIdentity();
    entry.bound = (entry.identity.resource != nullptr);
#endif
}

void Tracker::assertTracked(Resource* const resource, const sync::ResourceAccess expectedAccess) noexcept
{
    if (resource == nullptr || !resource->isBarrierTrackingInitialized())
    {
        return;
    }
    Operation op{};
    op.type = OpType::AssertTracked;
    op.identity = resource->getWholeResourceIdentity();
    op.stateAfter = State(expectedAccess, m_pipeline);
    m_currentList.add(static_cast<Operation&&>(op));
}

void Tracker::setTrackedAccess(
    const ResourceIdentity& identity, const sync::ResourceAccess access, const sync::PipelineStage pipelines
) noexcept
{
    if (identity.resource == nullptr || !identity.resource->isBarrierTrackingInitialized())
    {
        return;
    }
    Operation op{};
    op.type = OpType::SetTrackedAccess;
    op.identity = identity;
    op.stateAfter = State(access, pipelines);
    m_currentList.add(static_cast<Operation&&>(op));
}

void Tracker::clearUAVState(const UAVMode mode) noexcept
{
    for (gp::UInt32 slot = 0u; slot < kMaxSimultaneousUAVs; ++slot)
    {
        m_boundUAVs[static_cast<gp::UInt32>(mode)][slot] = BoundUAVSlot{};
    }
}

void Tracker::dispatchOrDrawBarrier(const UAVMode mode) noexcept
{
    const sync::ResourceAccess uavAccess =
        (mode == UAVMode::Graphics) ? sync::ResourceAccess::UAVGraphics : sync::ResourceAccess::UAVCompute;

    // Validate every bound UAV and detect aliasing duplicate UAV bindings in the same draw/dispatch!
    BoundUAVSlot* const table = m_boundUAVs[static_cast<gp::UInt32>(mode)];
    for (gp::UInt32 i = 0u; i < kMaxSimultaneousUAVs; ++i)
    {
        if (!table[i].bound || table[i].identity.resource == nullptr)
        {
            continue;
        }

        for (gp::UInt32 j = i + 1u; j < kMaxSimultaneousUAVs; ++j)
        {
            if (table[j].bound && table[j].identity.resource == table[i].identity.resource &&
                table[j].identity.subresourceRange == table[i].identity.subresourceRange)
            {
                char msg[256]{};
                const int len = std::snprintf(
                    msg,
                    sizeof(msg),
                    "Resource '%s' bound simultaneously to multiple UAV slots (%u and %u) in the same %s call.",
                    table[i].identity.resource->getDebugName(),
                    i,
                    j,
                    mode == UAVMode::Graphics ? "draw" : "dispatch"
                );
                if (len > 0)
                {
                    reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
                }
            }
        }

        assertAccess(table[i].identity, uavAccess);
    }
}

void Tracker::allUAVsOverlap(const bool allow) noexcept
{
    Operation op{};
    op.type = OpType::AllUAVsOverlap;
    op.enableFlag = allow;
    m_currentList.add(static_cast<Operation&&>(op));
}

void Tracker::specificUAVOverlap(const ResourceIdentity& identity, const bool allow) noexcept
{
    if (identity.resource == nullptr)
    {
        return;
    }
    Operation op{};
    op.type = OpType::SpecificUAVOverlap;
    op.identity = identity;
    op.enableFlag = allow;
    m_currentList.add(static_cast<Operation&&>(op));
}

OperationsList Tracker::finalizeOps() noexcept
{
    OperationsList result{};
    result.appendMove(static_cast<OperationsList&&>(m_currentList));
    return result;
}

bool Tracker::executeOp(
    const Operation& op, const sync::PipelineStage executePipeline, bool& inOutAllowAllUAVsOverlap
) noexcept
{
    switch (op.type)
    {
    case OpType::Rename:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->setDebugName(op.renameText.view());
        }
        return true;

    case OpType::BeginTransition:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->enumerateSubresources(
                op.identity.subresourceRange,
                [&op, executePipeline](SubresourceState& state, const SubresourceIndex idx) noexcept -> void
            {
                state.beginTransition(
                    *op.identity.resource, idx, op.stateBefore, op.stateAfter, op.transitionFlags, executePipeline
                );
            },
                true
            );
        }
        return true;

    case OpType::EndTransition:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->enumerateSubresources(
                op.identity.subresourceRange,
                [&op, executePipeline](SubresourceState& state, const SubresourceIndex idx) noexcept -> void
            {
                state.endTransition(*op.identity.resource, idx, op.stateBefore, op.stateAfter, executePipeline);
            },
                false
            );
        }
        return true;

    case OpType::AcquireTransient:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->m_transientState.acquire(*op.identity.resource, executePipeline);
        }
        return true;

    case OpType::DiscardTransientBegin:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->m_transientState.discardBegin(*op.identity.resource, executePipeline);
        }
        return true;

    case OpType::DiscardTransientEnd:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->m_transientState.discardEnd(*op.identity.resource, executePipeline);
        }
        return true;

    case OpType::AliasingOverlap:
        if (op.identity.resource != nullptr && op.secondResource != nullptr)
        {
            op.secondResource->m_transientState.aliasingOverlap(*op.secondResource, *op.identity.resource);
        }
        return true;

    case OpType::Assert:
        if (op.identity.resource != nullptr)
        {
            const bool allowOverlap = inOutAllowAllUAVsOverlap;
            op.identity.resource->enumerateSubresources(
                op.identity.subresourceRange,
                [&op, allowOverlap](SubresourceState& state, const SubresourceIndex idx) noexcept -> void
            {
                state.assertAccess(*op.identity.resource, idx, op.stateAfter, allowOverlap);
            },
                false
            );
        }
        return true;

    case OpType::AssertTracked:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->enumerateSubresources(
                op.identity.subresourceRange,
                [&op, executePipeline](const SubresourceState& state, const SubresourceIndex idx) noexcept -> void
            {
                state.assertTracked(*op.identity.resource, idx, op.stateAfter.access, executePipeline);
            },
                false
            );
        }
        return true;

    case OpType::SetTrackedAccess:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->enumerateSubresources(
                op.identity.subresourceRange,
                [&op](SubresourceState& state, const SubresourceIndex idx) noexcept -> void
            {
                state.setTracked(*op.identity.resource, idx, op.stateAfter.access, op.stateAfter.pipelines);
            },
                false
            );
        }
        return true;

    case OpType::Signal:
        if (op.fence != nullptr)
        {
            op.fence->signaledPipelines |= executePipeline;
        }
        return true;

    case OpType::Wait:
        if (op.fence != nullptr)
        {
            if ((op.fence->signaledPipelines & op.fence->signalingPipelines) != op.fence->signalingPipelines)
            {
                return false;
            }
            op.fence->completedPipelines |= executePipeline;
            if ((op.fence->completedPipelines & op.fence->waitingPipelines) == op.fence->waitingPipelines)
            {
                if (gp::memory::Malloc* const alloc = gp::memory::getGlobalMalloc(); alloc != nullptr)
                {
                    op.fence->~Fence();
                    alloc->deallocate(op.fence);
                }
            }
        }
        return true;

    case OpType::AllUAVsOverlap:
        if (op.enableFlag == inOutAllowAllUAVsOverlap)
        {
            reportValidationFailure(
                op.enableFlag ? "Duplicate beginUAVOverlap() call while global UAV overlap is already active."
                              : "Mismatched endUAVOverlap() call when global UAV overlap is not active."
            );
        }
        inOutAllowAllUAVsOverlap = op.enableFlag;
        return true;

    case OpType::SpecificUAVOverlap:
        if (op.identity.resource != nullptr)
        {
            op.identity.resource->enumerateSubresources(
                op.identity.subresourceRange,
                [&op, executePipeline](SubresourceState& state, const SubresourceIndex idx) noexcept -> void
            {
                if (op.enableFlag)
                {
                    state.beginUAVOverlap(*op.identity.resource, idx, executePipeline);
                }
                else
                {
                    state.endUAVOverlap(*op.identity.resource, idx, executePipeline);
                }
            },
                false
            );
        }
        return true;

    default:
        return true;
    }
}

void Tracker::replayOpQueue(const sync::PipelineStage pipeline, OperationsList&& ops) noexcept
{
    if (!sync::isSinglePipelineStage(pipeline))
    {
        return;
    }

    GlobalTrackerQueues& globalQueues = getGlobalTrackerQueues();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(globalQueues.mutex);

    PipelineReplayQueue& targetQueue = globalQueues.queues[pipeline];
    for (const Operation& op: ops.getOperations())
    {
        targetQueue.pending.emplaceBack(op);
    }
    ops.reset();

    // Drain all runnable operations across Graphics and AsyncCompute until quiescent or blocked on an unsignaled Fence.
    const sync::PipelineStage kStages[sync::kPipelineStageCount] = { sync::PipelineStage::Graphics,
                                                                     sync::PipelineStage::AsyncCompute };
    bool madeProgress = true;
    while (madeProgress)
    {
        madeProgress = false;
        for (const sync::PipelineStage stage: kStages)
        {
            PipelineReplayQueue& q = globalQueues.queues[stage];
            while (q.readIndex < q.pending.size())
            {
                if (!executeOp(q.pending[q.readIndex], stage, q.allowAllUAVsOverlap))
                {
                    break;
                }
                ++q.readIndex;
                madeProgress = true;
            }

            if (q.readIndex > 0u && q.readIndex == q.pending.size())
            {
                q.pending.clear();
                q.readIndex = 0u;
            }
        }
    }
}

void Tracker::resetAllPipelines() noexcept
{
    GlobalTrackerQueues& globalQueues = getGlobalTrackerQueues();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(globalQueues.mutex);
    for (PipelineReplayQueue& q: globalQueues.queues)
    {
        // Free any uncompleted Fence objects to avoid leaks when tests intentionally leave cross-queue waits
        // unsignaled.
        gp::Vector<Fence*> freedFences{};
        for (gp::USize idx = q.readIndex; idx < q.pending.size(); ++idx)
        {
            Fence* const fence = q.pending[idx].fence;
            if (fence == nullptr)
            {
                continue;
            }
            bool alreadyFreed = false;
            for (const Fence* const seen: freedFences)
            {
                if (seen == fence)
                {
                    alreadyFreed = true;
                    break;
                }
            }
            if (!alreadyFreed)
            {
                freedFences.emplaceBack(fence);
                if (gp::memory::Malloc* const alloc = gp::memory::getGlobalMalloc(); alloc != nullptr)
                {
                    fence->~Fence();
                    alloc->deallocate(fence);
                }
            }
        }
        q.pending.clear();
        q.readIndex = 0u;
        q.allowAllUAVsOverlap = false;
    }
}

}   // namespace gp::rhi::validation
