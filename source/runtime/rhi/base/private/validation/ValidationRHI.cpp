// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "validation/ValidationRHI.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/CommandListImmediate.hpp"
#include "core/RHIStrings.hpp"
#include "CoreMinimal.hpp"
#include "device/RHIGlobals.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/GlobalMemory.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "resources/BufferEnums.hpp"
#include "sync/Transition.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "validation/ValidationResource.hpp"
#include "validation/ValidationTypes.hpp"
#include "views/UnorderedAccessView.hpp"
#include <cstdio>
#include <new>   // IWYU pragma: keep
#include <utility>

namespace gp::rhi
{

namespace
{

/// @brief Platform command list wrapper pairing an inner backend command list with queued validation tracker ops.
class ValidationPlatformCommandList final : public command::IPlatformCommandList
{
public:
    command::IPlatformCommandList* innerCommandList{ nullptr };
    sync::PipelineStage pipeline{ sync::PipelineStage::Graphics };
    validation::OperationsList trackerOps{};

    ValidationPlatformCommandList(
        command::IPlatformCommandList* const inInner,
        const sync::PipelineStage inPipeline,
        validation::OperationsList&& inOps
    ) noexcept
        : innerCommandList(inInner)
        , pipeline(inPipeline)
        , trackerOps(std::move(inOps))
    {}
};

[[nodiscard]] resource::ExclusiveDepthStencil computeActualDSMode(const pipeline::DepthStencilStateDesc& desc) noexcept
{
    resource::ExclusiveDepthStencil mode = resource::ExclusiveDepthStencil::kDepthNopStencilNop;
    const bool usesDepth = desc.enableDepthWrite || (desc.depthTest != pipeline::CompareFunction::Always);
    if (usesDepth)
    {
        if (desc.enableDepthWrite)
        {
            mode = resource::ExclusiveDepthStencil::kDepthWriteStencilNop;
        }
        else
        {
            mode = resource::ExclusiveDepthStencil::kDepthReadStencilNop;
        }
    }

    const auto faceWritesStencil = [](const bool enabled,
                                      const pipeline::StencilOp failOp,
                                      const pipeline::StencilOp depthFailOp,
                                      const pipeline::StencilOp passOp,
                                      const gp::UInt8 writeMask) noexcept -> bool
    {
        if (!enabled || writeMask == 0u)
        {
            return false;
        }
        return (failOp != pipeline::StencilOp::Keep) || (depthFailOp != pipeline::StencilOp::Keep) ||
               (passOp != pipeline::StencilOp::Keep);
    };

    const bool usesStencil = desc.enableFrontFaceStencil || desc.enableBackFaceStencil;
    const bool writesStencil = faceWritesStencil(
                                   desc.enableFrontFaceStencil,
                                   desc.frontFaceStencilFailOp,
                                   desc.frontFaceDepthFailOp,
                                   desc.frontFacePassOp,
                                   desc.stencilWriteMask
                               ) ||
                               faceWritesStencil(
                                   desc.enableBackFaceStencil,
                                   desc.backFaceStencilFailOp,
                                   desc.backFaceDepthFailOp,
                                   desc.backFacePassOp,
                                   desc.stencilWriteMask
                               );

    if (usesStencil)
    {
        if (writesStencil)
        {
            mode.setStencilWrite();
        }
        else
        {
            if (mode.isDepthWrite())
            {
                mode = resource::ExclusiveDepthStencil::kDepthWriteStencilRead;
            }
            else if (mode.isDepthRead())
            {
                mode = resource::ExclusiveDepthStencil::kDepthReadStencilRead;
            }
            else
            {
                mode = resource::ExclusiveDepthStencil::kDepthNopStencilRead;
            }
        }
    }

    return mode;
}

[[nodiscard]] validation::ResourceIdentity resolveTransitionIdentity(const sync::TransitionInfo& info) noexcept
{
    validation::ResourceIdentity id{};
#if GP_ENABLE_RHI_VALIDATION
    if (info.type == sync::TransitionResourceType::UAV)
    {
        if (info.uav != nullptr)
        {
            id = info.uav->getViewIdentity();
        }
        return id;
    }

    resource::ViewableResource* const viewable = sync::getViewableResource(info);
    if (viewable == nullptr)
    {
        return id;
    }

    validation::Resource* const res = viewable->getValidationTrackerResource();
    if (res == nullptr || !res->isBarrierTrackingInitialized())
    {
        return id;
    }

    id.resource = res;
    if (info.isWholeResource())
    {
        id = res->getWholeResourceIdentity();
        return id;
    }

    id.subresourceRange.mipIndex = info.isAllMips() ? 0u : info.mipIndex;
    id.subresourceRange.numMips = info.isAllMips() ? res->getNumMips() : 1u;
    id.subresourceRange.arraySlice = info.isAllArraySlices() ? 0u : info.arraySlice;
    id.subresourceRange.numArraySlices = info.isAllArraySlices() ? res->getNumArraySlices() : 1u;
    id.subresourceRange.planeIndex = info.isAllPlaneSlices() ? 0u
                                                             : gp::math::min<gp::UInt8>(
                                                                   static_cast<gp::UInt8>(info.planeSlice),
                                                                   static_cast<gp::UInt8>(res->getNumPlanes() - 1u)
                                                               );
    id.subresourceRange.numPlanes = info.isAllPlaneSlices() ? res->getNumPlanes() : 1u;
#else
    static_cast<void>(info);
#endif
    return id;
}

[[nodiscard]] validation::Resource* resolveTransientResource(
    resource::Resource* const rawResource, const sync::TransientAliasingResourceType type
) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    if (rawResource == nullptr)
    {
        return nullptr;
    }
    if (type == sync::TransientAliasingResourceType::Texture)
    {
        return static_cast<resource::Texture*>(rawResource)->getValidationTrackerResource();
    }
    return static_cast<resource::Buffer*>(rawResource)->getValidationTrackerResource();
#else
    static_cast<void>(rawResource);
    static_cast<void>(type);
    return nullptr;
#endif
}

}   // namespace

ValidationRHI::ValidationRHI(DynamicRHI* const inRHI) noexcept
    : m_rhi(inRHI)
{
    GP_CHECKF(m_rhi != nullptr, "ValidationRHI requires a non-null concrete DynamicRHI instance.");
    validation::g_rhiValidationEnabled = true;
}

ValidationRHI::~ValidationRHI() noexcept
{
    validation::g_rhiValidationEnabled = false;
    m_parallelGraphicsContexts.clear();
    m_parallelComputeContexts.clear();
    m_defaultAsyncComputeContext.reset();
    m_defaultGraphicsContext.reset();
    delete m_rhi;
    m_rhi = nullptr;
}

void ValidationRHI::init() noexcept
{
    m_rhi->init();

    if (command::ICommandContext* const innerGfx = m_rhi->getDefaultContext(); innerGfx != nullptr)
    {
        m_defaultGraphicsContext = gp::makeUnique<ValidationContext>(*this, innerGfx);
    }

    command::IComputeContext* const innerCompute = m_rhi->getDefaultAsyncComputeContext();
    if (innerCompute != nullptr && innerCompute != m_rhi->getDefaultContext())
    {
        m_defaultAsyncComputeContext = gp::makeUnique<ValidationComputeContext>(*this, innerCompute);
    }
}

void ValidationRHI::postInit() noexcept
{
    m_rhi->postInit();
}

void ValidationRHI::shutdown() noexcept
{
    validation::Tracker::resetAllPipelines();
    m_parallelGraphicsContexts.clear();
    m_parallelComputeContexts.clear();
    m_defaultAsyncComputeContext.reset();
    m_defaultGraphicsContext.reset();
    m_rhi->shutdown();
}

const char* ValidationRHI::getName() const noexcept
{
    return m_rhi->getName();
}

InterfaceType ValidationRHI::getInterfaceType() const noexcept
{
    return m_rhi->getInterfaceType();
}

void* ValidationRHI::getNativeDevice() noexcept
{
    return m_rhi->getNativeDevice();
}

void ValidationRHI::tick(const gp::Float32 deltaTime) noexcept
{
    m_rhi->tick(deltaTime);
}

void ValidationRHI::processDeleteQueue() noexcept
{
    m_rhi->processDeleteQueue();
}

void ValidationRHI::flushPendingLogs() noexcept
{
    m_rhi->flushPendingLogs();
}

void ValidationRHI::acquireThreadOwnership() noexcept
{
    m_rhi->acquireThreadOwnership();
}

void ValidationRHI::releaseThreadOwnership() noexcept
{
    m_rhi->releaseThreadOwnership();
}

void ValidationRHI::suspendRendering() noexcept
{
    m_rhi->suspendRendering();
}

void ValidationRHI::resumeRendering() noexcept
{
    m_rhi->resumeRendering();
}

bool ValidationRHI::isRenderingSuspended() const noexcept
{
    return m_rhi->isRenderingSuspended();
}

gp::UInt64 ValidationRHI::getMinimumAlignmentForBufferBackedSRV(const PixelFormat format) noexcept
{
    return m_rhi->getMinimumAlignmentForBufferBackedSRV(format);
}

gp::UInt32 ValidationRHI::getPlatformTextureMaxSampleCount() const noexcept
{
    return m_rhi->getPlatformTextureMaxSampleCount();
}

pipeline::SamplerStateRef ValidationRHI::createSamplerState(const pipeline::SamplerStateDesc& initializer) noexcept
{
    return m_rhi->createSamplerState(initializer);
}

pipeline::RasterizerStateRef
    ValidationRHI::createRasterizerState(const pipeline::RasterizerStateDesc& initializer) noexcept
{
    return m_rhi->createRasterizerState(initializer);
}

pipeline::DepthStencilStateRef
    ValidationRHI::createDepthStencilState(const pipeline::DepthStencilStateDesc& initializer) noexcept
{
    pipeline::DepthStencilStateRef state = m_rhi->createDepthStencilState(initializer);
#if GP_ENABLE_RHI_VALIDATION
    if (state.isValid())
    {
        state->actualDSMode = computeActualDSMode(initializer);
    }
#endif
    return state;
}

pipeline::BlendStateRef ValidationRHI::createBlendState(const pipeline::BlendStateDesc& initializer) noexcept
{
    return m_rhi->createBlendState(initializer);
}

pipeline::VertexDeclarationRef
    ValidationRHI::createVertexDeclaration(const pipeline::VertexDeclarationElementList& elements) noexcept
{
    return m_rhi->createVertexDeclaration(elements);
}

shader::VertexShaderRef ValidationRHI::createVertexShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    return m_rhi->createVertexShader(code, hash);
}

shader::PixelShaderRef
    ValidationRHI::createPixelShader(const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    return m_rhi->createPixelShader(code, hash);
}

shader::GeometryShaderRef ValidationRHI::createGeometryShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    return m_rhi->createGeometryShader(code, hash);
}

shader::ComputeShaderRef ValidationRHI::createComputeShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    return m_rhi->createComputeShader(code, hash);
}

shader::MeshShaderRef
    ValidationRHI::createMeshShader(const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    return m_rhi->createMeshShader(code, hash);
}

shader::AmplificationShaderRef ValidationRHI::createAmplificationShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    return m_rhi->createAmplificationShader(code, hash);
}

pipeline::BoundShaderStateRef ValidationRHI::createBoundShaderState(
    pipeline::VertexDeclaration* const vertexDeclaration,
    shader::VertexShader* const vertexShader,
    shader::PixelShader* const pixelShader,
    shader::GeometryShader* const geometryShader
) noexcept
{
    return m_rhi->createBoundShaderState(vertexDeclaration, vertexShader, pixelShader, geometryShader);
}

pipeline::BoundShaderStateRef ValidationRHI::createBoundShaderState(
    shader::AmplificationShader* const amplificationShader,
    shader::MeshShader* const meshShader,
    shader::PixelShader* const pixelShader
) noexcept
{
    return m_rhi->createBoundShaderState(amplificationShader, meshShader, pixelShader);
}

pipeline::GraphicsPipelineStateRef
    ValidationRHI::createGraphicsPipelineState(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept
{
    if (initializer.boundShaderState.getVertexShader() == nullptr &&
        initializer.boundShaderState.getMeshShader() == nullptr)
    {
        validation::reportValidationFailure(
            "createGraphicsPipelineState requires either a valid vertexShader or meshShader."
        );
    }

#if GP_ENABLE_RHI_VALIDATION
    if (initializer.depthStencilState != nullptr)
    {
        const resource::ExclusiveDepthStencil actualDS = initializer.depthStencilState->actualDSMode;
        if (!actualDS.isValid(initializer.depthStencilAccess))
        {
            validation::reportValidationFailure(
                "createGraphicsPipelineState: DepthStencilState read/write flags are incompatible with initializer.depthStencilAccess."
            );
        }
    }
#endif

    pipeline::GraphicsPipelineStateRef pso = m_rhi->createGraphicsPipelineState(initializer);
#if GP_ENABLE_RHI_VALIDATION
    if (pso.isValid())
    {
        pso->dsMode = (initializer.depthStencilState != nullptr) ? initializer.depthStencilState->actualDSMode
                                                                 : initializer.depthStencilAccess;
    }
#endif
    return pso;
}

pipeline::ComputePipelineStateRef
    ValidationRHI::createComputePipelineState(shader::ComputeShader* const computeShader) noexcept
{
    if (computeShader == nullptr)
    {
        validation::reportValidationFailure("createComputePipelineState called with a null ComputeShader.");
    }
    return m_rhi->createComputePipelineState(computeShader);
}

pipeline::ComputePipelineStateRef
    ValidationRHI::createComputePipelineState(const pipeline::ComputePipelineStateInitializer& initializer) noexcept
{
    if (initializer.computeShader == nullptr)
    {
        validation::reportValidationFailure("createComputePipelineState called with a null initializer.computeShader.");
    }
    return m_rhi->createComputePipelineState(initializer);
}

gp::UInt64
    ValidationRHI::computeStatePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept
{
    return m_rhi->computeStatePrecachePSOHash(initializer);
}

gp::UInt64 ValidationRHI::computePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept
{
    return m_rhi->computePrecachePSOHash(initializer);
}

bool ValidationRHI::matchPrecachePSOInitializers(
    const pipeline::GraphicsPipelineStateInitializer& lhs, const pipeline::GraphicsPipelineStateInitializer& rhs
) noexcept
{
    return m_rhi->matchPrecachePSOInitializers(lhs, rhs);
}

resource::UniformBufferRef ValidationRHI::createUniformBuffer(
    const void* const contents,
    const resource::UniformBufferLayout* const layout,
    const resource::UniformBufferUsage usage,
    const resource::UniformBufferValidation validationMode
) noexcept
{
    if (layout == nullptr)
    {
        validation::reportValidationFailure("createUniformBuffer called with a null UniformBufferLayout.");
        return resource::UniformBufferRef{};
    }
    if (layout->getConstantBufferSize() > 0u && contents == nullptr &&
        validationMode == resource::UniformBufferValidation::ValidateResources)
    {
        validation::reportValidationFailure(
            "createUniformBuffer called with null contents for a non-zero constant buffer layout."
        );
    }

    resource::UniformBufferRef ub = m_rhi->createUniformBuffer(contents, layout, usage, validationMode);
#if GP_ENABLE_RHI_VALIDATION
    if (ub.isValid())
    {
        ub->getValidationResource().initLifetimeTracking(m_renderThreadFrameID, usage);
    }
#endif
    return ub;
}

void ValidationRHI::updateUniformBuffer(
    command::CommandListBase& cmdList, resource::UniformBuffer* const uniformBuffer, const void* const contents
) noexcept
{
    if (uniformBuffer == nullptr || contents == nullptr)
    {
        validation::reportValidationFailure("updateUniformBuffer requires non-null uniformBuffer and contents.");
        return;
    }
#if GP_ENABLE_RHI_VALIDATION
    uniformBuffer->getValidationResource().updateAllocation(m_renderThreadFrameID);
#endif
    m_rhi->updateUniformBuffer(cmdList, uniformBuffer, contents);
}

resource::BufferInitializer ValidationRHI::createBufferInitializer(
    command::CommandListBase& cmdList, const resource::BufferCreateDesc& createDesc
) noexcept
{
    return m_rhi->createBufferInitializer(cmdList, createDesc);
}

void* ValidationRHI::lockBuffer(
    command::CommandListBase& cmdList,
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 size,
    const resource::ResourceLockMode lockMode
) noexcept
{
    if (buffer == nullptr)
    {
        validation::reportValidationFailure("lockBuffer called with a null Buffer.");
        return nullptr;
    }
    if ((offset + size) > buffer->getSize())
    {
        validation::reportValidationFailure("lockBuffer range [offset, offset + size) exceeds buffer size.");
    }
    return m_rhi->lockBuffer(cmdList, buffer, offset, size, lockMode);
}

void ValidationRHI::unlockBuffer(command::CommandListBase& cmdList, resource::Buffer* const buffer) noexcept
{
    if (buffer == nullptr)
    {
        validation::reportValidationFailure("unlockBuffer called with a null Buffer.");
        return;
    }
    m_rhi->unlockBuffer(cmdList, buffer);
}

void* ValidationRHI::lockBufferBottomOfPipe(
    command::CommandListBase& cmdList,
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 size,
    const resource::ResourceLockMode lockMode
) noexcept
{
    return m_rhi->lockBufferBottomOfPipe(cmdList, buffer, offset, size, lockMode);
}

void ValidationRHI::unlockBufferBottomOfPipe(command::CommandListBase& cmdList, resource::Buffer* const buffer) noexcept
{
    m_rhi->unlockBufferBottomOfPipe(cmdList, buffer);
}

void ValidationRHI::replaceResources(
    command::CommandListBase& cmdList, const gp::ConstVectorView<resource::ResourceReplaceInfo> infos
) noexcept
{
    m_rhi->replaceResources(cmdList, infos);
}

resource::TextureInitializer ValidationRHI::createTextureInitializer(
    command::CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
) noexcept
{
    return m_rhi->createTextureInitializer(cmdList, createDesc);
}

resource::TextureReferenceRef ValidationRHI::createTextureReference(
    command::CommandListBase& cmdList, resource::Texture* const referencedTexture
) noexcept
{
    return m_rhi->createTextureReference(cmdList, referencedTexture);
}

void ValidationRHI::updateTextureReference(
    command::CommandListBase& cmdList,
    resource::TextureReference* const textureReference,
    resource::Texture* const newTexture
) noexcept
{
    if (textureReference == nullptr)
    {
        validation::reportValidationFailure("updateTextureReference called with a null TextureReference.");
        return;
    }
    m_rhi->updateTextureReference(cmdList, textureReference, newTexture);
}

resource::CalcTextureSizeResult
    ValidationRHI::calcTexturePlatformSize(const resource::TextureDesc& desc, const gp::UInt32 firstMipIndex) noexcept
{
    return m_rhi->calcTexturePlatformSize(desc, firstMipIndex);
}

gp::UInt64 ValidationRHI::computeMemorySize(resource::Texture* const texture) noexcept
{
    return m_rhi->computeMemorySize(texture);
}

void ValidationRHI::getTextureMemoryStats(resource::TextureMemoryStats& outStats) noexcept
{
    m_rhi->getTextureMemoryStats(outStats);
}

void ValidationRHI::getMemoryStats(profiling::RHIMemoryStats& outStats) noexcept
{
    m_rhi->getMemoryStats(outStats);
}

resource::TextureLockResult
    ValidationRHI::lockTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept
{
    if (args.texture == nullptr)
    {
        validation::reportValidationFailure("lockTexture called with a null Texture.");
        return resource::TextureLockResult{};
    }
    return m_rhi->lockTexture(cmdList, args);
}

void ValidationRHI::unlockTexture(
    command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args
) noexcept
{
    if (args.texture == nullptr)
    {
        validation::reportValidationFailure("unlockTexture called with a null Texture.");
        return;
    }
    m_rhi->unlockTexture(cmdList, args);
}

void ValidationRHI::updateTexture2D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion2D& updateRegion,
    const gp::UInt32 sourcePitch,
    const gp::UInt8* const sourceData
) noexcept
{
    m_rhi->updateTexture2D(cmdList, texture, mipIndex, updateRegion, sourcePitch, sourceData);
}

void ValidationRHI::updateTexture3D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion3D& updateRegion,
    const gp::UInt32 sourceRowPitch,
    const gp::UInt32 sourceDepthPitch,
    const gp::UInt8* const sourceData
) noexcept
{
    m_rhi->updateTexture3D(cmdList, texture, mipIndex, updateRegion, sourceRowPitch, sourceDepthPitch, sourceData);
}

resource::UpdateTexture3DData ValidationRHI::beginUpdateTexture3D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion3D& updateRegion
) noexcept
{
    return m_rhi->beginUpdateTexture3D(cmdList, texture, mipIndex, updateRegion);
}

void ValidationRHI::endUpdateTexture3D(
    command::CommandListBase& cmdList, resource::UpdateTexture3DData& updateData
) noexcept
{
    m_rhi->endUpdateTexture3D(cmdList, updateData);
}

void ValidationRHI::readSurfaceData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::Color>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    m_rhi->readSurfaceData(texture, rect, outData, flags);
}

void ValidationRHI::readSurfaceData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::LinearColor>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    m_rhi->readSurfaceData(texture, rect, outData, flags);
}

void ValidationRHI::readSurfaceFloatData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<resource::Float16Color>& outData,
    const resource::CubeFace cubeFace,
    const gp::Int32 arrayIndex,
    const gp::Int32 mipIndex
) noexcept
{
    m_rhi->readSurfaceFloatData(texture, rect, outData, cubeFace, arrayIndex, mipIndex);
}

void ValidationRHI::read3DSurfaceFloatData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    const resource::IntPoint zMinMax,
    gp::Vector<resource::Float16Color>& outData
) noexcept
{
    m_rhi->read3DSurfaceFloatData(texture, rect, zMinMax, outData);
}

view::ShaderResourceViewRef ValidationRHI::createShaderResourceView(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const view::ViewDesc& desc
) noexcept
{
    if (buffer == nullptr)
    {
        validation::reportValidationFailure("createShaderResourceView called with a null Buffer.");
        return view::ShaderResourceViewRef{};
    }
    return m_rhi->createShaderResourceView(cmdList, buffer, desc);
}

view::ShaderResourceViewRef ValidationRHI::createShaderResourceView(
    command::CommandListBase& cmdList, resource::Texture* const texture, const view::ViewDesc& desc
) noexcept
{
    if (texture == nullptr)
    {
        validation::reportValidationFailure("createShaderResourceView called with a null Texture.");
        return view::ShaderResourceViewRef{};
    }
    return m_rhi->createShaderResourceView(cmdList, texture, desc);
}

view::UnorderedAccessViewRef ValidationRHI::createUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const view::ViewDesc& desc
) noexcept
{
    if (buffer == nullptr)
    {
        validation::reportValidationFailure("createUnorderedAccessView called with a null Buffer.");
        return view::UnorderedAccessViewRef{};
    }
    if ((buffer->getUsage() & resource::BufferUsageFlags::UnorderedAccess) == resource::BufferUsageFlags::None)
    {
        validation::reportValidationFailure(
            "createUnorderedAccessView requires BufferUsageFlags::UnorderedAccess on the target Buffer."
        );
    }
    return m_rhi->createUnorderedAccessView(cmdList, buffer, desc);
}

view::UnorderedAccessViewRef ValidationRHI::createUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Texture* const texture, const view::ViewDesc& desc
) noexcept
{
    if (texture == nullptr)
    {
        validation::reportValidationFailure("createUnorderedAccessView called with a null Texture.");
        return view::UnorderedAccessViewRef{};
    }
    if ((texture->getFlags() & resource::TextureCreateFlags::UAV) == resource::TextureCreateFlags::None)
    {
        validation::reportValidationFailure(
            "createUnorderedAccessView requires TextureCreateFlags::UAV on the target Texture."
        );
    }
    return m_rhi->createUnorderedAccessView(cmdList, texture, desc);
}

sync::GPUFenceRef ValidationRHI::createGPUFence(const gp::Name name) noexcept
{
    return m_rhi->createGPUFence(name);
}

resource::StagingBufferRef ValidationRHI::createStagingBuffer() noexcept
{
    return m_rhi->createStagingBuffer();
}

void* ValidationRHI::lockStagingBuffer(
    resource::StagingBuffer* const stagingBuffer,
    sync::GPUFence* const fence,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    return m_rhi->lockStagingBuffer(stagingBuffer, fence, offset, numBytes);
}

void ValidationRHI::unlockStagingBuffer(resource::StagingBuffer* const stagingBuffer) noexcept
{
    m_rhi->unlockStagingBuffer(stagingBuffer);
}

sync::RenderQueryRef ValidationRHI::createRenderQuery(const sync::RenderQueryType queryType) noexcept
{
    return m_rhi->createRenderQuery(queryType);
}

sync::RenderQueryPoolRef
    ValidationRHI::createRenderQueryPool(const sync::RenderQueryType queryType, const gp::UInt32 numQueries) noexcept
{
    return m_rhi->createRenderQueryPool(queryType, numQueries);
}

bool ValidationRHI::getRenderQueryResult(
    sync::RenderQuery* const renderQuery, gp::UInt64& outResult, const bool wait, const gp::UInt32 gpuIndex
) noexcept
{
    return m_rhi->getRenderQueryResult(renderQuery, outResult, wait, gpuIndex);
}

void ValidationRHI::writeGPUFenceTopOfPipe(command::CommandListBase& cmdList, sync::GPUFence* const fence) noexcept
{
    m_rhi->writeGPUFenceTopOfPipe(cmdList, fence);
}

void ValidationRHI::beginRenderQueryTopOfPipe(
    command::CommandListBase& cmdList, sync::RenderQuery* const renderQuery
) noexcept
{
    m_rhi->beginRenderQueryTopOfPipe(cmdList, renderQuery);
}

void ValidationRHI::endRenderQueryTopOfPipe(
    command::CommandListBase& cmdList, sync::RenderQuery* const renderQuery
) noexcept
{
    m_rhi->endRenderQueryTopOfPipe(cmdList, renderQuery);
}

gp::USize ValidationRHI::getTransitionPrivateDataSizeInBytes() const noexcept
{
    return m_rhi->getTransitionPrivateDataSizeInBytes();
}

gp::USize ValidationRHI::getTransitionPrivateDataAlignInBytes() const noexcept
{
    return m_rhi->getTransitionPrivateDataAlignInBytes();
}

void ValidationRHI::createTransition(
    sync::Transition* const transition, const sync::TransitionCreateInfo& createInfo
) noexcept
{
#if GP_ENABLE_RHI_VALIDATION
    if (transition != nullptr)
    {
        validation::TransitionResource& valData = transition->getValidationData();
        const sync::PipelineStage srcPipes = createInfo.srcPipelines;
        const sync::PipelineStage dstPipes = createInfo.dstPipelines;

        // Cross-pipeline fence generation when srcPipelines != dstPipelines and NoFence is not set.
        const bool isCrossPipe = (srcPipes != dstPipes);
        const bool needsFence = isCrossPipe && ((createInfo.flags & sync::TransitionCreateFlags::NoFence) ==
                                                sync::TransitionCreateFlags::None);
        if (needsFence)
        {
            if (gp::memory::Malloc* const alloc = gp::memory::getGlobalMalloc(); alloc != nullptr)
            {
                void* const fenceMem =
                    alloc->allocate(sizeof(validation::Fence), static_cast<gp::UInt32>(alignof(validation::Fence)));
                validation::Fence* const fence = ::new (fenceMem) validation::Fence();
                fence->signalingPipelines = srcPipes;
                fence->waitingPipelines = dstPipes;

                const sync::PipelineStage kStages[sync::kPipelineStageCount] = { sync::PipelineStage::Graphics,
                                                                                 sync::PipelineStage::AsyncCompute };
                for (const sync::PipelineStage stage: kStages)
                {
                    if ((srcPipes & stage) != sync::PipelineStage::None)
                    {
                        validation::Operation sigOp{};
                        sigOp.type = validation::OpType::Signal;
                        sigOp.fence = fence;
                        valData.pendingSignals[stage].add(std::move(sigOp));
                    }
                    if ((dstPipes & stage) != sync::PipelineStage::None)
                    {
                        validation::Operation waitOp{};
                        waitOp.type = validation::OpType::Wait;
                        waitOp.fence = fence;
                        valData.pendingWaits[stage].add(std::move(waitOp));
                    }
                }
            }
        }

        // Per-resource state transition operations (recorded before DiscardTransientBegin so combined
        // Transition(State -> Discard) + TransientAliasingInfo::discard batches validate while still Acquired).
        for (const sync::TransitionInfo& info: createInfo.transitionInfos)
        {
            if (info.resource == nullptr)
            {
                continue;
            }

            if (info.accessAfter == sync::ResourceAccess::Unknown)
            {
                validation::reportValidationFailure("TransitionInfo::accessAfter cannot be ResourceAccess::Unknown.");
                continue;
            }

            if (sync::isInvalidAccess(info.accessAfter))
            {
                char msg[256]{};
                const int len = std::snprintf(
                    msg,
                    sizeof(msg),
                    "TransitionInfo::accessAfter (%s) combines mutually incompatible read/write states.",
                    getRHIAccessName(info.accessAfter).cString()
                );
                if (len > 0)
                {
                    validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
                }
                continue;
            }

            const validation::ResourceIdentity id = resolveTransitionIdentity(info);
            if (id.resource == nullptr)
            {
                continue;
            }

            const validation::State beforeState(info.accessBefore, srcPipes);
            const validation::State afterState(info.accessAfter, dstPipes);

            validation::Operation beginOp{};
            beginOp.type = validation::OpType::BeginTransition;
            beginOp.identity = id;
            beginOp.stateBefore = beforeState;
            beginOp.stateAfter = afterState;
            beginOp.transitionFlags = info.flags;
            valData.pendingOperationsBegin.add(std::move(beginOp));

            validation::Operation endOp{};
            endOp.type = validation::OpType::EndTransition;
            endOp.identity = id;
            endOp.stateBefore = beforeState;
            endOp.stateAfter = afterState;
            endOp.transitionFlags = info.flags;
            valData.pendingOperationsEnd.add(std::move(endOp));
        }

        // Transient aliasing acquire / discard / overlap operations.
        for (const sync::TransientAliasingInfo& aliasInfo: createInfo.aliasingInfos)
        {
            validation::Resource* const targetRes = resolveTransientResource(aliasInfo.resource, aliasInfo.type);
            if (targetRes == nullptr)
            {
                validation::reportValidationFailure(
                    "TransitionCreateInfo contains a null TransientAliasingInfo resource."
                );
                continue;
            }

            if (aliasInfo.isAcquire())
            {
                validation::Operation acqOp{};
                acqOp.type = validation::OpType::AcquireTransient;
                acqOp.identity = targetRes->getWholeResourceIdentity();
                valData.pendingAliases.add(std::move(acqOp));

                for (const sync::TransientAliasingOverlap& overlap: aliasInfo.overlaps)
                {
                    validation::Resource* const overlappedRes =
                        resolveTransientResource(overlap.resource, overlap.type);
                    if (overlappedRes != nullptr)
                    {
                        validation::Operation ovOp{};
                        ovOp.type = validation::OpType::AliasingOverlap;
                        ovOp.identity = targetRes->getWholeResourceIdentity();
                        ovOp.secondResource = overlappedRes;
                        valData.pendingAliasingOverlaps.add(std::move(ovOp));
                    }
                }
            }
            else
            {
                if (!aliasInfo.overlaps.isEmpty())
                {
                    validation::reportValidationFailure(
                        "TransientAliasingInfo::Discard must have an empty overlaps list."
                    );
                }

                validation::Operation discBegin{};
                discBegin.type = validation::OpType::DiscardTransientBegin;
                discBegin.identity = targetRes->getWholeResourceIdentity();
                valData.pendingOperationsBegin.add(std::move(discBegin));

                validation::Operation discEnd{};
                discEnd.type = validation::OpType::DiscardTransientEnd;
                discEnd.identity = targetRes->getWholeResourceIdentity();
                valData.pendingOperationsEnd.add(std::move(discEnd));
            }
        }
    }
#endif
    m_rhi->createTransition(transition, createInfo);
}

void ValidationRHI::releaseTransition(sync::Transition* const transition) noexcept
{
    m_rhi->releaseTransition(transition);
}

presentation::ViewportRef ValidationRHI::createViewport(
    void* const windowHandle,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    return m_rhi->createViewport(windowHandle, sizeX, sizeY, isFullscreen, preferredPixelFormat);
}

void ValidationRHI::resizeViewport(
    presentation::Viewport* const viewport,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    m_rhi->resizeViewport(viewport, sizeX, sizeY, isFullscreen, preferredPixelFormat);
}

resource::TextureRef ValidationRHI::getViewportBackBuffer(presentation::Viewport* const viewport) noexcept
{
    return m_rhi->getViewportBackBuffer(viewport);
}

void ValidationRHI::advanceFrameForGetViewportBackBuffer(presentation::Viewport* const viewport) noexcept
{
    m_rhi->advanceFrameForGetViewportBackBuffer(viewport);
}

void ValidationRHI::beginDrawingViewport(
    presentation::Viewport* const viewport, resource::Texture* const renderTarget
) noexcept
{
    m_rhi->beginDrawingViewport(viewport, renderTarget);
}

void ValidationRHI::endDrawingViewport(
    presentation::Viewport* const viewport, const presentation::PresentArgsWithStats& presentArgs
) noexcept
{
    m_rhi->endDrawingViewport(viewport, presentArgs);
}

void ValidationRHI::beginFrame() noexcept
{
    m_rhi->beginFrame();
}

void ValidationRHI::endFrame(const EndFrameArgs& args) noexcept
{
    ++m_renderThreadFrameID;
    m_rhi->endFrame(args);
}

void ValidationRHI::endFrameRenderThread() noexcept
{
    DynamicRHI::endFrameRenderThread();
}

bool ValidationRHI::getSupportedResolutions(
    gp::Vector<ScreenResolution>& outResolutions, const bool ignoreRefreshRate
) noexcept
{
    return m_rhi->getSupportedResolutions(outResolutions, ignoreRefreshRate);
}

void ValidationRHI::getSupportedResolution(gp::UInt32& inOutWidth, gp::UInt32& inOutHeight) noexcept
{
    m_rhi->getSupportedResolution(inOutWidth, inOutHeight);
}

void ValidationRHI::mapStagingSurface(
    resource::Texture* const texture,
    sync::GPUFence* const fence,
    void*& outData,
    gp::Int32& outWidth,
    gp::Int32& outHeight,
    const gp::UInt32 gpuIndex
) noexcept
{
    m_rhi->mapStagingSurface(texture, fence, outData, outWidth, outHeight, gpuIndex);
}

void ValidationRHI::unmapStagingSurface(resource::Texture* const texture, const gp::UInt32 gpuIndex) noexcept
{
    m_rhi->unmapStagingSurface(texture, gpuIndex);
}

void ValidationRHI::aliasTextureResources(resource::TextureRef& destTexture, resource::TextureRef& srcTexture) noexcept
{
    m_rhi->aliasTextureResources(destTexture, srcTexture);
}

void ValidationRHI::blockUntilGPUIdle() noexcept
{
    m_rhi->blockUntilGPUIdle();
}

void ValidationRHI::bindDebugLabelName(
    command::CommandListBase& cmdList, resource::Texture* const texture, const char* const name
) noexcept
{
    if (texture != nullptr && name != nullptr)
    {
        texture->setName(gp::Name(name));
    }
    m_rhi->bindDebugLabelName(cmdList, texture, name);
}

void ValidationRHI::bindDebugLabelName(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const char* const name
) noexcept
{
    if (buffer != nullptr && name != nullptr)
    {
        buffer->setName(gp::Name(name));
    }
    m_rhi->bindDebugLabelName(cmdList, buffer, name);
}

void ValidationRHI::bindDebugLabelName(
    command::CommandListBase& cmdList, view::UnorderedAccessView* const uav, const char* const name
) noexcept
{
    m_rhi->bindDebugLabelName(cmdList, uav, name);
}

command::ICommandContext* ValidationRHI::getDefaultContext() noexcept
{
    if (!m_defaultGraphicsContext.isValid() && m_rhi->getDefaultContext() != nullptr)
    {
        m_defaultGraphicsContext = gp::makeUnique<ValidationContext>(*this, m_rhi->getDefaultContext());
    }
    return m_defaultGraphicsContext.get();
}

command::IComputeContext* ValidationRHI::getDefaultAsyncComputeContext() noexcept
{
    command::IComputeContext* const innerAsync = m_rhi->getDefaultAsyncComputeContext();
    if (innerAsync == nullptr || innerAsync == m_rhi->getDefaultContext())
    {
        return getDefaultContext();
    }
    if (!m_defaultAsyncComputeContext.isValid())
    {
        m_defaultAsyncComputeContext = gp::makeUnique<ValidationComputeContext>(*this, innerAsync);
    }
    return m_defaultAsyncComputeContext.get();
}

command::IComputeContext* ValidationRHI::getCommandContext(const sync::PipelineStage pipeline) noexcept
{
    command::IComputeContext* const innerCtx = m_rhi->getCommandContext(pipeline);
    if (innerCtx == nullptr)
    {
        return nullptr;
    }

    if (innerCtx->getWrappingContext() != nullptr)
    {
        return innerCtx->getWrappingContext();
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_parallelContextMutex);
    if (pipeline == sync::PipelineStage::Graphics)
    {
        gp::UniquePtr<ValidationContext> wrapper =
            gp::makeUnique<ValidationContext>(*this, static_cast<command::ICommandContext*>(innerCtx));
        ValidationContext* const rawPtr = wrapper.get();
        m_parallelGraphicsContexts.emplaceBack(std::move(wrapper));
        return rawPtr;
    }

    gp::UniquePtr<ValidationComputeContext> wrapper = gp::makeUnique<ValidationComputeContext>(*this, innerCtx);
    ValidationComputeContext* const rawPtr = wrapper.get();
    m_parallelComputeContexts.emplaceBack(std::move(wrapper));
    return rawPtr;
}

command::IUploadContext* ValidationRHI::acquireUploadContext() noexcept
{
    return m_rhi->acquireUploadContext();
}

void ValidationRHI::finalizeContexts(FinalizeContextsArgs&& args, FinalizeContextsResult& outResult) noexcept
{
    FinalizeContextsArgs unwrappedArgs{};
    unwrappedArgs.uploadContext = args.uploadContext;
    unwrappedArgs.contexts.reserve(args.contexts.size());

    struct ExtractedOpsEntry
    {
        sync::PipelineStage pipeline{ sync::PipelineStage::Graphics };
        validation::OperationsList ops{};
    };

    gp::Vector<ExtractedOpsEntry> extractedOps{};
    extractedOps.reserve(args.contexts.size());

    for (command::IComputeContext* const ctx: args.contexts)
    {
        if (ctx == nullptr)
        {
            continue;
        }

        if (ctx->getPipeline() == sync::PipelineStage::Graphics)
        {
            ValidationContext* const valCtx = static_cast<ValidationContext*>(ctx);
            extractedOps.emplaceBack(
                ExtractedOpsEntry{ sync::PipelineStage::Graphics, valCtx->getStateTracker().finalizeOps() }
            );
            valCtx->resetContextState();
            unwrappedArgs.contexts.emplaceBack(valCtx->getInnerContext());
        }
        else
        {
            ValidationComputeContext* const valCtx = static_cast<ValidationComputeContext*>(ctx);
            extractedOps.emplaceBack(
                ExtractedOpsEntry{ sync::PipelineStage::AsyncCompute, valCtx->getStateTracker().finalizeOps() }
            );
            valCtx->resetContextState();
            unwrappedArgs.contexts.emplaceBack(valCtx->getInnerContext());
        }
    }

    FinalizeContextsResult innerResult{};
    m_rhi->finalizeContexts(std::move(unwrappedArgs), innerResult);
    outResult.uploadCommandList = innerResult.uploadCommandList;
    outResult.commandLists.clear();

    if (innerResult.commandLists.isEmpty())
    {
        // Backends like NullDynamicRHI execute synchronously without producing IPlatformCommandList objects; replay
        // tracker operations immediately!
        for (ExtractedOpsEntry& entry: extractedOps)
        {
            validation::Tracker::replayOpQueue(entry.pipeline, std::move(entry.ops));
        }
        return;
    }

    outResult.commandLists.reserve(innerResult.commandLists.size());
    for (gp::USize idx = 0u; idx < innerResult.commandLists.size(); ++idx)
    {
        const sync::PipelineStage pipe =
            (idx < extractedOps.size()) ? extractedOps[idx].pipeline : sync::PipelineStage::Graphics;
        validation::OperationsList ops =
            (idx < extractedOps.size()) ? std::move(extractedOps[idx].ops) : validation::OperationsList{};
        outResult.commandLists.emplaceBack(
            new ValidationPlatformCommandList(innerResult.commandLists[idx], pipe, std::move(ops))
        );
    }
}

void ValidationRHI::submitCommandLists(SubmitCommandListsArgs&& args) noexcept
{
    SubmitCommandListsArgs innerArgs{};
    innerArgs.flags = args.flags;
    innerArgs.commandLists.reserve(args.commandLists.size());

    for (command::IPlatformCommandList* const cmdList: args.commandLists)
    {
        if (cmdList == nullptr)
        {
            continue;
        }

        ValidationPlatformCommandList* const wrapper = static_cast<ValidationPlatformCommandList*>(cmdList);
        validation::Tracker::replayOpQueue(wrapper->pipeline, std::move(wrapper->trackerOps));
        if (wrapper->innerCommandList != nullptr)
        {
#if GP_WITH_RHI_BREADCRUMBS
            wrapper->innerCommandList->breadcrumbAllocators.appendUnique(wrapper->breadcrumbAllocators);
            if (!wrapper->innerCommandList->breadcrumbRange)
            {
                wrapper->innerCommandList->breadcrumbRange = wrapper->breadcrumbRange;
            }
#endif
            innerArgs.commandLists.emplaceBack(wrapper->innerCommandList);
        }
        delete wrapper;
    }

    if (!innerArgs.commandLists.isEmpty())
    {
        m_rhi->submitCommandLists(std::move(innerArgs));
    }
}

void ValidationRHI::validateThreadGroupCount(
    const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
) noexcept
{
    if (threadGroupCountX == 0u || threadGroupCountY == 0u || threadGroupCountZ == 0u)
    {
        char msg[256]{};
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Dispatch thread group count (%u, %u, %u) has a zero dimension; all dimensions must be >= 1.",
            threadGroupCountX,
            threadGroupCountY,
            threadGroupCountZ
        );
        if (len > 0)
        {
            validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    const RHIGlobals& globals = getRHIGlobals();
    const gp::UInt32 maxPerDim =
        (globals.maxDispatchThreadGroupsPerDimension > 0u) ? globals.maxDispatchThreadGroupsPerDimension : 65535u;

    if (threadGroupCountX > maxPerDim || threadGroupCountY > maxPerDim || threadGroupCountZ > maxPerDim)
    {
        char msg[256]{};
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Dispatch thread group count (%u, %u, %u) exceeds device limits (%u, %u, %u).",
            threadGroupCountX,
            threadGroupCountY,
            threadGroupCountZ,
            maxPerDim,
            maxPerDim,
            maxPerDim
        );
        if (len > 0)
        {
            validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
}

void ValidationRHI::validateIndirectArgsBuffer(
    const resource::Buffer* const argumentBuffer,
    const gp::UInt32 argumentOffset,
    const gp::UInt32 argumentSizeInBytes,
    const gp::UInt32 minStrideInBytes
) noexcept
{
    static_cast<void>(minStrideInBytes);
    if (argumentBuffer == nullptr)
    {
        validation::reportValidationFailure("Indirect draw/dispatch called with a null argumentBuffer.");
        return;
    }

    if ((argumentBuffer->getUsage() & resource::BufferUsageFlags::DrawIndirect) == resource::BufferUsageFlags::None)
    {
        char msg[256]{};
        const gp::String bufName = argumentBuffer->getName().toString();
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Buffer '%s' used as indirect argument buffer without BufferUsageFlags::DrawIndirect.",
            bufName.cString()
        );
        if (len > 0)
        {
            validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }

    if ((argumentOffset % 4u) != 0u)
    {
        validation::reportValidationFailure("Indirect argumentOffset must be a multiple of 4 bytes.");
    }

    if ((static_cast<gp::UInt64>(argumentOffset) + static_cast<gp::UInt64>(argumentSizeInBytes)) >
        static_cast<gp::UInt64>(argumentBuffer->getSize()))
    {
        char msg[256]{};
        const gp::String bufName = argumentBuffer->getName().toString();
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "Indirect argument range [offset=%u, size=%u] exceeds Buffer '%s' size (%u).",
            argumentOffset,
            argumentSizeInBytes,
            bufName.cString(),
            argumentBuffer->getSize()
        );
        if (len > 0)
        {
            validation::reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
}

void ValidationRHI::validateDispatchIndirectArgsBuffer(
    const resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    validateIndirectArgsBuffer(
        argumentBuffer,
        argumentOffset,
        static_cast<gp::UInt32>(sizeof(command::DispatchIndirectParameters)),
        static_cast<gp::UInt32>(sizeof(command::DispatchIndirectParameters))
    );
}

}   // namespace gp::rhi
