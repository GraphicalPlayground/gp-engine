// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierSettings.hpp"
#include "barriers/VulkanTransitionData.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/VulkanQueue.hpp"
#include "contexts/VulkanCommandContextProvider.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanDynamicRHI.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/VulkanBoundShaderState.hpp"
#include "pipeline/VulkanPipelinePrecache.hpp"
#include "pipeline/VulkanPipelineStateCacheManager.hpp"
#include "queries/VulkanRenderQueryRHI.hpp"
#include "resources/VulkanBufferInitializer.hpp"
#include "resources/VulkanBufferLocking.hpp"
#include "resources/VulkanReadbackStagingBuffer.hpp"
#include "resources/VulkanResourceReplace.hpp"
#include "resources/VulkanTextureInitializer.hpp"
#include "resources/VulkanTextureLocking.hpp"
#include "resources/VulkanTextureReadback.hpp"
#include "resources/VulkanTextureReference.hpp"
#include "resources/VulkanTextureSize.hpp"
#include "resources/VulkanUniformBuffer.hpp"
#include "shaders/VulkanShaderRHI.hpp"
#include "states/VulkanStateRHI.hpp"
#include "sync/GPUFence.hpp"
#include "sync/VulkanGPUFence.hpp"
#include "views/VulkanShaderResourceView.hpp"
#include "views/VulkanUnorderedAccessView.hpp"

namespace gp::rhi::vulkan
{

pipeline::SamplerStateRef VulkanDynamicRHI::createSamplerState(const pipeline::SamplerStateDesc& initializer) noexcept
{
    if (m_samplerCache == nullptr)
    {
        return pipeline::SamplerStateRef{};
    }
    return createVulkanSamplerState(*m_samplerCache, initializer);
}

pipeline::RasterizerStateRef
    VulkanDynamicRHI::createRasterizerState(const pipeline::RasterizerStateDesc& initializer) noexcept
{
    return createVulkanRasterizerState(initializer);
}

pipeline::DepthStencilStateRef
    VulkanDynamicRHI::createDepthStencilState(const pipeline::DepthStencilStateDesc& initializer) noexcept
{
    return createVulkanDepthStencilState(initializer);
}

pipeline::BlendStateRef VulkanDynamicRHI::createBlendState(const pipeline::BlendStateDesc& initializer) noexcept
{
    return createVulkanBlendState(initializer);
}

pipeline::VertexDeclarationRef
    VulkanDynamicRHI::createVertexDeclaration(const pipeline::VertexDeclarationElementList& elements) noexcept
{
    return createVulkanVertexDeclaration(elements);
}

shader::VertexShaderRef
    VulkanDynamicRHI::createVertexShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    if (!m_isInitialized)
    {
        return shader::VertexShaderRef{};
    }
    return createVulkanVertexShader(*m_shaderFactory, *m_device, m_deferredDeletionQueue.get(), code, hash);
}

shader::PixelShaderRef
    VulkanDynamicRHI::createPixelShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    if (!m_isInitialized)
    {
        return shader::PixelShaderRef{};
    }
    return createVulkanPixelShader(*m_shaderFactory, *m_device, m_deferredDeletionQueue.get(), code, hash);
}

shader::GeometryShaderRef
    VulkanDynamicRHI::createGeometryShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    if (!m_isInitialized)
    {
        return shader::GeometryShaderRef{};
    }
    return createVulkanGeometryShader(*m_shaderFactory, *m_device, m_deferredDeletionQueue.get(), code, hash);
}

shader::ComputeShaderRef
    VulkanDynamicRHI::createComputeShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    if (!m_isInitialized)
    {
        return shader::ComputeShaderRef{};
    }
    return createVulkanComputeShader(*m_shaderFactory, *m_device, m_deferredDeletionQueue.get(), code, hash);
}

shader::MeshShaderRef
    VulkanDynamicRHI::createMeshShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    if (!m_isInitialized)
    {
        return shader::MeshShaderRef{};
    }
    return createVulkanMeshShader(*m_shaderFactory, *m_device, m_deferredDeletionQueue.get(), code, hash);
}

shader::AmplificationShaderRef VulkanDynamicRHI::createAmplificationShader(
    gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    if (!m_isInitialized)
    {
        return shader::AmplificationShaderRef{};
    }
    return createVulkanAmplificationShader(*m_shaderFactory, *m_device, m_deferredDeletionQueue.get(), code, hash);
}

pipeline::BoundShaderStateRef VulkanDynamicRHI::createBoundShaderState(
    pipeline::VertexDeclaration* const vertexDeclaration,
    shader::VertexShader* const vertexShader,
    shader::PixelShader* const pixelShader,
    shader::GeometryShader* const geometryShader
) noexcept
{
    return createVulkanBoundShaderState(vertexDeclaration, vertexShader, pixelShader, geometryShader);
}

pipeline::BoundShaderStateRef VulkanDynamicRHI::createBoundShaderState(
    shader::AmplificationShader* const amplificationShader,
    shader::MeshShader* const meshShader,
    shader::PixelShader* const pixelShader
) noexcept
{
    return createVulkanBoundShaderState(amplificationShader, meshShader, pixelShader);
}

pipeline::GraphicsPipelineStateRef VulkanDynamicRHI::createGraphicsPipelineState(
    const pipeline::GraphicsPipelineStateInitializer& initializer
) noexcept
{
    if (m_pipelineCache == nullptr)
    {
        return pipeline::GraphicsPipelineStateRef{};
    }
    return m_pipelineCache->createGraphicsPipelineState(initializer);
}

pipeline::ComputePipelineStateRef
    VulkanDynamicRHI::createComputePipelineState(shader::ComputeShader* const computeShader) noexcept
{
    return createComputePipelineState(pipeline::ComputePipelineStateInitializer(computeShader));
}

pipeline::ComputePipelineStateRef
    VulkanDynamicRHI::createComputePipelineState(const pipeline::ComputePipelineStateInitializer& initializer) noexcept
{
    if (m_pipelineCache == nullptr)
    {
        return pipeline::ComputePipelineStateRef{};
    }
    return m_pipelineCache->createComputePipelineState(initializer);
}

gp::UInt64 VulkanDynamicRHI::computeStatePrecachePSOHash(
    const pipeline::GraphicsPipelineStateInitializer& initializer
) noexcept
{
    const gp::rhi::GpuVendorId vendorId = (m_device != nullptr) ? m_device->getVendorId() : gp::rhi::GpuVendorId{};
    return computeVulkanStatePrecachePSOHash(initializer, vendorId);
}

gp::UInt64
    VulkanDynamicRHI::computePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept
{
    const gp::rhi::GpuVendorId vendorId = (m_device != nullptr) ? m_device->getVendorId() : gp::rhi::GpuVendorId{};
    return computeVulkanPrecachePSOHash(initializer, vendorId);
}

bool VulkanDynamicRHI::matchPrecachePSOInitializers(
    const pipeline::GraphicsPipelineStateInitializer& lhs, const pipeline::GraphicsPipelineStateInitializer& rhs
) noexcept
{
    return matchVulkanPrecachePSOInitializers(lhs, rhs);
}

resource::UniformBufferRef VulkanDynamicRHI::createUniformBuffer(
    const void* const contents,
    const resource::UniformBufferLayout* const layout,
    const resource::UniformBufferUsage usage,
    const resource::UniformBufferValidation validation
) noexcept
{
    if (!m_isInitialized)
    {
        return resource::UniformBufferRef{};
    }
    return createVulkanUniformBuffer(m_resourceServices, contents, layout, usage, validation);
}

void VulkanDynamicRHI::updateUniformBuffer(
    command::CommandListBase& cmdList, resource::UniformBuffer* const uniformBuffer, const void* const contents
) noexcept
{
    updateVulkanUniformBuffer(cmdList, uniformBuffer, contents);
}

resource::BufferInitializer VulkanDynamicRHI::createBufferInitializer(
    command::CommandListBase& cmdList, const resource::BufferCreateDesc& createDesc
) noexcept
{
    return createVulkanBufferInitializer(m_resourceServices, cmdList, createDesc);
}

void* VulkanDynamicRHI::lockBuffer(
    command::CommandListBase& cmdList,
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 size,
    const resource::ResourceLockMode lockMode
) noexcept
{
    return lockVulkanBuffer(cmdList, buffer, offset, size, lockMode);
}

void VulkanDynamicRHI::unlockBuffer(command::CommandListBase& cmdList, resource::Buffer* const buffer) noexcept
{
    unlockVulkanBuffer(cmdList, buffer);
}

void* VulkanDynamicRHI::lockBufferBottomOfPipe(
    command::CommandListBase& cmdList,
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 size,
    const resource::ResourceLockMode lockMode
) noexcept
{
    return lockVulkanBufferBottomOfPipe(cmdList, buffer, offset, size, lockMode);
}

void VulkanDynamicRHI::unlockBufferBottomOfPipe(
    command::CommandListBase& cmdList, resource::Buffer* const buffer
) noexcept
{
    unlockVulkanBufferBottomOfPipe(cmdList, buffer);
}

void VulkanDynamicRHI::replaceResources(
    command::CommandListBase& cmdList, gp::ConstVectorView<resource::ResourceReplaceInfo> infos
) noexcept
{
    replaceVulkanResources(cmdList, infos);
}

resource::StagingBufferRef VulkanDynamicRHI::createStagingBuffer() noexcept
{
    return createVulkanStagingBuffer();
}

void* VulkanDynamicRHI::lockStagingBuffer(
    resource::StagingBuffer* const stagingBuffer,
    sync::GPUFence* const fence,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    // The backend lock requires a signaled fence; an unsignaled one means the copy is still in flight.
    if ((fence != nullptr) && !fence->poll())
    {
        blockUntilGPUIdle();
    }
    return lockVulkanStagingBuffer(stagingBuffer, fence, offset, numBytes);
}

void VulkanDynamicRHI::unlockStagingBuffer(resource::StagingBuffer* const stagingBuffer) noexcept
{
    unlockVulkanStagingBuffer(stagingBuffer);
}

resource::TextureInitializer VulkanDynamicRHI::createTextureInitializer(
    command::CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
) noexcept
{
    return createVulkanTextureInitializer(m_resourceServices, cmdList, createDesc);
}

resource::TextureReferenceRef VulkanDynamicRHI::createTextureReference(
    command::CommandListBase& cmdList, resource::Texture* const referencedTexture
) noexcept
{
    return createVulkanTextureReference(cmdList, referencedTexture);
}

void VulkanDynamicRHI::updateTextureReference(
    command::CommandListBase& cmdList,
    resource::TextureReference* const textureReference,
    resource::Texture* const newTexture
) noexcept
{
    updateVulkanTextureReference(cmdList, textureReference, newTexture);
}

resource::CalcTextureSizeResult VulkanDynamicRHI::calcTexturePlatformSize(
    const resource::TextureDesc& desc, const gp::UInt32 firstMipIndex
) noexcept
{
    return calcVulkanTexturePlatformSize(m_resourceServices, desc, firstMipIndex);
}

gp::UInt64 VulkanDynamicRHI::computeMemorySize(resource::Texture* const texture) noexcept
{
    return computeVulkanTextureMemorySize(texture);
}

void VulkanDynamicRHI::getTextureMemoryStats(resource::TextureMemoryStats& outStats) noexcept
{
    fillVulkanTextureMemoryStats(m_resourceServices, outStats);
}

resource::TextureLockResult VulkanDynamicRHI::lockTexture(
    command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args
) noexcept
{
    return lockVulkanTexture(cmdList, args);
}

void VulkanDynamicRHI::unlockTexture(
    command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args
) noexcept
{
    unlockVulkanTexture(cmdList, args);
}

void VulkanDynamicRHI::updateTexture2D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion2D& updateRegion,
    const gp::UInt32 sourcePitch,
    const gp::UInt8* const sourceData
) noexcept
{
    updateVulkanTexture2D(cmdList, texture, mipIndex, updateRegion, sourcePitch, sourceData);
}

void VulkanDynamicRHI::updateTexture3D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion3D& updateRegion,
    const gp::UInt32 sourceRowPitch,
    const gp::UInt32 sourceDepthPitch,
    const gp::UInt8* const sourceData
) noexcept
{
    updateVulkanTexture3D(cmdList, texture, mipIndex, updateRegion, sourceRowPitch, sourceDepthPitch, sourceData);
}

void VulkanDynamicRHI::readSurfaceData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::Color>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    if (!readVulkanSurfaceData(command::getImmediateCommandList(), texture, rect, outData, flags))
    {
        outData.clear();
    }
}

void VulkanDynamicRHI::readSurfaceData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::LinearColor>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    if (!readVulkanSurfaceData(command::getImmediateCommandList(), texture, rect, outData, flags))
    {
        outData.clear();
    }
}

void VulkanDynamicRHI::readSurfaceFloatData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<resource::Float16Color>& outData,
    const resource::CubeFace cubeFace,
    const gp::Int32 arrayIndex,
    const gp::Int32 mipIndex
) noexcept
{
    if (!readVulkanSurfaceFloatData(
            command::getImmediateCommandList(), texture, rect, outData, cubeFace, arrayIndex, mipIndex
        ))
    {
        outData.clear();
    }
}

void VulkanDynamicRHI::read3DSurfaceFloatData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    const resource::IntPoint zMinMax,
    gp::Vector<resource::Float16Color>& outData
) noexcept
{
    if (!read3DVulkanSurfaceFloatData(command::getImmediateCommandList(), texture, rect, zMinMax, outData))
    {
        outData.clear();
    }
}

void VulkanDynamicRHI::mapStagingSurface(
    resource::Texture* const texture,
    sync::GPUFence* const fence,
    void*& outData,
    gp::Int32& outWidth,
    gp::Int32& outHeight,
    const gp::UInt32 gpuIndex
) noexcept
{
    static_cast<void>(gpuIndex);
    if ((fence != nullptr) && !fence->poll())
    {
        blockUntilGPUIdle();
    }
    mapVulkanStagingSurface(texture, outData, outWidth, outHeight);
}

void VulkanDynamicRHI::unmapStagingSurface(resource::Texture* const texture, const gp::UInt32 gpuIndex) noexcept
{
    static_cast<void>(gpuIndex);
    unmapVulkanStagingSurface(texture);
}

void VulkanDynamicRHI::aliasTextureResources(
    resource::TextureRef& destTexture, resource::TextureRef& srcTexture
) noexcept
{
    aliasVulkanTextureResources(destTexture, srcTexture);
}

view::ShaderResourceViewRef VulkanDynamicRHI::createShaderResourceView(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const view::ViewDesc& desc
) noexcept
{
    return createVulkanShaderResourceView(cmdList, buffer, desc);
}

view::ShaderResourceViewRef VulkanDynamicRHI::createShaderResourceView(
    command::CommandListBase& cmdList, resource::Texture* const texture, const view::ViewDesc& desc
) noexcept
{
    return createVulkanShaderResourceView(cmdList, texture, desc);
}

view::UnorderedAccessViewRef VulkanDynamicRHI::createUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const view::ViewDesc& desc
) noexcept
{
    return createVulkanUnorderedAccessView(cmdList, buffer, desc);
}

view::UnorderedAccessViewRef VulkanDynamicRHI::createUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Texture* const texture, const view::ViewDesc& desc
) noexcept
{
    return createVulkanUnorderedAccessView(cmdList, texture, desc);
}

sync::GPUFenceRef VulkanDynamicRHI::createGPUFence(const gp::Name name) noexcept
{
    if (!m_isInitialized)
    {
        return sync::GPUFenceRef{};
    }
    return sync::GPUFenceRef(new VulkanGPUFence(*m_device, name, m_graphicsQueue->usesTimelineSemaphore()));
}

sync::RenderQueryRef VulkanDynamicRHI::createRenderQuery(const sync::RenderQueryType queryType) noexcept
{
    return createVulkanRenderQuery(queryType);
}

sync::RenderQueryPoolRef
    VulkanDynamicRHI::createRenderQueryPool(const sync::RenderQueryType queryType, const gp::UInt32 numQueries) noexcept
{
    return createVulkanRenderQueryPool(queryType, numQueries);
}

bool VulkanDynamicRHI::getRenderQueryResult(
    sync::RenderQuery* const renderQuery, gp::UInt64& outResult, const bool wait, const gp::UInt32 gpuIndex
) noexcept
{
    static_cast<void>(gpuIndex);
    const gp::Float32 timestampPeriod = (m_device != nullptr) ? m_device->getLimits().timestampPeriod : 1.0f;
    return getVulkanRenderQueryResult(renderQuery, outResult, wait, timestampPeriod);
}

void VulkanDynamicRHI::writeGPUFenceTopOfPipe(command::CommandListBase& cmdList, sync::GPUFence* const fence) noexcept
{
    if (m_contextProvider == nullptr)
    {
        return;
    }
    writeVulkanGPUFenceTopOfPipe(cmdList, *m_contextProvider, fence);
}

void VulkanDynamicRHI::beginRenderQueryTopOfPipe(
    command::CommandListBase& cmdList, sync::RenderQuery* const renderQuery
) noexcept
{
    beginVulkanRenderQueryTopOfPipe(cmdList, renderQuery);
}

void VulkanDynamicRHI::endRenderQueryTopOfPipe(
    command::CommandListBase& cmdList, sync::RenderQuery* const renderQuery
) noexcept
{
    if (m_contextProvider == nullptr)
    {
        return;
    }
    endVulkanRenderQueryTopOfPipe(cmdList, *m_contextProvider, renderQuery);
}

gp::USize VulkanDynamicRHI::getTransitionPrivateDataSizeInBytes() const noexcept
{
    return getVulkanTransitionPrivateDataSize();
}

gp::USize VulkanDynamicRHI::getTransitionPrivateDataAlignInBytes() const noexcept
{
    return getVulkanTransitionPrivateDataAlign();
}

void VulkanDynamicRHI::createTransition(
    sync::Transition* const transition, const sync::TransitionCreateInfo& createInfo
) noexcept
{
    if (m_device == nullptr)
    {
        return;
    }
    createVulkanTransition(*m_device, transition, createInfo, getVulkanBarrierSettings());
}

void VulkanDynamicRHI::releaseTransition(sync::Transition* const transition) noexcept
{
    releaseVulkanTransition(transition);
}

}   // namespace gp::rhi::vulkan
