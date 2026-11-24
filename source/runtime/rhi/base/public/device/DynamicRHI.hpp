// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"   // IWYU pragma: keep
#include "commands/CommandForward.hpp"
#include "commands/ICommandContext.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/Name.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/DeviceForward.hpp"   // IWYU pragma: keep
#include "device/DeviceTypes.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/pointers/RefCountedPtr.hpp"    // IWYU pragma: keep
#include "pipeline/BlendState.hpp"              // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/BoundShaderState.hpp"        // IWYU pragma: keep
#include "pipeline/ComputePipelineState.hpp"    // IWYU pragma: keep
#include "pipeline/DepthStencilState.hpp"       // IWYU pragma: keep
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineState.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineForward.hpp"
#include "pipeline/RasterizerState.hpp"   // IWYU pragma: keep
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerState.hpp"      // IWYU pragma: keep
#include "pipeline/SamplerStateDesc.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "presentation/PresentationForward.hpp"
#include "presentation/Viewport.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
#include "resources/StagingBuffer.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/TextureReference.hpp"
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include "RHIBase.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/AmplificationShader.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/MeshShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderTypes.hpp"   // IWYU pragma: keep
#include "shaders/VertexShader.hpp"
#include "sync/GPUFence.hpp"
#include "sync/RenderQuery.hpp"
#include "sync/RenderQueryPool.hpp"   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include "sync/Transition.hpp"
#include "sync/TransitionDesc.hpp"
#include "threading/sync/Mutex.hpp"
#include "views/ShaderResourceView.hpp"   // IWYU pragma: keep
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"

namespace gp::rhi
{

/// @brief Abstract hardware device interface implemented by every graphics backend (`rhi/null`, Vulkan, D3D12, etc.).
/// @details Owns backend initialization and shutdown, all GPU resource factories (states, shaders, PSOs, buffers,
///          textures, views, queries, fences, viewports), lock/unlock and readback paths, transition private-data
///          hooks, and command context acquisition/finalization/submission.
class GP_RHI_API DynamicRHI
{
public:
    /// @brief Constructs the base `DynamicRHI` interface.
    DynamicRHI() noexcept = default;

    DynamicRHI(const DynamicRHI&) = delete;
    DynamicRHI& operator=(const DynamicRHI&) = delete;
    DynamicRHI(DynamicRHI&&) = delete;
    DynamicRHI& operator=(DynamicRHI&&) = delete;

    /// @brief Virtual destructor.
    virtual ~DynamicRHI() noexcept = default;

public:
    /// @brief Performs core device creation and populates `getRHIGlobals()`.
    virtual void init() noexcept = 0;

    /// @brief Runs secondary startup tasks after core engine subsystems and command lists are initialized.
    virtual void postInit() noexcept
    {}

    /// @brief Tears down backend device objects after all command lists and deferred resources have flushed.
    virtual void shutdown() noexcept = 0;

    /// @brief Returns the short diagnostic name of this RHI backend (e.g., `"Null"`, `"Vulkan"`, `"D3D12"`).
    /// @return Null-terminated backend name string.
    [[nodiscard]] virtual const char* getName() const noexcept = 0;

    /// @brief Returns the `InterfaceType` enumerator identifying this backend API.
    /// @return Backend `InterfaceType`.
    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept
    {
        return InterfaceType::Hidden;
    }

    /// @brief Unwraps any validation wrapper layer to return the underlying concrete `DynamicRHI`.
    /// @return Non-validation `DynamicRHI` pointer (defaults to `this`).
    [[nodiscard]] virtual DynamicRHI* getNonValidationRHI() noexcept
    {
        return this;
    }

    /// @brief Returns the underlying native graphics API device pointer (e.g., `VkDevice`, `ID3D12Device*`), or
    /// `nullptr`.
    /// @return Opaque native device handle, or `nullptr`.
    [[nodiscard]] virtual void* getNativeDevice() noexcept
    {
        return nullptr;
    }

    /// @brief Advances periodic backend maintenance (reclaiming transient fences, descriptor pools, or telemetry).
    /// @param[in] deltaTime Frame delta time in seconds.
    virtual void tick(const gp::Float32 deltaTime) noexcept
    {
        static_cast<void>(deltaTime);
    }

    /// @brief Processes any backend-specific deferred deletion queues after RHI resource destruction.
    virtual void processDeleteQueue() noexcept
    {}

    /// @brief Flushes any buffered backend validation or diagnostic messages.
    virtual void flushPendingLogs() noexcept
    {}

    /// @brief Acquires thread ownership of the active graphics API context (required by single-threaded APIs).
    virtual void acquireThreadOwnership() noexcept
    {}

    /// @brief Releases thread ownership previously acquired by `acquireThreadOwnership()`.
    virtual void releaseThreadOwnership() noexcept
    {}

    /// @brief Suspends GPU rendering (e.g., during mobile/console background transitions).
    virtual void suspendRendering() noexcept
    {}

    /// @brief Resumes GPU rendering after `suspendRendering()`.
    virtual void resumeRendering() noexcept
    {}

    /// @brief Reports whether GPU rendering is currently suspended.
    /// @return True when suspended; false otherwise.
    [[nodiscard]] virtual bool isRenderingSuspended() const noexcept
    {
        return false;
    }

    /// @brief Returns the minimum required byte alignment for buffer-backed SRVs using `format`.
    /// @param[in] format Pixel format of the buffer SRV.
    /// @return Minimum alignment in bytes.
    [[nodiscard]] virtual gp::UInt64 getMinimumAlignmentForBufferBackedSRV(PixelFormat format) noexcept;

    /// @brief Returns the maximum MSAA sample count supported for render target textures on this backend.
    /// @return Maximum sample count (defaults to `8`).
    [[nodiscard]] virtual gp::UInt32 getPlatformTextureMaxSampleCount() const noexcept
    {
        return 8u;
    }

public:
    /// @brief Creates an immutable texture sampler state from `initializer`.
    /// @param[in] initializer Sampler filtering, addressing, and LOD parameters.
    /// @return Owning `pipeline::SamplerStateRef`.
    [[nodiscard]] virtual pipeline::SamplerStateRef
        createSamplerState(const pipeline::SamplerStateDesc& initializer) noexcept = 0;

    /// @brief Creates an immutable polygon rasterizer state from `initializer`.
    /// @param[in] initializer Fill, cull, depth-bias, and MSAA parameters.
    /// @return Owning `pipeline::RasterizerStateRef`.
    [[nodiscard]] virtual pipeline::RasterizerStateRef
        createRasterizerState(const pipeline::RasterizerStateDesc& initializer) noexcept = 0;

    /// @brief Creates an immutable depth-stencil state from `initializer`.
    /// @param[in] initializer Depth comparison/write and front/back stencil parameters.
    /// @return Owning `pipeline::DepthStencilStateRef`.
    [[nodiscard]] virtual pipeline::DepthStencilStateRef
        createDepthStencilState(const pipeline::DepthStencilStateDesc& initializer) noexcept = 0;

    /// @brief Creates an immutable color blend state from `initializer`.
    /// @param[in] initializer Per-render-target blend equations and write masks.
    /// @return Owning `pipeline::BlendStateRef`.
    [[nodiscard]] virtual pipeline::BlendStateRef
        createBlendState(const pipeline::BlendStateDesc& initializer) noexcept = 0;

    /// @brief Creates an immutable vertex input declaration from `elements`.
    /// @param[in] elements List of vertex attribute layout descriptors.
    /// @return Owning `pipeline::VertexDeclarationRef`.
    [[nodiscard]] virtual pipeline::VertexDeclarationRef
        createVertexDeclaration(const pipeline::VertexDeclarationElementList& elements) noexcept = 0;

public:
    /// @brief Creates a vertex shader from compiled bytecode and its 128-bit content hash.
    /// @param[in] code Compiled bytecode span.
    /// @param[in] hash 128-bit bytecode hash.
    /// @return Owning `shader::VertexShaderRef`.
    [[nodiscard]] virtual shader::VertexShaderRef
        createVertexShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept = 0;

    /// @brief Creates a pixel/fragment shader from compiled bytecode and its 128-bit content hash.
    /// @param[in] code Compiled bytecode span.
    /// @param[in] hash 128-bit bytecode hash.
    /// @return Owning `shader::PixelShaderRef`.
    [[nodiscard]] virtual shader::PixelShaderRef
        createPixelShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept = 0;

    /// @brief Creates a geometry shader from compiled bytecode and its 128-bit content hash.
    /// @param[in] code Compiled bytecode span.
    /// @param[in] hash 128-bit bytecode hash.
    /// @return Owning `shader::GeometryShaderRef`.
    [[nodiscard]] virtual shader::GeometryShaderRef
        createGeometryShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept = 0;

    /// @brief Creates a compute shader from compiled bytecode and its 128-bit content hash.
    /// @param[in] code Compiled bytecode span.
    /// @param[in] hash 128-bit bytecode hash.
    /// @return Owning `shader::ComputeShaderRef`.
    [[nodiscard]] virtual shader::ComputeShaderRef
        createComputeShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept = 0;

    /// @brief Creates a mesh shader from compiled bytecode and its 128-bit content hash (optional; defaults to null).
    /// @param[in] code Compiled bytecode span.
    /// @param[in] hash 128-bit bytecode hash.
    /// @return Owning `shader::MeshShaderRef`.
    [[nodiscard]] virtual shader::MeshShaderRef
        createMeshShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept;

    /// @brief Creates an amplification/task shader from compiled bytecode and hash (optional; defaults to null).
    /// @param[in] code Compiled bytecode span.
    /// @param[in] hash 128-bit bytecode hash.
    /// @return Owning `shader::AmplificationShaderRef`.
    [[nodiscard]] virtual shader::AmplificationShaderRef
        createAmplificationShader(gp::ConstVectorView<gp::UInt8> code, const shader::ShaderHash& hash) noexcept;

    /// @brief Creates a bound graphics shader combination linking a vertex declaration with programmable stages.
    /// @param[in] vertexDeclaration Vertex input attribute layout.
    /// @param[in] vertexShader Vertex shader stage.
    /// @param[in] pixelShader Pixel shader stage.
    /// @param[in] geometryShader Optional geometry shader stage.
    /// @return Owning `pipeline::BoundShaderStateRef`.
    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        pipeline::VertexDeclaration* vertexDeclaration,
        shader::VertexShader* vertexShader,
        shader::PixelShader* pixelShader,
        shader::GeometryShader* geometryShader = nullptr
    ) noexcept = 0;

    /// @brief Creates a bound mesh-pipeline shader combination linking amplification, mesh, and pixel stages.
    /// @param[in] amplificationShader Optional amplification (task) shader stage.
    /// @param[in] meshShader Mesh shader stage.
    /// @param[in] pixelShader Optional pixel shader stage.
    /// @return Owning `pipeline::BoundShaderStateRef` (defaults to null if unsupported).
    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        shader::AmplificationShader* amplificationShader,
        shader::MeshShader* meshShader,
        shader::PixelShader* pixelShader
    ) noexcept;

    /// @brief Creates a compiled monolithic graphics pipeline state object from `initializer`.
    /// @param[in] initializer Full graphics pipeline creation descriptor.
    /// @return Owning `pipeline::GraphicsPipelineStateRef`.
    [[nodiscard]] virtual pipeline::GraphicsPipelineStateRef
        createGraphicsPipelineState(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept = 0;

    /// @brief Creates a compiled compute pipeline state object for `computeShader`.
    /// @param[in] computeShader Target compute shader.
    /// @return Owning `pipeline::ComputePipelineStateRef`.
    [[nodiscard]] virtual pipeline::ComputePipelineStateRef
        createComputePipelineState(shader::ComputeShader* computeShader) noexcept = 0;

    /// @brief Creates a compiled compute pipeline state object from `initializer` (including specialization constants).
    /// @param[in] initializer Compute pipeline creation descriptor.
    /// @return Owning `pipeline::ComputePipelineStateRef` (defaults to delegating to
    /// `createComputePipelineState(shader)`).
    [[nodiscard]] virtual pipeline::ComputePipelineStateRef
        createComputePipelineState(const pipeline::ComputePipelineStateInitializer& initializer) noexcept;

    /// @brief Computes a 64-bit hash over fixed-function graphics PSO state (excluding shaders) for precaching.
    /// @param[in] initializer Graphics PSO descriptor.
    /// @return 64-bit state hash.
    [[nodiscard]] virtual gp::UInt64
        computeStatePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept;

    /// @brief Computes a 64-bit full PSO precache hash combining shaders, vertex declaration, and fixed-function state.
    /// @param[in] initializer Graphics PSO descriptor.
    /// @return 64-bit precache hash.
    [[nodiscard]] virtual gp::UInt64
        computePrecachePSOHash(const pipeline::GraphicsPipelineStateInitializer& initializer) noexcept;

    /// @brief Compares two graphics PSO initializers for precache equivalence.
    /// @param[in] lhs First initializer.
    /// @param[in] rhs Second initializer.
    /// @return True when both initializers compile to an equivalent hardware PSO.
    [[nodiscard]] virtual bool matchPrecachePSOInitializers(
        const pipeline::GraphicsPipelineStateInitializer& lhs, const pipeline::GraphicsPipelineStateInitializer& rhs
    ) noexcept;

public:
    /// @brief Creates a uniform buffer initialized with `contents` matching `layout`.
    /// @param[in] contents Pointer to CPU parameter struct bytes (may be `nullptr` for uninitialized creation).
    /// @param[in] layout Reflected uniform buffer layout.
    /// @param[in] usage Expected update frequency (`SingleDraw`, `SingleFrame`, `MultiFrame`).
    /// @param[in] validation Optional validation level.
    /// @return Owning `resource::UniformBufferRef`.
    [[nodiscard]] virtual resource::UniformBufferRef createUniformBuffer(
        const void* contents,
        const resource::UniformBufferLayout* layout,
        resource::UniformBufferUsage usage,
        resource::UniformBufferValidation validation = resource::UniformBufferValidation::ValidateResources
    ) noexcept = 0;

    /// @brief Updates an existing uniform buffer's constant data and resource bindings.
    /// @param[in,out] cmdList Active command list driving the update.
    /// @param[in] uniformBuffer Target uniform buffer.
    /// @param[in] contents Pointer to new parameter struct bytes.
    virtual void updateUniformBuffer(
        command::CommandListBase& cmdList, resource::UniformBuffer* uniformBuffer, const void* contents
    ) noexcept = 0;

    /// @brief Creates a `BufferInitializer` for allocating and uploading a GPU `Buffer`.
    /// @param[in,out] cmdList Active command list driving creation/upload.
    /// @param[in] createDesc Buffer size, stride, usage, and initialization action.
    /// @return Move-only `resource::BufferInitializer`.
    [[nodiscard]] virtual resource::BufferInitializer createBufferInitializer(
        command::CommandListBase& cmdList, const resource::BufferCreateDesc& createDesc
    ) noexcept = 0;

    /// @brief Locks a buffer at the top of the RHI pipe (default implementation stages write-only uploads or flushes
    ///        before calling `lockBufferBottomOfPipe()`).
    /// @param[in,out] cmdList Active command list.
    /// @param[in] buffer Target buffer.
    /// @param[in] offset Byte offset.
    /// @param[in] size Byte length.
    /// @param[in] lockMode CPU lock mode.
    /// @return Mapped CPU pointer.
    [[nodiscard]] virtual void* lockBuffer(
        command::CommandListBase& cmdList,
        resource::Buffer* buffer,
        gp::UInt32 offset,
        gp::UInt32 size,
        resource::ResourceLockMode lockMode
    ) noexcept;

    /// @brief Unlocks a buffer previously locked via `lockBuffer()`.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] buffer Target buffer.
    virtual void unlockBuffer(command::CommandListBase& cmdList, resource::Buffer* buffer) noexcept;

    /// @brief Backend bottom-of-pipe buffer map executed directly on the RHI timeline.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] buffer Target buffer.
    /// @param[in] offset Byte offset.
    /// @param[in] size Byte length.
    /// @param[in] lockMode CPU lock mode.
    /// @return Mapped CPU pointer.
    [[nodiscard]] virtual void* lockBufferBottomOfPipe(
        command::CommandListBase& cmdList,
        resource::Buffer* buffer,
        gp::UInt32 offset,
        gp::UInt32 size,
        resource::ResourceLockMode lockMode
    ) noexcept = 0;

    /// @brief Backend bottom-of-pipe buffer unmap executed directly on the RHI timeline.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] buffer Target buffer.
    virtual void unlockBufferBottomOfPipe(command::CommandListBase& cmdList, resource::Buffer* buffer) noexcept = 0;

    /// @brief Replaces backing GPU allocations for each `(dst, src)` pair in `infos`.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] infos Span of resource replacement pairs.
    virtual void replaceResources(
        command::CommandListBase& cmdList, gp::ConstVectorView<resource::ResourceReplaceInfo> infos
    ) noexcept;

public:
    /// @brief Creates a `TextureInitializer` for allocating and populating a GPU `Texture`.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] createDesc Texture dimensions, format, mip count, flags, and initialization action.
    /// @return Move-only `resource::TextureInitializer`.
    [[nodiscard]] virtual resource::TextureInitializer createTextureInitializer(
        command::CommandListBase& cmdList, const resource::TextureCreateDesc& createDesc
    ) noexcept = 0;

    /// @brief Creates a `TextureReference` indirection wrapper pointing at `referencedTexture`.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] referencedTexture Optional initial target texture.
    /// @return Owning `resource::TextureReferenceRef`.
    [[nodiscard]] virtual resource::TextureReferenceRef
        createTextureReference(command::CommandListBase& cmdList, resource::Texture* referencedTexture) noexcept;

    /// @brief Rebinds `textureReference` to target `newTexture` on the RHI timeline.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] textureReference Target indirection wrapper.
    /// @param[in] newTexture New target texture (or `nullptr` for default texture).
    virtual void updateTextureReference(
        command::CommandListBase& cmdList, resource::TextureReference* textureReference, resource::Texture* newTexture
    ) noexcept;

    /// @brief Computes platform-aligned GPU allocation byte size and alignment for `desc`.
    /// @param[in] desc Texture descriptor.
    /// @param[in] firstMipIndex First resident mip level.
    /// @return Byte size and alignment.
    [[nodiscard]] virtual resource::CalcTextureSizeResult
        calcTexturePlatformSize(const resource::TextureDesc& desc, gp::UInt32 firstMipIndex = 0u) noexcept;

    /// @brief Estimates the GPU memory footprint in bytes consumed by `texture`.
    /// @param[in] texture Target texture resource (may be `nullptr`).
    /// @return Estimated byte footprint, or `0` if `texture` is null or unsupported.
    [[nodiscard]] virtual gp::UInt64 computeMemorySize(resource::Texture* texture) noexcept;

    /// @brief Populates hardware texture memory statistics into `outStats`.
    /// @param[out] outStats Destination memory statistics struct.
    virtual void getTextureMemoryStats(resource::TextureMemoryStats& outStats) noexcept;

    /// @brief Maps a texture subresource for CPU read or write access.
    /// @param[in,out] cmdList Immediate command list.
    /// @param[in] args Target texture, mip, slice, face, and lock mode.
    /// @return Mapped pointer, row pitch, and byte count.
    [[nodiscard]] virtual resource::TextureLockResult
        lockTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept = 0;

    /// @brief Unmaps a texture subresource previously locked via `lockTexture()`.
    /// @param[in,out] cmdList Immediate command list.
    /// @param[in] args Matching subresource lock arguments.
    virtual void
        unlockTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept = 0;

    /// @brief Uploads CPU pixel rows into a 2D texture sub-rectangle.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] texture Target 2D texture.
    /// @param[in] mipIndex Target mip level.
    /// @param[in] updateRegion Sub-rectangle in texels.
    /// @param[in] sourcePitch Row stride in bytes.
    /// @param[in] sourceData Source pixel bytes.
    virtual void updateTexture2D(
        command::CommandListBase& cmdList,
        resource::Texture* texture,
        gp::UInt32 mipIndex,
        const resource::UpdateTextureRegion2D& updateRegion,
        gp::UInt32 sourcePitch,
        const gp::UInt8* sourceData
    ) noexcept = 0;

    /// @brief Uploads CPU voxel slices into a 3D texture sub-box.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] texture Target 3D texture.
    /// @param[in] mipIndex Target mip level.
    /// @param[in] updateRegion Sub-box in texels.
    /// @param[in] sourceRowPitch Row stride in bytes.
    /// @param[in] sourceDepthPitch Slice stride in bytes.
    /// @param[in] sourceData Source voxel bytes.
    virtual void updateTexture3D(
        command::CommandListBase& cmdList,
        resource::Texture* texture,
        gp::UInt32 mipIndex,
        const resource::UpdateTextureRegion3D& updateRegion,
        gp::UInt32 sourceRowPitch,
        gp::UInt32 sourceDepthPitch,
        const gp::UInt8* sourceData
    ) noexcept = 0;

    /// @brief Allocates a staged CPU buffer for updating a 3D texture region.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] texture Target 3D texture.
    /// @param[in] mipIndex Target mip level.
    /// @param[in] updateRegion Sub-box in texels.
    /// @return Staged upload buffer descriptor.
    [[nodiscard]] virtual resource::UpdateTexture3DData beginUpdateTexture3D(
        command::CommandListBase& cmdList,
        resource::Texture* texture,
        gp::UInt32 mipIndex,
        const resource::UpdateTextureRegion3D& updateRegion
    ) noexcept;

    /// @brief Commits a staged 3D texture upload and frees any allocated staging bytes.
    /// @param[in,out] cmdList Active command list.
    /// @param[in,out] updateData Upload descriptor returned by `beginUpdateTexture3D()`.
    virtual void
        endUpdateTexture3D(command::CommandListBase& cmdList, resource::UpdateTexture3DData& updateData) noexcept;

    /// @brief Reads back 8-bit BGRA surface pixels from `texture` within `rect`.
    /// @param[in] texture Source GPU texture.
    /// @param[in] rect Sub-rectangle to read.
    /// @param[out] outData Destination color array.
    /// @param[in] flags Mip, slice, face, and conversion options.
    virtual void readSurfaceData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<gp::math::Color>& outData,
        const resource::ReadSurfaceDataFlags& flags
    ) noexcept = 0;

    /// @brief Reads back 32-bit linear RGBA surface pixels from `texture` within `rect`.
    /// @details Default implementation calls the `Color` overload and converts each pixel to `LinearColor`.
    /// @param[in] texture Source GPU texture.
    /// @param[in] rect Sub-rectangle to read.
    /// @param[out] outData Destination linear color array.
    /// @param[in] flags Mip, slice, face, and conversion options.
    virtual void readSurfaceData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<gp::math::LinearColor>& outData,
        const resource::ReadSurfaceDataFlags& flags
    ) noexcept;

    /// @brief Reads back 16-bit half-float surface pixels from `texture` within `rect`.
    /// @param[in] texture Source GPU texture.
    /// @param[in] rect Sub-rectangle to read.
    /// @param[out] outData Destination half-float color array.
    /// @param[in] cubeFace Target cubemap face.
    /// @param[in] arrayIndex Target array slice.
    /// @param[in] mipIndex Target mip level.
    virtual void readSurfaceFloatData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        gp::Vector<resource::Float16Color>& outData,
        resource::CubeFace cubeFace,
        gp::Int32 arrayIndex,
        gp::Int32 mipIndex
    ) noexcept;

public:
    /// @brief Creates a shader resource view over a GPU buffer.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] buffer Target buffer.
    /// @param[in] desc Buffer SRV descriptor.
    /// @return Owning `view::ShaderResourceViewRef`.
    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        command::CommandListBase& cmdList, resource::Buffer* buffer, const view::ViewDesc& desc
    ) noexcept = 0;

    /// @brief Creates a shader resource view over a GPU texture.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] texture Target texture.
    /// @param[in] desc Texture SRV descriptor.
    /// @return Owning `view::ShaderResourceViewRef`.
    [[nodiscard]] virtual view::ShaderResourceViewRef createShaderResourceView(
        command::CommandListBase& cmdList, resource::Texture* texture, const view::ViewDesc& desc
    ) noexcept = 0;

    /// @brief Creates an unordered access view over a GPU buffer.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] buffer Target buffer.
    /// @param[in] desc Buffer UAV descriptor.
    /// @return Owning `view::UnorderedAccessViewRef`.
    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase& cmdList, resource::Buffer* buffer, const view::ViewDesc& desc
    ) noexcept = 0;

    /// @brief Creates an unordered access view over a GPU texture.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] texture Target texture.
    /// @param[in] desc Texture UAV descriptor.
    /// @return Owning `view::UnorderedAccessViewRef`.
    [[nodiscard]] virtual view::UnorderedAccessViewRef createUnorderedAccessView(
        command::CommandListBase& cmdList, resource::Texture* texture, const view::ViewDesc& desc
    ) noexcept = 0;

public:
    /// @brief Creates a GPU-to-CPU synchronization fence with diagnostic label `name`.
    /// @param[in] name Diagnostic fence name.
    /// @return Owning `sync::GPUFenceRef`.
    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(gp::Name name) noexcept = 0;

    /// @brief Creates a CPU-readable staging buffer for GPU readback.
    /// @return Owning `resource::StagingBufferRef`.
    [[nodiscard]] virtual resource::StagingBufferRef createStagingBuffer() noexcept;

    /// @brief Maps `[offset, offset + numBytes)` of `stagingBuffer` into CPU memory.
    /// @param[in] stagingBuffer Target staging buffer.
    /// @param[in] fence Optional fence verifying GPU copy completion.
    /// @param[in] offset Byte offset.
    /// @param[in] numBytes Number of bytes to map.
    /// @return Mapped CPU pointer.
    [[nodiscard]] virtual void* lockStagingBuffer(
        resource::StagingBuffer* stagingBuffer, sync::GPUFence* fence, gp::UInt32 offset, gp::UInt32 numBytes
    ) noexcept;

    /// @brief Unmaps `stagingBuffer`.
    /// @param[in] stagingBuffer Target staging buffer.
    virtual void unlockStagingBuffer(resource::StagingBuffer* stagingBuffer) noexcept;

    /// @brief Creates a hardware render query of `queryType`.
    /// @param[in] queryType Metric captured (`Occlusion` or `AbsoluteTime`).
    /// @return Owning `sync::RenderQueryRef`.
    [[nodiscard]] virtual sync::RenderQueryRef createRenderQuery(sync::RenderQueryType queryType) noexcept = 0;

    /// @brief Creates a reusable pool of render queries of `queryType`.
    /// @param[in] queryType Query metric type.
    /// @param[in] numQueries Optional initial capacity hint.
    /// @return Owning `sync::RenderQueryPoolRef`.
    [[nodiscard]] virtual sync::RenderQueryPoolRef
        createRenderQueryPool(sync::RenderQueryType queryType, gp::UInt32 numQueries = 0u) noexcept;

    /// @brief Reads the result of a previously ended render query.
    /// @param[in] renderQuery Target query object.
    /// @param[out] outResult Destination query result.
    /// @param[in] wait True to block until available.
    /// @param[in] gpuIndex Multi-GPU node index (`0` default).
    /// @return True when available.
    [[nodiscard]] virtual bool getRenderQueryResult(
        sync::RenderQuery* renderQuery, gp::UInt64& outResult, bool wait, gp::UInt32 gpuIndex = 0u
    ) noexcept = 0;

    /// @brief Hook invoked at the top of the pipe when a `writeGPUFence` command is recorded.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] fence Target GPU fence.
    virtual void writeGPUFenceTopOfPipe(command::CommandListBase& cmdList, sync::GPUFence* fence) noexcept;

    /// @brief Hook invoked at the top of the pipe when `beginRenderQuery` is recorded.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] renderQuery Target query.
    virtual void beginRenderQueryTopOfPipe(command::CommandListBase& cmdList, sync::RenderQuery* renderQuery) noexcept;

    /// @brief Hook invoked at the top of the pipe when `endRenderQuery` is recorded.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] renderQuery Target query.
    virtual void endRenderQueryTopOfPipe(command::CommandListBase& cmdList, sync::RenderQuery* renderQuery) noexcept;

    /// @brief Returns the byte size of backend private data appended to every `sync::Transition` allocation.
    /// @return Byte size (`0` when backend stores no private transition payload).
    [[nodiscard]] virtual gp::USize getTransitionPrivateDataSizeInBytes() const noexcept
    {
        return 0u;
    }

    /// @brief Returns the required alignment for backend private data appended to every `sync::Transition`.
    /// @return Power-of-two alignment in bytes (`0` when private data size is zero).
    [[nodiscard]] virtual gp::USize getTransitionPrivateDataAlignInBytes() const noexcept
    {
        return 0u;
    }

    /// @brief Initializes backend private data inside `transition` from `createInfo`.
    /// @param[in,out] transition Newly allocated `sync::Transition`.
    /// @param[in] createInfo Source/destination pipelines, flags, transitions, and aliasing operations.
    virtual void createTransition(sync::Transition* transition, const sync::TransitionCreateInfo& createInfo) noexcept
    {
        static_cast<void>(transition);
        static_cast<void>(createInfo);
    }

    /// @brief Releases backend private data inside `transition` right before memory is freed.
    /// @param[in,out] transition Retiring `sync::Transition`.
    virtual void releaseTransition(sync::Transition* transition) noexcept
    {
        static_cast<void>(transition);
    }

public:
    /// @brief Creates a presentation swapchain viewport bound to `windowHandle`.
    /// @param[in] windowHandle Native OS window handle.
    /// @param[in] sizeX Initial width in pixels.
    /// @param[in] sizeY Initial height in pixels.
    /// @param[in] isFullscreen True to start in fullscreen mode.
    /// @param[in] preferredPixelFormat Preferred backbuffer pixel format.
    /// @return Owning `presentation::ViewportRef`.
    [[nodiscard]] virtual presentation::ViewportRef createViewport(
        void* windowHandle, gp::UInt32 sizeX, gp::UInt32 sizeY, bool isFullscreen, PixelFormat preferredPixelFormat
    ) noexcept = 0;

    /// @brief Resizes or toggles fullscreen mode on an existing presentation viewport.
    /// @param[in] viewport Target viewport.
    /// @param[in] sizeX New width in pixels.
    /// @param[in] sizeY New height in pixels.
    /// @param[in] isFullscreen True for fullscreen mode.
    /// @param[in] preferredPixelFormat Updated pixel format (`Unknown` keeps current format).
    virtual void resizeViewport(
        presentation::Viewport* viewport,
        gp::UInt32 sizeX,
        gp::UInt32 sizeY,
        bool isFullscreen,
        PixelFormat preferredPixelFormat = PixelFormat::Unknown
    ) noexcept = 0;

    /// @brief Returns the current swapchain backbuffer texture for `viewport`.
    /// @param[in] viewport Target presentation viewport.
    /// @return Owning `resource::TextureRef` for the active backbuffer.
    [[nodiscard]] virtual resource::TextureRef getViewportBackBuffer(presentation::Viewport* viewport) noexcept = 0;

    /// @brief Advances the internal swapchain frame index for `viewport`.
    /// @param[in] viewport Target presentation viewport.
    virtual void advanceFrameForGetViewportBackBuffer(presentation::Viewport* viewport) noexcept = 0;

    /// @brief Begins drawing commands targeting `viewport`.
    /// @param[in] viewport Target viewport.
    /// @param[in] renderTarget Optional backbuffer texture override.
    virtual void beginDrawingViewport(presentation::Viewport* viewport, resource::Texture* renderTarget) noexcept = 0;

    /// @brief Finishes drawing into `viewport` and optionally presents the backbuffer.
    /// @param[in] viewport Target viewport.
    /// @param[in] present True to present.
    /// @param[in] lockToVsync True to sync to vertical blank.
    virtual void endDrawingViewport(presentation::Viewport* viewport, bool present, bool lockToVsync) noexcept = 0;

    /// @brief Marks the beginning of a frame on the RHI backend.
    virtual void beginFrame() noexcept
    {}

    /// @brief Marks the end of a frame on the RHI backend.
    /// @param[in] args Frame boundary metadata.
    virtual void endFrame(const EndFrameArgs& args) noexcept
    {
        static_cast<void>(args);
    }

    /// @brief Render-thread end-of-frame hook invoked by `CommandListImmediate::endFrame()`.
    virtual void endFrameRenderThread() noexcept;

    /// @brief Enumerates supported display modes into `outResolutions`.
    /// @param[out] outResolutions Destination vector appended with supported resolutions.
    /// @param[in] ignoreRefreshRate True to deduplicate identical `(width, height)` entries.
    /// @return True when enumeration succeeded.
    [[nodiscard]] virtual bool
        getSupportedResolutions(gp::Vector<ScreenResolution>& outResolutions, bool ignoreRefreshRate = false) noexcept;

    /// @brief Adjusts `(inOutWidth, inOutHeight)` to the nearest display resolution supported by the backend.
    /// @param[in,out] inOutWidth Requested width; updated to the closest supported width.
    /// @param[in,out] inOutHeight Requested height; updated to the closest supported height.
    virtual void getSupportedResolution(gp::UInt32& inOutWidth, gp::UInt32& inOutHeight) noexcept;

    /// @brief Maps a CPU-readable staging texture surface after `fence` signals.
    /// @param[in] texture Source staging texture.
    /// @param[in] fence Optional fence guarding GPU completion.
    /// @param[out] outData Receives mapped CPU pixel pointer.
    /// @param[out] outWidth Receives mapped row width in pixels.
    /// @param[out] outHeight Receives mapped height in pixels.
    /// @param[in] gpuIndex Multi-GPU node index (`0` default).
    virtual void mapStagingSurface(
        resource::Texture* texture,
        sync::GPUFence* fence,
        void*& outData,
        gp::Int32& outWidth,
        gp::Int32& outHeight,
        gp::UInt32 gpuIndex = 0u
    ) noexcept;

    /// @brief Unmaps a staging texture surface previously mapped via `mapStagingSurface()`.
    /// @param[in] texture Target staging texture.
    /// @param[in] gpuIndex Multi-GPU node index (`0` default).
    virtual void unmapStagingSurface(resource::Texture* texture, gp::UInt32 gpuIndex = 0u) noexcept;

    /// @brief Reads back 16-bit half-float RGBA pixels from a 3D volume texture slice range into `outData`.
    /// @param[in] texture Source 3D texture.
    /// @param[in] rect 2D sub-rectangle in texels.
    /// @param[in] zMinMax Depth slice range `[minZ, maxZ)`.
    /// @param[out] outData Destination vector receiving half-float colors.
    virtual void read3DSurfaceFloatData(
        resource::Texture* texture,
        const resource::IntRect& rect,
        resource::IntPoint zMinMax,
        gp::Vector<resource::Float16Color>& outData
    ) noexcept;

    /// @brief Aliases backing GPU memory from `srcTexture` onto `destTexture`.
    /// @param[in,out] destTexture Destination texture receiving aliased storage.
    /// @param[in,out] srcTexture Source texture providing backing storage.
    virtual void aliasTextureResources(resource::TextureRef& destTexture, resource::TextureRef& srcTexture) noexcept;

    /// @brief Blocks until all submitted GPU work has finished executing.
    virtual void blockUntilGPUIdle() noexcept = 0;

    /// @brief Attaches a backend debug label name to `texture`.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] texture Target texture.
    /// @param[in] name Null-terminated debug name.
    virtual void
        bindDebugLabelName(command::CommandListBase& cmdList, resource::Texture* texture, const char* name) noexcept;

    /// @brief Attaches a backend debug label name to `buffer`.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] buffer Target buffer.
    /// @param[in] name Null-terminated debug name.
    virtual void
        bindDebugLabelName(command::CommandListBase& cmdList, resource::Buffer* buffer, const char* name) noexcept;

    /// @brief Attaches a backend debug label name to `uav`.
    /// @param[in,out] cmdList Active command list.
    /// @param[in] uav Target unordered access view.
    /// @param[in] name Null-terminated debug name.
    virtual void bindDebugLabelName(
        command::CommandListBase& cmdList, view::UnorderedAccessView* uav, const char* name
    ) noexcept;

public:
    /// @brief Returns the default immediate graphics `ICommandContext`.
    /// @return Non-null pointer to the primary graphics command context.
    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept = 0;

    /// @brief Returns the default immediate async compute context (defaults to `getDefaultContext()`).
    /// @return Pointer to the primary async compute context.
    [[nodiscard]] virtual command::IComputeContext* getDefaultAsyncComputeContext() noexcept
    {
        return getDefaultContext();
    }

    /// @brief Acquires a parallel command context for `pipeline` (`nullptr` if parallel recording is unsupported).
    /// @param[in] pipeline Target execution pipeline (`Graphics` or `AsyncCompute`).
    /// @return Newly acquired context, or `nullptr` to fall back to the default context.
    [[nodiscard]] virtual command::IComputeContext* getCommandContext(sync::PipelineStage pipeline) noexcept
    {
        static_cast<void>(pipeline);
        return nullptr;
    }

    /// @brief Acquires an optional backend upload context (`nullptr` if unsupported).
    /// @return Pointer to an `IUploadContext`, or `nullptr`.
    [[nodiscard]] virtual command::IUploadContext* acquireUploadContext() noexcept
    {
        return nullptr;
    }

    /// @brief Finalizes recorded command contexts into backend platform command lists ready for submission.
    /// @param[in,out] args Contexts to finalize.
    /// @param[out] outResult Finalized platform command lists.
    virtual void finalizeContexts(FinalizeContextsArgs&& args, FinalizeContextsResult& outResult) noexcept
    {
        static_cast<void>(args);
        outResult.commandLists.clear();
    }

    /// @brief Submits finalized platform command lists to hardware GPU queues.
    /// @param[in,out] args Finalized command lists and submission flags.
    virtual void submitCommandLists(SubmitCommandListsArgs&& args) noexcept
    {
        static_cast<void>(args);
    }

protected:
    /// @brief Tracks an outstanding CPU write-staging buffer allocated by default `DynamicRHI::lockBuffer()`.
    struct LockedBufferRecord
    {
        /// @brief Locked target GPU buffer.
        resource::Buffer* buffer{ nullptr };

        /// @brief Byte offset into `buffer`.
        gp::UInt32 offset{ 0u };

        /// @brief Byte size of the locked range.
        gp::UInt32 size{ 0u };

        /// @brief Lock access mode requested by the caller.
        resource::ResourceLockMode lockMode{ resource::ResourceLockMode::ReadOnly };

        /// @brief Temporary CPU staging buffer (for staged writes) or direct mapped pointer.
        void* stagingData{ nullptr };

        /// @brief True when `stagingData` is a temporary CPU staging allocation committed on unlock.
        bool isStagedWrite{ false };
    };

    /// @brief Mutex protecting `m_lockedBuffers`.
    gp::thread::Mutex m_lockTrackerMutex{};

    /// @brief Active top-of-pipe buffer lock records tracked by the base `DynamicRHI::lockBuffer()` helper.
    gp::Vector<LockedBufferRecord> m_lockedBuffers{};

    /// @brief Monotonic frame counter incremented by `endFrameRenderThread()`.
    gp::UInt64 m_frameCounter{ 0u };
};

/// @brief Returns the active global `DynamicRHI` instance (may be `nullptr` before `rhiInit()`).
/// @return Non-owning pointer to the active `DynamicRHI`.
[[nodiscard]] GP_RHI_API DynamicRHI* getDynamicRHI() noexcept;

/// @brief Installs or clears the global `DynamicRHI` pointer (used by `rhiInit()`, `rhiExit()`, and tests).
/// @param[in] rhi New active `DynamicRHI` pointer, or `nullptr`.
GP_RHI_API void setDynamicRHI(DynamicRHI* rhi) noexcept;

/// @brief Reports whether a valid `DynamicRHI` instance is currently installed.
/// @return True when `getDynamicRHI() != nullptr`.
[[nodiscard]] GP_RHI_API bool hasValidDynamicRHI() noexcept;

/// @brief Returns the `InterfaceType` of the active `DynamicRHI`, or `InterfaceType::NotSet` when uninitialized.
/// @return Active `InterfaceType`.
[[nodiscard]] GP_RHI_API InterfaceType getInterfaceType() noexcept;

/// @brief Statically casts the active (non-validation) `DynamicRHI` to concrete backend type `InRHIType*`.
/// @tparam InRHIType Concrete `DynamicRHI` subclass.
/// @param[in] rhi Optional explicit `DynamicRHI*` (defaults to `getDynamicRHI()`).
/// @return Pointer to `InRHIType`, or `nullptr` if no `DynamicRHI` is installed.
template <typename InRHIType>
[[nodiscard]] inline InRHIType* castDynamicRHI(DynamicRHI* const rhi = getDynamicRHI()) noexcept
{
    return (rhi != nullptr) ? static_cast<InRHIType*>(rhi->getNonValidationRHI()) : nullptr;
}

/// @brief Helper creating an immutable `UniformBufferLayout` from `desc`.
/// @param[in] desc Uniform buffer layout creation descriptor.
/// @return Owning `resource::UniformBufferLayoutRef`.
[[nodiscard]] GP_RHI_API resource::UniformBufferLayoutRef
    createUniformBufferLayout(const resource::UniformBufferLayoutDesc& desc) noexcept;

/// @brief Helper creating a `RenderQueryPool` through the active `DynamicRHI`.
/// @param[in] queryType Query metric type.
/// @param[in] numQueries Optional initial capacity hint.
/// @return Owning `sync::RenderQueryPoolRef`.
[[nodiscard]] GP_RHI_API sync::RenderQueryPoolRef
    createRenderQueryPool(sync::RenderQueryType queryType, gp::UInt32 numQueries = 0u) noexcept;

/// @brief Helper allocating a `sync::Transition` through `TransitionController` using the active backend hooks.
/// @param[in] createInfo Source/destination pipelines, flags, transitions, and aliasing info.
/// @return Newly allocated `sync::Transition*`.
[[nodiscard]] GP_RHI_API sync::Transition* createTransition(const sync::TransitionCreateInfo& createInfo) noexcept;

/// @brief Initializes the RHI subsystem with `rhi`, wiring `TransitionController` hooks, calling `rhi->init()`, and
///        configuring `getRHIGlobals().isRHIInitialized`.
/// @param[in] rhi Newly allocated backend `DynamicRHI` instance (ownership transferred until `rhiExit()`).
GP_RHI_API void rhiInit(DynamicRHI* rhi) noexcept;

/// @brief Runs secondary post-initialization (`postInit()`) on the active `DynamicRHI`.
GP_RHI_API void rhiPostInit() noexcept;

/// @brief Flushes all commands and deferred deletions, calls `shutdown()`, deletes the active `DynamicRHI`, resets
///        `TransitionController` hooks, and resets `getRHIGlobals()`.
GP_RHI_API void rhiExit() noexcept;

}   // namespace gp::rhi
