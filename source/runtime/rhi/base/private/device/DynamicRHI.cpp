// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/DynamicRHI.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "commands/CommandPacket.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "device/DefaultRenderQueryPool.hpp"
#include "device/GenericStagingBuffer.hpp"
#include "device/RHIGlobals.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/GlobalMemory.hpp"
#include "memory/Memory.hpp"
#include "resources/TextureUtils.hpp"
#include "sync/Transition.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <utility>

namespace gp::rhi
{

namespace
{

DynamicRHI* g_dynamicRHI = nullptr;

void transitionCreateTrampoline(
    sync::Transition* const transition, const sync::TransitionCreateInfo& createInfo
) noexcept
{
    if (g_dynamicRHI != nullptr)
    {
        g_dynamicRHI->createTransition(transition, createInfo);
    }
}

void transitionReleaseTrampoline(sync::Transition* const transition) noexcept
{
    if (g_dynamicRHI != nullptr)
    {
        g_dynamicRHI->releaseTransition(transition);
    }
}

struct StagedBufferAllocation final
{
    void* data{ nullptr };

    explicit StagedBufferAllocation(void* const inData = nullptr) noexcept
        : data(inData)
    {}

    StagedBufferAllocation(const StagedBufferAllocation&) = delete;
    StagedBufferAllocation& operator=(const StagedBufferAllocation&) = delete;

    StagedBufferAllocation(StagedBufferAllocation&& other) noexcept
        : data(other.data)
    {
        other.data = nullptr;
    }

    StagedBufferAllocation& operator=(StagedBufferAllocation&& other) noexcept
    {
        if (this != &other)
        {
            release();
            data = other.data;
            other.data = nullptr;
        }
        return *this;
    }

    ~StagedBufferAllocation() noexcept
    {
        release();
    }

    void release() noexcept
    {
        if (data != nullptr)
        {
            if (gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc(); allocator != nullptr)
            {
                allocator->deallocate(data);
            }
            data = nullptr;
        }
    }
};

}   // namespace

gp::UInt64 DynamicRHI::getMinimumAlignmentForBufferBackedSRV(const PixelFormat format) noexcept
{
    if (!isValidPixelFormat(format))
    {
        return 1u;
    }
    const PixelFormatInfo& info = getPixelFormatInfo(format);
    return (info.blockBytes > 0u) ? static_cast<gp::UInt64>(info.blockBytes) : 1u;
}

shader::MeshShaderRef
    DynamicRHI::createMeshShader(const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    static_cast<void>(code);
    static_cast<void>(hash);
    return shader::MeshShaderRef{};
}

shader::AmplificationShaderRef DynamicRHI::createAmplificationShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    static_cast<void>(code);
    static_cast<void>(hash);
    return shader::AmplificationShaderRef{};
}

pipeline::BoundShaderStateRef DynamicRHI::createBoundShaderState(
    shader::AmplificationShader* const amplificationShader,
    shader::MeshShader* const meshShader,
    shader::PixelShader* const pixelShader
) noexcept
{
    static_cast<void>(amplificationShader);
    static_cast<void>(meshShader);
    static_cast<void>(pixelShader);
    return pipeline::BoundShaderStateRef{};
}

pipeline::ComputePipelineStateRef
    DynamicRHI::createComputePipelineState(const pipeline::ComputePipelineStateInitializer& initializer) noexcept
{
    return createComputePipelineState(initializer.computeShader);
}

void* DynamicRHI::lockBuffer(
    command::CommandListBase& cmdList,
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 size,
    const resource::ResourceLockMode lockMode
) noexcept
{
    if (buffer == nullptr || size == 0u)
    {
        return nullptr;
    }

    if (cmdList.isBottomOfPipe())
    {
        return lockBufferBottomOfPipe(cmdList, buffer, offset, size, lockMode);
    }

    if (lockMode != resource::ResourceLockMode::ReadOnly)
    {
        gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
        GP_CHECKF(allocator != nullptr, "Global engine allocator must be valid for staged buffer lock.");
        void* const stagingData = allocator->allocate(static_cast<gp::USize>(size), 16u);
        GP_CHECKF(stagingData != nullptr, "Failed to allocate temporary CPU staging memory for lockBuffer().");

        {
            gp::thread::UniqueLock<gp::thread::Mutex> lock(m_lockTrackerMutex);
            m_lockedBuffers.emplaceBack(LockedBufferRecord{ buffer, offset, size, lockMode, stagingData, true });
        }
        return stagingData;
    }

    GP_CHECKF(cmdList.isImmediate(), "ReadOnly lockBuffer() at top-of-pipe requires an immediate command list.");
    cmdList.getAsImmediate().immediateFlush(command::ImmediateFlushType::FlushRHIThread);

    void* const mapped = lockBufferBottomOfPipe(cmdList, buffer, offset, size, lockMode);
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_lockTrackerMutex);
        m_lockedBuffers.emplaceBack(LockedBufferRecord{ buffer, offset, size, lockMode, mapped, false });
    }
    return mapped;
}

void DynamicRHI::unlockBuffer(command::CommandListBase& cmdList, resource::Buffer* const buffer) noexcept
{
    if (buffer == nullptr)
    {
        return;
    }

    if (cmdList.isBottomOfPipe())
    {
        unlockBufferBottomOfPipe(cmdList, buffer);
        return;
    }

    LockedBufferRecord record{};
    bool found = false;
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_lockTrackerMutex);
        for (gp::USize i = m_lockedBuffers.size(); i > 0u; --i)
        {
            if (m_lockedBuffers[i - 1u].buffer == buffer)
            {
                record = m_lockedBuffers[i - 1u];
                m_lockedBuffers.erase(m_lockedBuffers.begin() + static_cast<gp::ISize>(i - 1u));
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        unlockBufferBottomOfPipe(cmdList, buffer);
        return;
    }

    if (record.isStagedWrite)
    {
        resource::BufferRef bufferRef(buffer);
        StagedBufferAllocation stagingOwner(record.stagingData);
        const gp::UInt32 offset = record.offset;
        const gp::UInt32 size = record.size;
        const resource::ResourceLockMode lockMode = record.lockMode;

        cmdList.enqueueLambda(
            [this, bufferRef = std::move(bufferRef), stagingOwner = std::move(stagingOwner), offset, size, lockMode](
                command::CommandListBase& execCmdList
            ) mutable noexcept -> void
        {
            void* const dst = lockBufferBottomOfPipe(execCmdList, bufferRef.get(), offset, size, lockMode);
            if (dst != nullptr && stagingOwner.data != nullptr)
            {
                gp::memory::copyMemory(dst, stagingOwner.data, static_cast<gp::USize>(size));
            }
            unlockBufferBottomOfPipe(execCmdList, bufferRef.get());
            stagingOwner.release();
        }
        );
        return;
    }

    if (cmdList.isImmediate())
    {
        cmdList.getAsImmediate().immediateFlush(command::ImmediateFlushType::FlushRHIThread);
    }
    unlockBufferBottomOfPipe(cmdList, buffer);
}

void DynamicRHI::replaceResources(
    command::CommandListBase& cmdList, const gp::ConstVectorView<resource::ResourceReplaceInfo> infos
) noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(infos);
}

resource::TextureReferenceRef DynamicRHI::createTextureReference(
    command::CommandListBase& cmdList, resource::Texture* const referencedTexture
) noexcept
{
    static_cast<void>(cmdList);
    return resource::TextureReferenceRef(new resource::TextureReference(referencedTexture));
}

void DynamicRHI::updateTextureReference(
    command::CommandListBase& cmdList,
    resource::TextureReference* const textureReference,
    resource::Texture* const newTexture
) noexcept
{
    if (textureReference == nullptr)
    {
        return;
    }

    if (cmdList.isBottomOfPipe())
    {
        textureReference->setReferencedTexture(newTexture);
        return;
    }

    resource::TextureReferenceRef refPtr(textureReference);
    resource::TextureRef newTexPtr(newTexture);
    cmdList.enqueueLambda(
        [refPtr = std::move(refPtr), newTexPtr = std::move(newTexPtr)](command::CommandListBase&) noexcept -> void
    {
        refPtr->setReferencedTexture(newTexPtr.get());
    }
    );
}

resource::CalcTextureSizeResult
    DynamicRHI::calcTexturePlatformSize(const resource::TextureDesc& desc, const gp::UInt32 firstMipIndex) noexcept
{
    if (!isValidPixelFormat(desc.format) || desc.extent.x <= 0 || desc.extent.y <= 0 || desc.numMips == 0u ||
        firstMipIndex >= static_cast<gp::UInt32>(desc.numMips) || firstMipIndex >= 32u)
    {
        return resource::CalcTextureSizeResult{};
    }

    resource::TextureDesc subDesc = desc;
    if (firstMipIndex > 0u)
    {
        subDesc.extent.x = gp::math::max<gp::Int32>(1, desc.extent.x >> firstMipIndex);
        subDesc.extent.y = gp::math::max<gp::Int32>(1, desc.extent.y >> firstMipIndex);
        if (desc.isTexture3D())
        {
            subDesc.depth = static_cast<gp::UInt16>(gp::math::max<gp::UInt32>(1u, desc.depth >> firstMipIndex));
        }
        subDesc.numMips = static_cast<gp::UInt8>(desc.numMips - firstMipIndex);
    }

    const gp::UInt64 totalSize = resource::calculateTextureSize(subDesc);
    const gp::UInt32 alignment = (totalSize > 0u) ? 65536u : 0u;
    return resource::CalcTextureSizeResult{ totalSize, alignment };
}

gp::UInt64 DynamicRHI::computeMemorySize(resource::Texture* const texture) noexcept
{
    if (texture == nullptr)
    {
        return 0u;
    }
    return resource::calculateTextureSize(texture->getDesc());
}

void DynamicRHI::getTextureMemoryStats(resource::TextureMemoryStats& outStats) noexcept
{
    outStats = resource::TextureMemoryStats{};
}

resource::UpdateTexture3DData DynamicRHI::beginUpdateTexture3D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion3D& updateRegion
) noexcept
{
    static_cast<void>(cmdList);
    resource::UpdateTexture3DData result{};
    result.texture = texture;
    result.mipIndex = mipIndex;
    result.updateRegion = updateRegion;

    if (texture == nullptr || updateRegion.width == 0u || updateRegion.height == 0u || updateRegion.depth == 0u)
    {
        return result;
    }

    const PixelFormat format = texture->getFormat();
    const gp::UInt64 rowBytes = resource::calculateImageBytes(updateRegion.width, 1u, 1u, format);
    const gp::UInt64 sliceBytes = resource::calculateImageBytes(updateRegion.width, updateRegion.height, 1u, format);
    const gp::UInt64 totalBytes =
        resource::calculateImageBytes(updateRegion.width, updateRegion.height, updateRegion.depth, format);

    result.rowPitch = static_cast<gp::UInt32>(rowBytes);
    result.depthPitch = static_cast<gp::UInt32>(sliceBytes);
    result.dataSizeBytes = static_cast<gp::UInt32>(totalBytes);

    if (totalBytes > 0u)
    {
        gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
        GP_CHECKF(allocator != nullptr, "Global engine allocator must be valid for beginUpdateTexture3D().");
        result.data = static_cast<gp::UInt8*>(allocator->allocate(static_cast<gp::USize>(totalBytes), 16u));
        result.isManualFree = (result.data != nullptr);
    }

    return result;
}

void DynamicRHI::endUpdateTexture3D(
    command::CommandListBase& cmdList, resource::UpdateTexture3DData& updateData
) noexcept
{
    if (updateData.texture != nullptr && updateData.data != nullptr && updateData.dataSizeBytes > 0u)
    {
        updateTexture3D(
            cmdList,
            updateData.texture,
            updateData.mipIndex,
            updateData.updateRegion,
            updateData.rowPitch,
            updateData.depthPitch,
            updateData.data
        );
    }

    if (updateData.isManualFree && updateData.data != nullptr)
    {
        if (gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc(); allocator != nullptr)
        {
            allocator->deallocate(updateData.data);
        }
    }

    updateData.data = nullptr;
    updateData.dataSizeBytes = 0u;
    updateData.isManualFree = false;
}

void DynamicRHI::readSurfaceData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::LinearColor>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    gp::Vector<gp::math::Color> ldrData{};
    readSurfaceData(texture, rect, ldrData, flags);
    outData.clear();
    outData.reserve(ldrData.size());
    for (const gp::math::Color& pixel: ldrData)
    {
        outData.emplaceBack(pixel.reinterpretAsLinear());
    }
}

void DynamicRHI::readSurfaceFloatData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<resource::Float16Color>& outData,
    const resource::CubeFace cubeFace,
    const gp::Int32 arrayIndex,
    const gp::Int32 mipIndex
) noexcept
{
    static_cast<void>(texture);
    static_cast<void>(rect);
    static_cast<void>(outData);
    static_cast<void>(cubeFace);
    static_cast<void>(arrayIndex);
    static_cast<void>(mipIndex);
}

void DynamicRHI::read3DSurfaceFloatData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    const resource::IntPoint zMinMax,
    gp::Vector<resource::Float16Color>& outData
) noexcept
{
    static_cast<void>(texture);
    static_cast<void>(rect);
    static_cast<void>(zMinMax);
    static_cast<void>(outData);
}

void DynamicRHI::mapStagingSurface(
    resource::Texture* const texture,
    sync::GPUFence* const fence,
    void*& outData,
    gp::Int32& outWidth,
    gp::Int32& outHeight,
    const gp::UInt32 gpuIndex
) noexcept
{
    static_cast<void>(texture);
    static_cast<void>(fence);
    static_cast<void>(gpuIndex);
    outData = nullptr;
    outWidth = 0;
    outHeight = 0;
}

void DynamicRHI::unmapStagingSurface(resource::Texture* const texture, const gp::UInt32 gpuIndex) noexcept
{
    static_cast<void>(texture);
    static_cast<void>(gpuIndex);
}

void DynamicRHI::aliasTextureResources(resource::TextureRef& destTexture, resource::TextureRef& srcTexture) noexcept
{
    static_cast<void>(destTexture);
    static_cast<void>(srcTexture);
}

void DynamicRHI::getSupportedResolution(gp::UInt32& inOutWidth, gp::UInt32& inOutHeight) noexcept
{
    static_cast<void>(inOutWidth);
    static_cast<void>(inOutHeight);
}

resource::StagingBufferRef DynamicRHI::createStagingBuffer() noexcept
{
    return resource::StagingBufferRef(new GenericStagingBuffer());
}

void* DynamicRHI::lockStagingBuffer(
    resource::StagingBuffer* const stagingBuffer,
    sync::GPUFence* const fence,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    static_cast<void>(fence);
    return (stagingBuffer != nullptr) ? stagingBuffer->lock(offset, numBytes) : nullptr;
}

void DynamicRHI::unlockStagingBuffer(resource::StagingBuffer* const stagingBuffer) noexcept
{
    if (stagingBuffer != nullptr)
    {
        stagingBuffer->unlock();
    }
}

sync::RenderQueryPoolRef
    DynamicRHI::createRenderQueryPool(const sync::RenderQueryType queryType, const gp::UInt32 numQueries) noexcept
{
    return sync::RenderQueryPoolRef(new DefaultRenderQueryPool(queryType, this, numQueries));
}

void DynamicRHI::writeGPUFenceTopOfPipe(command::CommandListBase& cmdList, sync::GPUFence* const fence) noexcept
{
    if (cmdList.isBottomOfPipe())
    {
        cmdList.getComputeContext().writeGPUFence(fence);
        return;
    }
    cmdList.emplaceCommand<command::CommandWriteGPUFence>(fence);
}

void DynamicRHI::beginRenderQueryTopOfPipe(
    command::CommandListBase& cmdList, sync::RenderQuery* const renderQuery
) noexcept
{
    if (cmdList.isBottomOfPipe())
    {
        cmdList.getContext().beginRenderQuery(renderQuery);
        return;
    }
    cmdList.emplaceCommand<command::CommandBeginRenderQuery>(renderQuery);
}

void DynamicRHI::endRenderQueryTopOfPipe(
    command::CommandListBase& cmdList, sync::RenderQuery* const renderQuery
) noexcept
{
    if (cmdList.isBottomOfPipe())
    {
        cmdList.getContext().endRenderQuery(renderQuery);
        return;
    }
    cmdList.emplaceCommand<command::CommandEndRenderQuery>(renderQuery);
}

void DynamicRHI::endFrameRenderThread() noexcept
{
    ++m_frameCounter;
    EndFrameArgs args{};
    args.frameNumber = m_frameCounter;
    endFrame(args);
}

bool DynamicRHI::getSupportedResolutions(
    gp::Vector<ScreenResolution>& outResolutions, const bool ignoreRefreshRate
) noexcept
{
    static_cast<void>(outResolutions);
    static_cast<void>(ignoreRefreshRate);
    return false;
}

void DynamicRHI::bindDebugLabelName(
    command::CommandListBase& cmdList, resource::Texture* const texture, const char* const name
) noexcept
{
    static_cast<void>(cmdList);
    if (texture != nullptr && name != nullptr)
    {
        texture->setName(gp::Name(name));
    }
}

void DynamicRHI::bindDebugLabelName(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const char* const name
) noexcept
{
    static_cast<void>(cmdList);
    if (buffer != nullptr && name != nullptr)
    {
        buffer->setName(gp::Name(name));
    }
}

void DynamicRHI::bindDebugLabelName(
    command::CommandListBase& cmdList, view::UnorderedAccessView* const uav, const char* const name
) noexcept
{
    if (uav == nullptr)
    {
        return;
    }
    if (resource::Texture* const texture = uav->getTexture(); texture != nullptr)
    {
        cmdList.bindDebugLabelName(texture, name);
    }
    else if (resource::Buffer* const buffer = uav->getBuffer(); buffer != nullptr)
    {
        cmdList.bindDebugLabelName(buffer, name);
    }
}

DynamicRHI* getDynamicRHI() noexcept
{
    return g_dynamicRHI;
}

void setDynamicRHI(DynamicRHI* const rhi) noexcept
{
    g_dynamicRHI = rhi;
}

bool hasValidDynamicRHI() noexcept
{
    return g_dynamicRHI != nullptr;
}

InterfaceType getInterfaceType() noexcept
{
    return (g_dynamicRHI != nullptr) ? g_dynamicRHI->getInterfaceType() : InterfaceType::NotSet;
}

resource::UniformBufferLayoutRef createUniformBufferLayout(const resource::UniformBufferLayoutDesc& desc) noexcept
{
    return resource::UniformBufferLayoutRef(new resource::UniformBufferLayout(desc));
}

sync::RenderQueryPoolRef
    createRenderQueryPool(const sync::RenderQueryType queryType, const gp::UInt32 numQueries) noexcept
{
    DynamicRHI* const rhi = getDynamicRHI();
    return (rhi != nullptr) ? rhi->createRenderQueryPool(queryType, numQueries) : sync::RenderQueryPoolRef{};
}

sync::Transition* createTransition(const sync::TransitionCreateInfo& createInfo) noexcept
{
    return sync::TransitionController::create(createInfo);
}

void rhiInit(DynamicRHI* const rhi) noexcept
{
    GP_CHECKF(rhi != nullptr, "rhiInit() requires a non-null DynamicRHI instance.");
    GP_CHECKF(g_dynamicRHI == nullptr, "rhiInit() called while another DynamicRHI is still active.");

    g_dynamicRHI = rhi;

    sync::TransitionBackendHooks hooks{};
    hooks.privateDataSizeInBytes = g_dynamicRHI->getTransitionPrivateDataSizeInBytes();
    hooks.privateDataAlignInBytes = g_dynamicRHI->getTransitionPrivateDataAlignInBytes();
    hooks.onCreate = &transitionCreateTrampoline;
    hooks.onRelease = &transitionReleaseTrampoline;
    sync::TransitionController::configureBackend(hooks);

    g_dynamicRHI->init();

    // Refresh transition hooks in case init() determined backend private-data layout dynamically.
    hooks.privateDataSizeInBytes = g_dynamicRHI->getTransitionPrivateDataSizeInBytes();
    hooks.privateDataAlignInBytes = g_dynamicRHI->getTransitionPrivateDataAlignInBytes();
    sync::TransitionController::configureBackend(hooks);

    getRHIGlobals().isRHIInitialized = true;
}

void rhiPostInit() noexcept
{
    if (g_dynamicRHI != nullptr)
    {
        g_dynamicRHI->postInit();
    }
}

void rhiExit() noexcept
{
    if (g_dynamicRHI == nullptr)
    {
        return;
    }

    command::getCommandListExecutor().immediateFlush(command::ImmediateFlushType::FlushRHIThreadFlushResources);
    resource::TextureReference::setDefaultTexture(nullptr);
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    g_dynamicRHI->shutdown();

    // Drain any resources released inside shutdown() before destroying the backend device.
    command::getCommandListExecutor().immediateFlush(command::ImmediateFlushType::FlushRHIThreadFlushResources);
    resource::ResourceDeletionQueue::flushPendingDeletes(true);

    delete g_dynamicRHI;
    g_dynamicRHI = nullptr;

    sync::TransitionController::resetBackend();
    command::getCommandListExecutor().reset();
    getRHIGlobals().reset();
    g_minClipZ = 0.0f;
    g_projectionSignY = 1.0f;
}

}   // namespace gp::rhi
