// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/DeviceForward.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "RHIBase.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPixelFormat.hpp"

namespace gp::rhi
{

/// @brief Minimum clip-space Z value (`0.0f` for D3D/Vulkan/Metal, `-1.0f` for legacy OpenGL).
GP_RHI_API extern gp::Float32 g_minClipZ;

/// @brief Clip-space Y projection sign (`1.0f` normal, `-1.0f` when inverted).
GP_RHI_API extern gp::Float32 g_projectionSignY;

/// @brief Per-vertex-element format support table populated by the active RHI backend during `init()`.
struct VertexElementTypeSupportInfo
{
    /// @brief Boolean lookup indexed by `pipeline::VertexElementType`.
    gp::Array<bool, pipeline::kVertexElementTypeCount> elementCaps{};

    /// @brief Constructs a table with every vertex element format marked unsupported until initialized.
    constexpr VertexElementTypeSupportInfo() noexcept = default;

    /// @brief Queries whether `type` is supported by the active hardware backend.
    /// @param[in] type Vertex element format to query.
    /// @return True when supported.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isSupported(const pipeline::VertexElementType type) const noexcept
    {
        const gp::USize index = static_cast<gp::USize>(type);
        return (index < elementCaps.size()) ? elementCaps[index] : false;
    }

    /// @brief Sets hardware support status for `type`.
    /// @param[in] type Vertex element format.
    /// @param[in] supported True if supported.
    GP_FORCEINLINE_HINT constexpr void
        setSupported(const pipeline::VertexElementType type, const bool supported) noexcept
    {
        const gp::USize index = static_cast<gp::USize>(type);
        if (index < elementCaps.size())
        {
            elementCaps[index] = supported;
        }
    }
};

/// @brief Hardware ray tracing capability flags and limits reported by the active backend.
struct RayTracingCapabilities
{
    /// @brief True when hardware ray tracing pipelines and acceleration structures are supported.
    bool supported{ false };

    /// @brief True when inline ray query intrinsics are supported inside compute/graphics shaders.
    bool supportsInlineRayTracing{ false };

    /// @brief True when acceleration structures can be built asynchronously on the compute queue.
    bool supportsAsyncBuild{ false };

    /// @brief True when ray tracing shader binding tables can be updated in parallel.
    bool supportsPersistentSBTs{ false };

    /// @brief Required byte alignment for acceleration structure buffers.
    gp::UInt32 accelerationStructureAlignment{ 256u };

    /// @brief Required byte alignment for acceleration structure build scratch buffers.
    gp::UInt32 scratchBufferAlignment{ 256u };

    /// @brief Required byte alignment for shader binding table records.
    gp::UInt32 shaderTableAlignment{ 64u };
};

/// @brief Variable Rate Shading (VRS) tier and tile-size capabilities reported by the active backend.
struct VariableRateShadingCapabilities
{
    /// @brief True when Tier 1 per-draw shading rate selection is supported.
    bool supportsPipeline{ false };

    /// @brief True when Tier 2 screen-space shading rate image attachments are supported.
    bool supportsAttachment{ false };

    /// @brief True when larger coarse pixel sizes (`2x4`, `4x2`, `4x4`) are supported.
    bool supportsLargerVariableRates{ false };

    /// @brief Width in pixels of each tile in a VRS attachment texture.
    gp::UInt32 imageTileMinWidth{ 0u };

    /// @brief Height in pixels of each tile in a VRS attachment texture.
    gp::UInt32 imageTileMinHeight{ 0u };

    /// @brief Maximum tile width in pixels for VRS attachment textures.
    gp::UInt32 imageTileMaxWidth{ 0u };

    /// @brief Maximum tile height in pixels for VRS attachment textures.
    gp::UInt32 imageTileMaxHeight{ 0u };
};

/// @brief Shader bundle and work graph capabilities reported by the active backend.
struct ShaderBundleCapabilities
{
    /// @brief True when GPU-driven shader bundles are supported.
    bool supported{ false };

    /// @brief True when work graphs are supported.
    bool supportsWorkGraphs{ false };
};

/// @brief Reserved / tiled virtual GPU resource capabilities reported by the active backend.
struct ReservedResourceCapabilities
{
    /// @brief True when reserved buffers and commit transitions are supported.
    bool supported{ false };

    /// @brief Byte size of one physical GPU tile page (typically 64 KiB).
    gp::UInt64 tileSizeInBytes{ 65536u };
};

/// @brief Parallel render-pass command recording capabilities reported by the active backend.
struct ParallelRenderPassCapabilities
{
    /// @brief True when secondary/parallel command lists may record inside one render pass simultaneously.
    bool supported{ false };
};

/// @brief Adapter identification metadata populated by the active RHI backend during startup.
struct GPUInfo
{
    /// @brief Human-readable adapter description string.
    gp::String adapterName{};

    /// @brief PCI vendor identifier (`GpuVendorId`).
    GpuVendorId vendorId{ GpuVendorId::NotQueried };

    /// @brief Hardware PCI device ID.
    gp::UInt32 deviceId{ 0u };

    /// @brief Hardware revision ID.
    gp::UInt32 deviceRevision{ 0u };

    /// @brief Driver version string reported by the vendor API.
    gp::String driverVersion{};

    /// @brief Driver release date string.
    gp::String driverDate{};

    /// @brief Dedicated local video memory reported by the active adapter in bytes.
    gp::UInt64 dedicatedVideoMemory{ 0u };

    /// @brief True when the active adapter is an integrated / unified memory GPU.
    bool deviceIsIntegrated{ false };
};

/// @brief Global RHI capability, limit, pixel-format, and adapter telemetry table populated by the active backend.
/// @details Accessed via `getRHIGlobals()`. Reset cleanly by `reset()` during `rhiExit()` so backend swaps in tests
///          or tools never observe stale limits from a prior device.
struct GP_RHI_API RHIGlobals
{
    /// @brief True after `rhiInit()` has successfully initialized a `DynamicRHI`.
    bool isRHIInitialized{ false };

    /// @brief True when the active backend is the headless `NullDynamicRHI`.
    bool usingNullRHI{ false };

    /// @brief True when backend API debug validation layers are enabled.
    bool isDebugLayerEnabled{ false };

    /// @brief True when the active backend supports multithreaded RHI command submission.
    bool supportsRHIThread{ false };

    /// @brief True when the active backend supports general multithreaded RHI access.
    bool supportsMultithreading{ false };

    /// @brief True when the active backend supports executing RHI translation on task threads.
    bool supportsRHIOnTaskThread{ false };

    /// @brief True when the active backend supports parallel command list recording.
    bool supportsParallelRHIExecute{ false };

    /// @brief True when resources may be created concurrently across worker threads.
    bool supportsMultithreadedResources{ false };

    /// @brief True when `DynamicRHI::createBufferInitializer()` / `createTextureInitializer()` support async creation.
    bool supportsAsyncTextureCreation{ false };

    /// @brief True when asynchronous compute queues are available.
    bool supportsAsyncCompute{ false };

    /// @brief True when async compute executes efficiently without heavy queue serialization penalties.
    bool supportsEfficientAsyncCompute{ false };

    /// @brief True when hardware depth bounds testing is available.
    bool supportsDepthBoundsTest{ false };

    /// @brief True when hardware mesh and amplification shaders are supported.
    bool supportsMeshShadersTier0{ false };

    /// @brief True when mesh shader Tier 1 (primitive attributes, full limits) is supported.
    bool supportsMeshShadersTier1{ false };

    /// @brief True when bindless resource descriptors are enabled on the active device.
    bool supportsBindlessResources{ false };

    /// @brief True when bindless sampler descriptors are enabled on the active device.
    bool supportsBindlessSamplers{ false };

    /// @brief True when hardware timestamp queries (`RenderQueryType::AbsoluteTime`) are supported.
    bool supportsTimestampQueries{ false };

    /// @brief True when hardware occlusion queries (`RenderQueryType::Occlusion`) are supported.
    bool supportsOcclusionQueries{ false };

    /// @brief True when indirect draw commands are natively supported.
    bool supportsDrawIndirect{ true };

    /// @brief True when `multiDrawIndexedPrimitiveIndirect` is natively supported.
    bool supportsMultiDrawIndirect{ false };

    /// @brief True when GPU-driven indirect draw count buffers are supported.
    bool supportsIndirectDrawCount{ false };

    /// @brief True when transient memory aliasing heaps are supported.
    bool supportsTransientResourceAliasing{ false };

    /// @brief True when transient `ResourceAccess::Discard` transitions must resolve their prior state from tracked
    ///        resource state rather than treating discard as an undefined layout transition.
    bool needsTransientDiscardStateTracking{ false };

    /// @brief True when dynamic uniform buffer offset binding is supported.
    bool supportsDynamicUniformBufferOffsets{ false };

    /// @brief True when texture streaming is supported on this backend.
    bool supportsTextureStreaming{ false };

    /// @brief True when 3D volume textures are supported.
    bool supportsTexture3D{ true };

    /// @brief True when texture views / format reinterpretation are supported.
    bool supportsTextureViews{ true };

    /// @brief True when UAV views may alias a different pixel format than the backing texture.
    bool supportsUAVFormatAliasing{ false };

    /// @brief True when pixel shaders may bind unordered access views.
    bool supportsPixelShaderUAVs{ true };

    /// @brief True when per-render-target blend states are supported independently.
    bool supportsSeparateRenderTargetBlendState{ true };

    /// @brief True when dual-source color blending is supported.
    bool supportsDualSrcBlending{ false };

    /// @brief True when conservative rasterization is supported.
    bool supportsConservativeRasterization{ false };

    /// @brief True when inline 4x32-bit shader root constants are supported.
    bool supportsShaderRootConstants{ false };

    /// @brief True when 64-bit integer atomics are supported in shaders.
    bool supportsAtomicUInt64{ false };

    /// @brief True when split resource transitions (`beginTransitions`/`endTransitions`) are natively supported.
    bool supportsSplitBarriers{ false };

    /// @brief True when `ResourceLockMode::WriteOnlyNoOverwrite` is supported without stalling.
    bool supportsMapWriteNoOverwrite{ false };

    /// @brief True when asynchronous PSO precaching is supported.
    bool supportsPSOPrecaching{ false };

    /// @brief True when serialized PSO file caches are supported.
    bool supportsPipelineFileCache{ false };

    /// @brief True when the backend requires explicit SRV/UAV unbind calls on resource state changes.
    bool needsShaderUnbinds{ true };

    /// @brief True when deferred resource deletion requires extra frame latency beyond a single fence.
    bool needsExtraDeletionLatency{ false };

    /// @brief True when HDR swapchain presentation is supported by the active display and backend.
    bool supportsHDROutput{ false };

    /// @brief True when HDR output is currently active.
    bool isHDREnabled{ false };

    /// @brief Monotonic count of completed viewport presentations.
    gp::UInt32 presentCounter{ 0u };

    /// @brief True when hardware wave/subgroup operations are available.
    bool supportsWaveOperations{ false };

    /// @brief Minimum wave/subgroup lane count reported by the GPU (`0` when unsupported).
    gp::UInt32 minimumWaveSize{ 0u };

    /// @brief Maximum wave/subgroup lane count reported by the GPU (`0` when unsupported).
    gp::UInt32 maximumWaveSize{ 0u };

    /// @brief Maximum constant/uniform buffer byte size supported by shaders.
    gp::UInt32 maxConstantBufferByteSize{ 65536u };

    /// @brief Maximum compute threadgroup shared memory size in bytes.
    gp::UInt32 maxComputeSharedMemory{ 32768u };

    /// @brief Maximum simultaneous sampler bindings per shader stage.
    gp::UInt32 maxTextureSamplers{ 16u };

    /// @brief Maximum simultaneous UAV bindings per shader stage.
    gp::UInt32 maxSimultaneousUAVs{ 8u };

    /// @brief Maximum supported 2D texture width or height in texels.
    gp::UInt32 maxTextureDimensions{ 16384u };

    /// @brief Maximum supported 3D volume texture dimension in texels.
    gp::UInt32 maxVolumeTextureDimensions{ 2048u };

    /// @brief Maximum supported cubemap face dimension in texels.
    gp::UInt32 maxCubeTextureDimensions{ 16384u };

    /// @brief Maximum supported texture array slice count.
    gp::UInt32 maxTextureArrayLayers{ 2048u };

    /// @brief Maximum supported mipmap levels in a single texture.
    gp::UInt32 maxTextureMipCount{ kMaxTextureMipCount };

    /// @brief Maximum supported buffer allocation size in bytes.
    gp::UInt64 maxBufferSize{ 1ull << 31u };

    /// @brief Maximum dispatch thread group count along X, Y, or Z.
    gp::UInt32 maxDispatchThreadGroupsPerDimension{ 65535u };

    /// @brief Maximum active GPU nodes in multi-GPU configurations (`1` for single GPU).
    gp::UInt32 numGPUs{ 1u };

    /// @brief Highest feature level supported by the active backend.
    FeatureLevel maxSupportedFeatureLevel{ FeatureLevel::SM5 };

    /// @brief Hardware support level for static shader binding layouts.
    StaticShaderBindingLayoutSupport staticShaderBindingLayoutSupport{ StaticShaderBindingLayoutSupport::Unsupported };

    /// @brief Active adapter identification and driver version metadata.
    GPUInfo gpuInfo{};

    /// @brief Supported vertex attribute format table.
    VertexElementTypeSupportInfo vertexElementTypeSupport{};

    /// @brief Per-pixel-format hardware usage capability bitmasks.
    gp::Array<PixelFormatCapabilities, kPixelFormatCount> pixelFormatCapabilities{};

    /// @brief Ray tracing capability block.
    RayTracingCapabilities rayTracing{};

    /// @brief Variable Rate Shading capability block.
    VariableRateShadingCapabilities variableRateShading{};

    /// @brief Shader bundle and work graph capability block.
    ShaderBundleCapabilities shaderBundles{};

    /// @brief Reserved/tiled resource capability block.
    ReservedResourceCapabilities reservedResources{};

    /// @brief Parallel render-pass capability block.
    ParallelRenderPassCapabilities parallelRenderPasses{};

    /// @brief Resets all capability flags, limits, and metadata back to safe defaults.
    void reset() noexcept;

    /// @brief Returns the capability flags registered for `format`.
    /// @param[in] format Pixel format to query.
    /// @return `PixelFormatCapabilities` mask (or `None` if out of range).
    [[nodiscard]] PixelFormatCapabilities getPixelFormatCapabilities(const PixelFormat format) const noexcept
    {
        const gp::USize index = static_cast<gp::USize>(format);
        return (index < pixelFormatCapabilities.size()) ? pixelFormatCapabilities[index]
                                                        : PixelFormatCapabilities::None;
    }

    /// @brief Updates the capability flags registered for `format`.
    /// @param[in] format Pixel format to configure.
    /// @param[in] capabilities Hardware capability bitmask.
    void setPixelFormatCapabilities(const PixelFormat format, const PixelFormatCapabilities capabilities) noexcept
    {
        const gp::USize index = static_cast<gp::USize>(format);
        if (index < pixelFormatCapabilities.size())
        {
            pixelFormatCapabilities[index] = capabilities;
        }
    }
};

/// @brief Returns the global `RHIGlobals` capability and telemetry table.
/// @return Mutable reference to the process-wide `RHIGlobals`.
[[nodiscard]] GP_RHI_API RHIGlobals& getRHIGlobals() noexcept;

}   // namespace gp::rhi
