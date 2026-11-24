// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep
#include "templates/NumericLimits.hpp"
#include "templates/Optional.hpp"
#include "views/ViewForward.hpp"

namespace gp::rhi::sync
{

/// @brief Identifies a mip level, array slice, and plane slice range within a texture resource.
struct SubresourceRange
{
    /// @brief Plane slice index for the depth component of a depth-stencil texture.
    static constexpr gp::UInt16 kDepthPlaneSlice = 0u;

    /// @brief Plane slice index for the stencil component of a depth-stencil texture.
    static constexpr gp::UInt16 kStencilPlaneSlice = 1u;

    /// @brief Sentinel selecting every mip, array slice, or plane along a given subresource axis.
    static constexpr gp::UInt16 kAllSubresources = gp::NumericLimits<gp::UInt16>::max();

    /// @brief Target mip level, or `kAllSubresources` for the full mip chain.
    gp::UInt16 mipIndex{ kAllSubresources };

    /// @brief Target array slice, or `kAllSubresources` for all slices.
    gp::UInt16 arraySlice{ kAllSubresources };

    /// @brief Target plane slice, or `kAllSubresources` for all planes.
    gp::UInt16 planeSlice{ kAllSubresources };

    /// @brief Constructs a range covering the entire resource.
    constexpr SubresourceRange() noexcept = default;

    /// @brief Constructs a range addressing specific mip, array, and plane indices with bounds validation.
    /// @param[in] inMipIndex Mip level index, or `kAllSubresources`.
    /// @param[in] inArraySlice Array slice index, or `kAllSubresources`.
    /// @param[in] inPlaneSlice Plane slice index, or `kAllSubresources`.
    constexpr SubresourceRange(
        const gp::UInt32 inMipIndex, const gp::UInt32 inArraySlice, const gp::UInt32 inPlaneSlice
    ) noexcept
        : mipIndex(static_cast<gp::UInt16>(inMipIndex))
        , arraySlice(static_cast<gp::UInt16>(inArraySlice))
        , planeSlice(static_cast<gp::UInt16>(inPlaneSlice))
    {
        if !consteval
        {
            GP_CHECK_SLOW(inMipIndex <= kAllSubresources);
            GP_CHECK_SLOW(inArraySlice <= kAllSubresources);
            GP_CHECK_SLOW(inPlaneSlice <= kAllSubresources);
        }
    }

    /// @brief Reports whether this range selects every mip level.
    /// @return True when `mipIndex == kAllSubresources`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAllMips() const noexcept
    {
        return mipIndex == kAllSubresources;
    }

    /// @brief Reports whether this range selects every array slice.
    /// @return True when `arraySlice == kAllSubresources`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAllArraySlices() const noexcept
    {
        return arraySlice == kAllSubresources;
    }

    /// @brief Reports whether this range selects every plane slice.
    /// @return True when `planeSlice == kAllSubresources`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAllPlaneSlices() const noexcept
    {
        return planeSlice == kAllSubresources;
    }

    /// @brief Reports whether this range covers the whole resource across all three axes.
    /// @return True when mips, array slices, and plane slices are all `kAllSubresources`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isWholeResource() const noexcept
    {
        return isAllMips() && isAllArraySlices() && isAllPlaneSlices();
    }

    /// @brief Reports whether this range addresses only subresource `(0, 0, 0)`.
    /// @return True when mip, array slice, and plane slice are all zero.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isZeroIndexResource() const noexcept
    {
        return (mipIndex == 0u) && (arraySlice == 0u) && (planeSlice == 0u);
    }

    /// @brief Reports whether this range targets only the stencil plane and excludes depth.
    /// @return True when `planeSlice == kStencilPlaneSlice`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool ignoreDepthPlane() const noexcept
    {
        return planeSlice == kStencilPlaneSlice;
    }

    /// @brief Reports whether this range targets only the depth plane and excludes stencil.
    /// @return True when `planeSlice == kDepthPlaneSlice`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool ignoreStencilPlane() const noexcept
    {
        return planeSlice == kDepthPlaneSlice;
    }

    /// @brief Compares two subresource ranges for exact equality.
    /// @param[in] rhs Range to compare against.
    /// @return True when mip, array slice, and plane slice all match.
    [[nodiscard]] constexpr bool operator==(const SubresourceRange& rhs) const noexcept = default;
};

/// @brief Physical memory commit size request for a reserved GPU resource.
/// @details Growing or shrinking a reserved resource maps or unmaps physical pages at the tail of its committed
///          region without reallocating views. Newly committed pages contain undefined data until written.
struct CommitResourceInfo
{
    /// @brief Target committed size in bytes (must not exceed the resource's maximum virtual size).
    gp::UInt64 sizeInBytes{ 0u };

    /// @brief Constructs a commit request for `inSizeInBytes`.
    /// @param[in] inSizeInBytes Target committed byte size.
    constexpr explicit CommitResourceInfo(const gp::UInt64 inSizeInBytes) noexcept
        : sizeInBytes(inSizeInBytes)
    {}

    /// @brief Compares two commit requests for equality.
    /// @param[in] rhs Request to compare against.
    /// @return True when `sizeInBytes` matches.
    [[nodiscard]] constexpr bool operator==(const CommitResourceInfo& rhs) const noexcept = default;
};

/// @brief Barrier transition descriptor for a single RHI resource or texture subresource range.
struct TransitionInfo : public SubresourceRange
{
    union
    {
        resource::Resource* resource{ nullptr };
        resource::ViewableResource* viewableResource;
        resource::Texture* texture;
        resource::Buffer* buffer;
        view::UnorderedAccessView* uav;
        resource::RayTracingAccelerationStructure* bvh;
    };

    /// @brief Active resource pointer kind stored in the union above.
    TransitionResourceType type{ TransitionResourceType::Unknown };

    /// @brief Resource access state before the barrier, or `ResourceAccess::Unknown` to use tracked state.
    ResourceAccess accessBefore{ ResourceAccess::Unknown };

    /// @brief Resource access state after the barrier.
    ResourceAccess accessAfter{ ResourceAccess::Unknown };

    /// @brief Per-resource transition behavior flags.
    ResourceTransitionFlags flags{ ResourceTransitionFlags::None };

    /// @brief Optional physical memory commit change paired with this buffer transition.
    gp::Optional<CommitResourceInfo> commitInfo{};

    /// @brief Constructs an empty transition entry (`type == TransitionResourceType::Unknown`).
    constexpr TransitionInfo() noexcept = default;

    /// @brief Constructs a texture transition entry with explicit before/after states and subresource range.
    /// @param[in] inTexture Target texture pointer.
    /// @param[in] inPreviousState Resource state before the barrier.
    /// @param[in] inNewState Resource state after the barrier.
    /// @param[in] inFlags Optional transition flags.
    /// @param[in] inMipIndex Target mip level, or `kAllSubresources`.
    /// @param[in] inArraySlice Target array slice, or `kAllSubresources`.
    /// @param[in] inPlaneSlice Target plane slice, or `kAllSubresources`.
    constexpr TransitionInfo(
        resource::Texture* const inTexture,
        const ResourceAccess inPreviousState,
        const ResourceAccess inNewState,
        const ResourceTransitionFlags inFlags = ResourceTransitionFlags::None,
        const gp::UInt32 inMipIndex = kAllSubresources,
        const gp::UInt32 inArraySlice = kAllSubresources,
        const gp::UInt32 inPlaneSlice = kAllSubresources
    ) noexcept
        : SubresourceRange(inMipIndex, inArraySlice, inPlaneSlice)
        , texture(inTexture)
        , type(TransitionResourceType::Texture)
        , accessBefore(inPreviousState)
        , accessAfter(inNewState)
        , flags(inFlags)
    {}

    /// @brief Constructs a UAV transition entry with explicit before/after states.
    /// @param[in] inUav Target unordered access view pointer.
    /// @param[in] inPreviousState Resource state before the barrier.
    /// @param[in] inNewState Resource state after the barrier.
    /// @param[in] inFlags Optional transition flags.
    constexpr TransitionInfo(
        view::UnorderedAccessView* const inUav,
        const ResourceAccess inPreviousState,
        const ResourceAccess inNewState,
        const ResourceTransitionFlags inFlags = ResourceTransitionFlags::None
    ) noexcept
        : uav(inUav)
        , type(TransitionResourceType::UAV)
        , accessBefore(inPreviousState)
        , accessAfter(inNewState)
        , flags(inFlags)
    {}

    /// @brief Constructs a buffer transition entry with explicit before/after states.
    /// @param[in] inBuffer Target buffer pointer.
    /// @param[in] inPreviousState Resource state before the barrier.
    /// @param[in] inNewState Resource state after the barrier.
    /// @param[in] inFlags Optional transition flags.
    constexpr TransitionInfo(
        resource::Buffer* const inBuffer,
        const ResourceAccess inPreviousState,
        const ResourceAccess inNewState,
        const ResourceTransitionFlags inFlags = ResourceTransitionFlags::None
    ) noexcept
        : buffer(inBuffer)
        , type(TransitionResourceType::Buffer)
        , accessBefore(inPreviousState)
        , accessAfter(inNewState)
        , flags(inFlags)
    {}

    /// @brief Constructs a reserved buffer transition paired with a physical memory commit size update.
    /// @param[in] inBuffer Target reserved buffer pointer.
    /// @param[in] inPreviousState Resource state before the barrier.
    /// @param[in] inNewState Resource state after the barrier.
    /// @param[in] inCommitInfo Target physical commit size in bytes.
    constexpr TransitionInfo(
        resource::Buffer* const inBuffer,
        const ResourceAccess inPreviousState,
        const ResourceAccess inNewState,
        const CommitResourceInfo inCommitInfo
    ) noexcept
        : buffer(inBuffer)
        , type(TransitionResourceType::Buffer)
        , accessBefore(inPreviousState)
        , accessAfter(inNewState)
        , commitInfo(inCommitInfo)
    {}

    /// @brief Constructs a ray tracing acceleration structure transition entry.
    /// @param[in] inBvh Target acceleration structure pointer.
    /// @param[in] inPreviousState Resource state before the barrier.
    /// @param[in] inNewState Resource state after the barrier.
    /// @param[in] inFlags Optional transition flags.
    constexpr TransitionInfo(
        resource::RayTracingAccelerationStructure* const inBvh,
        const ResourceAccess inPreviousState,
        const ResourceAccess inNewState,
        const ResourceTransitionFlags inFlags = ResourceTransitionFlags::None
    ) noexcept
        : bvh(inBvh)
        , type(TransitionResourceType::BVH)
        , accessBefore(inPreviousState)
        , accessAfter(inNewState)
        , flags(inFlags)
    {}

    /// @brief Constructs a whole-texture transition using `ResourceAccess::Unknown` as the before state.
    /// @param[in] inTexture Target texture pointer.
    /// @param[in] inNewState Resource state after the barrier.
    constexpr TransitionInfo(resource::Texture* const inTexture, const ResourceAccess inNewState) noexcept
        : texture(inTexture)
        , type(TransitionResourceType::Texture)
        , accessAfter(inNewState)
    {}

    /// @brief Constructs a UAV transition using `ResourceAccess::Unknown` as the before state.
    /// @param[in] inUav Target unordered access view pointer.
    /// @param[in] inNewState Resource state after the barrier.
    constexpr TransitionInfo(view::UnorderedAccessView* const inUav, const ResourceAccess inNewState) noexcept
        : uav(inUav)
        , type(TransitionResourceType::UAV)
        , accessAfter(inNewState)
    {}

    /// @brief Constructs a buffer transition using `ResourceAccess::Unknown` as the before state.
    /// @param[in] inBuffer Target buffer pointer.
    /// @param[in] inNewState Resource state after the barrier.
    constexpr TransitionInfo(resource::Buffer* const inBuffer, const ResourceAccess inNewState) noexcept
        : buffer(inBuffer)
        , type(TransitionResourceType::Buffer)
        , accessAfter(inNewState)
    {}

    /// @brief Compares two transition entries across resource pointer, access states, flags, commit info, and range.
    /// @param[in] rhs Entry to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const TransitionInfo& rhs) const noexcept
    {
        if (type != rhs.type || accessBefore != rhs.accessBefore || accessAfter != rhs.accessAfter ||
            flags != rhs.flags || !SubresourceRange::operator==(rhs))
        {
            return false;
        }

        if (commitInfo.hasValue() != rhs.commitInfo.hasValue())
        {
            return false;
        }
        if (commitInfo.hasValue() && (commitInfo.value() != rhs.commitInfo.value()))
        {
            return false;
        }

        switch (type)
        {
        case TransitionResourceType::Texture:
            return texture == rhs.texture;
        case TransitionResourceType::Buffer:
            return buffer == rhs.buffer;
        case TransitionResourceType::UAV:
            return uav == rhs.uav;
        case TransitionResourceType::BVH:
            return bvh == rhs.bvh;
        case TransitionResourceType::Unknown:
        default:
            return resource == rhs.resource;
        }
    }
};

/// @brief Identifies a prior transient resource whose aliased memory overlaps an acquiring resource.
struct TransientAliasingOverlap
{
    union
    {
        resource::Resource* resource{ nullptr };
        resource::Texture* texture;
        resource::Buffer* buffer;
    };

    /// @brief Concrete resource kind stored in the union above.
    TransientAliasingResourceType type{ TransientAliasingResourceType::Texture };

    /// @brief Constructs an empty overlap entry.
    constexpr TransientAliasingOverlap() noexcept = default;

    /// @brief Constructs an overlap entry from a generic resource pointer and explicit kind.
    /// @param[in] inResource Overlapping resource pointer.
    /// @param[in] inType Concrete resource kind.
    constexpr TransientAliasingOverlap(
        resource::Resource* const inResource, const TransientAliasingResourceType inType
    ) noexcept
        : resource(inResource)
        , type(inType)
    {}

    /// @brief Constructs a texture overlap entry.
    /// @param[in] inTexture Overlapping texture pointer.
    constexpr explicit(false) TransientAliasingOverlap(resource::Texture* const inTexture) noexcept
        : texture(inTexture)
        , type(TransientAliasingResourceType::Texture)
    {}

    /// @brief Constructs a buffer overlap entry.
    /// @param[in] inBuffer Overlapping buffer pointer.
    constexpr explicit(false) TransientAliasingOverlap(resource::Buffer* const inBuffer) noexcept
        : buffer(inBuffer)
        , type(TransientAliasingResourceType::Buffer)
    {}

    /// @brief Reports whether this overlap references a texture.
    /// @return True when `type == TransientAliasingResourceType::Texture`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isTexture() const noexcept
    {
        return type == TransientAliasingResourceType::Texture;
    }

    /// @brief Reports whether this overlap references a buffer.
    /// @return True when `type == TransientAliasingResourceType::Buffer`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isBuffer() const noexcept
    {
        return type == TransientAliasingResourceType::Buffer;
    }

    /// @brief Compares two overlap entries by resource pointer and kind.
    /// @param[in] other Entry to compare against.
    /// @return True when both entries reference the same resource.
    [[nodiscard]] constexpr bool operator==(const TransientAliasingOverlap& other) const noexcept
    {
        if (type != other.type)
        {
            return false;
        }
        return (type == TransientAliasingResourceType::Texture) ? (texture == other.texture) : (buffer == other.buffer);
    }
};

/// @brief Describes an acquire or discard aliasing barrier for a transient heap resource.
struct TransientAliasingInfo
{
    union
    {
        resource::Resource* resource{ nullptr };
        resource::Texture* texture;
        resource::Buffer* buffer;
    };

    /// @brief Prior overlapping resources to synchronize on acquire; must be empty on discard.
    gp::ConstVectorView<TransientAliasingOverlap> overlaps{};

    /// @brief Concrete resource kind stored in the union above.
    TransientAliasingResourceType type{ TransientAliasingResourceType::Texture };

    /// @brief Whether this entry acquires or discards aliased backing memory.
    TransientAliasingAction action{ TransientAliasingAction::Acquire };

    /// @brief Constructs an empty aliasing info entry.
    constexpr TransientAliasingInfo() noexcept = default;

    /// @brief Creates an acquire aliasing descriptor for `inTexture`.
    /// @param[in] inTexture Transient texture being acquired.
    /// @param[in] inOverlaps Prior resources whose backing memory overlaps `inTexture`.
    /// @return Configured `TransientAliasingInfo`.
    [[nodiscard]] static constexpr TransientAliasingInfo acquire(
        resource::Texture* const inTexture, const gp::ConstVectorView<TransientAliasingOverlap> inOverlaps = {}
    ) noexcept
    {
        TransientAliasingInfo info{};
        info.texture = inTexture;
        info.overlaps = inOverlaps;
        info.type = TransientAliasingResourceType::Texture;
        info.action = TransientAliasingAction::Acquire;
        return info;
    }

    /// @brief Creates an acquire aliasing descriptor for `inBuffer`.
    /// @param[in] inBuffer Transient buffer being acquired.
    /// @param[in] inOverlaps Prior resources whose backing memory overlaps `inBuffer`.
    /// @return Configured `TransientAliasingInfo`.
    [[nodiscard]] static constexpr TransientAliasingInfo acquire(
        resource::Buffer* const inBuffer, const gp::ConstVectorView<TransientAliasingOverlap> inOverlaps = {}
    ) noexcept
    {
        TransientAliasingInfo info{};
        info.buffer = inBuffer;
        info.overlaps = inOverlaps;
        info.type = TransientAliasingResourceType::Buffer;
        info.action = TransientAliasingAction::Acquire;
        return info;
    }

    /// @brief Creates a discard aliasing descriptor for `inTexture`.
    /// @param[in] inTexture Transient texture being discarded.
    /// @return Configured `TransientAliasingInfo` with empty overlaps.
    [[nodiscard]] static constexpr TransientAliasingInfo discard(resource::Texture* const inTexture) noexcept
    {
        TransientAliasingInfo info{};
        info.texture = inTexture;
        info.type = TransientAliasingResourceType::Texture;
        info.action = TransientAliasingAction::Discard;
        return info;
    }

    /// @brief Creates a discard aliasing descriptor for `inBuffer`.
    /// @param[in] inBuffer Transient buffer being discarded.
    /// @return Configured `TransientAliasingInfo` with empty overlaps.
    [[nodiscard]] static constexpr TransientAliasingInfo discard(resource::Buffer* const inBuffer) noexcept
    {
        TransientAliasingInfo info{};
        info.buffer = inBuffer;
        info.type = TransientAliasingResourceType::Buffer;
        info.action = TransientAliasingAction::Discard;
        return info;
    }

    /// @brief Reports whether this descriptor represents a memory acquire.
    /// @return True when `action == TransientAliasingAction::Acquire`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAcquire() const noexcept
    {
        return action == TransientAliasingAction::Acquire;
    }

    /// @brief Reports whether this descriptor represents a memory discard.
    /// @return True when `action == TransientAliasingAction::Discard`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isDiscard() const noexcept
    {
        return action == TransientAliasingAction::Discard;
    }

    /// @brief Reports whether this descriptor targets a texture.
    /// @return True when `type == TransientAliasingResourceType::Texture`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isTexture() const noexcept
    {
        return type == TransientAliasingResourceType::Texture;
    }

    /// @brief Reports whether this descriptor targets a buffer.
    /// @return True when `type == TransientAliasingResourceType::Buffer`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isBuffer() const noexcept
    {
        return type == TransientAliasingResourceType::Buffer;
    }

    /// @brief Compares two aliasing descriptors by target resource, kind, and action.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when resource pointer, kind, and action all match.
    [[nodiscard]] constexpr bool operator==(const TransientAliasingInfo& rhs) const noexcept
    {
        if (type != rhs.type || action != rhs.action)
        {
            return false;
        }
        return (type == TransientAliasingResourceType::Texture) ? (texture == rhs.texture) : (buffer == rhs.buffer);
    }
};

/// @brief Creation parameters describing a batch of barrier transitions and transient aliasing operations.
struct TransitionCreateInfo
{
    /// @brief Execution pipelines where the transition begins.
    PipelineStage srcPipelines{ PipelineStage::None };

    /// @brief Execution pipelines where the transition ends.
    PipelineStage dstPipelines{ PipelineStage::None };

    /// @brief Creation flags controlling fencing, splitting, and render-pass rules.
    TransitionCreateFlags flags{ TransitionCreateFlags::None };

    /// @brief Contiguous view of per-resource state transitions.
    gp::ConstVectorView<TransitionInfo> transitionInfos{};

    /// @brief Contiguous view of transient memory aliasing operations.
    gp::ConstVectorView<TransientAliasingInfo> aliasingInfos{};

    /// @brief Constructs an empty transition creation descriptor.
    constexpr TransitionCreateInfo() noexcept = default;

    /// @brief Constructs a transition creation descriptor with explicit pipeline masks and barrier arrays.
    /// @param[in] inSrcPipelines Source execution pipelines.
    /// @param[in] inDstPipelines Destination execution pipelines.
    /// @param[in] inFlags Transition creation flags.
    /// @param[in] inTransitionInfos Per-resource barrier entries.
    /// @param[in] inAliasingInfos Transient aliasing entries.
    constexpr TransitionCreateInfo(
        const PipelineStage inSrcPipelines,
        const PipelineStage inDstPipelines,
        const TransitionCreateFlags inFlags = TransitionCreateFlags::None,
        const gp::ConstVectorView<TransitionInfo> inTransitionInfos = {},
        const gp::ConstVectorView<TransientAliasingInfo> inAliasingInfos = {}
    ) noexcept
        : srcPipelines(inSrcPipelines)
        , dstPipelines(inDstPipelines)
        , flags(inFlags)
        , transitionInfos(inTransitionInfos)
        , aliasingInfos(inAliasingInfos)
    {}

    /// @brief Compares two transition creation descriptors element by element.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when pipeline masks, flags, and every transition and aliasing entry match.
    [[nodiscard]] constexpr bool operator==(const TransitionCreateInfo& rhs) const noexcept
    {
        if (srcPipelines != rhs.srcPipelines || dstPipelines != rhs.dstPipelines || flags != rhs.flags)
        {
            return false;
        }

        if (transitionInfos.size() != rhs.transitionInfos.size() || aliasingInfos.size() != rhs.aliasingInfos.size())
        {
            return false;
        }

        for (gp::USize i = 0u; i < transitionInfos.size(); ++i)
        {
            if (transitionInfos[i] != rhs.transitionInfos[i])
            {
                return false;
            }
        }

        for (gp::USize i = 0u; i < aliasingInfos.size(); ++i)
        {
            if (aliasingInfos[i] != rhs.aliasingInfos[i])
            {
                return false;
            }
        }

        return true;
    }
};

/// @brief Pairs a tracked resource access mask with the pipelines that last accessed the resource.
struct TrackedAccess
{
    /// @brief Last known GPU access state.
    ResourceAccess access{ ResourceAccess::Unknown };

    /// @brief Execution pipelines currently holding `access`.
    PipelineStage pipelines{ PipelineStage::None };

    /// @brief Constructs an unknown tracked access state.
    constexpr TrackedAccess() noexcept = default;

    /// @brief Constructs a tracked access state with an access mask and `PipelineStage::None`.
    /// @param[in] inAccess Initial access state.
    constexpr explicit(false) TrackedAccess(const ResourceAccess inAccess) noexcept
        : access(inAccess)
    {}

    /// @brief Constructs a tracked access state with explicit access and pipeline masks.
    /// @param[in] inAccess Initial access state.
    /// @param[in] inPipelines Active pipeline mask.
    constexpr TrackedAccess(const ResourceAccess inAccess, const PipelineStage inPipelines) noexcept
        : access(inAccess)
        , pipelines(inPipelines)
    {}

    /// @brief Compares two tracked access values for equality.
    /// @param[in] rhs Value to compare against.
    /// @return True when both access and pipelines match.
    [[nodiscard]] constexpr bool operator==(const TrackedAccess& rhs) const noexcept = default;
};

/// @brief Associates a `ViewableResource` with its tracked GPU access state and pipeline ownership.
struct TrackedAccessInfo : public TrackedAccess
{
    /// @brief Target viewable resource whose state is being reported or updated.
    resource::ViewableResource* resource{ nullptr };

    /// @brief Constructs an empty tracked access record.
    constexpr TrackedAccessInfo() noexcept = default;

    /// @brief Constructs a tracked access record for `inResource`.
    /// @param[in] inResource Target viewable resource pointer.
    /// @param[in] inAccess Resource access state.
    /// @param[in] inPipelines Pipeline stage mask.
    constexpr TrackedAccessInfo(
        resource::ViewableResource* const inResource, const ResourceAccess inAccess, const PipelineStage inPipelines
    ) noexcept
        : TrackedAccess(inAccess, inPipelines)
        , resource(inResource)
    {}

    /// @brief Compares two tracked access records for equality.
    /// @param[in] rhs Record to compare against.
    /// @return True when resource pointer, access state, and pipelines all match.
    [[nodiscard]] constexpr bool operator==(const TrackedAccessInfo& rhs) const noexcept = default;
};

/// @brief Resolves the underlying `resource::ViewableResource` referenced by `info`, unwrapping texture references and
///        unordered access views.
/// @param[in] info Transition descriptor to resolve.
/// @return Resolved `ViewableResource*`, or `nullptr` for null resources, acceleration structures, or unknown entries.
[[nodiscard]] GP_RHI_API resource::ViewableResource* getViewableResource(const TransitionInfo& info) noexcept;

}   // namespace gp::rhi::sync
