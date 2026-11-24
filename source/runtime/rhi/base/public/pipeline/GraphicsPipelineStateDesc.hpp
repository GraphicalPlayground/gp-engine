// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "containers/arrays/Array.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "CoreMinimal.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "resources/TextureEnums.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderTypes.hpp"

namespace gp::rhi::pipeline
{

/// @brief Render target format, flag, and load/store subset extracted from a render pass for PSO initialization.
struct GraphicsPipelineRenderTargetsInfo
{
    /// @brief Fixed array of color attachment pixel formats indexed by MRT slot.
    using RenderTargetFormats = gp::Array<PixelFormat, kMaxSimultaneousRenderTargets>;

    /// @brief Fixed array of color attachment creation flags indexed by MRT slot.
    using RenderTargetFlags = gp::Array<resource::TextureCreateFlags, kMaxSimultaneousRenderTargets>;

    /// @brief Number of active color render target slots.
    gp::UInt32 renderTargetsEnabled{ 0u };

    /// @brief Color attachment pixel formats.
    RenderTargetFormats renderTargetFormats{};

    /// @brief Color attachment creation flags.
    RenderTargetFlags renderTargetFlags{};

    /// @brief Depth-stencil attachment pixel format, or `PixelFormat::Unknown` if unbound.
    PixelFormat depthStencilTargetFormat{ PixelFormat::Unknown };

    /// @brief Depth-stencil attachment creation flags.
    resource::TextureCreateFlags depthStencilTargetFlag{ resource::TextureCreateFlags::None };

    /// @brief Load policy for the depth plane.
    command::RenderTargetLoadAction depthTargetLoadAction{ command::RenderTargetLoadAction::NoAction };

    /// @brief Store policy for the depth plane.
    command::RenderTargetStoreAction depthTargetStoreAction{ command::RenderTargetStoreAction::NoAction };

    /// @brief Load policy for the stencil plane.
    command::RenderTargetLoadAction stencilTargetLoadAction{ command::RenderTargetLoadAction::NoAction };

    /// @brief Store policy for the stencil plane.
    command::RenderTargetStoreAction stencilTargetStoreAction{ command::RenderTargetStoreAction::NoAction };

    /// @brief Exclusive depth-stencil read/write state active for the pass.
    resource::ExclusiveDepthStencil depthStencilAccess{ resource::ExclusiveDepthStencil::kDepthNopStencilNop };

    /// @brief MSAA sample count shared across bound attachments.
    gp::UInt16 numSamples{ 1u };

    /// @brief Tile-based subpass optimization hint.
    SubpassHint subpassHint{ SubpassHint::None };

    /// @brief Active subpass index inside the render pass.
    gp::UInt8 subpassIndex{ 0u };

    /// @brief Number of multiview layers rendered simultaneously (`0` when disabled).
    gp::UInt8 multiViewCount{ 0u };

    /// @brief True when a variable rate shading or fragment density attachment is bound.
    bool hasFragmentDensityAttachment{ false };

    /// @brief Constructs an empty render targets info with all formats set to `PixelFormat::Unknown`.
    constexpr GraphicsPipelineRenderTargetsInfo() noexcept
    {
        for (gp::UInt32 i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
        {
            renderTargetFormats[i] = PixelFormat::Unknown;
            renderTargetFlags[i] = resource::TextureCreateFlags::None;
        }
    }
};

/// @brief Complete immutable creation descriptor for a `GraphicsPipelineState`.
/// @details Captures bound shader stages, fixed-function state objects, primitive topology, render target formats and
///          load/store policies, MSAA sample count, conservative rasterization, depth bounds, and VRS state. Precache
///          hint bits (`flags` and `statePrecachePSOHash`) are excluded from logical equality.
class GraphicsPipelineStateInitializer
{
public:
    /// @brief Fixed array of color attachment pixel formats indexed by MRT slot.
    using RenderTargetFormats = GraphicsPipelineRenderTargetsInfo::RenderTargetFormats;

    /// @brief Fixed array of color attachment creation flags indexed by MRT slot.
    using RenderTargetFlags = GraphicsPipelineRenderTargetsInfo::RenderTargetFlags;

    /// @brief Color target flag bits relevant to pipeline state identity.
    static constexpr resource::TextureCreateFlags kRelevantRenderTargetFlagMask =
        resource::TextureCreateFlags::SRGB | resource::TextureCreateFlags::Shared;

    /// @brief Depth-stencil target flag bits relevant to pipeline state identity.
    static constexpr resource::TextureCreateFlags kRelevantDepthStencilFlagMask =
        resource::TextureCreateFlags::SRGB | resource::TextureCreateFlags::Shared |
        resource::TextureCreateFlags::DepthStencilTargetable;

public:
    /// @brief Shader stages and vertex input layout bound to this pipeline.
    BoundShaderStateInput boundShaderState{};

    /// @brief Fixed-function blend state object.
    BlendState* blendState{ nullptr };

    /// @brief Fixed-function rasterizer state object.
    RasterizerState* rasterizerState{ nullptr };

    /// @brief Fixed-function depth-stencil state object.
    DepthStencilState* depthStencilState{ nullptr };

    /// @brief Primitive topology class drawn with this pipeline.
    PrimitiveType primitiveType{ PrimitiveType::TriangleList };

    /// @brief Number of active color render target slots.
    gp::UInt32 renderTargetsEnabled{ 0u };

    /// @brief Pixel formats of bound color attachments.
    RenderTargetFormats renderTargetFormats{};

    /// @brief Creation flags of bound color attachments.
    RenderTargetFlags renderTargetFlags{};

    /// @brief Pixel format of the depth-stencil attachment, or `PixelFormat::Unknown`.
    PixelFormat depthStencilTargetFormat{ PixelFormat::Unknown };

    /// @brief Creation flags of the depth-stencil attachment.
    resource::TextureCreateFlags depthStencilTargetFlag{ resource::TextureCreateFlags::None };

    /// @brief Depth plane load action for the compatible render pass.
    command::RenderTargetLoadAction depthTargetLoadAction{ command::RenderTargetLoadAction::NoAction };

    /// @brief Depth plane store action for the compatible render pass.
    command::RenderTargetStoreAction depthTargetStoreAction{ command::RenderTargetStoreAction::NoAction };

    /// @brief Stencil plane load action for the compatible render pass.
    command::RenderTargetLoadAction stencilTargetLoadAction{ command::RenderTargetLoadAction::NoAction };

    /// @brief Stencil plane store action for the compatible render pass.
    command::RenderTargetStoreAction stencilTargetStoreAction{ command::RenderTargetStoreAction::NoAction };

    /// @brief Exclusive depth-stencil read/write state.
    resource::ExclusiveDepthStencil depthStencilAccess{ resource::ExclusiveDepthStencil::kDepthNopStencilNop };

    /// @brief MSAA sample count for bound render targets.
    gp::UInt16 numSamples{ 1u };

    /// @brief Subpass optimization hint.
    SubpassHint subpassHint{ SubpassHint::None };

    /// @brief Zero-based subpass index within the render pass.
    gp::UInt8 subpassIndex{ 0u };

    /// @brief Conservative rasterization mode.
    ConservativeRasterization conservativeRasterization{ ConservativeRasterization::Disabled };

    /// @brief True when hardware depth-bounds testing is enabled.
    bool depthBounds{ false };

    /// @brief Number of multiview layers (`0` when multiview is inactive).
    gp::UInt8 multiViewCount{ 0u };

    /// @brief True when a fragment density or VRS image attachment is bound.
    bool hasFragmentDensityAttachment{ false };

    /// @brief True when variable rate shading is allowed for draws using this PSO.
    bool allowVariableRateShading{ true };

    /// @brief Draw-level variable rate shading tile rate.
    VRSShadingRate shadingRate{ VRSShadingRate::Rate1x1 };

    /// @brief Cached precache hash of non-shader state fields (`0` when uncomputed).
    gp::UInt64 statePrecachePSOHash{ 0u };

    /// @brief Packed 16-bit non-hashed precache and file-cache origin hint flags.
    /// @details Layout: bits `[0..10]` reserved, bit `11` = `psoPrecache`, bit `12` = `fromPSOFileCache`,
    ///          bits `[13..15]` = `PSOPrecacheCompileType`.
    gp::UInt16 rawFlags{ 0u };

public:
    /// @brief Constructs a default initializer with no shaders and all render target formats set to `Unknown`.
    GraphicsPipelineStateInitializer() noexcept
    {
        for (gp::UInt32 i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
        {
            renderTargetFormats[i] = PixelFormat::Unknown;
            renderTargetFlags[i] = resource::TextureCreateFlags::None;
        }
    }

    /// @brief Constructs a fully specified graphics pipeline state initializer.
    /// @param[in] inBoundShaderState Shader stages and vertex layout.
    /// @param[in] inBlendState Blend state object.
    /// @param[in] inRasterizerState Rasterizer state object.
    /// @param[in] inDepthStencilState Depth-stencil state object.
    /// @param[in] inPrimitiveType Draw primitive topology.
    /// @param[in] inRenderTargetsEnabled Number of active color render targets.
    /// @param[in] inRenderTargetFormats Color attachment pixel formats.
    /// @param[in] inRenderTargetFlags Color attachment creation flags.
    /// @param[in] inDepthStencilTargetFormat Depth-stencil pixel format.
    /// @param[in] inDepthStencilTargetFlag Depth-stencil creation flags.
    /// @param[in] inDepthTargetLoadAction Depth load action.
    /// @param[in] inDepthTargetStoreAction Depth store action.
    /// @param[in] inStencilTargetLoadAction Stencil load action.
    /// @param[in] inStencilTargetStoreAction Stencil store action.
    /// @param[in] inDepthStencilAccess Exclusive depth-stencil access mode.
    /// @param[in] inNumSamples MSAA sample count.
    /// @param[in] inSubpassHint Tile-based subpass hint.
    /// @param[in] inSubpassIndex Active subpass index.
    /// @param[in] inConservativeRasterization Conservative rasterization mode.
    /// @param[in] inDepthBounds Whether depth bounds testing is active.
    /// @param[in] inMultiViewCount Multiview layer count.
    /// @param[in] inHasFragmentDensityAttachment Whether a fragment density attachment is bound.
    /// @param[in] inAllowVariableRateShading Whether VRS is allowed.
    /// @param[in] inShadingRate Draw-level VRS tile rate.
    GraphicsPipelineStateInitializer(
        const BoundShaderStateInput& inBoundShaderState,
        BlendState* const inBlendState,
        RasterizerState* const inRasterizerState,
        DepthStencilState* const inDepthStencilState,
        const PrimitiveType inPrimitiveType,
        const gp::UInt32 inRenderTargetsEnabled,
        const RenderTargetFormats& inRenderTargetFormats,
        const RenderTargetFlags& inRenderTargetFlags,
        const PixelFormat inDepthStencilTargetFormat,
        const resource::TextureCreateFlags inDepthStencilTargetFlag,
        const command::RenderTargetLoadAction inDepthTargetLoadAction,
        const command::RenderTargetStoreAction inDepthTargetStoreAction,
        const command::RenderTargetLoadAction inStencilTargetLoadAction,
        const command::RenderTargetStoreAction inStencilTargetStoreAction,
        const resource::ExclusiveDepthStencil inDepthStencilAccess,
        const gp::UInt16 inNumSamples,
        const SubpassHint inSubpassHint,
        const gp::UInt8 inSubpassIndex,
        const ConservativeRasterization inConservativeRasterization,
        const bool inDepthBounds,
        const gp::UInt8 inMultiViewCount,
        const bool inHasFragmentDensityAttachment,
        const bool inAllowVariableRateShading,
        const VRSShadingRate inShadingRate
    ) noexcept
        : boundShaderState(inBoundShaderState)
        , blendState(inBlendState)
        , rasterizerState(inRasterizerState)
        , depthStencilState(inDepthStencilState)
        , primitiveType(inPrimitiveType)
        , renderTargetsEnabled(inRenderTargetsEnabled)
        , renderTargetFormats(inRenderTargetFormats)
        , renderTargetFlags(inRenderTargetFlags)
        , depthStencilTargetFormat(inDepthStencilTargetFormat)
        , depthStencilTargetFlag(inDepthStencilTargetFlag)
        , depthTargetLoadAction(inDepthTargetLoadAction)
        , depthTargetStoreAction(inDepthTargetStoreAction)
        , stencilTargetLoadAction(inStencilTargetLoadAction)
        , stencilTargetStoreAction(inStencilTargetStoreAction)
        , depthStencilAccess(inDepthStencilAccess)
        , numSamples(inNumSamples)
        , subpassHint(inSubpassHint)
        , subpassIndex(inSubpassIndex)
        , conservativeRasterization(inConservativeRasterization)
        , depthBounds(inDepthBounds)
        , multiViewCount(inMultiViewCount)
        , hasFragmentDensityAttachment(inHasFragmentDensityAttachment)
        , allowVariableRateShading(inAllowVariableRateShading)
        , shadingRate(inShadingRate)
    {}

    /// @brief Copies render target formats, flags, load/store actions, and sample count from `rtInfo`.
    /// @param[in] rtInfo Extracted render pass target information.
    void applyRenderTargetsInfo(const GraphicsPipelineRenderTargetsInfo& rtInfo) noexcept
    {
        renderTargetsEnabled = rtInfo.renderTargetsEnabled;
        renderTargetFormats = rtInfo.renderTargetFormats;
        renderTargetFlags = rtInfo.renderTargetFlags;
        depthStencilTargetFormat = rtInfo.depthStencilTargetFormat;
        depthStencilTargetFlag = rtInfo.depthStencilTargetFlag;
        depthTargetLoadAction = rtInfo.depthTargetLoadAction;
        depthTargetStoreAction = rtInfo.depthTargetStoreAction;
        stencilTargetLoadAction = rtInfo.stencilTargetLoadAction;
        stencilTargetStoreAction = rtInfo.stencilTargetStoreAction;
        depthStencilAccess = rtInfo.depthStencilAccess;
        numSamples = rtInfo.numSamples;
        subpassHint = rtInfo.subpassHint;
        subpassIndex = rtInfo.subpassIndex;
        multiViewCount = rtInfo.multiViewCount;
        hasFragmentDensityAttachment = rtInfo.hasFragmentDensityAttachment;
    }

    /// @brief Counts the number of valid color render target slots up to `renderTargetsEnabled`.
    /// @return Number of color slots with `format != PixelFormat::Unknown`.
    [[nodiscard]] gp::UInt32 computeNumValidRenderTargets() const noexcept
    {
        if (renderTargetsEnabled > 0u)
        {
            const gp::UInt32 limit = (renderTargetsEnabled <= kMaxSimultaneousRenderTargets)
                                         ? renderTargetsEnabled
                                         : kMaxSimultaneousRenderTargets;
            for (gp::Int32 i = static_cast<gp::Int32>(limit) - 1; i >= 0; --i)
            {
                if (renderTargetFormats[static_cast<gp::USize>(i)] != PixelFormat::Unknown)
                {
                    return static_cast<gp::UInt32>(i + 1);
                }
            }
        }
        return 0u;
    }

    /// @brief Reports whether this initializer originates from async PSO precaching.
    /// @return True when the `psoPrecache` hint bit is set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isPSOPrecache() const noexcept
    {
        return (rawFlags & (1u << 11u)) != 0u;
    }

    /// @brief Marks whether this initializer originates from async PSO precaching.
    /// @param[in] inPrecache True for precache builds.
    GP_FORCEINLINE_HINT void setPSOPrecache(const bool inPrecache) noexcept
    {
        if (inPrecache)
        {
            rawFlags = static_cast<gp::UInt16>(rawFlags | (1u << 11u));
        }
        else
        {
            rawFlags = static_cast<gp::UInt16>(rawFlags & ~(1u << 11u));
        }
    }

    /// @brief Reports whether this initializer was loaded from a serialized PSO file cache.
    /// @return True when the `fromPSOFileCache` hint bit is set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isFromPSOFileCache() const noexcept
    {
        return (rawFlags & (1u << 12u)) != 0u;
    }

    /// @brief Marks whether this initializer was loaded from a serialized PSO file cache.
    /// @param[in] inFromCache True when loaded from cache.
    GP_FORCEINLINE_HINT void setFromPSOFileCache(const bool inFromCache) noexcept
    {
        if (inFromCache)
        {
            rawFlags = static_cast<gp::UInt16>(rawFlags | (1u << 12u));
        }
        else
        {
            rawFlags = static_cast<gp::UInt16>(rawFlags & ~(1u << 12u));
        }
    }

    /// @brief Returns the precache compile pass category hint.
    /// @return Stored `PSOPrecacheCompileType`.
    [[nodiscard]] GP_FORCEINLINE_HINT PSOPrecacheCompileType getPrecacheCompileType() const noexcept
    {
        return static_cast<PSOPrecacheCompileType>((rawFlags >> 13u) & 0x7u);
    }

    /// @brief Sets the precache compile pass category hint.
    /// @param[in] inType Pass category hint.
    GP_FORCEINLINE_HINT void setPrecacheCompileType(const PSOPrecacheCompileType inType) noexcept
    {
        const gp::UInt16 cleared = static_cast<gp::UInt16>(rawFlags & ~(0x7u << 13u));
        const gp::UInt16 bits = static_cast<gp::UInt16>((static_cast<gp::UInt16>(inType) & 0x7u) << 13u);
        rawFlags = static_cast<gp::UInt16>(cleared | bits);
    }

    /// @brief Compares all pipeline-defining fields, masking attachment flags to PSO-relevant bits and ignoring hints.
    /// @param[in] rhs Initializer to compare against.
    /// @return True when both initializers describe an identical graphics pipeline state.
    [[nodiscard]] bool operator==(const GraphicsPipelineStateInitializer& rhs) const noexcept
    {
        if (boundShaderState != rhs.boundShaderState || blendState != rhs.blendState ||
            rasterizerState != rhs.rasterizerState || depthStencilState != rhs.depthStencilState ||
            primitiveType != rhs.primitiveType || renderTargetsEnabled != rhs.renderTargetsEnabled ||
            depthStencilTargetFormat != rhs.depthStencilTargetFormat ||
            ((depthStencilTargetFlag & kRelevantDepthStencilFlagMask) !=
             (rhs.depthStencilTargetFlag & kRelevantDepthStencilFlagMask)) ||
            depthTargetLoadAction != rhs.depthTargetLoadAction ||
            depthTargetStoreAction != rhs.depthTargetStoreAction ||
            stencilTargetLoadAction != rhs.stencilTargetLoadAction ||
            stencilTargetStoreAction != rhs.stencilTargetStoreAction || depthStencilAccess != rhs.depthStencilAccess ||
            numSamples != rhs.numSamples || subpassHint != rhs.subpassHint || subpassIndex != rhs.subpassIndex ||
            conservativeRasterization != rhs.conservativeRasterization || depthBounds != rhs.depthBounds ||
            multiViewCount != rhs.multiViewCount || hasFragmentDensityAttachment != rhs.hasFragmentDensityAttachment ||
            allowVariableRateShading != rhs.allowVariableRateShading || shadingRate != rhs.shadingRate)
        {
            return false;
        }

        const gp::UInt32 count = (renderTargetsEnabled <= kMaxSimultaneousRenderTargets)
                                     ? renderTargetsEnabled
                                     : kMaxSimultaneousRenderTargets;
        for (gp::UInt32 i = 0u; i < count; ++i)
        {
            if (renderTargetFormats[i] != rhs.renderTargetFormats[i] ||
                ((renderTargetFlags[i] & kRelevantRenderTargetFlagMask) !=
                 (rhs.renderTargetFlags[i] & kRelevantRenderTargetFlagMask)))
            {
                return false;
            }
        }

        return true;
    }
};

/// @brief Creation descriptor for a `ComputePipelineState`.
struct ComputePipelineStateInitializer
{
    /// @brief Compute shader stage compiled into the pipeline state.
    shader::ComputeShader* computeShader{ nullptr };

    /// @brief Optional specialization constant keys applied to `computeShader`.
    shader::SpecializationContainer specializationConstants{};

    /// @brief Packed 16-bit non-hashed precache and file-cache origin hint flags.
    /// @details Layout: bits `[0..13]` reserved, bit `14` = `psoPrecache`, bit `15` = `fromPSOFileCache`.
    gp::UInt16 rawFlags{ 0u };

    /// @brief Reports whether this compute initializer originates from async PSO precaching.
    /// @return True when the `psoPrecache` hint bit is set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isPSOPrecache() const noexcept
    {
        return (rawFlags & (1u << 14u)) != 0u;
    }

    /// @brief Marks whether this compute initializer originates from async PSO precaching.
    /// @param[in] inPrecache True for precache builds.
    GP_FORCEINLINE_HINT void setPSOPrecache(const bool inPrecache) noexcept
    {
        rawFlags = inPrecache ? static_cast<gp::UInt16>(rawFlags | (1u << 14u))
                              : static_cast<gp::UInt16>(rawFlags & ~(1u << 14u));
    }

    /// @brief Reports whether this compute initializer was loaded from a serialized PSO file cache.
    /// @return True when the `fromPSOFileCache` hint bit is set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isFromPSOFileCache() const noexcept
    {
        return (rawFlags & (1u << 15u)) != 0u;
    }

    /// @brief Marks whether this compute initializer was loaded from a serialized PSO file cache.
    /// @param[in] inFromCache True when loaded from cache.
    GP_FORCEINLINE_HINT void setFromPSOFileCache(const bool inFromCache) noexcept
    {
        rawFlags = inFromCache ? static_cast<gp::UInt16>(rawFlags | (1u << 15u))
                               : static_cast<gp::UInt16>(rawFlags & ~(1u << 15u));
    }

    /// @brief Constructs an empty compute pipeline initializer.
    ComputePipelineStateInitializer() noexcept = default;

    /// @brief Constructs a compute pipeline initializer for `inComputeShader`.
    /// @param[in] inComputeShader Compute shader stage pointer.
    explicit ComputePipelineStateInitializer(shader::ComputeShader* const inComputeShader) noexcept
        : computeShader(inComputeShader)
    {}

    /// @brief Constructs a compute pipeline initializer with specialization constants.
    /// @param[in] inComputeShader Compute shader stage pointer.
    /// @param[in] inSpecializationConstants Specialization constant keys applied to `inComputeShader`.
    ComputePipelineStateInitializer(
        shader::ComputeShader* const inComputeShader, const shader::SpecializationView inSpecializationConstants
    ) noexcept
        : computeShader(inComputeShader)
    {
        if (!inSpecializationConstants.isEmpty())
        {
            specializationConstants.reserve(inSpecializationConstants.size());
            for (const shader::SpecializationKey key: inSpecializationConstants)
            {
                specializationConstants.emplaceBack(key);
            }
        }
    }

    /// @brief Compares shader pointer and specialization keys, ignoring precache hint bits.
    /// @param[in] rhs Initializer to compare against.
    /// @return True when shader and specialization keys match.
    [[nodiscard]] bool operator==(const ComputePipelineStateInitializer& rhs) const noexcept
    {
        return (computeShader == rhs.computeShader) && (specializationConstants == rhs.specializationConstants);
    }
};

}   // namespace gp::rhi::pipeline
