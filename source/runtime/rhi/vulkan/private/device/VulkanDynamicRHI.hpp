// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"         // IWYU pragma: keep
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "containers/strings/Name.hpp"
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "contexts/VulkanContextDesc.hpp"
#include "contexts/VulkanContextForward.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"            // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "device/DeviceTypes.hpp"
#include "instance/VulkanInstance.hpp"
#include "IVulkanDynamicRHI.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "pipeline/IVulkanPipelineDeletionListener.hpp"
#include "pipeline/PipelineForward.hpp"                 // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"           // IWYU pragma: keep
#include "presentation/PresentArgs.hpp"
#include "presentation/PresentationForward.hpp"         // IWYU pragma: keep
#include "presentation/VulkanPresentationDesc.hpp"
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "profiling/ProfilingForward.hpp"               // IWYU pragma: keep
#include "queries/VulkanQueryForward.hpp"               // IWYU pragma: keep
#include "renderpass/VulkanRenderPassForward.hpp"       // IWYU pragma: keep
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "shaders/ShaderForward.hpp"         // IWYU pragma: keep
#include "shaders/ShaderTypes.hpp"           // IWYU pragma: keep
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "states/VulkanStateForward.hpp"     // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"              // IWYU pragma: keep
#include "sync/VulkanSyncPoint.hpp"
#include "threading/sync/Mutex.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewForward.hpp"   // IWYU pragma: keep
#include "VulkanForward.hpp"       // IWYU pragma: keep
#include "VulkanThirdParty.hpp"    // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Vulkan implementation of `DynamicRHI`: owns the instance, the device and every device-level service.
/// @details The class is deliberately thin. Each subsystem (memory, staging, deferred deletion, queues, descriptors,
///          render passes, pipelines, contexts, presentation) already lives in its own type with its own tests; the
///          dynamic RHI only
///          1. builds them in dependency order in `init()` and tears them down in reverse in `shutdown()`,
///          2. forwards every factory to the matching `createVulkan*` free function with the right service bundle,
///          3. turns recorded contexts into platform command lists (`finalizeContexts`) and hands them to the queues
///             (`submitCommandLists`),
///          4. runs the per-frame maintenance (staging recycling, deferred deletions, descriptor and pipeline garbage
///             collection) in `endFrame()`.
///
///          Lifetime: the services outlive every resource because `rhiExit()` flushes the resource deletion queue
///          before `shutdown()`. The RHI registers itself as the pipeline deletion listener so command contexts never
///          keep pointers to destroyed pipelines.
/// @note Not copyable or movable. Every method except the factories must be called from the RHI thread.
class GP_RHI_VULKAN_API VulkanDynamicRHI final
    : public IVulkanDynamicRHI
    , public IVulkanPipelineDeletionListener
{
public:
    /// @brief Constructs an uninitialized RHI; nothing touches Vulkan until `init()`.
    VulkanDynamicRHI() noexcept;

    /// @brief Shuts the RHI down when the owner forgot to.
    virtual ~VulkanDynamicRHI() noexcept override;

    VulkanDynamicRHI(const VulkanDynamicRHI&) = delete;
    VulkanDynamicRHI& operator=(const VulkanDynamicRHI&) = delete;
    VulkanDynamicRHI(VulkanDynamicRHI&&) = delete;
    VulkanDynamicRHI& operator=(VulkanDynamicRHI&&) = delete;

public:
    /// @copydoc DynamicRHI::init()
    /// @note Loads the loader, creates the instance and device, then every service in dependency order. On failure the
    ///       RHI stays uninitialized (`isInitialized()` is false) and logs the failing step.
    virtual void init() noexcept override;

    /// @copydoc DynamicRHI::postInit()
    /// @note Submits the uploads recorded during `init()` (default descriptor texture) and waits for them.
    virtual void postInit() noexcept override;

    /// @copydoc DynamicRHI::shutdown()
    /// @note Idempotent. Waits for the GPU, then destroys every service in reverse creation order.
    virtual void shutdown() noexcept override;

    /// @copydoc DynamicRHI::getName()
    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "Vulkan";
    }

    /// @copydoc DynamicRHI::getNativeDevice()
    /// @note Returns the `VkDevice` handle.
    [[nodiscard]] virtual void* getNativeDevice() noexcept override;

    /// @copydoc DynamicRHI::tick()
    /// @note Polls queue completions so sync points and deferred deletions make progress between frames.
    virtual void tick(const gp::Float32 deltaTime) noexcept override;

    /// @copydoc DynamicRHI::processDeleteQueue()
    /// @note Releases every native handle whose GPU work has completed.
    virtual void processDeleteQueue() noexcept override;

    /// @copydoc DynamicRHI::getMinimumAlignmentForBufferBackedSRV()
    /// @note Returns `minTexelBufferOffsetAlignment`.
    [[nodiscard]] virtual gp::UInt64 getMinimumAlignmentForBufferBackedSRV(PixelFormat format) noexcept override;

    /// @copydoc DynamicRHI::getPlatformTextureMaxSampleCount()
    /// @note Returns the highest bit of `framebufferColorSampleCounts`.
    [[nodiscard]] virtual gp::UInt32 getPlatformTextureMaxSampleCount() const noexcept override;

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

    /// @copydoc DynamicRHI::createStagingBuffer()
    [[nodiscard]] virtual resource::StagingBufferRef createStagingBuffer() noexcept override;

    /// @copydoc DynamicRHI::lockStagingBuffer()
    [[nodiscard]] virtual void* lockStagingBuffer(
        resource::StagingBuffer* stagingBuffer, sync::GPUFence* fence, gp::UInt32 offset, gp::UInt32 numBytes
    ) noexcept override;

    /// @copydoc DynamicRHI::unlockStagingBuffer()
    virtual void unlockStagingBuffer(resource::StagingBuffer* stagingBuffer) noexcept override;

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

    /// @copydoc DynamicRHI::readSurfaceData(resource::Texture*, const resource::IntRect&, gp::Vector<gp::math::Color>&,
    ///          const resource::ReadSurfaceDataFlags&)
    /// @note Copies on the immediate command list and blocks until the GPU finished.
    virtual void readSurfaceData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<gp::math::Color>& outData,
        const resource::ReadSurfaceDataFlags& flags
    ) noexcept override;

    /// @copydoc DynamicRHI::readSurfaceData(resource::Texture*, const resource::IntRect&,
    ///          gp::Vector<gp::math::LinearColor>&, const resource::ReadSurfaceDataFlags&)
    /// @note Copies on the immediate command list and blocks until the GPU finished.
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

    /// @copydoc DynamicRHI::mapStagingSurface()
    /// @note When `fence` has not signaled yet, waits for the GPU to go idle before mapping.
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

    /// @copydoc DynamicRHI::createGPUFence()
    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(gp::Name name) noexcept override;

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

    /// @copydoc DynamicRHI::createViewport()
    /// @note `windowHandle` must point to a `gp::hal::NativeWindowHandle`.
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

    using DynamicRHI::endDrawingViewport;

    /// @copydoc DynamicRHI::endDrawingViewport(presentation::Viewport*, const presentation::PresentArgsWithStats&)
    virtual void endDrawingViewport(
        presentation::Viewport* viewport, const presentation::PresentArgsWithStats& presentArgs
    ) noexcept override;

    /// @copydoc DynamicRHI::getSupportedResolutions()
    [[nodiscard]] virtual bool getSupportedResolutions(
        gp::Vector<ScreenResolution>& outResolutions, bool ignoreRefreshRate = false
    ) noexcept override;

    /// @copydoc DynamicRHI::getSupportedResolution()
    virtual void getSupportedResolution(gp::UInt32& inOutWidth, gp::UInt32& inOutHeight) noexcept override;

    /// @copydoc DynamicRHI::beginFrame()
    virtual void beginFrame() noexcept override;

    /// @copydoc DynamicRHI::endFrame()
    /// @note Runs the per-frame maintenance: staging recycling, freed pages, deferred deletions gated on the last
    ///       submitted work, descriptor and query pool garbage collection, pipeline LRU, volatile blocks.
    virtual void endFrame(const EndFrameArgs& args) noexcept override;

    /// @copydoc DynamicRHI::blockUntilGPUIdle()
    /// @note Submits pending uploads, waits for the device, then drains completed payloads and deletions.
    virtual void blockUntilGPUIdle() noexcept override;

    /// @copydoc DynamicRHI::bindDebugLabelName(command::CommandListBase&, resource::Texture*, const char*)
    /// @note Also names the `VkImage` for debuggers and validation messages.
    virtual void bindDebugLabelName(
        command::CommandListBase& cmdList, resource::Texture* texture, const char* name
    ) noexcept override;

    /// @copydoc DynamicRHI::bindDebugLabelName(command::CommandListBase&, resource::Buffer*, const char*)
    /// @note Also names the `VkBuffer` for debuggers and validation messages.
    virtual void bindDebugLabelName(
        command::CommandListBase& cmdList, resource::Buffer* buffer, const char* name
    ) noexcept override;

    using DynamicRHI::bindDebugLabelName;

    /// @copydoc DynamicRHI::getDefaultContext()
    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept override;

    /// @copydoc DynamicRHI::getDefaultAsyncComputeContext()
    /// @note Async compute work is recorded on the graphics context: contexts always report the graphics pipeline, so
    ///       a separate compute-queue context would produce barriers for the wrong queue.
    [[nodiscard]] virtual command::IComputeContext* getDefaultAsyncComputeContext() noexcept override;

    /// @copydoc DynamicRHI::getCommandContext()
    /// @note Hands out pooled graphics contexts for parallel recording; returns `nullptr` for async compute so it falls
    ///       back to `getDefaultAsyncComputeContext()`.
    [[nodiscard]] virtual command::IComputeContext* getCommandContext(sync::PipelineStage pipeline) noexcept override;

    /// @copydoc DynamicRHI::acquireUploadContext()
    /// @note Pops a warm context from the upload pool (thread-safe). It must come back through
    ///       `FinalizeContextsArgs::uploadContext`, which finalizes it and returns it to the pool.
    [[nodiscard]] virtual command::IUploadContext* acquireUploadContext() noexcept override;

    /// @copydoc DynamicRHI::getMemoryStats()
    /// @note Reads the memory budget extension when available, the heap sizes otherwise.
    virtual void getMemoryStats(profiling::RHIMemoryStats& outStats) noexcept override;

    /// @copydoc DynamicRHI::finalizeContexts()
    /// @note Produces one platform command list per context, in order. Pending uploads (the provider's, then
    ///       `args.uploadContext`) are prepended to the first one so they execute before any work that reads the
    ///       uploaded data; with no context they get a list of their own. `outResult.uploadCommandList` stays null.
    ///       Pooled contexts go back to the pool.
    virtual void finalizeContexts(FinalizeContextsArgs&& args, FinalizeContextsResult& outResult) noexcept override;

    /// @copydoc DynamicRHI::submitCommandLists()
    /// @note Hands every payload to its queue and submits; ownership of the platform command lists is taken.
    virtual void submitCommandLists(SubmitCommandListsArgs&& args) noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanApiVersion()
    [[nodiscard]] virtual gp::UInt32 getVulkanApiVersion() const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanInstanceHandle()
    [[nodiscard]] virtual void* getVulkanInstanceHandle() const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanPhysicalDeviceHandle()
    [[nodiscard]] virtual void* getVulkanPhysicalDeviceHandle() const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanDeviceHandle()
    [[nodiscard]] virtual void* getVulkanDeviceHandle() const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanGraphicsQueueHandle()
    [[nodiscard]] virtual void* getVulkanGraphicsQueueHandle() const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanGraphicsQueueIndex()
    [[nodiscard]] virtual gp::UInt32 getVulkanGraphicsQueueIndex() const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanGraphicsQueueFamilyIndex()
    [[nodiscard]] virtual gp::UInt32 getVulkanGraphicsQueueFamilyIndex() const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanDeviceUUID()
    [[nodiscard]] virtual const gp::UInt8* getVulkanDeviceUUID() const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanInstanceProcAddress()
    [[nodiscard]] virtual void* getVulkanInstanceProcAddress(const char* functionName) const noexcept override;

    /// @copydoc IVulkanDynamicRHI::getVulkanDeviceProcAddress()
    [[nodiscard]] virtual void* getVulkanDeviceProcAddress(const char* functionName) const noexcept override;

    /// @copydoc IVulkanPipelineDeletionListener::onGraphicsPipelineDeleted()
    /// @note Clears the pipeline from the pending graphics state of every context.
    virtual void onGraphicsPipelineDeleted(const VulkanGraphicsPipelineState& pipeline) noexcept override;

    /// @copydoc IVulkanPipelineDeletionListener::onComputePipelineDeleted()
    /// @note Clears the pipeline from the pending compute state of every context.
    virtual void onComputePipelineDeleted(const VulkanComputePipelineState& pipeline) noexcept override;

    /// @brief Reports whether `init()` completed.
    /// @return True between a successful `init()` and `shutdown()`.
    [[nodiscard]] bool isInitialized() const noexcept
    {
        return m_isInitialized;
    }

    /// @brief Returns the logical device.
    /// @return Device, or `nullptr` when uninitialized.
    [[nodiscard]] VulkanDevice* getDevice() const noexcept
    {
        return m_device.get();
    }

    /// @brief Returns the graphics queue.
    /// @return Queue, or `nullptr` when uninitialized.
    [[nodiscard]] VulkanQueue* getGraphicsQueue() const noexcept
    {
        return m_graphicsQueue.get();
    }

    /// @brief Returns the dedicated async compute queue.
    /// @return Queue, or `nullptr` when the device has none.
    [[nodiscard]] VulkanQueue* getAsyncComputeQueue() const noexcept
    {
        return m_computeQueue.get();
    }

    /// @brief Returns the service bundle resources are created with.
    /// @return Resource services; all null when uninitialized.
    [[nodiscard]] const VulkanResourceServices& getResourceServices() const noexcept
    {
        return m_resourceServices;
    }

    /// @brief Returns the render pass manager.
    /// @return Manager, or `nullptr` when uninitialized.
    [[nodiscard]] VulkanRenderPassManager* getRenderPassManager() const noexcept
    {
        return m_renderPassManager.get();
    }

    /// @brief Returns the registry of the live viewports.
    /// @return Registry, or `nullptr` when uninitialized.
    [[nodiscard]] VulkanViewportRegistry* getViewportRegistry() const noexcept
    {
        return m_viewportRegistry.get();
    }

    /// @brief Returns the pool upload contexts are acquired from.
    /// @return Pool, or `nullptr` when uninitialized.
    [[nodiscard]] VulkanUploadContextPool* getUploadContextPool() const noexcept
    {
        return m_uploadPool.get();
    }

    /// @brief Returns the pipeline state cache.
    /// @return Cache, or `nullptr` when uninitialized.
    [[nodiscard]] VulkanPipelineStateCacheManager* getPipelineStateCache() const noexcept
    {
        return m_pipelineCache.get();
    }

    /// @brief Returns the default descriptors handed to every context.
    /// @return Default sampler, image view and global samplers.
    [[nodiscard]] const VulkanDescriptorDefaults& getDescriptorDefaults() const noexcept
    {
        return m_descriptorDefaults;
    }

    /// @brief Returns the number of pooled parallel contexts created so far.
    /// @return Parallel context count.
    [[nodiscard]] gp::USize getNumParallelContexts() const noexcept;

    /// @brief Returns the number of pooled parallel contexts currently free.
    /// @return Free parallel context count.
    [[nodiscard]] gp::USize getNumFreeParallelContexts() const noexcept;

    /// @brief Returns the sync point of the most recently finalized graphics work.
    /// @return Sync point; null before the first submission.
    [[nodiscard]] const VulkanSyncPointRef& getLastSubmittedSyncPoint() const noexcept
    {
        return m_lastSubmittedSyncPoint;
    }

private:
    /// @brief Loads the loader and creates the instance.
    /// @return True on success.
    [[nodiscard]] bool initInstance() noexcept;

    /// @brief Selects the physical device and creates the logical device.
    /// @return True on success.
    [[nodiscard]] bool initDevice() noexcept;

    /// @brief Creates the memory, staging, queue, descriptor, render pass, state, shader and pipeline services.
    /// @return True on success.
    [[nodiscard]] bool initServices() noexcept;

    /// @brief Creates the default descriptor texture and samplers.
    void initDescriptorDefaults() noexcept;

    /// @brief Creates the default context, the upload pool and the context provider.
    void initContexts() noexcept;

    /// @brief Destroys every object `init()` created, in reverse order; safe on a partial initialization.
    void releaseAll() noexcept;

    /// @brief Submits the upload context, if it recorded anything, and optionally waits for it.
    /// @param[in] waitForCompletion True to block until the uploads finished on the GPU.
    void flushPendingUploads(bool waitForCompletion) noexcept;

    /// @brief Polls every queue for completed payloads.
    void pollQueues() noexcept;

    /// @brief Returns the concrete context behind an RHI context.
    /// @param[in] context RHI context, possibly wrapped.
    /// @return Vulkan context.
    [[nodiscard]] static VulkanCommandListContext& toVulkanContext(command::IComputeContext& context) noexcept;

    /// @brief Hands a pooled parallel context back to the pool; ignores the default context.
    /// @param[in] context Context to release.
    void releaseParallelContext(VulkanCommandListContext& context) noexcept;

private:
    /// @brief Vulkan instance; also owns the debug messenger.
    VulkanInstance m_instance{};

    /// @brief Logical device.
    gp::UniquePtr<VulkanDevice> m_device{};

    /// @brief Device memory heaps and budgets.
    gp::UniquePtr<VulkanDeviceMemoryManager> m_deviceMemoryManager{};

    /// @brief Sub-allocating memory manager.
    gp::UniquePtr<VulkanMemoryManager> m_memoryManager{};

    /// @brief Native handle deletion queue gated on GPU sync points.
    gp::UniquePtr<VulkanDeferredDeletionQueue> m_deferredDeletionQueue{};

    /// @brief Host-visible staging buffer pool.
    gp::UniquePtr<VulkanStagingManager> m_stagingManager{};

    /// @brief Fence pool backing the queues.
    gp::UniquePtr<VulkanFenceManager> m_fenceManager{};

    /// @brief Graphics queue; every context records for it.
    gp::UniquePtr<VulkanQueue> m_graphicsQueue{};

    /// @brief Dedicated async compute queue, when the device exposes one (used by presentation services).
    gp::UniquePtr<VulkanQueue> m_computeQueue{};

    /// @brief Hands finalized payloads to their queue.
    gp::UniquePtr<VulkanImmediatePayloadSubmitter> m_submitter{};

    /// @brief Occlusion and timing query pools.
    gp::UniquePtr<VulkanQueryPoolManager> m_queryPoolManager{};

    /// @brief Per-frame descriptor pool sets.
    gp::UniquePtr<VulkanDescriptorPoolsManager> m_descriptorPoolsManager{};

    /// @brief Cache of immutable descriptor sets.
    gp::UniquePtr<VulkanDescriptorSetCache> m_descriptorSetCache{};

    /// @brief Render passes and framebuffers.
    gp::UniquePtr<VulkanRenderPassManager> m_renderPassManager{};

    /// @brief Deduplicated sampler states.
    gp::UniquePtr<VulkanSamplerStateCache> m_samplerCache{};

    /// @brief Deduplicated shader modules.
    gp::UniquePtr<VulkanShaderFactory> m_shaderFactory{};

    /// @brief Graphics and compute pipelines and their layouts.
    gp::UniquePtr<VulkanPipelineStateCacheManager> m_pipelineCache{};

    /// @brief Volatile uniform buffer blocks.
    gp::UniquePtr<VulkanTempBlockAllocator> m_tempBlockAllocator{};

    /// @brief Pool of upload contexts.
    gp::UniquePtr<VulkanUploadContextPool> m_uploadPool{};

    /// @brief Seam resources use to reach command contexts.
    gp::UniquePtr<VulkanCommandContextProvider> m_contextProvider{};

    /// @brief Immediate graphics context.
    gp::UniquePtr<VulkanCommandListContext> m_defaultContext{};

    /// @brief Guards the parallel context pool and the deletion listener against concurrent use.
    mutable gp::thread::Mutex m_contextMutex{};

    /// @brief Every parallel context ever created; reused through `m_freeParallelContexts`.
    gp::Vector<gp::UniquePtr<VulkanCommandListContext>> m_parallelContexts{};

    /// @brief Parallel contexts ready to be handed out.
    gp::Vector<VulkanCommandListContext*> m_freeParallelContexts{};

    /// @brief Services handed to resources.
    VulkanResourceServices m_resourceServices{};

    /// @brief Services handed to viewports.
    VulkanViewportServices m_viewportServices{};

    /// @brief Live viewports, stripped of their native objects at shutdown before the device dies.
    gp::UniquePtr<VulkanViewportRegistry> m_viewportRegistry{};

    /// @brief Services handed to graphics contexts and upload contexts.
    VulkanContextServices m_contextServices{};

    /// @brief Defaults bound where a shader declares a resource nobody set.
    VulkanDescriptorDefaults m_descriptorDefaults{};

    /// @brief Texture backing `m_descriptorDefaults.defaultImageView`.
    resource::TextureRef m_defaultTexture{};

    /// @brief Sampler backing `m_descriptorDefaults.defaultSampler`.
    pipeline::SamplerStateRef m_defaultSampler{};

    /// @brief Samplers backing `m_descriptorDefaults.globalSamplers`.
    gp::Array<pipeline::SamplerStateRef, kVulkanNumGlobalSamplers> m_globalSamplers{};

    /// @brief Sync point of the most recent graphics work; gates deferred deletions at frame end.
    VulkanSyncPointRef m_lastSubmittedSyncPoint{};

    /// @brief UUID of the selected physical device, kept for `getVulkanDeviceUUID()`.
    gp::Array<gp::UInt8, VK_UUID_SIZE> m_deviceUuid{};

    /// @brief Frame number of the last `endFrame()`.
    gp::UInt64 m_currentFrame{ 0u };

    /// @brief True when this RHI loaded the loader library and must unload it.
    bool m_ownsLoaderLibrary{ false };

    /// @brief True when the instance was created.
    bool m_hasInstance{ false };

    /// @brief True between a successful `init()` and `shutdown()`.
    bool m_isInitialized{ false };

    /// @brief True while registered as the process-wide pipeline deletion listener.
    bool m_isPipelineListener{ false };
};

}   // namespace gp::rhi::vulkan
