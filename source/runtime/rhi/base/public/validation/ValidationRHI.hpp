// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DynamicRHI.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "RHIBase.hpp"
#include "threading/sync/Mutex.hpp"
#include "validation/ValidationContext.hpp"
#include "validation/ValidationForward.hpp"   // IWYU pragma: keep

namespace gp::rhi
{

/// @brief Backend-agnostic `DynamicRHI` decorator validating barriers, render passes, shader parameters, PSOs,
///        indirect arguments, uniform buffer lifetimes, and transient resource aliasing.
/// @details Wraps any concrete `DynamicRHI` (`rhi/null`, `rhi/vulkan`, `rhi/d3d12`, `rhi/metal`, etc.) without native
///          graphics API dependencies. Takes ownership of `inRHI` so `rhiExit()` tears down both the validation layer
///          and the underlying backend cleanly.
class GP_RHI_API ValidationRHI final : public DynamicRHI
{
    friend class ValidationComputeContext;
    friend class ValidationContext;

public:
    /// @brief Constructs a validation wrapper taking ownership of `inRHI`.
    /// @param[in] inRHI Non-null concrete backend `DynamicRHI` to wrap.
    explicit ValidationRHI(DynamicRHI* inRHI) noexcept;

    /// @copydoc DynamicRHI::~DynamicRHI()
    virtual ~ValidationRHI() noexcept override;

public:
    /// @copydoc DynamicRHI::init()
    virtual void init() noexcept override;

    /// @copydoc DynamicRHI::postInit()
    virtual void postInit() noexcept override;

    /// @copydoc DynamicRHI::shutdown()
    virtual void shutdown() noexcept override;

    /// @copydoc DynamicRHI::getName()
    [[nodiscard]] virtual const char* getName() const noexcept override;

    /// @copydoc DynamicRHI::getInterfaceType()
    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept override;

    /// @copydoc DynamicRHI::getNonValidationRHI()
    [[nodiscard]] virtual DynamicRHI* getNonValidationRHI() noexcept override
    {
        return m_rhi;
    }

    /// @copydoc DynamicRHI::getNativeDevice()
    [[nodiscard]] virtual void* getNativeDevice() noexcept override;

    /// @copydoc DynamicRHI::tick()
    virtual void tick(gp::Float32 deltaTime) noexcept override;

    /// @copydoc DynamicRHI::processDeleteQueue()
    virtual void processDeleteQueue() noexcept override;

    /// @copydoc DynamicRHI::flushPendingLogs()
    virtual void flushPendingLogs() noexcept override;

    /// @copydoc DynamicRHI::acquireThreadOwnership()
    virtual void acquireThreadOwnership() noexcept override;

    /// @copydoc DynamicRHI::releaseThreadOwnership()
    virtual void releaseThreadOwnership() noexcept override;

    /// @copydoc DynamicRHI::suspendRendering()
    virtual void suspendRendering() noexcept override;

    /// @copydoc DynamicRHI::resumeRendering()
    virtual void resumeRendering() noexcept override;

    /// @copydoc DynamicRHI::isRenderingSuspended()
    [[nodiscard]] virtual bool isRenderingSuspended() const noexcept override;

    /// @copydoc DynamicRHI::getMinimumAlignmentForBufferBackedSRV()
    [[nodiscard]] virtual gp::UInt64 getMinimumAlignmentForBufferBackedSRV(PixelFormat format) noexcept override;

    /// @copydoc DynamicRHI::getPlatformTextureMaxSampleCount()
    [[nodiscard]] virtual gp::UInt32 getPlatformTextureMaxSampleCount() const noexcept override;

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
    /// shader::PixelShader*, shader::GeometryShader*)
    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        pipeline::VertexDeclaration* vertexDeclaration,
        shader::VertexShader* vertexShader,
        shader::PixelShader* pixelShader,
        shader::GeometryShader* geometryShader = nullptr
    ) noexcept override;

    /// @copydoc DynamicRHI::createBoundShaderState(shader::AmplificationShader*, shader::MeshShader*,
    /// shader::PixelShader*)
    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        shader::AmplificationShader* amplificationShader,
        shader::MeshShader* meshShader,
        shader::PixelShader* pixelShader
    ) noexcept override;

    /// @copydoc DynamicRHI::createGraphicsPipelineState()
    [[nodiscard]] virtual pipeline::GraphicsPipelineStateRef
        createGraphicsPipelineState(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept override;

    /// @copydoc DynamicRHI::createComputePipelineState(shader::ComputeShader*)
    [[nodiscard]] virtual pipeline::ComputePipelineStateRef
        createComputePipelineState(shader::ComputeShader* computeShader) noexcept override;

    /// @copydoc DynamicRHI::createComputePipelineState(const pipeline::ComputePipelineStateInitializer&)
    [[nodiscard]] virtual pipeline::ComputePipelineStateRef
        createComputePipelineState(const pipeline::ComputePipelineStateInitializer& initializer) noexcept override;

    /// @copydoc DynamicRHI::computeStatePrecachePSOHash()
    [[nodiscard]] virtual gp::UInt64
        computeStatePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept override;

    /// @copydoc DynamicRHI::computePrecachePSOHash()
    [[nodiscard]] virtual gp::UInt64
        computePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept override;

    /// @copydoc DynamicRHI::matchPrecachePSOInitializers()
    [[nodiscard]] virtual bool matchPrecachePSOInitializers(
        const pipeline::GraphicsPipelineStateInitializer& lhs, const pipeline::GraphicsPipelineStateInitializer& rhs
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

    /// @copydoc DynamicRHI::lockBuffer()
    [[nodiscard]] virtual void* lockBuffer(
        command::CommandListBase& cmdList,
        resource::Buffer* buffer,
        gp::UInt32 offset,
        gp::UInt32 size,
        resource::ResourceLockMode lockMode
    ) noexcept override;

    /// @copydoc DynamicRHI::unlockBuffer()
    virtual void unlockBuffer(command::CommandListBase& cmdList, resource::Buffer* buffer) noexcept override;

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

    /// @copydoc DynamicRHI::replaceResources()
    virtual void replaceResources(
        command::CommandListBase& cmdList, gp::ConstVectorView<resource::ResourceReplaceInfo> infos
    ) noexcept override;

public:
    /// @copydoc DynamicRHI::createTextureInitializer()
    [[nodiscard]] virtual resource::TextureInitializer createTextureInitializer(
        command::CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
    ) noexcept override;

    /// @copydoc DynamicRHI::createTextureReference()
    [[nodiscard]] virtual resource::TextureReferenceRef createTextureReference(
        command::CommandListBase& cmdList, resource::Texture* referencedTexture
    ) noexcept override;

    /// @copydoc DynamicRHI::updateTextureReference()
    virtual void updateTextureReference(
        command::CommandListBase& cmdList, resource::TextureReference* textureReference, resource::Texture* newTexture
    ) noexcept override;

    /// @copydoc DynamicRHI::calcTexturePlatformSize()
    [[nodiscard]] virtual resource::CalcTextureSizeResult
        calcTexturePlatformSize(const resource::TextureDesc& desc, gp::UInt32 firstMipIndex = 0u) noexcept override;

    /// @copydoc DynamicRHI::computeMemorySize()
    [[nodiscard]] virtual gp::UInt64 computeMemorySize(resource::Texture* texture) noexcept override;

    /// @copydoc DynamicRHI::getTextureMemoryStats()
    virtual void getTextureMemoryStats(resource::TextureMemoryStats& outStats) noexcept override;

    /// @copydoc DynamicRHI::getMemoryStats()
    virtual void getMemoryStats(profiling::RHIMemoryStats& outStats) noexcept override;

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

    /// @copydoc DynamicRHI::beginUpdateTexture3D()
    [[nodiscard]] virtual resource::UpdateTexture3DData beginUpdateTexture3D(
        command::CommandListBase& cmdList,
        resource::Texture* texture,
        gp::UInt32 mipIndex,
        const resource::UpdateTextureRegion3D& updateRegion
    ) noexcept override;

    /// @copydoc DynamicRHI::endUpdateTexture3D()
    virtual void endUpdateTexture3D(
        command::CommandListBase& cmdList, resource::UpdateTexture3DData& updateData
    ) noexcept override;

    /// @copydoc DynamicRHI::readSurfaceData(resource::Texture*, const resource::IntRect&, gp::Vector<gp::math::Color>&,
    /// const resource::ReadSurfaceDataFlags&)
    virtual void readSurfaceData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<gp::math::Color>& outData,
        const resource::ReadSurfaceDataFlags& flags
    ) noexcept override;

    /// @copydoc DynamicRHI::readSurfaceData(resource::Texture*, const resource::IntRect&,
    /// gp::Vector<gp::math::LinearColor>&, const resource::ReadSurfaceDataFlags&)
    virtual void readSurfaceData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<gp::math::LinearColor>& outData,
        const resource::ReadSurfaceDataFlags& flags
    ) noexcept override;

    /// @copydoc DynamicRHI::readSurfaceFloatData()
    virtual void readSurfaceFloatData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<resource::Float16Color>& outData,
        resource::CubeFace cubeFace,
        gp::Int32 arrayIndex,
        gp::Int32 mipIndex
    ) noexcept override;

    /// @copydoc DynamicRHI::read3DSurfaceFloatData()
    virtual void read3DSurfaceFloatData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        resource::IntPoint zMinMax,
        gp::Vector<resource::Float16Color>& outData
    ) noexcept override;

public:
    /// @copydoc DynamicRHI::createShaderResourceView(command::CommandListBase&, resource::Buffer*, const
    /// view::ViewDesc&)
    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        command::CommandListBase& cmdList, resource::Buffer* buffer, const view::ViewDesc& desc
    ) noexcept override;

    /// @copydoc DynamicRHI::createShaderResourceView(command::CommandListBase&, resource::Texture*, const
    /// view::ViewDesc&)
    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        command::CommandListBase& cmdList, resource::Texture* texture, const view::ViewDesc& desc
    ) noexcept override;

    /// @copydoc DynamicRHI::createUnorderedAccessView(command::CommandListBase&, resource::Buffer*, const
    /// view::ViewDesc&)
    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase& cmdList, resource::Buffer* buffer, const view::ViewDesc& desc
    ) noexcept override;

    /// @copydoc DynamicRHI::createUnorderedAccessView(command::CommandListBase&, resource::Texture*, const
    /// view::ViewDesc&)
    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase& cmdList, resource::Texture* texture, const view::ViewDesc& desc
    ) noexcept override;

public:
    /// @copydoc DynamicRHI::createGPUFence()
    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(gp::Name name) noexcept override;

    /// @copydoc DynamicRHI::createStagingBuffer()
    [[nodiscard]] virtual resource::StagingBufferRef createStagingBuffer() noexcept override;

    /// @copydoc DynamicRHI::lockStagingBuffer()
    [[nodiscard]] virtual void* lockStagingBuffer(
        resource::StagingBuffer* stagingBuffer, sync::GPUFence* fence, gp::UInt32 offset, gp::UInt32 numBytes
    ) noexcept override;

    /// @copydoc DynamicRHI::unlockStagingBuffer()
    virtual void unlockStagingBuffer(resource::StagingBuffer* stagingBuffer) noexcept override;

    /// @copydoc DynamicRHI::createRenderQuery()
    [[nodiscard]] virtual sync::RenderQueryRef createRenderQuery(sync::RenderQueryType queryType) noexcept override;

    /// @copydoc DynamicRHI::createRenderQueryPool()
    [[nodiscard]] virtual sync::RenderQueryPoolRef
        createRenderQueryPool(sync::RenderQueryType queryType, gp::UInt32 numQueries = 0u) noexcept override;

    /// @copydoc DynamicRHI::getRenderQueryResult()
    [[nodiscard]] virtual bool getRenderQueryResult(
        sync::RenderQuery* renderQuery, gp::UInt64& outResult, bool wait, gp::UInt32 gpuIndex = 0u
    ) noexcept override;

    /// @copydoc DynamicRHI::writeGPUFenceTopOfPipe()
    virtual void writeGPUFenceTopOfPipe(command::CommandListBase& cmdList, sync::GPUFence* fence) noexcept override;

    /// @copydoc DynamicRHI::beginRenderQueryTopOfPipe()
    virtual void
        beginRenderQueryTopOfPipe(command::CommandListBase& cmdList, sync::RenderQuery* renderQuery) noexcept override;

    /// @copydoc DynamicRHI::endRenderQueryTopOfPipe()
    virtual void
        endRenderQueryTopOfPipe(command::CommandListBase& cmdList, sync::RenderQuery* renderQuery) noexcept override;

    /// @copydoc DynamicRHI::getTransitionPrivateDataSizeInBytes()
    [[nodiscard]] virtual gp::USize getTransitionPrivateDataSizeInBytes() const noexcept override;

    /// @copydoc DynamicRHI::getTransitionPrivateDataAlignInBytes()
    [[nodiscard]] virtual gp::USize getTransitionPrivateDataAlignInBytes() const noexcept override;

    /// @copydoc DynamicRHI::createTransition()
    virtual void
        createTransition(sync::Transition* transition, const sync::TransitionCreateInfo& createInfo) noexcept override;

    /// @copydoc DynamicRHI::releaseTransition()
    virtual void releaseTransition(sync::Transition* transition) noexcept override;

public:
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
    virtual void endDrawingViewport(
        presentation::Viewport* viewport, const presentation::PresentArgsWithStats& presentArgs
    ) noexcept override;

    /// @copydoc DynamicRHI::beginFrame()
    virtual void beginFrame() noexcept override;

    /// @copydoc DynamicRHI::endFrame()
    virtual void endFrame(const EndFrameArgs& args) noexcept override;

    /// @copydoc DynamicRHI::endFrameRenderThread()
    virtual void endFrameRenderThread() noexcept override;

    /// @copydoc DynamicRHI::getSupportedResolutions()
    [[nodiscard]] virtual bool getSupportedResolutions(
        gp::Vector<ScreenResolution>& outResolutions, bool ignoreRefreshRate = false
    ) noexcept override;

    /// @copydoc DynamicRHI::getSupportedResolution()
    virtual void getSupportedResolution(gp::UInt32& inOutWidth, gp::UInt32& inOutHeight) noexcept override;

    /// @copydoc DynamicRHI::mapStagingSurface()
    virtual void mapStagingSurface(
        resource::Texture* texture,
        sync::GPUFence* fence,
        void*& outData,
        gp::Int32& outWidth,
        gp::Int32& outHeight,
        gp::UInt32 gpuIndex = 0u
    ) noexcept override;

    /// @copydoc DynamicRHI::unmapStagingSurface()
    virtual void unmapStagingSurface(resource::Texture* texture, gp::UInt32 gpuIndex = 0u) noexcept override;

    /// @copydoc DynamicRHI::aliasTextureResources()
    virtual void
        aliasTextureResources(resource::TextureRef& destTexture, resource::TextureRef& srcTexture) noexcept override;

    /// @copydoc DynamicRHI::blockUntilGPUIdle()
    virtual void blockUntilGPUIdle() noexcept override;

    /// @copydoc DynamicRHI::bindDebugLabelName(command::CommandListBase&, resource::Texture*, const char*)
    virtual void bindDebugLabelName(
        command::CommandListBase& cmdList, resource::Texture* texture, const char* name
    ) noexcept override;

    /// @copydoc DynamicRHI::bindDebugLabelName(command::CommandListBase&, resource::Buffer*, const char*)
    virtual void bindDebugLabelName(
        command::CommandListBase& cmdList, resource::Buffer* buffer, const char* name
    ) noexcept override;

    /// @copydoc DynamicRHI::bindDebugLabelName(command::CommandListBase&, view::UnorderedAccessView*, const char*)
    virtual void bindDebugLabelName(
        command::CommandListBase& cmdList, view::UnorderedAccessView* uav, const char* name
    ) noexcept override;

public:
    /// @copydoc DynamicRHI::getDefaultContext()
    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept override;

    /// @copydoc DynamicRHI::getDefaultAsyncComputeContext()
    [[nodiscard]] virtual command::IComputeContext* getDefaultAsyncComputeContext() noexcept override;

    /// @copydoc DynamicRHI::getCommandContext()
    [[nodiscard]] virtual command::IComputeContext* getCommandContext(sync::PipelineStage pipeline) noexcept override;

    /// @copydoc DynamicRHI::acquireUploadContext()
    [[nodiscard]] virtual command::IUploadContext* acquireUploadContext() noexcept override;

    /// @copydoc DynamicRHI::finalizeContexts()
    virtual void finalizeContexts(FinalizeContextsArgs&& args, FinalizeContextsResult& outResult) noexcept override;

    /// @copydoc DynamicRHI::submitCommandLists()
    virtual void submitCommandLists(SubmitCommandListsArgs&& args) noexcept override;

public:
    /// @brief Returns the current validation frame ID used to audit `SingleFrame` / `SingleDraw` uniform buffers.
    /// @return Monotonic validation frame counter.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getValidationFrameID() const noexcept
    {
        return m_renderThreadFrameID;
    }

    /// @brief Validates non-zero thread group counts within `getRHIGlobals()` dispatch dimension limits.
    static void validateThreadGroupCount(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept;

    /// @brief Validates indirect draw argument buffer usage flags, 4-byte offset alignment, and bounds.
    static void validateIndirectArgsBuffer(
        const resource::Buffer* argumentBuffer,
        gp::UInt32 argumentOffset,
        gp::UInt32 argumentSizeInBytes,
        gp::UInt32 minStrideInBytes
    ) noexcept;

    /// @brief Validates indirect compute/mesh dispatch argument buffer usage flags, alignment, and bounds.
    static void
        validateDispatchIndirectArgsBuffer(const resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept;

private:
    DynamicRHI* m_rhi{ nullptr };
    gp::UniquePtr<ValidationContext> m_defaultGraphicsContext{};
    gp::UniquePtr<ValidationComputeContext> m_defaultAsyncComputeContext{};
    gp::thread::Mutex m_parallelContextMutex{};
    gp::Vector<gp::UniquePtr<ValidationContext>> m_parallelGraphicsContexts{};
    gp::Vector<gp::UniquePtr<ValidationComputeContext>> m_parallelComputeContexts{};
    gp::UInt64 m_renderThreadFrameID{ 1u };
};

}   // namespace gp::rhi
