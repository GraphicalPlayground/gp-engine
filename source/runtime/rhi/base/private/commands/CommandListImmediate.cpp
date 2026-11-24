// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandListImmediate.hpp"
#include "commands/CommandListExecutor.hpp"
#include "device/DynamicRHI.hpp"
#include "sync/GPUFence.hpp"

namespace gp::rhi::command
{

void CommandListImmediate::immediateFlush(const ImmediateFlushType type, const SubmitFlags submitFlags) noexcept
{
    getCommandListExecutor().immediateFlush(type, submitFlags);
}

void CommandListImmediate::queueAsyncCommandListSubmit(
    const gp::ConstVectorView<CommandListBase*> commandLists
) noexcept
{
    getCommandListExecutor().submit(commandLists, SubmitFlags::SubmitToGPU);
}

void CommandListImmediate::blockUntilGPUIdle(const bool flushResources) noexcept
{
    const ImmediateFlushType flushType =
        flushResources ? ImmediateFlushType::FlushRHIThreadFlushResources : ImmediateFlushType::FlushRHIThread;
    immediateFlush(flushType);
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->blockUntilGPUIdle();
    }
}

void CommandListImmediate::beginDrawingViewport(
    presentation::Viewport* const viewport, resource::Texture* const renderTarget
) noexcept
{
    m_persistentState.insideDrawingViewport = true;
    if (isBottomOfPipe())
    {
        if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
        {
            rhi->beginDrawingViewport(viewport, renderTarget);
        }
        return;
    }

    enqueueLambda(
        [viewport, renderTarget](CommandListImmediate&) noexcept -> void
    {
        if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
        {
            rhi->beginDrawingViewport(viewport, renderTarget);
        }
    }
    );
}

void CommandListImmediate::endDrawingViewport(
    presentation::Viewport* const viewport, const bool present, const bool lockToVsync
) noexcept
{
    m_persistentState.insideDrawingViewport = false;
    if (isBottomOfPipe())
    {
        if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
        {
            rhi->endDrawingViewport(viewport, present, lockToVsync);
        }
        immediateFlush(ImmediateFlushType::DispatchToRHIThread);
        return;
    }

    enqueueLambda(
        [viewport, present, lockToVsync](CommandListImmediate&) noexcept -> void
    {
        if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
        {
            rhi->endDrawingViewport(viewport, present, lockToVsync);
        }
    }
    );
    immediateFlush(ImmediateFlushType::DispatchToRHIThread);
}

void CommandListImmediate::beginFrame() noexcept
{
    if (isBottomOfPipe())
    {
        if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
        {
            rhi->beginFrame();
        }
        return;
    }

    enqueueLambda(
        [](CommandListImmediate&) noexcept -> void
    {
        if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
        {
            rhi->beginFrame();
        }
    }
    );
}

void CommandListImmediate::endFrame() noexcept
{
    immediateFlush(ImmediateFlushType::FlushRHIThreadFlushResources, SubmitFlags::EndFrame);
    getCommandListExecutor().latchBypass();
}

void CommandListImmediate::advanceFrameForGetViewportBackBuffer(presentation::Viewport* const viewport) noexcept
{
    if (isBottomOfPipe())
    {
        if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
        {
            rhi->advanceFrameForGetViewportBackBuffer(viewport);
        }
        return;
    }

    enqueueLambda(
        [viewport](CommandListImmediate&) noexcept -> void
    {
        if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
        {
            rhi->advanceFrameForGetViewportBackBuffer(viewport);
        }
    }
    );
}

void CommandListImmediate::readSurfaceData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::Color>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    immediateFlush(ImmediateFlushType::FlushRHIThread);
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->readSurfaceData(texture, rect, outData, flags);
    }
}

void CommandListImmediate::readSurfaceData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::LinearColor>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    immediateFlush(ImmediateFlushType::FlushRHIThread);
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->readSurfaceData(texture, rect, outData, flags);
    }
}

void CommandListImmediate::readSurfaceFloatData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<resource::Float16Color>& outData,
    const resource::CubeFace cubeFace,
    const gp::Int32 arrayIndex,
    const gp::Int32 mipIndex
) noexcept
{
    immediateFlush(ImmediateFlushType::FlushRHIThread);
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->readSurfaceFloatData(texture, rect, outData, cubeFace, arrayIndex, mipIndex);
    }
}

bool CommandListImmediate::getRenderQueryResult(
    sync::RenderQuery* const renderQuery, gp::UInt64& outResult, const bool wait, const gp::UInt32 gpuIndex
) noexcept
{
    if (wait)
    {
        immediateFlush(ImmediateFlushType::FlushRHIThread);
    }
    DynamicRHI* const rhi = getDynamicRHI();
    return (rhi != nullptr) ? rhi->getRenderQueryResult(renderQuery, outResult, wait, gpuIndex) : false;
}

resource::TextureLockResult CommandListImmediate::lockTexture(const resource::TextureLockArgs& args) noexcept
{
    if (args.needsRHIFlush)
    {
        immediateFlush(ImmediateFlushType::FlushRHIThread);
    }
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListImmediate::lockTexture requires a valid DynamicRHI.");
    return rhi->lockTexture(*this, args);
}

void CommandListImmediate::unlockTexture(const resource::TextureLockArgs& args) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListImmediate::unlockTexture requires a valid DynamicRHI.");
    rhi->unlockTexture(*this, args);
}

resource::UpdateTexture3DData CommandListImmediate::beginUpdateTexture3D(
    resource::Texture* const texture, const gp::UInt32 mipIndex, const resource::UpdateTextureRegion3D& updateRegion
) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListImmediate::beginUpdateTexture3D requires a valid DynamicRHI.");
    return rhi->beginUpdateTexture3D(*this, texture, mipIndex, updateRegion);
}

void CommandListImmediate::endUpdateTexture3D(resource::UpdateTexture3DData& updateData) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    GP_CHECKF(rhi != nullptr, "CommandListImmediate::endUpdateTexture3D requires a valid DynamicRHI.");
    rhi->endUpdateTexture3D(*this, updateData);
}

void* CommandListImmediate::lockStagingBuffer(
    resource::StagingBuffer* const stagingBuffer,
    sync::GPUFence* const fence,
    const gp::UInt32 offset,
    const gp::UInt32 sizeRHI
) noexcept
{
    if (fence == nullptr || !fence->poll() || fence->hasPendingWriteCommands())
    {
        immediateFlush(ImmediateFlushType::FlushRHIThread);
    }

    DynamicRHI* const rhi = getDynamicRHI();
    return (rhi != nullptr) ? rhi->lockStagingBuffer(stagingBuffer, fence, offset, sizeRHI) : nullptr;
}

void CommandListImmediate::unlockStagingBuffer(resource::StagingBuffer* const stagingBuffer) noexcept
{
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->unlockStagingBuffer(stagingBuffer);
    }
}

IUploadContext* CommandListImmediate::acquireUploadContext() noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    return (rhi != nullptr) ? rhi->acquireUploadContext() : nullptr;
}

}   // namespace gp::rhi::command
