// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/NullDynamicRHI.hpp"
#include "device/DynamicRHI.hpp"   // IWYU pragma: keep
#include "device/GenericStagingBuffer.hpp"
#include "device/RHIGlobals.hpp"
#include "maths/base/Arithmetic.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/SamplerState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "presentation/Viewport.hpp"
#include "resources/NullBuffer.hpp"
#include "resources/NullTexture.hpp"
#include "resources/NullUniformBuffer.hpp"
#include "resources/TextureUtils.hpp"
#include "RHIDefinitions.hpp"
#include "shaders/AmplificationShader.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/MeshShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/VertexShader.hpp"
#include "sync/NullGPUFence.hpp"
#include "sync/RenderQuery.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include <utility>

namespace gp::rhi::null
{

NullDynamicRHI::NullDynamicRHI() noexcept
{
    RHIGlobals& globals = getRHIGlobals();
    globals.maxTextureDimensions = 16384u;
    globals.maxTextureMipCount = kMaxTextureMipCount;
}

NullDynamicRHI::~NullDynamicRHI() noexcept = default;

void NullDynamicRHI::init() noexcept
{
    RHIGlobals& globals = getRHIGlobals();
    globals.usingNullRHI = true;
    globals.maxTextureDimensions = 16384u;
    globals.maxVolumeTextureDimensions = 2048u;
    globals.maxCubeTextureDimensions = 16384u;
    globals.maxTextureArrayLayers = 2048u;
    globals.maxTextureMipCount = kMaxTextureMipCount;
    globals.maxSupportedFeatureLevel = FeatureLevel::SM5;
    globals.supportsTimestampQueries = false;
    globals.supportsOcclusionQueries = false;
    globals.gpuInfo.adapterName = "Null";
    globals.gpuInfo.vendorId = GpuVendorId::NotQueried;

    const PixelFormatCapabilities nullCaps = PixelFormatCapabilities::AllTextureFlags |
                                             PixelFormatCapabilities::AllBufferFlags |
                                             PixelFormatCapabilities::AllUAVFlags;
    for (gp::USize formatIndex = 1u; formatIndex < kPixelFormatCount; ++formatIndex)
    {
        globals.setPixelFormatCapabilities(static_cast<PixelFormat>(formatIndex), nullCaps);
    }

    const gp::USize maxVertexType = pipeline::kVertexElementTypeCount;
    for (gp::USize typeIndex = 1u; typeIndex < maxVertexType; ++typeIndex)
    {
        globals.vertexElementTypeSupport.setSupported(static_cast<pipeline::VertexElementType>(typeIndex), true);
    }
}

void NullDynamicRHI::shutdown() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_memoryBufferMutex);
    m_memoryBuffer.clear();
    m_memoryBuffer.shrinkToFit();
}

pipeline::SamplerStateRef NullDynamicRHI::createSamplerState(const pipeline::SamplerStateDesc& initializer) noexcept
{
    static_cast<void>(initializer);
    return pipeline::SamplerStateRef(new pipeline::SamplerState());
}

pipeline::RasterizerStateRef
    NullDynamicRHI::createRasterizerState(const pipeline::RasterizerStateDesc& initializer) noexcept
{
    static_cast<void>(initializer);
    return pipeline::RasterizerStateRef(new pipeline::RasterizerState());
}

pipeline::DepthStencilStateRef
    NullDynamicRHI::createDepthStencilState(const pipeline::DepthStencilStateDesc& initializer) noexcept
{
    static_cast<void>(initializer);
    return pipeline::DepthStencilStateRef(new pipeline::DepthStencilState());
}

pipeline::BlendStateRef NullDynamicRHI::createBlendState(const pipeline::BlendStateDesc& initializer) noexcept
{
    static_cast<void>(initializer);
    return pipeline::BlendStateRef(new pipeline::BlendState());
}

pipeline::VertexDeclarationRef
    NullDynamicRHI::createVertexDeclaration(const pipeline::VertexDeclarationElementList& elements) noexcept
{
    static_cast<void>(elements);
    return pipeline::VertexDeclarationRef(new pipeline::VertexDeclaration());
}

shader::VertexShaderRef NullDynamicRHI::createVertexShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    static_cast<void>(code);
    shader::VertexShaderRef shader(new shader::VertexShader());
    shader->setHash(hash);
    return shader;
}

shader::PixelShaderRef NullDynamicRHI::createPixelShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    static_cast<void>(code);
    shader::PixelShaderRef shader(new shader::PixelShader());
    shader->setHash(hash);
    return shader;
}

shader::GeometryShaderRef NullDynamicRHI::createGeometryShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    static_cast<void>(code);
    shader::GeometryShaderRef shader(new shader::GeometryShader());
    shader->setHash(hash);
    return shader;
}

shader::ComputeShaderRef NullDynamicRHI::createComputeShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    static_cast<void>(code);
    shader::ComputeShaderRef shader(new shader::ComputeShader());
    shader->setHash(hash);
    return shader;
}

shader::MeshShaderRef
    NullDynamicRHI::createMeshShader(const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept
{
    static_cast<void>(code);
    shader::MeshShaderRef shader(new shader::MeshShader());
    shader->setHash(hash);
    return shader;
}

shader::AmplificationShaderRef NullDynamicRHI::createAmplificationShader(
    const gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
) noexcept
{
    static_cast<void>(code);
    shader::AmplificationShaderRef shader(new shader::AmplificationShader());
    shader->setHash(hash);
    return shader;
}

pipeline::BoundShaderStateRef NullDynamicRHI::createBoundShaderState(
    pipeline::VertexDeclaration* const vertexDeclaration,
    shader::VertexShader* const vertexShader,
    shader::PixelShader* const pixelShader,
    shader::GeometryShader* const geometryShader
) noexcept
{
    static_cast<void>(vertexDeclaration);
    static_cast<void>(vertexShader);
    static_cast<void>(pixelShader);
    static_cast<void>(geometryShader);
    return pipeline::BoundShaderStateRef(new pipeline::BoundShaderState());
}

pipeline::BoundShaderStateRef NullDynamicRHI::createBoundShaderState(
    shader::AmplificationShader* const amplificationShader,
    shader::MeshShader* const meshShader,
    shader::PixelShader* const pixelShader
) noexcept
{
    static_cast<void>(amplificationShader);
    static_cast<void>(meshShader);
    static_cast<void>(pixelShader);
    return pipeline::BoundShaderStateRef(new pipeline::BoundShaderState());
}

resource::UniformBufferRef NullDynamicRHI::createUniformBuffer(
    const void* const contents,
    const resource::UniformBufferLayout* const layout,
    const resource::UniformBufferUsage usage,
    const resource::UniformBufferValidation validation
) noexcept
{
    static_cast<void>(contents);
    static_cast<void>(usage);
    static_cast<void>(validation);
    return resource::UniformBufferRef(new NullUniformBuffer(layout));
}

void NullDynamicRHI::updateUniformBuffer(
    command::CommandListBase& cmdList, resource::UniformBuffer* const uniformBuffer, const void* const contents
) noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(uniformBuffer);
    static_cast<void>(contents);
}

resource::BufferInitializer NullDynamicRHI::createBufferInitializer(
    command::CommandListBase& cmdList, const resource::BufferCreateDesc& createDesc
) noexcept
{
    resource::BufferRef buffer(new NullBuffer(createDesc));

    if (createDesc.initAction == resource::BufferInitAction::ResourceArray && createDesc.initialData != nullptr)
    {
        createDesc.initialData->discard();
    }

    void* writableData = nullptr;
    gp::UInt64 writableSize = 0u;
    if (createDesc.initAction == resource::BufferInitAction::Initializer)
    {
        writableData = getStaticBuffer(createDesc.size);
        writableSize = createDesc.size;
    }

    return resource::BufferInitializer(
        cmdList, std::move(buffer), writableData, writableSize, resource::BufferInitializer::FinalizeCallback{}
    );
}

void* NullDynamicRHI::lockBufferBottomOfPipe(
    command::CommandListBase& cmdList,
    resource::Buffer* const buffer,
    const gp::UInt32 offset,
    const gp::UInt32 size,
    const resource::ResourceLockMode lockMode
) noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(lockMode);
    const gp::UInt64 requestedEnd = static_cast<gp::UInt64>(offset) + static_cast<gp::UInt64>(size);
    const gp::UInt64 requiredSize =
        (buffer != nullptr) ? gp::math::max<gp::UInt64>(buffer->getSize(), requestedEnd) : requestedEnd;
    return getStaticBuffer(requiredSize);
}

void NullDynamicRHI::unlockBufferBottomOfPipe(
    command::CommandListBase& cmdList, resource::Buffer* const buffer
) noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(buffer);
}

resource::TextureInitializer NullDynamicRHI::createTextureInitializer(
    command::CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
) noexcept
{
    resource::TextureRef texture(new NullTexture(createDesc));

    if (createDesc.initAction == resource::TextureInitAction::BulkData && createDesc.bulkData != nullptr)
    {
        createDesc.bulkData->discard();
    }

    resource::TextureInitializer::SubresourceCallback subresourceCallback{};
    if (createDesc.initAction == resource::TextureInitAction::Initializer)
    {
        const resource::TextureDesc desc = static_cast<const resource::TextureDesc&>(createDesc);
        const gp::UInt64 totalBytes = resource::calculateTextureSize(desc);
        static_cast<void>(getStaticBuffer(totalBytes));

        subresourceCallback = [this, desc](
                                  const resource::TextureSubresourceIndex index
                              ) noexcept -> resource::TextureSubresourceInitializer
        {
            if (index.faceIndex < 0 || index.arrayIndex < 0 || index.mipIndex < 0)
            {
                return resource::TextureSubresourceInitializer{};
            }
            gp::UInt64 rowStride = 0u;
            gp::UInt64 subresourceSize = 0u;
            const gp::UInt64 offset = resource::calculateSubresourceOffset(
                desc,
                static_cast<gp::UInt32>(index.faceIndex),
                static_cast<gp::UInt32>(index.arrayIndex),
                static_cast<gp::UInt32>(index.mipIndex),
                rowStride,
                subresourceSize
            );
            if (subresourceSize == 0u)
            {
                return resource::TextureSubresourceInitializer{};
            }

            gp::UInt8* const basePtr = getStaticBuffer(offset + subresourceSize);
            return resource::TextureSubresourceInitializer{ basePtr + offset, rowStride, subresourceSize };
        };
    }

    return resource::TextureInitializer(
        cmdList, std::move(texture), std::move(subresourceCallback), resource::TextureInitializer::FinalizeCallback{}
    );
}

resource::CalcTextureSizeResult
    NullDynamicRHI::calcTexturePlatformSize(const resource::TextureDesc& desc, const gp::UInt32 firstMipIndex) noexcept
{
    static_cast<void>(desc);
    static_cast<void>(firstMipIndex);
    return resource::CalcTextureSizeResult{};
}

gp::UInt64 NullDynamicRHI::computeMemorySize(resource::Texture* const texture) noexcept
{
    static_cast<void>(texture);
    return 0u;
}

resource::TextureLockResult
    NullDynamicRHI::lockTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept
{
    static_cast<void>(cmdList);
    if (args.texture == nullptr)
    {
        return resource::TextureLockResult{};
    }

    const resource::TextureDesc& desc = args.texture->getDesc();
    const gp::UInt32 clampedMip = (args.mipIndex < 32u) ? args.mipIndex : 31u;
    const gp::UInt32 mipWidth = gp::math::max<gp::UInt32>(1u, static_cast<gp::UInt32>(desc.extent.x) >> clampedMip);
    const gp::UInt32 mipHeight = gp::math::max<gp::UInt32>(1u, static_cast<gp::UInt32>(desc.extent.y) >> clampedMip);

    gp::UInt32 stride = 0u;
    gp::UInt64 byteCount = 0u;
    gp::UInt8* const data = getStaticTextureBuffer(mipWidth, mipHeight, desc.format, stride, byteCount);

    return resource::TextureLockResult{ data, byteCount, stride };
}

void NullDynamicRHI::unlockTexture(
    command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args
) noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(args);
}

void NullDynamicRHI::updateTexture2D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion2D& updateRegion,
    const gp::UInt32 sourcePitch,
    const gp::UInt8* const sourceData
) noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(texture);
    static_cast<void>(mipIndex);
    static_cast<void>(updateRegion);
    static_cast<void>(sourcePitch);
    static_cast<void>(sourceData);
}

void NullDynamicRHI::updateTexture3D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion3D& updateRegion,
    const gp::UInt32 sourceRowPitch,
    const gp::UInt32 sourceDepthPitch,
    const gp::UInt8* const sourceData
) noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(texture);
    static_cast<void>(mipIndex);
    static_cast<void>(updateRegion);
    static_cast<void>(sourceRowPitch);
    static_cast<void>(sourceDepthPitch);
    static_cast<void>(sourceData);
}

void NullDynamicRHI::readSurfaceData(
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::Color>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    static_cast<void>(texture);
    static_cast<void>(flags);

    const gp::Int32 width = rect.width();
    const gp::Int32 height = rect.height();
    if (width <= 0 || height <= 0)
    {
        return;
    }

    const gp::USize pixelCount = static_cast<gp::USize>(width) * static_cast<gp::USize>(height);
    const gp::USize oldSize = outData.size();
    outData.resize(oldSize + pixelCount, gp::math::Color(0u, 0u, 0u, 0u));
}

view::ShaderResourceViewRef NullDynamicRHI::createShaderResourceView(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const view::ViewDesc& desc
) noexcept
{
    static_cast<void>(cmdList);
    return view::ShaderResourceViewRef(new view::ShaderResourceView(buffer, desc));
}

view::ShaderResourceViewRef NullDynamicRHI::createShaderResourceView(
    command::CommandListBase& cmdList, resource::Texture* const texture, const view::ViewDesc& desc
) noexcept
{
    static_cast<void>(cmdList);
    return view::ShaderResourceViewRef(new view::ShaderResourceView(texture, desc));
}

view::UnorderedAccessViewRef NullDynamicRHI::createUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const view::ViewDesc& desc
) noexcept
{
    static_cast<void>(cmdList);
    return view::UnorderedAccessViewRef(new view::UnorderedAccessView(buffer, desc));
}

view::UnorderedAccessViewRef NullDynamicRHI::createUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Texture* const texture, const view::ViewDesc& desc
) noexcept
{
    static_cast<void>(cmdList);
    return view::UnorderedAccessViewRef(new view::UnorderedAccessView(texture, desc));
}

sync::GPUFenceRef NullDynamicRHI::createGPUFence(const gp::Name name) noexcept
{
    return sync::GPUFenceRef(new NullGPUFence(name));
}

sync::RenderQueryRef NullDynamicRHI::createRenderQuery(const sync::RenderQueryType queryType) noexcept
{
    static_cast<void>(queryType);
    return sync::RenderQueryRef(new sync::RenderQuery());
}

bool NullDynamicRHI::getRenderQueryResult(
    sync::RenderQuery* const renderQuery, gp::UInt64& outResult, const bool wait, const gp::UInt32 gpuIndex
) noexcept
{
    static_cast<void>(renderQuery);
    static_cast<void>(outResult);
    static_cast<void>(wait);
    static_cast<void>(gpuIndex);
    return true;
}

presentation::ViewportRef NullDynamicRHI::createViewport(
    void* const windowHandle,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    static_cast<void>(windowHandle);
    static_cast<void>(sizeX);
    static_cast<void>(sizeY);
    static_cast<void>(isFullscreen);
    static_cast<void>(preferredPixelFormat);
    return presentation::ViewportRef(new presentation::Viewport());
}

void NullDynamicRHI::resizeViewport(
    presentation::Viewport* const viewport,
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const bool isFullscreen,
    const PixelFormat preferredPixelFormat
) noexcept
{
    static_cast<void>(viewport);
    static_cast<void>(sizeX);
    static_cast<void>(sizeY);
    static_cast<void>(isFullscreen);
    static_cast<void>(preferredPixelFormat);
}

resource::TextureRef NullDynamicRHI::getViewportBackBuffer(presentation::Viewport* const viewport) noexcept
{
    static_cast<void>(viewport);
    const resource::TextureCreateDesc desc = resource::TextureCreateDesc::create2D("NullBackBuffer")
                                                 .setExtent(1, 1)
                                                 .setFormat(PixelFormat::B8G8R8A8_UNorm)
                                                 .addFlags(resource::TextureCreateFlags::RenderTargetable)
                                                 .setInitialState(sync::ResourceAccess::Present);
    return resource::TextureRef(new NullTexture(desc));
}

void NullDynamicRHI::advanceFrameForGetViewportBackBuffer(presentation::Viewport* const viewport) noexcept
{
    static_cast<void>(viewport);
}

void NullDynamicRHI::beginDrawingViewport(
    presentation::Viewport* const viewport, resource::Texture* const renderTarget
) noexcept
{
    static_cast<void>(viewport);
    static_cast<void>(renderTarget);
}

void NullDynamicRHI::endDrawingViewport(
    presentation::Viewport* const viewport, const presentation::PresentArgsWithStats& presentArgs
) noexcept
{
    static_cast<void>(viewport);
    static_cast<void>(presentArgs);
}

void NullDynamicRHI::blockUntilGPUIdle() noexcept
{}

command::ICommandContext* NullDynamicRHI::getDefaultContext() noexcept
{
    return this;
}

void NullDynamicRHI::dispatchComputeShader(
    const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
) noexcept
{
    static_cast<void>(threadGroupCountX);
    static_cast<void>(threadGroupCountY);
    static_cast<void>(threadGroupCountZ);
}

void NullDynamicRHI::dispatchIndirectComputeShader(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    static_cast<void>(argumentBuffer);
    static_cast<void>(argumentOffset);
}

void NullDynamicRHI::beginTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    static_cast<void>(transitions);
}

void NullDynamicRHI::endTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    static_cast<void>(transitions);
}

void NullDynamicRHI::clearUAVFloat(
    view::UnorderedAccessView* const unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
) noexcept
{
    static_cast<void>(unorderedAccessView);
    static_cast<void>(values);
}

void NullDynamicRHI::clearUAVUint(
    view::UnorderedAccessView* const unorderedAccessView, const resource::UIntVector4& values
) noexcept
{
    static_cast<void>(unorderedAccessView);
    static_cast<void>(values);
}

void NullDynamicRHI::setShaderParameters(
    shader::ComputeShader* const shader,
    const gp::ConstVectorView<gp::UInt8> inParametersData,
    const gp::ConstVectorView<shader::ShaderParameter> inParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
) noexcept
{
    static_cast<void>(shader);
    static_cast<void>(inParametersData);
    static_cast<void>(inParameters);
    static_cast<void>(inResourceParameters);
    static_cast<void>(inBindlessParameters);
}

void NullDynamicRHI::setShaderUnbinds(
    shader::ComputeShader* const shader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
) noexcept
{
    static_cast<void>(shader);
    static_cast<void>(inUnbinds);
}

void NullDynamicRHI::setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept
{
    static_cast<void>(uniformBuffers);
}

void NullDynamicRHI::copyToStagingBuffer(
    resource::Buffer* const sourceBuffer,
    resource::StagingBuffer* const destinationStagingBuffer,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    static_cast<void>(sourceBuffer);
    static_cast<void>(offset);
    if (destinationStagingBuffer != nullptr)
    {
        static_cast<GenericStagingBuffer*>(destinationStagingBuffer)->writeShadowData(nullptr, numBytes);
    }
}

void NullDynamicRHI::writeGPUFence(sync::GPUFence* const fence) noexcept
{
    static_cast<void>(fence);
}

void NullDynamicRHI::beginRenderQuery(sync::RenderQuery* const renderQuery) noexcept
{
    static_cast<void>(renderQuery);
}

void NullDynamicRHI::endRenderQuery(sync::RenderQuery* const renderQuery) noexcept
{
    static_cast<void>(renderQuery);
}

void NullDynamicRHI::setStreamSource(
    const gp::UInt32 streamIndex, resource::Buffer* const vertexBuffer, const gp::UInt32 offset
) noexcept
{
    static_cast<void>(streamIndex);
    static_cast<void>(vertexBuffer);
    static_cast<void>(offset);
}

void NullDynamicRHI::setViewport(
    const gp::Float32 minX,
    const gp::Float32 minY,
    const gp::Float32 minZ,
    const gp::Float32 maxX,
    const gp::Float32 maxY,
    const gp::Float32 maxZ
) noexcept
{
    static_cast<void>(minX);
    static_cast<void>(minY);
    static_cast<void>(minZ);
    static_cast<void>(maxX);
    static_cast<void>(maxY);
    static_cast<void>(maxZ);
}

void NullDynamicRHI::setScissorRect(
    const bool enable, const gp::UInt32 minX, const gp::UInt32 minY, const gp::UInt32 maxX, const gp::UInt32 maxY
) noexcept
{
    static_cast<void>(enable);
    static_cast<void>(minX);
    static_cast<void>(minY);
    static_cast<void>(maxX);
    static_cast<void>(maxY);
}

void NullDynamicRHI::setShaderParameters(
    shader::GraphicsShader* const shader,
    const gp::ConstVectorView<gp::UInt8> inParametersData,
    const gp::ConstVectorView<shader::ShaderParameter> inParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
) noexcept
{
    static_cast<void>(shader);
    static_cast<void>(inParametersData);
    static_cast<void>(inParameters);
    static_cast<void>(inResourceParameters);
    static_cast<void>(inBindlessParameters);
}

void NullDynamicRHI::setShaderUnbinds(
    shader::GraphicsShader* const shader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
) noexcept
{
    static_cast<void>(shader);
    static_cast<void>(inUnbinds);
}

void NullDynamicRHI::drawPrimitive(
    const gp::UInt32 baseVertexIndex, const gp::UInt32 numPrimitives, const gp::UInt32 numInstances
) noexcept
{
    static_cast<void>(baseVertexIndex);
    static_cast<void>(numPrimitives);
    static_cast<void>(numInstances);
}

void NullDynamicRHI::drawPrimitiveIndirect(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    static_cast<void>(argumentBuffer);
    static_cast<void>(argumentOffset);
}

void NullDynamicRHI::drawIndexedPrimitive(
    resource::Buffer* const indexBuffer,
    const gp::Int32 baseVertexIndex,
    const gp::UInt32 firstInstance,
    const gp::UInt32 numVertices,
    const gp::UInt32 startIndex,
    const gp::UInt32 numPrimitives,
    const gp::UInt32 numInstances
) noexcept
{
    static_cast<void>(indexBuffer);
    static_cast<void>(baseVertexIndex);
    static_cast<void>(firstInstance);
    static_cast<void>(numVertices);
    static_cast<void>(startIndex);
    static_cast<void>(numPrimitives);
    static_cast<void>(numInstances);
}

void NullDynamicRHI::drawIndexedPrimitiveIndirect(
    resource::Buffer* const indexBuffer, resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    static_cast<void>(indexBuffer);
    static_cast<void>(argumentBuffer);
    static_cast<void>(argumentOffset);
}

void NullDynamicRHI::beginRenderPass(const command::RenderPassInfo& inInfo, const char* const name) noexcept
{
    static_cast<void>(name);
    m_renderPassInfo = inInfo;
}

void NullDynamicRHI::endRenderPass() noexcept
{
    m_renderPassInfo = command::RenderPassInfo{};
}

void NullDynamicRHI::copyTexture(
    resource::Texture* const sourceTexture,
    resource::Texture* const destTexture,
    const command::CopyTextureInfo& copyInfo
) noexcept
{
    static_cast<void>(sourceTexture);
    static_cast<void>(destTexture);
    static_cast<void>(copyInfo);
}

void NullDynamicRHI::copyBufferRegion(
    resource::Buffer* const destBuffer,
    const gp::UInt64 dstOffset,
    resource::Buffer* const sourceBuffer,
    const gp::UInt64 srcOffset,
    const gp::UInt64 numBytes
) noexcept
{
    static_cast<void>(destBuffer);
    static_cast<void>(dstOffset);
    static_cast<void>(sourceBuffer);
    static_cast<void>(srcOffset);
    static_cast<void>(numBytes);
}

void NullDynamicRHI::setBoundShaderState(pipeline::BoundShaderState* const boundShaderState) noexcept
{
    static_cast<void>(boundShaderState);
}

void NullDynamicRHI::setDepthStencilState(
    pipeline::DepthStencilState* const newState, const gp::UInt32 stencilRef
) noexcept
{
    static_cast<void>(newState);
    static_cast<void>(stencilRef);
}

void NullDynamicRHI::setRasterizerState(pipeline::RasterizerState* const newState) noexcept
{
    static_cast<void>(newState);
}

void NullDynamicRHI::setBlendState(
    pipeline::BlendState* const newState, const gp::math::LinearColor& blendFactor
) noexcept
{
    static_cast<void>(newState);
    static_cast<void>(blendFactor);
}

void NullDynamicRHI::enableDepthBoundsTest(
    const bool enable, const gp::Float32 minDepth, const gp::Float32 maxDepth
) noexcept
{
    static_cast<void>(enable);
    static_cast<void>(minDepth);
    static_cast<void>(maxDepth);
}

void NullDynamicRHI::setComputeShader(shader::ComputeShader* const computeShader) noexcept
{
    static_cast<void>(computeShader);
}

gp::UInt8* NullDynamicRHI::getStaticBuffer(const gp::UInt64 size) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_memoryBufferMutex);
    const gp::USize requiredSize = static_cast<gp::USize>(gp::math::max<gp::UInt64>(size, 1u));
    if (m_memoryBuffer.size() < requiredSize)
    {
        m_memoryBuffer.resize(requiredSize, 0u);
    }
    return m_memoryBuffer.data();
}

gp::UInt8* NullDynamicRHI::getStaticTextureBuffer(
    const gp::UInt32 sizeX,
    const gp::UInt32 sizeY,
    const PixelFormat format,
    gp::UInt32& outStride,
    gp::UInt64& outByteCount
) noexcept
{
    outStride = 0u;
    outByteCount = 0u;

    if (isValidPixelFormat(format) && sizeX > 0u && sizeY > 0u)
    {
        const PixelFormatInfo& info = getPixelFormatInfo(format);
        if (info.blockBytes > 0u && info.blockSizeX > 0u && info.blockSizeY > 0u)
        {
            const gp::UInt64 blocksX =
                (static_cast<gp::UInt64>(sizeX) + static_cast<gp::UInt64>(info.blockSizeX) - 1u) / info.blockSizeX;
            const gp::UInt64 blocksY =
                (static_cast<gp::UInt64>(sizeY) + static_cast<gp::UInt64>(info.blockSizeY) - 1u) / info.blockSizeY;
            outStride = static_cast<gp::UInt32>(blocksX * static_cast<gp::UInt64>(info.blockBytes));
            outByteCount = static_cast<gp::UInt64>(outStride) * blocksY;
        }
    }

    return getStaticBuffer(outByteCount);
}

}   // namespace gp::rhi::null
