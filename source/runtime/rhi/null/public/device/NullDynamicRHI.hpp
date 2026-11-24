// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "commands/CommandTypes.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/Name.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/DynamicRHI.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "maths/vector/Vector4.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/PipelineForward.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerStateDesc.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "presentation/PresentationForward.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "RHINull.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderParameters.hpp"
#include "shaders/ShaderTypes.hpp"   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include "threading/sync/Mutex.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewForward.hpp"

namespace gp::rhi::null
{

/// @brief Headless no-op `DynamicRHI` backend for dedicated servers, build tools, and RHI contract tests.
/// @details Implements `DynamicRHIPSOFallback` and `ICommandContextPSOFallback` on one object so `getDefaultContext()`
///          returns `this`. CPU lock and initializer writes share a single grow-only scratch buffer (`m_memoryBuffer`),
///          avoiding per-resource storage while still handing callers valid writable pointers.
class GP_RHI_NULL_API NullDynamicRHI final
    : public DynamicRHIPSOFallback
    , public command::ICommandContextPSOFallback
{
public:
    /// @brief Constructs an uninitialized `NullDynamicRHI` instance and sets default texture dimension limits.
    NullDynamicRHI() noexcept;

    /// @copydoc DynamicRHI::~DynamicRHI()
    virtual ~NullDynamicRHI() noexcept override;

public:
    /// @copydoc DynamicRHI::init()
    virtual void init() noexcept override;

    /// @copydoc DynamicRHI::shutdown()
    virtual void shutdown() noexcept override;

    /// @copydoc DynamicRHI::getName()
    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "Null";
    }

    /// @copydoc DynamicRHI::getInterfaceType()
    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept override
    {
        return InterfaceType::Null;
    }

public:
    /// @copydoc DynamicRHI::createSamplerState()
    [[nodiscard]] virtual pipeline::SamplerStateRef
        createSamplerState(const pipeline::SamplerStateDesc& initializer) noexcept override;

    /// @copydoc DynamicRHI::createRasterizerState()
    [[nodiscard]] virtual pipeline::RasterizerStateRef
        createRasterizerState(const pipeline::RasterizerStateDesc& initializer) noexcept override;

    /// @copydoc DynamicRHI::createDepthStencilState()
    [[nodiscard]] virtual pipeline::DepthStencilStateRef
        createDepthStencilState(const pipeline::DepthStencilStateDesc& initializer) noexcept override;

    /// @copydoc DynamicRHI::createBlendState()
    [[nodiscard]] virtual pipeline::BlendStateRef
        createBlendState(const pipeline::BlendStateDesc& initializer) noexcept override;

    /// @copydoc DynamicRHI::createVertexDeclaration()
    [[nodiscard]] virtual pipeline::VertexDeclarationRef
        createVertexDeclaration(const pipeline::VertexDeclarationElementList& elements) noexcept override;

public:
    /// @copydoc DynamicRHI::createVertexShader()
    [[nodiscard]] virtual shader::VertexShaderRef
        createVertexShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept override;

    /// @copydoc DynamicRHI::createPixelShader()
    [[nodiscard]] virtual shader::PixelShaderRef
        createPixelShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept override;

    /// @copydoc DynamicRHI::createGeometryShader()
    [[nodiscard]] virtual shader::GeometryShaderRef
        createGeometryShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept override;

    /// @copydoc DynamicRHI::createComputeShader()
    [[nodiscard]] virtual shader::ComputeShaderRef
        createComputeShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept override;

    /// @copydoc DynamicRHI::createMeshShader()
    [[nodiscard]] virtual shader::MeshShaderRef
        createMeshShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept override;

    /// @copydoc DynamicRHI::createAmplificationShader()
    [[nodiscard]] virtual shader::AmplificationShaderRef createAmplificationShader(
        gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash
    ) noexcept override;

    /// @copydoc DynamicRHI::createBoundShaderState(pipeline::VertexDeclaration*, shader::VertexShader*,
    ///          shader::PixelShader*, shader::GeometryShader*)
    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        pipeline::VertexDeclaration* vertexDeclaration,
        shader::VertexShader* vertexShader,
        shader::PixelShader* pixelShader,
        shader::GeometryShader* geometryShader = nullptr
    ) noexcept override;

    /// @copydoc DynamicRHI::createBoundShaderState(shader::AmplificationShader*, shader::MeshShader*,
    ///          shader::PixelShader*)
    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        shader::AmplificationShader* amplificationShader,
        shader::MeshShader* meshShader,
        shader::PixelShader* pixelShader
    ) noexcept override;

public:
    /// @copydoc DynamicRHI::createUniformBuffer()
    [[nodiscard]] virtual resource::UniformBufferRef createUniformBuffer(
        const void* contents,
        const resource::UniformBufferLayout* layout,
        resource::UniformBufferUsage usage,
        resource::UniformBufferValidation validation = resource::UniformBufferValidation::ValidateResources
    ) noexcept override;

    /// @copydoc DynamicRHI::updateUniformBuffer()
    virtual void updateUniformBuffer(
        command::CommandListBase& cmdList, resource::UniformBuffer* uniformBuffer, const void* contents
    ) noexcept override;

    /// @copydoc DynamicRHI::createBufferInitializer()
    [[nodiscard]] virtual resource::BufferInitializer createBufferInitializer(
        command::CommandListBase& cmdList, const resource::BufferCreateDesc& createDesc
    ) noexcept override;

    /// @copydoc DynamicRHI::lockBufferBottomOfPipe()
    [[nodiscard]] virtual void* lockBufferBottomOfPipe(
        command::CommandListBase& cmdList,
        resource::Buffer* buffer,
        gp::UInt32 offset,
        gp::UInt32 size,
        resource::ResourceLockMode lockMode
    ) noexcept override;

    /// @copydoc DynamicRHI::unlockBufferBottomOfPipe()
    virtual void
        unlockBufferBottomOfPipe(command::CommandListBase& cmdList, resource::Buffer* buffer) noexcept override;

public:
    /// @copydoc DynamicRHI::createTextureInitializer()
    [[nodiscard]] virtual resource::TextureInitializer createTextureInitializer(
        command::CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
    ) noexcept override;

    /// @copydoc DynamicRHI::calcTexturePlatformSize()
    /// @note The null backend allocates no GPU memory and returns zero size and alignment.
    [[nodiscard]] virtual resource::CalcTextureSizeResult
        calcTexturePlatformSize(const resource::TextureDesc& desc, gp::UInt32 firstMipIndex) noexcept override;

    /// @copydoc DynamicRHI::computeMemorySize()
    /// @note The null backend consumes zero GPU memory and returns `0`.
    [[nodiscard]] virtual gp::UInt64 computeMemorySize(resource::Texture* texture) noexcept override;

    /// @copydoc DynamicRHI::lockTexture()
    [[nodiscard]] virtual resource::TextureLockResult
        lockTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept override;

    /// @copydoc DynamicRHI::unlockTexture()
    virtual void
        unlockTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept override;

    /// @copydoc DynamicRHI::updateTexture2D()
    virtual void updateTexture2D(
        command::CommandListBase& cmdList,
        resource::Texture* texture,
        gp::UInt32 mipIndex,
        const resource::UpdateTextureRegion2D& updateRegion,
        gp::UInt32 sourcePitch,
        const gp::UInt8* sourceData
    ) noexcept override;

    /// @copydoc DynamicRHI::updateTexture3D()
    virtual void updateTexture3D(
        command::CommandListBase& cmdList,
        resource::Texture* texture,
        gp::UInt32 mipIndex,
        const resource::UpdateTextureRegion3D& updateRegion,
        gp::UInt32 sourceRowPitch,
        gp::UInt32 sourceDepthPitch,
        const gp::UInt8* sourceData
    ) noexcept override;

    using DynamicRHI::readSurfaceData;

    /// @copydoc DynamicRHI::readSurfaceData(resource::Texture*, const resource::IntRect&, gp::Vector<gp::math::Color>&,
    ///          const resource::ReadSurfaceDataFlags&)
    virtual void readSurfaceData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<gp::math::Color>& outData,
        const resource::ReadSurfaceDataFlags& flags
    ) noexcept override;

public:
    /// @copydoc DynamicRHI::createShaderResourceView(command::CommandListBase&, resource::Buffer*, const
    ///          view::ViewDesc&)
    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        command::CommandListBase& cmdList, resource::Buffer* buffer, const view::ViewDesc& desc
    ) noexcept override;

    /// @copydoc DynamicRHI::createShaderResourceView(command::CommandListBase&, resource::Texture*, const
    ///          view::ViewDesc&)
    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        command::CommandListBase& cmdList, resource::Texture* texture, const view::ViewDesc& desc
    ) noexcept override;

    /// @copydoc DynamicRHI::createUnorderedAccessView(command::CommandListBase&, resource::Buffer*, const
    ///          view::ViewDesc&)
    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase& cmdList, resource::Buffer* buffer, const view::ViewDesc& desc
    ) noexcept override;

    /// @copydoc DynamicRHI::createUnorderedAccessView(command::CommandListBase&, resource::Texture*, const
    ///          view::ViewDesc&)
    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase& cmdList, resource::Texture* texture, const view::ViewDesc& desc
    ) noexcept override;

public:
    /// @copydoc DynamicRHI::createGPUFence()
    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(gp::Name name) noexcept override;

    /// @copydoc DynamicRHI::createRenderQuery()
    [[nodiscard]] virtual sync::RenderQueryRef createRenderQuery(sync::RenderQueryType queryType) noexcept override;

    /// @copydoc DynamicRHI::getRenderQueryResult()
    [[nodiscard]] virtual bool getRenderQueryResult(
        sync::RenderQuery* renderQuery, gp::UInt64& outResult, bool wait, gp::UInt32 gpuIndex = 0u
    ) noexcept override;

    /// @copydoc DynamicRHI::createViewport()
    [[nodiscard]] virtual presentation::ViewportRef createViewport(
        void* windowHandle, gp::UInt32 sizeX, gp::UInt32 sizeY, bool isFullscreen, PixelFormat preferredPixelFormat
    ) noexcept override;

    /// @copydoc DynamicRHI::resizeViewport()
    virtual void resizeViewport(
        presentation::Viewport* viewport,
        gp::UInt32 sizeX,
        gp::UInt32 sizeY,
        bool isFullscreen,
        PixelFormat preferredPixelFormat = PixelFormat::Unknown
    ) noexcept override;

    /// @copydoc DynamicRHI::getViewportBackBuffer()
    [[nodiscard]] virtual resource::TextureRef
        getViewportBackBuffer(presentation::Viewport* viewport) noexcept override;

    /// @copydoc DynamicRHI::advanceFrameForGetViewportBackBuffer()
    virtual void advanceFrameForGetViewportBackBuffer(presentation::Viewport* viewport) noexcept override;

    /// @copydoc DynamicRHI::beginDrawingViewport()
    virtual void
        beginDrawingViewport(presentation::Viewport* viewport, resource::Texture* renderTarget) noexcept override;

    /// @copydoc DynamicRHI::endDrawingViewport()
    virtual void endDrawingViewport(presentation::Viewport* viewport, bool present, bool lockToVsync) noexcept override;

    /// @copydoc DynamicRHI::blockUntilGPUIdle()
    virtual void blockUntilGPUIdle() noexcept override;

    /// @copydoc DynamicRHI::getDefaultContext()
    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept override;

public:
    /// @copydoc command::IComputeContext::dispatchComputeShader()
    virtual void dispatchComputeShader(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept override;

    /// @copydoc command::IComputeContext::dispatchIndirectComputeShader()
    virtual void
        dispatchIndirectComputeShader(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept override;

    /// @copydoc command::IComputeContext::beginTransitions()
    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept override;

    /// @copydoc command::IComputeContext::endTransitions()
    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept override;

    /// @copydoc command::IComputeContext::clearUAVFloat()
    virtual void clearUAVFloat(
        view::UnorderedAccessView* unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
    ) noexcept override;

    /// @copydoc command::IComputeContext::clearUAVUint()
    virtual void clearUAVUint(
        view::UnorderedAccessView* unorderedAccessView, const resource::UIntVector4& values
    ) noexcept override;

    /// @copydoc command::IComputeContext::setShaderParameters()
    virtual void setShaderParameters(
        shader::ComputeShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept override;

    /// @copydoc command::IComputeContext::setShaderUnbinds()
    virtual void setShaderUnbinds(
        shader::ComputeShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept override;

    /// @copydoc command::IComputeContext::setStaticUniformBuffers()
    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept override;

    /// @copydoc command::IComputeContext::copyToStagingBuffer()
    virtual void copyToStagingBuffer(
        resource::Buffer* sourceBuffer,
        resource::StagingBuffer* destinationStagingBuffer,
        gp::UInt32 offset,
        gp::UInt32 numBytes
    ) noexcept override;

    /// @copydoc command::IComputeContext::writeGPUFence()
    virtual void writeGPUFence(sync::GPUFence* fence) noexcept override;

public:
    /// @copydoc command::ICommandContext::beginRenderQuery()
    virtual void beginRenderQuery(sync::RenderQuery* renderQuery) noexcept override;

    /// @copydoc command::ICommandContext::endRenderQuery()
    virtual void endRenderQuery(sync::RenderQuery* renderQuery) noexcept override;

    /// @copydoc command::ICommandContext::setStreamSource()
    virtual void
        setStreamSource(gp::UInt32 streamIndex, resource::Buffer* vertexBuffer, gp::UInt32 offset) noexcept override;

    /// @copydoc command::ICommandContext::setViewport()
    virtual void setViewport(
        gp::Float32 minX, gp::Float32 minY, gp::Float32 minZ, gp::Float32 maxX, gp::Float32 maxY, gp::Float32 maxZ
    ) noexcept override;

    /// @copydoc command::ICommandContext::setScissorRect()
    virtual void setScissorRect(
        bool enable, gp::UInt32 minX, gp::UInt32 minY, gp::UInt32 maxX, gp::UInt32 maxY
    ) noexcept override;

    /// @copydoc command::ICommandContext::setShaderParameters(shader::GraphicsShader*, ...)
    virtual void setShaderParameters(
        shader::GraphicsShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept override;

    /// @copydoc command::ICommandContext::setShaderUnbinds(shader::GraphicsShader*, ...)
    virtual void setShaderUnbinds(
        shader::GraphicsShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept override;

    /// @copydoc command::ICommandContext::drawPrimitive()
    virtual void
        drawPrimitive(gp::UInt32 baseVertexIndex, gp::UInt32 numPrimitives, gp::UInt32 numInstances) noexcept override;

    /// @copydoc command::ICommandContext::drawPrimitiveIndirect()
    virtual void drawPrimitiveIndirect(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept override;

    /// @copydoc command::ICommandContext::drawIndexedPrimitive()
    virtual void drawIndexedPrimitive(
        resource::Buffer* indexBuffer,
        gp::Int32 baseVertexIndex,
        gp::UInt32 firstInstance,
        gp::UInt32 numVertices,
        gp::UInt32 startIndex,
        gp::UInt32 numPrimitives,
        gp::UInt32 numInstances
    ) noexcept override;

    /// @copydoc command::ICommandContext::drawIndexedPrimitiveIndirect()
    virtual void drawIndexedPrimitiveIndirect(
        resource::Buffer* indexBuffer, resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset
    ) noexcept override;

    /// @copydoc command::ICommandContext::beginRenderPass()
    virtual void beginRenderPass(const command::RenderPassInfo& inInfo, const char* name) noexcept override;

    /// @copydoc command::ICommandContext::endRenderPass()
    virtual void endRenderPass() noexcept override;

    /// @copydoc command::ICommandContext::copyTexture()
    virtual void copyTexture(
        resource::Texture* sourceTexture, resource::Texture* destTexture, const command::CopyTextureInfo& copyInfo
    ) noexcept override;

    /// @copydoc command::ICommandContext::copyBufferRegion()
    virtual void copyBufferRegion(
        resource::Buffer* destBuffer,
        gp::UInt64 dstOffset,
        resource::Buffer* sourceBuffer,
        gp::UInt64 srcOffset,
        gp::UInt64 numBytes
    ) noexcept override;

public:
    /// @copydoc command::ICommandContextPSOFallback::setBoundShaderState()
    virtual void setBoundShaderState(pipeline::BoundShaderState* boundShaderState) noexcept override;

    /// @copydoc command::ICommandContextPSOFallback::setDepthStencilState()
    virtual void setDepthStencilState(pipeline::DepthStencilState* newState, gp::UInt32 stencilRef) noexcept override;

    /// @copydoc command::ICommandContextPSOFallback::setRasterizerState()
    virtual void setRasterizerState(pipeline::RasterizerState* newState) noexcept override;

    /// @copydoc command::ICommandContextPSOFallback::setBlendState()
    virtual void
        setBlendState(pipeline::BlendState* newState, const gp::math::LinearColor& blendFactor) noexcept override;

    /// @copydoc command::ICommandContextPSOFallback::enableDepthBoundsTest()
    virtual void enableDepthBoundsTest(bool enable, gp::Float32 minDepth, gp::Float32 maxDepth) noexcept override;

    /// @copydoc command::ICommandContextPSOFallback::setComputeShader()
    virtual void setComputeShader(shader::ComputeShader* computeShader) noexcept override;

public:
    /// @brief Returns a pointer into the grow-only shared scratch buffer guaranteed to hold at least `size` bytes.
    /// @param[in] size Minimum required byte capacity.
    /// @return Writable pointer to zero-initialized scratch memory.
    [[nodiscard]] gp::UInt8* getStaticBuffer(gp::UInt64 size) noexcept;

    /// @brief Computes row stride and byte count for a `(sizeX, sizeY)` surface in `format` and returns scratch memory.
    /// @param[in] sizeX Width in texels.
    /// @param[in] sizeY Height in texels.
    /// @param[in] format Texture pixel format.
    /// @param[out] outStride Receives computed row stride in bytes.
    /// @param[out] outByteCount Receives total byte count of the surface.
    /// @return Writable pointer into the shared scratch buffer.
    [[nodiscard]] gp::UInt8* getStaticTextureBuffer(
        gp::UInt32 sizeX, gp::UInt32 sizeY, PixelFormat format, gp::UInt32& outStride, gp::UInt64& outByteCount
    ) noexcept;

private:
    /// @brief Mutex synchronizing grow-only resizes of `m_memoryBuffer`.
    gp::thread::Mutex m_memoryBufferMutex{};

    /// @brief Single shared grow-only scratch buffer backing all null buffer/texture locks and initializers.
    gp::Vector<gp::UInt8> m_memoryBuffer{};
};

}   // namespace gp::rhi::null
