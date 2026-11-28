// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/DeviceForward.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Bitwise.hpp"
#include "maths/vector/Vector2.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "RHIBase.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPixelFormat.hpp"
#include "views/ViewEnums.hpp"
#include <type_traits>

namespace gp::rhi
{

/// @brief Minimum clip-space Z value (`0.0f` for D3D/Vulkan/Metal, `-1.0f` for legacy OpenGL).
GP_RHI_API extern gp::Float32 g_minClipZ;

/// @brief Clip-space Y projection sign (`1.0f` normal, `-1.0f` when inverted).
GP_RHI_API extern gp::Float32 g_projectionSignY;

/// @brief Total number of entries in `RHIGlobals::defaultMSAASampleOffsets` (`1 + 2 + 4 + 8 + 16`).
inline constexpr gp::USize kDefaultMSAASampleOffsetCount = 1u + 2u + 4u + 8u + 16u;

/// @brief Wrapper around RHI capability/limit values supporting preview feature-level clamping overrides.
/// @details Mirrors preview-overridable RHI globals so mobile or lower-tier preview modes can restrict booleans via
///          logical-AND and integral limits via `min(previewValue, hardwareValue)` without losing the underlying
///          hardware capability.
/// @tparam InValueType Value type wrapped by this global (`bool`, integer limit, or enum).
template <typename InValueType>
class RHIGlobal
{
public:
    /// @brief Constructs a zero/default-initialized `RHIGlobal`.
    constexpr RHIGlobal() noexcept = default;

    /// @brief Constructs an `RHIGlobal` initialized to `inValue`.
    /// @param[in] inValue Initial hardware value.
    constexpr explicit(false) RHIGlobal(const InValueType& inValue) noexcept
        : m_value(inValue)
    {}

    /// @brief Updates the underlying hardware value (retaining any active preview override).
    /// @param[in] inValue New hardware value.
    /// @return Reference to `*this`.
    constexpr RHIGlobal& operator=(const InValueType& inValue) noexcept
    {
        m_value = inValue;
        return *this;
    }

    /// @brief Installs a preview override clamped against the hardware value for booleans and integers.
    /// @param[in] inValue Requested preview ceiling/flag.
    constexpr void setPreviewOverride(const InValueType& inValue) noexcept
    {
        m_previewValue = inValue;
        m_hasPreviewOverride = true;
    }

    /// @brief Clears any active preview override so queries return the raw hardware value.
    constexpr void clearPreviewOverride() noexcept
    {
        m_hasPreviewOverride = false;
        m_previewValue = InValueType{};
    }

    /// @brief Reports whether a preview override is currently active.
    /// @return True when `setPreviewOverride()` has been called without `clearPreviewOverride()`.
    [[nodiscard]] constexpr bool hasPreviewOverride() const noexcept
    {
        return m_hasPreviewOverride;
    }

    /// @brief Returns the raw un-overridden hardware value.
    /// @return Underlying hardware value.
    [[nodiscard]] constexpr InValueType getHardwareValue() const noexcept
    {
        return m_value;
    }

    /// @brief Resolves the effective value (clamped by any active preview override).
    /// @return Effective capability or limit value.
    [[nodiscard]] constexpr InValueType getValue() const noexcept
    {
        if (!m_hasPreviewOverride)
        {
            return m_value;
        }

        if constexpr (std::is_same_v<InValueType, bool>)
        {
            return m_previewValue && m_value;
        }
        else if constexpr (std::is_integral_v<InValueType>)
        {
            return gp::math::min<InValueType>(m_previewValue, m_value);
        }
        else
        {
            return m_previewValue;
        }
    }

    /// @brief Implicit conversion returning `getValue()`.
    /// @return Effective value.
    [[nodiscard]] constexpr operator InValueType() const noexcept
    {
        return getValue();
    }

private:
    InValueType m_value{};
    InValueType m_previewValue{};
    bool m_hasPreviewOverride{ false };
};

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
    /// @brief True when hardware ray tracing acceleration structures and ray tracing workloads are supported.
    bool supported{ false };

    /// @brief True when full ray tracing shader pipelines (raygen, miss, hit group, callable) are supported.
    bool supportsShaders{ false };

    /// @brief True when inline ray query intrinsics are supported inside compute/graphics shaders.
    bool supportsInlineRayTracing{ false };

    /// @brief True when new shaders may be incrementally appended to an existing ray tracing PSO.
    bool supportsPSOAdditions{ false };

    /// @brief True when indirect ray tracing dispatch commands are supported.
    bool supportsDispatchIndirect{ false };

    /// @brief True when acceleration structures can be built asynchronously on the compute queue.
    bool supportsAsyncBuild{ false };

    /// @brief True when top-level acceleration structures support GPU-driven indirect builds.
    bool supportsBuildTopLevelAccelerationStructureIndirect{ false };

    /// @brief True when ray tracing dispatches may run on the async compute queue.
    bool supportsAsyncRayTraceDispatch{ false };

    /// @brief True when indirect ray tracing dispatches may run on the async compute queue.
    bool supportsAsyncRayTraceDispatchIndirect{ false };

    /// @brief True when cluster-level acceleration structure operations are supported.
    bool supportsClusterOps{ false };

    /// @brief True when vendor hit-token intrinsics are supported.
    bool supportsAMDHitToken{ false };

    /// @brief True when inline ray tracing requires a bound shader binding table to fetch geometry metadata.
    bool requiresInlineRayTracingSBT{ false };

    /// @brief True when inlined ray tracing shader callbacks are supported.
    bool supportsInlinedCallbacks{ false };

    /// @brief True when ray tracing shader binding tables can be persistently retained and updated in parallel.
    bool supportsPersistentSBTs{ false };

    /// @brief True when loose shader parameters can be embedded directly inside SBT shader records.
    bool supportsLooseParamsInShaderRecord{ false };

    /// @brief True when post-build acceleration structure compaction is supported.
    bool supportsAccelerationStructureCompaction{ false };

    /// @brief True when acceleration structures can be serialized to offline storage.
    bool supportsSerializeAccelerationStructure{ false };

    /// @brief True when building acceleration structures requires a separate hit-group contribution buffer.
    bool requiresSeparateHitGroupContributionsBuffer{ false };

    /// @brief True when linear swept sphere curve primitives are supported.
    bool supportsLinearSweptSpheres{ false };

    /// @brief True when opacity micromap arrays are supported.
    bool supportsOpacityMicromaps{ false };

    /// @brief Required byte alignment for acceleration structure buffers.
    gp::UInt32 accelerationStructureAlignment{ 256u };

    /// @brief Required byte alignment for cluster-level acceleration structure buffers.
    gp::UInt32 clusterAccelerationStructureAlignment{ 0u };

    /// @brief Required byte alignment for cluster-level acceleration structure template buffers.
    gp::UInt32 clusterAccelerationStructureTemplateAlignment{ 0u };

    /// @brief Required byte alignment for opacity micromap buffers.
    gp::UInt32 opacityMicromapAlignment{ 0u };

    /// @brief Required byte alignment for acceleration structure build scratch buffers.
    gp::UInt32 scratchBufferAlignment{ 256u };

    /// @brief Required byte alignment for shader binding table records.
    gp::UInt32 shaderTableAlignment{ 64u };

    /// @brief Byte size of one hardware ray tracing instance descriptor inside TLAS instance buffers.
    gp::UInt32 instanceDescriptorSize{ 0u };
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

    /// @brief True when non-trivial combiner operations (`Min`, `Max`, `Sum`) between pipeline and attachment VRS work.
    bool supportsComplexCombinerOps{ false };

    /// @brief True when VRS shading rate attachments may be 2D texture arrays.
    bool supportsAttachmentArrayTextures{ false };

    /// @brief True when deferred shading-rate image updates are supported.
    bool supportsLateUpdate{ false };

    /// @brief True when sub-tile pixel offsets for VRS attachments are supported.
    bool supportsImageOffsets{ false };

    /// @brief Minimum tile width in pixels for VRS attachment textures.
    gp::UInt32 imageTileMinWidth{ 0u };

    /// @brief Minimum tile height in pixels for VRS attachment textures.
    gp::UInt32 imageTileMinHeight{ 0u };

    /// @brief Maximum tile width in pixels for VRS attachment textures.
    gp::UInt32 imageTileMaxWidth{ 0u };

    /// @brief Maximum tile height in pixels for VRS attachment textures.
    gp::UInt32 imageTileMaxHeight{ 0u };

    /// @brief Encoding format expected inside a screen-space VRS attachment texture.
    pipeline::VRSImageDataType imageDataType{ pipeline::VRSImageDataType::NotSupported };

    /// @brief Pixel format required for screen-space VRS attachment textures.
    PixelFormat imageFormat{ PixelFormat::Unknown };

    /// @brief Quantized per-eye pixel offsets applied to VRS attachments.
    gp::Array<resource::IntPoint, 2u> imageOffsets{};

    /// @brief Required quantization granularity in pixels for `imageOffsets`.
    resource::IntPoint imageOffsetGranularity{};
};

/// @brief Shader bundle and work graph capabilities reported by the active backend.
struct ShaderBundleCapabilities
{
    /// @brief True when GPU-driven shader bundle dispatch is supported.
    bool supported{ false };

    /// @brief True when compute shader bundle dispatch via work graphs is supported.
    bool supportsWorkGraphs{ false };

    /// @brief True when graphics shader bundle dispatch via work graphs is supported.
    bool supportsWorkGraphGraphicsDispatch{ false };

    /// @brief True when shader bundle dispatches may translate concurrently on worker threads.
    bool supportsParallel{ false };

    /// @brief True when the backend requires shared bindless parameter tables for shader bundles.
    bool requiresSharedBindlessParameters{ false };
};

/// @brief Reserved / tiled virtual GPU resource capabilities reported by the active backend.
struct ReservedResourceCapabilities
{
    /// @brief True when reserved buffers and 2D textures with commit transitions are supported.
    bool supported{ false };

    /// @brief True when 3D volume textures can be created with `TextureCreateFlags::ReservedResource`.
    bool supportsVolumeTextures{ false };

    /// @brief True when physical page commit transitions can execute directly on the async compute queue.
    bool supportsCommitOnAsyncCompute{ false };

    /// @brief Smallest mip extent in texels permitted on multi-slice reserved texture arrays.
    gp::Int32 textureArrayMinimumMipDimension{ static_cast<gp::Int32>(kDefaultReservedTextureArrayMinMipDimension) };

    /// @brief Byte size of one physical GPU tile page (guaranteed 64 KiB across backends).
    gp::UInt64 tileSizeInBytes{ 65536u };

    /// @brief Current virtual address space reserved across all active reserved buffers and textures in bytes.
    gp::Int64 virtualSize{ 0 };
};

/// @brief Parallel render-pass command recording capabilities reported by the active backend.
struct ParallelRenderPassCapabilities
{
    /// @brief True when secondary/parallel command lists may record inside one render pass simultaneously.
    bool supported{ false };

    /// @brief True when parallel render passes require dedicated contexts from `getParallelCommandContext()`.
    bool usesCustomContexts{ false };

    /// @brief True when parent command list translation must wait for child parallel pass translations to finish.
    bool parentWaitForChildTranslate{ false };

    /// @brief True when child parallel pass translations must wait until the parent reaches `beginParallelRenderPass`.
    bool childWaitForParentBegin{ false };
};

/// @brief Adapter identification metadata populated by the active RHI backend during startup.
struct GPUInfo
{
    /// @brief Human-readable adapter description string.
    gp::String adapterName{};

    /// @brief Internal driver build version string reported by the OS/driver registry.
    gp::String adapterInternalDriverVersion{};

    /// @brief User-facing driver version string reported by the vendor API.
    gp::String adapterUserDriverVersion{};

    /// @brief PCI vendor identifier (`GpuVendorId`).
    GpuVendorId vendorId{ GpuVendorId::NotQueried };

    /// @brief Hardware PCI device ID.
    gp::UInt32 deviceId{ 0u };

    /// @brief Hardware revision ID.
    gp::UInt32 deviceRevision{ 0u };

    /// @brief Vendor-specific GPU architecture generation number (`0` if unknown).
    gp::UInt32 adapterGeneration{ 0u };

    /// @brief Driver version string reported by the vendor API.
    gp::String driverVersion{};

    /// @brief Driver release date string.
    gp::String driverDate{};

    /// @brief Dedicated local video memory reported by the active adapter in bytes.
    gp::UInt64 dedicatedVideoMemory{ 0u };

    /// @brief True when the installed driver version matches a known stability/corruption deny-list entry.
    bool adapterDriverOnDenyList{ false };

    /// @brief True when the active adapter is a legacy pre-GCN AMD architecture.
    bool isAMDPreGCNArchitecture{ false };

    /// @brief True when the active adapter is an integrated / unified memory GPU.
    bool deviceIsIntegrated{ false };
};

/// @brief Global RHI capability, limit, pixel-format, memory counter, and adapter telemetry table.
/// @details Accessed via `getRHIGlobals()`. Reset cleanly by `reset()` during `rhiExit()` so backend swaps in tests
///          or tools never observe stale limits or memory counters from a prior device.
struct GP_RHI_API RHIGlobals
{
    /// @brief Constructs `RHIGlobals` with standard MSAA sample offsets initialized.
    RHIGlobals() noexcept;

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

    /// @brief True when the active backend supports parallel command list recording/translation.
    bool supportsParallelRHIExecute{ false };

    /// @brief True when RHI submit tasks may execute concurrently with subsequent RHI translation tasks.
    bool supportsConcurrentTranslateAndSubmit{ false };

    /// @brief True when rendering tasks can initialize resources concurrently while a separate RHI thread runs.
    bool supportsParallelRenderingTasksWithSeparateRHIThread{ true };

    /// @brief True when the RHI thread benefits from early batch kicks during heavy frame recording.
    bool rhiThreadNeedsKicking{ false };

    /// @brief True when resources may be created concurrently across worker threads.
    bool supportsMultithreadedResources{ false };

    /// @brief True when shader objects may be compiled/created concurrently across worker threads.
    bool supportsMultithreadedShaderCreation{ true };

    /// @brief True when `DynamicRHI::createBufferInitializer()` / `createTextureInitializer()` support async creation.
    bool supportsAsyncTextureCreation{ false };

    /// @brief True when asynchronous compute queues are available.
    bool supportsAsyncCompute{ false };

    /// @brief True when async compute executes efficiently without heavy queue serialization penalties.
    bool supportsEfficientAsyncCompute{ false };

    /// @brief True when buffer uploads on creation/lock can complete directly on async compute queues.
    bool supportsAsyncComputeBufferUploads{ false };

    /// @brief True when hardware depth bounds testing is available.
    bool supportsDepthBoundsTest{ false };

    /// @brief True when explicit access to hierarchical depth metadata (`HTile`) is supported.
    bool supportsExplicitHTile{ false };

    /// @brief True when explicit access to MSAA fragment mask metadata (`FMask`) is supported.
    bool supportsExplicitFMask{ false };

    /// @brief True when `ICommandContext::resummarizeHTile()` is supported.
    bool supportsResummarizeHTile{ false };

    /// @brief True when depth targets may also bind unordered access views.
    bool supportsDepthUAV{ false };

    /// @brief True when hardware mesh and amplification shaders are supported.
    bool supportsMeshShadersTier0{ false };

    /// @brief True when mesh shader Tier 1 (primitive attributes, full limits) is supported.
    bool supportsMeshShadersTier1{ false };

    /// @brief True when shader work graphs Tier 1.0 are supported.
    bool supportsShaderWorkGraphsTier1{ false };

    /// @brief True when shader work graphs Tier 1.1 are supported.
    bool supportsShaderWorkGraphsTier1_1{ false };

    /// @brief True when partial shader programs are supported.
    bool supportsPartialPrograms{ false };

    /// @brief True when shaders can read GPU hardware timers directly (`GetShaderTimestamp`).
    bool supportsShaderTimestamp{ false };

    /// @brief True when hardware bindless descriptor indexing is supported by the device (even if not active).
    bool supportsBindless{ false };

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

    /// @brief True when non-zero `baseVertexIndex` draw arguments are natively supported.
    bool supportsBaseVertexIndex{ true };

    /// @brief True when non-zero `firstInstance` draw arguments are natively supported.
    bool supportsFirstInstance{ false };

    /// @brief True when quad primitive topology (`PrimitiveType::QuadList`) is supported.
    bool supportsQuadTopology{ false };

    /// @brief True when screen-aligned rectangle primitive topology (`PrimitiveType::RectList`) is supported.
    bool supportsRectTopology{ false };

    /// @brief True when hardware primitive shaders are supported.
    bool supportsPrimitiveShaders{ false };

    /// @brief True when transient memory aliasing heaps are supported.
    bool supportsTransientResourceAliasing{ false };

    /// @brief True when transient `ResourceAccess::Discard` transitions must resolve their prior state from tracked
    ///        resource state rather than treating discard as an undefined layout transition.
    bool needsTransientDiscardStateTracking{ false };

    /// @brief True when enhanced barriers are active on the backend.
    bool enhancedBarriersActive{ false };

    /// @brief True when the backend requires explicit high-level `CopySrc` / `CopyDest` transitions.
    bool needsExtraTransitions{ false };

    /// @brief True when the backend requires explicit transitions for `SRVGraphicsNonPixel` bindings.
    bool needsSRVGraphicsNonPixelWorkaround{ false };

    /// @brief True when the backend distinguishes `SRVGraphicsPixel` from `SRVGraphicsNonPixel` transitions.
    bool supportsDedicatedSRVGraphicsNonPixelAccess{ false };

    /// @brief True when dynamic uniform buffer offset binding is supported.
    bool supportsDynamicUniformBufferOffsets{ false };

    /// @brief True when texture streaming is supported on this backend.
    bool supportsTextureStreaming{ false };

    /// @brief True when `updateFromBufferTexture2D` is supported.
    bool supportsUpdateFromBufferTexture{ false };

    /// @brief True when 3D volume textures are supported.
    bool supportsTexture3D{ true };

    /// @brief True when texture views / format reinterpretation are supported.
    bool supportsTextureViews{ true };

    /// @brief True when UAV views may alias a different pixel format than the backing texture.
    bool supportsUAVFormatAliasing{ false };

    /// @brief True when pixel shaders may bind unordered access views.
    bool supportsPixelShaderUAVs{ true };

    /// @brief True when rasterizer order views (ROVs / fragment shader interlock) are supported.
    bool supportsRasterOrderViews{ false };

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

    /// @brief True when native 16-bit shader arithmetic and loads (SM 6.2+) are supported.
    bool supportsNative16BitOps{ false };

    /// @brief True when barycentrics shader semantics are supported.
    bool supportsBarycentricsSemantic{ false };

    /// @brief True when shader execution reordering (SER / HitObject) intrinsics are supported.
    bool supportsShaderExecutionReordering{ false };

    /// @brief True when shader execution reordering is hardware-accelerated on the active GPU.
    bool supportsAcceleratedShaderExecutionReordering{ false };

    /// @brief True when split resource transitions (`beginTransitions`/`endTransitions`) are natively supported.
    bool supportsSplitBarriers{ false };

    /// @brief True when `ResourceLockMode::WriteOnlyNoOverwrite` is supported without stalling.
    bool supportsMapWriteNoOverwrite{ false };

    /// @brief True when `lockBuffer()` returns a persistent direct GPU memory pointer.
    bool supportsDirectGPUMemoryLock{ false };

    /// @brief True when fast hardware partial sub-rectangle clears are supported.
    bool supportsFastPartialClear{ false };

    /// @brief True when MSAA depth-stencil resolve attachments are supported.
    bool supportsDepthStencilResolve{ false };

    /// @brief True when custom shader MSAA resolve is supported.
    bool supportsMSAAShaderResolve{ false };

    /// @brief True when lossy/fixed-rate framebuffer compression is supported.
    bool supportsLossyFramebufferCompression{ false };

    /// @brief True when any slice of a texture array can be bound as an RTV without `TargetArraySlicesIndependently`.
    bool supportsBindingTexArrayPerSlice{ false };

    /// @brief True when dynamic resolution scaling is supported.
    bool supportsDynamicResolution{ false };

    /// @brief True when background pipeline precompilation from worker threads is supported.
    bool supportsAsyncPipelinePrecompile{ true };

    /// @brief True when asynchronous PSO precaching is supported.
    bool supportsPSOPrecaching{ false };

    /// @brief True when serialized PSO file caches are supported.
    bool supportsPipelineFileCache{ false };

    /// @brief True when lazy shader code loading from native shader libraries is currently active.
    bool lazyShaderCodeLoading{ false };

    /// @brief True when the backend supports lazy shader code loading.
    bool supportsLazyShaderCodeLoading{ false };

    /// @brief True when the backend requires explicit SRV/UAV unbind calls on resource state changes.
    bool needsShaderUnbinds{ true };

    /// @brief True when deferred resource deletion requires extra frame latency beyond a single fence.
    bool needsExtraDeletionLatency{ false };

    /// @brief True when the active GPU is integrated into the host CPU package.
    bool deviceIsIntegrated{ false };

    /// @brief True when the device uses a Unified Memory Architecture with separate CPU/GPU virtual addresses.
    bool deviceIsUMASeparateVA{ false };

    /// @brief True when the device uses a cache-coherent Unified Memory Architecture.
    bool deviceIsCacheCoherentUMA{ false };

    /// @brief True when host-visible device-local upload heaps (Resizable BAR) are available.
    bool deviceSupportsGPUUploadHeaps{ false };

    /// @brief True when HDR swapchain presentation is supported by the active display and backend.
    bool supportsHDROutput{ false };

    /// @brief True when HDR output is currently active.
    bool isHDREnabled{ false };

    /// @brief Preferred backbuffer pixel format for HDR presentation.
    PixelFormat hdrDisplayOutputFormat{ PixelFormat::R16G16B16A16_Float };

    /// @brief Preferred backbuffer pixel format for 10-bit SDR presentation.
    PixelFormat displayOutputFormat10Bit{ PixelFormat::R10G10B10A2_UNorm };

    /// @brief Monotonic count of completed viewport presentations.
    gp::UInt64 presentCounter{ 1u };

    /// @brief Memory currently allocated by streaming textures in KiB.
    gp::UInt64 streamingTextureMemorySizeInKB{ 0u };

    /// @brief Memory currently allocated by non-streaming textures in KiB.
    gp::UInt64 nonStreamingTextureMemorySizeInKB{ 0u };

    /// @brief Configured streaming texture pool budget in bytes (`0` means unlimited).
    gp::Int64 texturePoolSize{ 0 };

    /// @brief Configured streaming pool percentage of total graphics memory (`0` uses fixed `texturePoolSize`).
    gp::Int32 poolSizeVRAMPercentage{ 0 };

    /// @brief Local video memory demoted to system memory due to over-budget pressure in bytes.
    gp::UInt64 demotedLocalMemorySize{ 0u };

    /// @brief Memory currently allocated across GPU buffers in bytes.
    gp::UInt64 bufferMemorySize{ 0u };

    /// @brief Memory currently allocated across GPU uniform buffers in bytes.
    gp::UInt64 uniformBufferMemorySize{ 0u };

    /// @brief True when hardware wave/subgroup operations are available.
    bool supportsWaveOperations{ false };

    /// @brief Minimum wave/subgroup lane count reported by the GPU (`0` when unsupported).
    gp::UInt32 minimumWaveSize{ 0u };

    /// @brief Maximum wave/subgroup lane count reported by the GPU (`0` when unsupported).
    gp::UInt32 maximumWaveSize{ 0u };

    /// @brief Optimal persistent compute thread group count to saturate the GPU (`0` if unqueried).
    gp::UInt32 persistentThreadGroupCount{ 0u };

    /// @brief Maximum invocations (threads) permitted in a single compute workgroup.
    gp::UInt32 maxWorkGroupInvocations{ 1024u };

    /// @brief Maximum constant/uniform buffer byte size supported by shaders.
    gp::UInt64 maxConstantBufferByteSize{ 65536u };

    /// @brief Maximum compute threadgroup shared memory size in bytes.
    gp::UInt64 maxComputeSharedMemory{ 32768u };

    /// @brief Maximum simultaneous sampler bindings per shader stage.
    gp::UInt32 maxTextureSamplers{ 16u };

    /// @brief Minimum guaranteed simultaneous UAV bindings across all stages.
    static constexpr gp::UInt32 kMinGuaranteedSimultaneousUAVs = 8u;

    /// @brief Maximum simultaneous UAV bindings per shader stage.
    gp::UInt32 maxSimultaneousUAVs{ kMinGuaranteedSimultaneousUAVs };

    /// @brief Maximum shadow depth buffer width in texels.
    gp::UInt32 maxShadowDepthBufferSizeX{ 2048u };

    /// @brief Maximum shadow depth buffer height in texels.
    gp::UInt32 maxShadowDepthBufferSizeY{ 2048u };

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

    /// @brief Maximum element count addressable by a typed buffer SRV/UAV.
    gp::UInt64 maxViewDimensionForTypedBuffer{ 1ull << 32u };

    /// @brief Maximum byte span addressable by a structured or raw buffer SRV/UAV.
    gp::UInt64 maxViewSizeBytesForNonTypedBuffer{ 1ull << 32u };

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

    /// @brief Standard pixel-center subpixel sample offsets for 1x, 2x, 4x, 8x, and 16x MSAA.
    gp::Array<gp::math::Vector2<gp::Float32>, kDefaultMSAASampleOffsetCount> defaultMSAASampleOffsets{};

    /// @brief Descriptor type masks indicating which descriptor types share unified hardware heaps.
    gp::Vector<view::DescriptorTypeMask> descriptorTypeGroups{};

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

    /// @brief Resets all capability flags, limits, memory counters, and metadata back to safe defaults.
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

/// @brief Computes the flat index in `RHIGlobals::defaultMSAASampleOffsets` for `(numSamples, sampleIndex)`.
/// @param[in] numSamples Power-of-two MSAA sample count in `{1, 2, 4, 8, 16}`.
/// @param[in] sampleIndex Zero-based sample index in `[0, numSamples)`.
/// @return Flat index into `defaultMSAASampleOffsets`.
[[nodiscard]] inline constexpr gp::USize
    calculateMSAASampleArrayIndex(const gp::UInt32 numSamples, const gp::UInt32 sampleIndex) noexcept
{
    GP_CHECK(numSamples > 0u && numSamples <= 16u && gp::math::isPowerOfTwo(numSamples));
    GP_CHECK(sampleIndex < numSamples);
    return static_cast<gp::USize>((numSamples - 1u) + sampleIndex);
}

/// @brief Returns the subpixel sample offset from pixel center for `(numSamples, sampleIndex)`.
/// @param[in] numSamples Power-of-two MSAA sample count in `{1, 2, 4, 8, 16}`.
/// @param[in] sampleIndex Zero-based sample index in `[0, numSamples)`.
/// @return Subpixel offset in `[-1, 1]`.
[[nodiscard]] inline gp::math::Vector2<gp::Float32>
    getMSAASampleOffsets(const gp::UInt32 numSamples, const gp::UInt32 sampleIndex) noexcept
{
    const gp::USize arrayIndex = calculateMSAASampleArrayIndex(numSamples, sampleIndex);
    const RHIGlobals& globals = getRHIGlobals();
    return (arrayIndex < globals.defaultMSAASampleOffsets.size()) ? globals.defaultMSAASampleOffsets[arrayIndex]
                                                                  : gp::math::Vector2<gp::Float32>{ 0.0f, 0.0f };
}

}   // namespace gp::rhi
