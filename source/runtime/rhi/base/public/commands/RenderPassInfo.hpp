// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "commands/CommandTypes.hpp"
#include "containers/arrays/Array.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "resources/Texture.hpp"
#include "RHIBase.hpp"
#include "RHIDefinitions.hpp"
#include "sync/SyncEnums.hpp"

namespace gp::rhi::command
{

/// @brief Single color render target attachment descriptor with mip/slice selection and load/store actions.
struct RenderTargetView
{
    /// @brief Target color texture pointer, or `nullptr` if unbound.
    resource::Texture* texture{ nullptr };

    /// @brief Target mip level index.
    gp::UInt32 mipIndex{ 0u };

    /// @brief Target array slice index, or `~0u` for all slices.
    gp::UInt32 arraySliceIndex{ 0xFFFFFFFFu };

    /// @brief Pass-entry load policy.
    RenderTargetLoadAction loadAction{ RenderTargetLoadAction::NoAction };

    /// @brief Pass-exit store policy.
    RenderTargetStoreAction storeAction{ RenderTargetStoreAction::NoAction };

    /// @brief Constructs an unbound color render target view.
    constexpr RenderTargetView() noexcept = default;

    /// @brief Constructs a color render target view with explicit load and store actions.
    /// @param[in] inTexture Color texture pointer.
    /// @param[in] inLoadAction Pass-entry load policy.
    /// @param[in] inStoreAction Pass-exit store policy.
    constexpr RenderTargetView(
        resource::Texture* const inTexture,
        const RenderTargetLoadAction inLoadAction = RenderTargetLoadAction::Load,
        const RenderTargetStoreAction inStoreAction = RenderTargetStoreAction::Store
    ) noexcept
        : texture(inTexture)
        , loadAction(inLoadAction)
        , storeAction(inStoreAction)
    {}

    /// @brief Constructs a color render target view targeting a specific mip level and array slice.
    /// @param[in] inTexture Color texture pointer.
    /// @param[in] inMipIndex Target mip level.
    /// @param[in] inArraySliceIndex Target array slice, or `~0u` for all slices.
    /// @param[in] inLoadAction Pass-entry load policy.
    /// @param[in] inStoreAction Pass-exit store policy.
    constexpr RenderTargetView(
        resource::Texture* const inTexture,
        const gp::UInt32 inMipIndex,
        const gp::UInt32 inArraySliceIndex,
        const RenderTargetLoadAction inLoadAction = RenderTargetLoadAction::Load,
        const RenderTargetStoreAction inStoreAction = RenderTargetStoreAction::Store
    ) noexcept
        : texture(inTexture)
        , mipIndex(inMipIndex)
        , arraySliceIndex(inArraySliceIndex)
        , loadAction(inLoadAction)
        , storeAction(inStoreAction)
    {}

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs View to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const RenderTargetView& rhs) const noexcept = default;
};

/// @brief Depth-stencil attachment descriptor carrying separate depth/stencil actions and exclusive access mode.
struct DepthRenderTargetView
{
    /// @brief Target depth-stencil texture pointer, or `nullptr` if unbound.
    resource::Texture* texture{ nullptr };

    /// @brief Load action for the depth plane.
    RenderTargetLoadAction depthLoadAction{ RenderTargetLoadAction::NoAction };

    /// @brief Store action for the depth plane.
    RenderTargetStoreAction depthStoreAction{ RenderTargetStoreAction::NoAction };

    /// @brief Load action for the stencil plane.
    RenderTargetLoadAction stencilLoadAction{ RenderTargetLoadAction::NoAction };

    /// @brief Constructs an unbound depth-stencil view.
    constexpr DepthRenderTargetView() noexcept = default;

    /// @brief Constructs a depth-stencil view with separate depth and stencil load/store actions.
    /// @param[in] inTexture Depth-stencil texture pointer.
    /// @param[in] inDepthLoadAction Depth load policy.
    /// @param[in] inDepthStoreAction Depth store policy.
    /// @param[in] inStencilLoadAction Stencil load policy.
    /// @param[in] inStencilStoreAction Stencil store policy.
    /// @param[in] inDepthStencilAccess Exclusive depth-stencil read/write access mode.
    constexpr DepthRenderTargetView(
        resource::Texture* const inTexture,
        const RenderTargetLoadAction inDepthLoadAction,
        const RenderTargetStoreAction inDepthStoreAction,
        const RenderTargetLoadAction inStencilLoadAction,
        const RenderTargetStoreAction inStencilStoreAction,
        const resource::ExclusiveDepthStencil inDepthStencilAccess =
            resource::ExclusiveDepthStencil::kDepthWriteStencilWrite
    ) noexcept
        : texture(inTexture)
        , depthLoadAction(inDepthLoadAction)
        , depthStoreAction(inDepthStoreAction)
        , stencilLoadAction(inStencilLoadAction)
        , m_stencilStoreAction(inStencilStoreAction)
        , m_depthStencilAccess(inDepthStencilAccess)
    {}

    /// @brief Constructs a depth-stencil view sharing load and store actions across both planes.
    /// @param[in] inTexture Depth-stencil texture pointer.
    /// @param[in] inLoadAction Shared load policy.
    /// @param[in] inStoreAction Shared store policy.
    /// @param[in] inDepthStencilAccess Exclusive depth-stencil read/write access mode.
    constexpr DepthRenderTargetView(
        resource::Texture* const inTexture,
        const RenderTargetLoadAction inLoadAction = RenderTargetLoadAction::Load,
        const RenderTargetStoreAction inStoreAction = RenderTargetStoreAction::Store,
        const resource::ExclusiveDepthStencil inDepthStencilAccess =
            resource::ExclusiveDepthStencil::kDepthWriteStencilWrite
    ) noexcept
        : texture(inTexture)
        , depthLoadAction(inLoadAction)
        , depthStoreAction(inStoreAction)
        , stencilLoadAction(inLoadAction)
        , m_stencilStoreAction(inStoreAction)
        , m_depthStencilAccess(inDepthStencilAccess)
    {}

    /// @brief Returns the store action for the stencil plane.
    /// @return Stencil `RenderTargetStoreAction`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr RenderTargetStoreAction getStencilStoreAction() const noexcept
    {
        return m_stencilStoreAction;
    }

    /// @brief Returns the exclusive depth-stencil access mode.
    /// @return `resource::ExclusiveDepthStencil`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr resource::ExclusiveDepthStencil getDepthStencilAccess() const noexcept
    {
        return m_depthStencilAccess;
    }

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs View to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const DepthRenderTargetView& rhs) const noexcept = default;

private:
    RenderTargetStoreAction m_stencilStoreAction{ RenderTargetStoreAction::NoAction };
    resource::ExclusiveDepthStencil m_depthStencilAccess{ resource::ExclusiveDepthStencil::kDepthNopStencilNop };
};

/// @brief Flattened render target binding array used by backends and render pass validation.
struct SetRenderTargetsInfo
{
    /// @brief Bound color render target views.
    gp::Array<RenderTargetView, kMaxSimultaneousRenderTargets> colorRenderTarget{};

    /// @brief Number of bound color render targets.
    gp::Int32 numColorRenderTargets{ 0 };

    /// @brief True when at least one color target uses `RenderTargetLoadAction::Clear`.
    bool clearColor{ false };

    /// @brief Optional MSAA resolve destination views paired with `colorRenderTarget`.
    gp::Array<RenderTargetView, kMaxSimultaneousRenderTargets> colorResolveRenderTarget{};

    /// @brief True when any color or depth-stencil attachment has a resolve destination.
    bool hasResolveAttachments{ false };

    /// @brief Bound depth-stencil attachment view.
    DepthRenderTargetView depthStencilRenderTarget{};

    /// @brief Optional depth-stencil MSAA resolve destination view.
    DepthRenderTargetView depthStencilResolveRenderTarget{};

    /// @brief True when the depth plane uses `RenderTargetLoadAction::Clear`.
    bool clearDepth{ false };

    /// @brief True when the stencil plane uses `RenderTargetLoadAction::Clear`.
    bool clearStencil{ false };

    /// @brief Optional screen-space Variable Rate Shading attachment texture.
    resource::Texture* shadingRateTexture{ nullptr };

    /// @brief Combiner applied to `shadingRateTexture`.
    pipeline::VRSRateCombiner shadingRateTextureCombiner{ pipeline::VRSRateCombiner::Passthrough };

    /// @brief Multiview layer count (`0` when disabled).
    gp::UInt8 multiViewCount{ 0u };

    /// @brief Optional tile-access scissor region for the render pass.
    resource::IntRect accessRect{};

    /// @brief Updates depth and stencil clear flags together.
    /// @param[in] inClearDepth True when clearing depth.
    /// @param[in] inClearStencil True when clearing stencil.
    constexpr void setClearDepthStencil(const bool inClearDepth, const bool inClearStencil = false) noexcept
    {
        clearDepth = inClearDepth;
        clearStencil = inClearStencil;
    }

    /// @brief Computes a deterministic 64-bit hash over all bound attachment pointers and actions.
    /// @return 64-bit hash digest.
    [[nodiscard]] gp::UInt64 calculateHash() const noexcept
    {
        gp::hashing::Fnv1a64 hasher{};
        hasher.append(numColorRenderTargets);
        hasher.append(clearColor);
        hasher.append(hasResolveAttachments);
        hasher.append(clearDepth);
        hasher.append(clearStencil);
        hasher.append(multiViewCount);
        hasher.append(static_cast<gp::UInt8>(shadingRateTextureCombiner));
        hasher.append(reinterpret_cast<gp::UIntPtr>(shadingRateTexture));

        const gp::Int32 count = (numColorRenderTargets <= static_cast<gp::Int32>(kMaxSimultaneousRenderTargets))
                                    ? numColorRenderTargets
                                    : static_cast<gp::Int32>(kMaxSimultaneousRenderTargets);
        for (gp::Int32 i = 0; i < count; ++i)
        {
            const RenderTargetView& rt = colorRenderTarget[static_cast<gp::USize>(i)];
            hasher.append(reinterpret_cast<gp::UIntPtr>(rt.texture));
            hasher.append(rt.mipIndex);
            hasher.append(rt.arraySliceIndex);
            hasher.append(static_cast<gp::UInt8>(rt.loadAction));
            hasher.append(static_cast<gp::UInt8>(rt.storeAction));

            const RenderTargetView& resolveRT = colorResolveRenderTarget[static_cast<gp::USize>(i)];
            hasher.append(reinterpret_cast<gp::UIntPtr>(resolveRT.texture));
            hasher.append(resolveRT.mipIndex);
            hasher.append(resolveRT.arraySliceIndex);
        }

        hasher.append(reinterpret_cast<gp::UIntPtr>(depthStencilRenderTarget.texture));
        hasher.append(static_cast<gp::UInt8>(depthStencilRenderTarget.depthLoadAction));
        hasher.append(static_cast<gp::UInt8>(depthStencilRenderTarget.depthStoreAction));
        hasher.append(static_cast<gp::UInt8>(depthStencilRenderTarget.stencilLoadAction));
        hasher.append(static_cast<gp::UInt8>(depthStencilRenderTarget.getStencilStoreAction()));
        depthStencilRenderTarget.getDepthStencilAccess().hashAppend(hasher);
        hasher.append(reinterpret_cast<gp::UIntPtr>(depthStencilResolveRenderTarget.texture));
        hasher.append(accessRect.min.x);
        hasher.append(accessRect.min.y);
        hasher.append(accessRect.max.x);
        hasher.append(accessRect.max.y);
        return hasher.finalize();
    }
};

/// @brief Complete render pass descriptor passed to `ICommandContext::beginRenderPass()` and
/// `CommandList::beginRenderPass()`.
struct GP_RHI_API RenderPassInfo
{
    /// @brief Color attachment slot inside a render pass.
    struct ColorEntry
    {
        /// @brief Target color texture, or `nullptr` if unused.
        resource::Texture* renderTarget{ nullptr };

        /// @brief Optional MSAA resolve destination texture.
        resource::Texture* resolveTarget{ nullptr };

        /// @brief Target array slice (`-1` selects all slices).
        gp::Int32 arraySlice{ -1 };

        /// @brief Target mip level index.
        gp::UInt8 mipIndex{ 0u };

        /// @brief Combined load and store actions for this color attachment.
        RenderTargetActions action{ RenderTargetActions::DontLoadDontStore };
    };

    /// @brief Depth-stencil attachment slot inside a render pass.
    struct DepthStencilEntry
    {
        /// @brief Target depth-stencil texture, or `nullptr` if unused.
        resource::Texture* depthStencilTarget{ nullptr };

        /// @brief Optional MSAA resolve destination texture.
        resource::Texture* resolveTarget{ nullptr };

        /// @brief Combined depth and stencil load/store actions.
        DepthStencilTargetActions action{ DepthStencilTargetActions::DontLoadDontStore };

        /// @brief Exclusive depth-stencil read/write state.
        resource::ExclusiveDepthStencil exclusiveDepthStencil{ resource::ExclusiveDepthStencil::kDepthNopStencilNop };
    };

public:
    /// @brief Bound color attachment entries.
    gp::Array<ColorEntry, kMaxSimultaneousRenderTargets> colorRenderTargets{};

    /// @brief Bound depth-stencil attachment entry.
    DepthStencilEntry depthStencilRenderTarget{};

    /// @brief Optional sub-region for MSAA resolve operations.
    ResolveRect resolveRect{};

    /// @brief Optional access scissor rectangle bounding render pass writes.
    resource::IntRect accessRect{};

    /// @brief Optional screen-space Variable Rate Shading attachment texture.
    resource::TextureRef shadingRateTexture{};

    /// @brief Combiner mode applied to `shadingRateTexture`.
    pipeline::VRSRateCombiner shadingRateTextureCombiner{ pipeline::VRSRateCombiner::Passthrough };

    /// @brief Fallback resource access state used when a pass has no active render targets.
    sync::ResourceAccess nopAccess{ sync::ResourceAccess::Unknown };

    /// @brief Maximum number of occlusion queries issued inside this render pass.
    gp::UInt32 numOcclusionQueries{ 0u };

    /// @brief True when occlusion queries may be executed inside this render pass.
    bool occlusionQueries{ false };

    /// @brief Multiview layer count (`0` when disabled).
    gp::UInt8 multiViewCount{ 0u };

    /// @brief Tile-based subpass optimization hint.
    pipeline::SubpassHint subpassHint{ pipeline::SubpassHint::None };

public:
    /// @brief Constructs an empty render pass descriptor.
    RenderPassInfo() noexcept = default;

    /// @brief Constructs a single-color-target render pass descriptor.
    /// @param[in] colorRT Color render target texture.
    /// @param[in] colorAction Load/store action for `colorRT`.
    /// @param[in] resolveRT Optional MSAA resolve destination.
    /// @param[in] mipIndex Target mip index.
    /// @param[in] arraySlice Target array slice (`-1` for all slices).
    RenderPassInfo(
        resource::Texture* const colorRT,
        const RenderTargetActions colorAction,
        resource::Texture* const resolveRT = nullptr,
        const gp::UInt8 mipIndex = 0u,
        const gp::Int32 arraySlice = -1
    ) noexcept
    {
        colorRenderTargets[0].renderTarget = colorRT;
        colorRenderTargets[0].resolveTarget = resolveRT;
        colorRenderTargets[0].action = colorAction;
        colorRenderTargets[0].mipIndex = mipIndex;
        colorRenderTargets[0].arraySlice = arraySlice;
    }

    /// @brief Constructs a depth-only render pass descriptor.
    /// @param[in] depthRT Depth-stencil texture.
    /// @param[in] depthAction Depth and stencil load/store actions.
    /// @param[in] depthStencilAccess Exclusive depth-stencil read/write mode.
    /// @param[in] resolveDepthRT Optional depth resolve destination.
    RenderPassInfo(
        resource::Texture* const depthRT,
        const DepthStencilTargetActions depthAction,
        const resource::ExclusiveDepthStencil depthStencilAccess =
            resource::ExclusiveDepthStencil::kDepthWriteStencilWrite,
        resource::Texture* const resolveDepthRT = nullptr
    ) noexcept
    {
        depthStencilRenderTarget.depthStencilTarget = depthRT;
        depthStencilRenderTarget.resolveTarget = resolveDepthRT;
        depthStencilRenderTarget.action = depthAction;
        depthStencilRenderTarget.exclusiveDepthStencil = depthStencilAccess;
    }

    /// @brief Constructs a single-color plus depth-stencil render pass descriptor.
    /// @param[in] colorRT Color render target texture.
    /// @param[in] colorAction Load/store action for `colorRT`.
    /// @param[in] depthRT Depth-stencil texture.
    /// @param[in] depthAction Depth and stencil load/store actions.
    /// @param[in] depthStencilAccess Exclusive depth-stencil read/write mode.
    /// @param[in] resolveColorRT Optional color resolve destination.
    /// @param[in] resolveDepthRT Optional depth resolve destination.
    RenderPassInfo(
        resource::Texture* const colorRT,
        const RenderTargetActions colorAction,
        resource::Texture* const depthRT,
        const DepthStencilTargetActions depthAction,
        const resource::ExclusiveDepthStencil depthStencilAccess =
            resource::ExclusiveDepthStencil::kDepthWriteStencilWrite,
        resource::Texture* const resolveColorRT = nullptr,
        resource::Texture* const resolveDepthRT = nullptr
    ) noexcept
    {
        colorRenderTargets[0].renderTarget = colorRT;
        colorRenderTargets[0].resolveTarget = resolveColorRT;
        colorRenderTargets[0].action = colorAction;
        depthStencilRenderTarget.depthStencilTarget = depthRT;
        depthStencilRenderTarget.resolveTarget = resolveDepthRT;
        depthStencilRenderTarget.action = depthAction;
        depthStencilRenderTarget.exclusiveDepthStencil = depthStencilAccess;
    }

    /// @brief Constructs a multi-render-target (MRT) pass with optional depth-stencil attachment.
    /// @param[in] numColorRTs Number of entries in `colorRTs`.
    /// @param[in] colorRTs Array of color target pointers.
    /// @param[in] colorAction Shared load/store action for all color targets.
    /// @param[in] depthRT Optional depth-stencil texture.
    /// @param[in] depthAction Depth and stencil load/store actions.
    /// @param[in] depthStencilAccess Exclusive depth-stencil read/write mode.
    RenderPassInfo(
        const gp::UInt32 numColorRTs,
        resource::Texture* const* const colorRTs,
        const RenderTargetActions colorAction,
        resource::Texture* const depthRT = nullptr,
        const DepthStencilTargetActions depthAction = DepthStencilTargetActions::DontLoadDontStore,
        const resource::ExclusiveDepthStencil depthStencilAccess = resource::ExclusiveDepthStencil::kDepthNopStencilNop
    ) noexcept
    {
        const gp::UInt32 count =
            (numColorRTs <= kMaxSimultaneousRenderTargets) ? numColorRTs : kMaxSimultaneousRenderTargets;
        for (gp::UInt32 i = 0u; i < count; ++i)
        {
            colorRenderTargets[i].renderTarget = colorRTs[i];
            colorRenderTargets[i].action = colorAction;
        }
        depthStencilRenderTarget.depthStencilTarget = depthRT;
        depthStencilRenderTarget.action = depthAction;
        depthStencilRenderTarget.exclusiveDepthStencil =
            (depthRT != nullptr && depthStencilAccess == resource::ExclusiveDepthStencil::kDepthNopStencilNop)
                ? resource::ExclusiveDepthStencil::kDepthWriteStencilWrite
                : depthStencilAccess;
    }

    /// @brief Reports whether a non-empty `accessRect` scissor has been specified.
    /// @return True when `accessRect.width() > 0 && accessRect.height() > 0`.
    [[nodiscard]] constexpr bool hasAccessRect() const noexcept
    {
        return (accessRect.width() > 0) && (accessRect.height() > 0);
    }

    /// @brief Counts the number of contiguous bound color attachments starting from slot 0.
    /// @return Active color attachment count in `[0, kMaxSimultaneousRenderTargets]`.
    [[nodiscard]] gp::UInt32 getNumColorRenderTargets() const noexcept
    {
        gp::UInt32 count = 0u;
        for (gp::UInt32 i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
        {
            if (colorRenderTargets[i].renderTarget == nullptr)
            {
                break;
            }
            ++count;
        }
        return count;
    }

    /// @brief Extracts PSO-compatible render target format, flag, load/store, and MSAA metadata.
    /// @return Populated `pipeline::GraphicsPipelineRenderTargetsInfo`.
    [[nodiscard]] pipeline::GraphicsPipelineRenderTargetsInfo extractRenderTargetsInfo() const noexcept
    {
        pipeline::GraphicsPipelineRenderTargetsInfo info{};
        info.renderTargetsEnabled = getNumColorRenderTargets();

        gp::UInt16 detectedSamples = 0u;
        for (gp::UInt32 i = 0u; i < info.renderTargetsEnabled; ++i)
        {
            const resource::Texture* const rt = colorRenderTargets[i].renderTarget;
            if (rt != nullptr)
            {
                info.renderTargetFormats[i] = rt->getFormat();
                info.renderTargetFlags[i] = rt->getFlags();
                if (detectedSamples == 0u)
                {
                    detectedSamples = static_cast<gp::UInt16>(rt->getNumSamples());
                }
            }
        }

        if (depthStencilRenderTarget.depthStencilTarget != nullptr)
        {
            const resource::Texture* const ds = depthStencilRenderTarget.depthStencilTarget;
            info.depthStencilTargetFormat = ds->getFormat();
            info.depthStencilTargetFlag = ds->getFlags();
            const RenderTargetActions depthActions = getDepthActions(depthStencilRenderTarget.action);
            const RenderTargetActions stencilActions = getStencilActions(depthStencilRenderTarget.action);
            info.depthTargetLoadAction = getLoadAction(depthActions);
            info.depthTargetStoreAction = getStoreAction(depthActions);
            info.stencilTargetLoadAction = getLoadAction(stencilActions);
            info.stencilTargetStoreAction = getStoreAction(stencilActions);
            info.depthStencilAccess = depthStencilRenderTarget.exclusiveDepthStencil;
            if (detectedSamples == 0u)
            {
                detectedSamples = static_cast<gp::UInt16>(ds->getNumSamples());
            }
        }

        info.numSamples = (detectedSamples > 0u) ? detectedSamples : 1u;
        info.subpassHint = subpassHint;
        info.multiViewCount = multiViewCount;
        info.hasFragmentDensityAttachment = shadingRateTexture.isValid();
        return info;
    }

    /// @brief Converts this render pass descriptor into a flattened `SetRenderTargetsInfo`.
    /// @param[out] outRTInfo Destination flattened render target descriptor.
    void convertToRenderTargetsInfo(SetRenderTargetsInfo& outRTInfo) const noexcept
    {
        outRTInfo = SetRenderTargetsInfo{};
        const gp::UInt32 numColor = getNumColorRenderTargets();
        outRTInfo.numColorRenderTargets = static_cast<gp::Int32>(numColor);

        for (gp::UInt32 i = 0u; i < numColor; ++i)
        {
            const ColorEntry& entry = colorRenderTargets[i];
            const RenderTargetLoadAction loadAct = getLoadAction(entry.action);
            const RenderTargetStoreAction storeAct = getStoreAction(entry.action);
            const gp::UInt32 sliceIdx =
                (entry.arraySlice >= 0) ? static_cast<gp::UInt32>(entry.arraySlice) : 0xFFFFFFFFu;

            outRTInfo.colorRenderTarget[i] =
                RenderTargetView(entry.renderTarget, entry.mipIndex, sliceIdx, loadAct, storeAct);
            if (loadAct == RenderTargetLoadAction::Clear)
            {
                outRTInfo.clearColor = true;
            }

            if (entry.resolveTarget != nullptr)
            {
                outRTInfo.colorResolveRenderTarget[i] =
                    RenderTargetView(entry.resolveTarget, entry.mipIndex, sliceIdx, loadAct, storeAct);
                outRTInfo.hasResolveAttachments = true;
            }
        }

        if (depthStencilRenderTarget.depthStencilTarget != nullptr)
        {
            const RenderTargetActions depthActions = getDepthActions(depthStencilRenderTarget.action);
            const RenderTargetActions stencilActions = getStencilActions(depthStencilRenderTarget.action);
            const RenderTargetLoadAction depthLoad = getLoadAction(depthActions);
            const RenderTargetStoreAction depthStore = getStoreAction(depthActions);
            const RenderTargetLoadAction stencilLoad = getLoadAction(stencilActions);
            const RenderTargetStoreAction stencilStore = getStoreAction(stencilActions);

            outRTInfo.depthStencilRenderTarget = DepthRenderTargetView(
                depthStencilRenderTarget.depthStencilTarget,
                depthLoad,
                depthStore,
                stencilLoad,
                stencilStore,
                depthStencilRenderTarget.exclusiveDepthStencil
            );
            outRTInfo.clearDepth = (depthLoad == RenderTargetLoadAction::Clear);
            outRTInfo.clearStencil = (stencilLoad == RenderTargetLoadAction::Clear);

            if (depthStencilRenderTarget.resolveTarget != nullptr &&
                depthStencilRenderTarget.resolveTarget != depthStencilRenderTarget.depthStencilTarget)
            {
                outRTInfo.depthStencilResolveRenderTarget = DepthRenderTargetView(
                    depthStencilRenderTarget.resolveTarget,
                    depthLoad,
                    depthStore,
                    stencilLoad,
                    stencilStore,
                    depthStencilRenderTarget.exclusiveDepthStencil
                );
                outRTInfo.hasResolveAttachments = true;
            }
        }

        outRTInfo.shadingRateTexture = shadingRateTexture.get();
        outRTInfo.shadingRateTextureCombiner = shadingRateTextureCombiner;
        outRTInfo.multiViewCount = multiViewCount;
        outRTInfo.accessRect = accessRect;
    }

    /// @brief Validates attachment contiguity, sample-count consistency, resolve contracts, and access rect bounds.
    /// @return True when the render pass descriptor satisfies RHI invariants; false otherwise.
    [[nodiscard]] bool validate() const noexcept
    {
        bool seenNull = false;
        gp::UInt32 expectedSamples = 0u;
        gp::Int32 maxTargetWidth = 0;
        gp::Int32 maxTargetHeight = 0;
        const bool allowCustomResolveSampleMismatch = (subpassHint == pipeline::SubpassHint::CustomResolveSubpass);

        for (gp::UInt32 i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
        {
            const ColorEntry& entry = colorRenderTargets[i];
            const resource::Texture* const rt = entry.renderTarget;
            if (rt == nullptr)
            {
                seenNull = true;
                continue;
            }
            if (seenNull)
            {
                return false;
            }

            const gp::UInt32 samples = rt->getNumSamples();
            if (expectedSamples == 0u)
            {
                expectedSamples = samples;
                const gp::UInt32 shift = (entry.mipIndex < 32u) ? entry.mipIndex : 31u;
                maxTargetWidth = gp::math::max<gp::Int32>(1, rt->getDesc().extent.x >> shift);
                maxTargetHeight = gp::math::max<gp::Int32>(1, rt->getDesc().extent.y >> shift);
            }
            else if (!allowCustomResolveSampleMismatch && samples != expectedSamples)
            {
                return false;
            }

            if (getStoreAction(entry.action) == RenderTargetStoreAction::MultisampleResolve)
            {
                if (samples <= 1u || entry.resolveTarget == nullptr || entry.resolveTarget->getNumSamples() != 1u)
                {
                    return false;
                }
            }
        }

        const resource::Texture* const ds = depthStencilRenderTarget.depthStencilTarget;
        if (ds != nullptr)
        {
            const gp::UInt32 dsSamples = ds->getNumSamples();
            if (expectedSamples == 0u)
            {
                expectedSamples = dsSamples;
                maxTargetWidth = ds->getDesc().extent.x;
                maxTargetHeight = ds->getDesc().extent.y;
            }
            else if (!allowCustomResolveSampleMismatch && dsSamples != expectedSamples)
            {
                return false;
            }

            const RenderTargetStoreAction depthStore = getStoreAction(getDepthActions(depthStencilRenderTarget.action));
            const RenderTargetStoreAction stencilStore =
                getStoreAction(getStencilActions(depthStencilRenderTarget.action));
            if (depthStore == RenderTargetStoreAction::MultisampleResolve ||
                stencilStore == RenderTargetStoreAction::MultisampleResolve)
            {
                if (dsSamples <= 1u || depthStencilRenderTarget.resolveTarget == nullptr ||
                    depthStencilRenderTarget.resolveTarget->getNumSamples() != 1u)
                {
                    return false;
                }
            }
        }
        else
        {
            if (depthStencilRenderTarget.action != DepthStencilTargetActions::DontLoadDontStore ||
                depthStencilRenderTarget.exclusiveDepthStencil != resource::ExclusiveDepthStencil::kDepthNopStencilNop)
            {
                return false;
            }
        }

        if (hasAccessRect() && maxTargetWidth > 0 && maxTargetHeight > 0)
        {
            if (accessRect.min.x < 0 || accessRect.min.y < 0 || accessRect.max.x > maxTargetWidth ||
                accessRect.max.y > maxTargetHeight)
            {
                return false;
            }
        }

        return true;
    }
};

}   // namespace gp::rhi::command
