// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "RHIBase.hpp"
#include "sync/SyncEnums.hpp"
#include "validation/ValidationEnums.hpp"
#include "validation/ValidationForward.hpp"   // IWYU pragma: keep
#include "validation/ValidationTypes.hpp"

namespace gp::rhi::validation
{

/// @brief State machine tracking barrier transitions and UAV overlap for a single subresource (or whole resource).
struct GP_RHI_API SubresourceState
{
    /// @brief Per-pipeline barrier and UAV-overlap state.
    struct PipelineState
    {
        State previous{};
        State current{};
        sync::ResourceTransitionFlags flags{ sync::ResourceTransitionFlags::None };
        bool isTransitioning{ false };
        bool allUAVsOverlap{ false };
        bool explicitUAVOverlap{ false };
        bool usedWithAllUAVsOverlap{ false };
        bool usedWithExplicitUAVOverlap{ false };
    };

    sync::PipelineArray<PipelineState> pipelines{};

    /// @brief Initializes every pipeline slot to `(initialAccess, Graphics)`.
    /// @param[in] initialAccess Creation-time `sync::ResourceAccess` state.
    explicit SubresourceState(sync::ResourceAccess initialAccess = sync::ResourceAccess::Unknown) noexcept;

    void beginTransition(
        Resource& resource,
        SubresourceIndex subresourceIndex,
        const State& stateBefore,
        const State& stateAfter,
        sync::ResourceTransitionFlags newFlags,
        sync::PipelineStage executePipeline
    ) noexcept;

    void endTransition(
        Resource& resource,
        SubresourceIndex subresourceIndex,
        const State& stateBefore,
        const State& stateAfter,
        sync::PipelineStage executePipeline
    ) noexcept;

    void assertAccess(
        Resource& resource, SubresourceIndex subresourceIndex, const State& requiredState, bool allowAllUAVsOverlap
    ) noexcept;

    void assertTracked(
        Resource& resource,
        SubresourceIndex subresourceIndex,
        sync::ResourceAccess expectedAccess,
        sync::PipelineStage executePipeline
    ) const noexcept;

    void setTracked(
        Resource& resource,
        SubresourceIndex subresourceIndex,
        sync::ResourceAccess trackedAccess,
        sync::PipelineStage trackedPipelines
    ) noexcept;

    void beginUAVOverlap(Resource& resource, SubresourceIndex subresourceIndex, sync::PipelineStage pipeline) noexcept;
    void endUAVOverlap(Resource& resource, SubresourceIndex subresourceIndex, sync::PipelineStage pipeline) noexcept;
};

/// @brief Tracks transient heap acquire, discard, and aliasing-overlap ordering for a resource.
struct GP_RHI_API TransientState
{
    bool isTransient{ false };
    TransientStatus status{ TransientStatus::NonTransient };

    constexpr TransientState() noexcept = default;

    void setTransient(const bool transient) noexcept
    {
        isTransient = transient;
        status = transient ? TransientStatus::Released : TransientStatus::NonTransient;
    }

    [[nodiscard]] bool isAcquired() const noexcept
    {
        return !isTransient || (status == TransientStatus::Acquired);
    }

    void acquire(Resource& resource, sync::PipelineStage pipeline) noexcept;
    void discardBegin(Resource& resource, sync::PipelineStage pipeline) noexcept;
    void discardEnd(Resource& resource, sync::PipelineStage pipeline) noexcept;
    void aliasingOverlap(const Resource& overlappedResource, const Resource& acquiringResource) const noexcept;
};

/// @brief Tracks subresource states, debug naming, and transient lifecycle for a viewable RHI resource.
/// @details Uses lazy subresource array allocation: whole-resource barriers and views operate directly on inline
///          `m_wholeResourceState` with zero heap allocations until a partial subresource range is accessed!
class GP_RHI_API Resource
{
    friend class Tracker;
    friend struct SubresourceState;
    friend struct TransientState;

public:
    Resource() noexcept = default;
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    Resource(Resource&&) = delete;
    Resource& operator=(Resource&&) = delete;
    virtual ~Resource() noexcept = default;

public:
    /// @brief Initializes barrier tracking dimensions, initial GPU state, transient flag, and debug name.
    /// @param[in] numMips Total mip level count (`>= 1`).
    /// @param[in] numArraySlices Total array slice count (`>= 1`, multiplied by 6 for cubemaps).
    /// @param[in] numPlanes Number of independently tracked planes (`1`..`ResourcePlane::Max`).
    /// @param[in] initialAccess Creation-time `sync::ResourceAccess` state.
    /// @param[in] debugName Diagnostic resource label.
    /// @param[in] isTransient True if allocated from a transient resource heap.
    void initBarrierTracking(
        gp::UInt32 numMips,
        gp::UInt32 numArraySlices,
        gp::UInt32 numPlanes,
        sync::ResourceAccess initialAccess,
        gp::StringView debugName,
        bool isTransient = false
    ) noexcept;

    /// @brief Marks or unmarks this resource as transient for acquire/discard/overlap validation.
    /// @param[in] isTransient True to treat this resource as transient (`TransientStatus::Released` initially).
    void setTransient(const bool isTransient) noexcept
    {
        m_transientState.setTransient(isTransient);
    }

    /// @brief Reports whether `initBarrierTracking()` has been called on this resource.
    /// @return True when initialized.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isBarrierTrackingInitialized() const noexcept
    {
        return (m_numMips > 0u) && (m_numArraySlices > 0u) && (m_numPlanes > 0u);
    }

    /// @brief Returns a `ResourceIdentity` covering every mip, slice, and plane of this resource.
    /// @return Whole-resource `ResourceIdentity`.
    [[nodiscard]] ResourceIdentity getWholeResourceIdentity() noexcept;

    /// @brief Returns a `ResourceIdentity` covering a single plane across all mips and slices.
    /// @param[in] plane Target `ResourcePlane`.
    /// @return Plane-scoped `ResourceIdentity`.
    [[nodiscard]] ResourceIdentity getPlaneIdentity(ResourcePlane plane) noexcept;

    /// @brief Updates the debug name reported in validation error messages.
    /// @param[in] debugName New debug name.
    void setDebugName(gp::StringView debugName) noexcept;

    /// @brief Returns the debug name stored on this validation resource (`"<unnamed>"` when empty).
    /// @return Null-terminated C string.
    [[nodiscard]] const char* getDebugName() const noexcept
    {
        return m_debugName.isEmpty() ? "<unnamed>" : m_debugName.cString();
    }

    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt16 getNumMips() const noexcept
    {
        return m_numMips;
    }

    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt16 getNumArraySlices() const noexcept
    {
        return m_numArraySlices;
    }

    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt8 getNumPlanes() const noexcept
    {
        return m_numPlanes;
    }

    [[nodiscard]] GP_FORCEINLINE_HINT const TransientState& getTransientState() const noexcept
    {
        return m_transientState;
    }

    [[nodiscard]] GP_FORCEINLINE_HINT LoggingMode getLoggingMode() const noexcept
    {
        return m_loggingMode;
    }

    GP_FORCEINLINE_HINT void setLoggingMode(const LoggingMode mode) noexcept
    {
        m_loggingMode = mode;
    }

    [[nodiscard]] bool isWholeResourceRange(const SubresourceRange& range) const noexcept
    {
        return (range.mipIndex == 0u) && (range.numMips == m_numMips) && (range.arraySlice == 0u) &&
               (range.numArraySlices == m_numArraySlices) && (range.planeIndex == 0u) &&
               (range.numPlanes == m_numPlanes);
    }

    /// @brief Adopts barrier state from `other` during backing resource replacement.
    /// @param[in,out] other Source validation resource.
    void takeOwnership(Resource& other) noexcept;

    /// @brief Resets barrier tracking dimensions and state when backing GPU memory is released.
    void releaseOwnership() noexcept;

private:
    template <typename InFunc>
    void enumerateSubresources(
        const SubresourceRange& range, InFunc&& callback, bool isBeginTransition = false
    ) noexcept;

private:
    gp::String m_debugName{};
    SubresourceState m_wholeResourceState{};
    gp::Vector<SubresourceState> m_subresourceStates{};
    TransientState m_transientState{};
    LoggingMode m_loggingMode{ LoggingMode::None };
    gp::UInt16 m_numMips{ 0u };
    gp::UInt16 m_numArraySlices{ 0u };
    gp::UInt8 m_numPlanes{ 0u };
};

/// @brief Barrier tracker specialization embedded inside `resource::Buffer`.
class GP_RHI_API BufferResource final : public Resource
{
public:
    BufferResource() noexcept = default;
};

/// @brief Barrier tracker specialization embedded inside `resource::Texture`.
class GP_RHI_API TextureResource final : public Resource
{
public:
    TextureResource() noexcept = default;
};

/// @brief Lifetime and single-frame/single-draw update validator embedded inside `resource::UniformBuffer`.
class GP_RHI_API UniformBufferResource final
{
public:
    gp::UInt64 allocatedFrameID{ 0u };
    resource::UniformBufferUsage usage{ resource::UniformBufferUsage::MultiFrame };
    bool initialized{ false };

    void initLifetimeTracking(gp::UInt64 frameID, resource::UniformBufferUsage inUsage) noexcept;
    void updateAllocation(gp::UInt64 frameID) noexcept;
    void validateLifeTime(gp::UInt64 currentFrameID, gp::StringView layoutName) const noexcept;
};

/// @brief Tagged validation operation recorded into an `OperationsList` and replayed in queue submission order.
struct Operation
{
    OpType type{ OpType::Assert };
    ResourceIdentity identity{};
    Resource* secondResource{ nullptr };
    State stateBefore{};
    State stateAfter{};
    Fence* fence{ nullptr };
    sync::PipelineStage pipeline{ sync::PipelineStage::None };
    sync::ResourceTransitionFlags transitionFlags{ sync::ResourceTransitionFlags::None };
    bool enableFlag{ false };
    gp::String renameText{};
};

/// @brief Ordered list of `Operation` records queued on a `Tracker` or `TransitionResource`.
class GP_RHI_API OperationsList final
{
public:
    OperationsList() noexcept = default;

    void add(Operation&& op) noexcept
    {
        m_ops.emplaceBack(static_cast<Operation&&>(op));
    }

    void append(const OperationsList& other) noexcept;
    void appendMove(OperationsList&& other) noexcept;

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_ops.isEmpty();
    }

    [[nodiscard]] gp::USize size() const noexcept
    {
        return m_ops.size();
    }

    [[nodiscard]] const gp::Vector<Operation>& getOperations() const noexcept
    {
        return m_ops;
    }

    void reset() noexcept
    {
        m_ops.clear();
    }

private:
    gp::Vector<Operation> m_ops{};
};

/// @brief Validation operations attached to a `sync::Transition` for replay on `beginTransitions` and `endTransitions`.
struct GP_RHI_API TransitionResource
{
    sync::PipelineArray<OperationsList> pendingSignals{};
    sync::PipelineArray<OperationsList> pendingWaits{};
    OperationsList pendingAliases{};
    OperationsList pendingAliasingOverlaps{};
    OperationsList pendingOperationsBegin{};
    OperationsList pendingOperationsEnd{};
};

/// @brief Per-context/per-pipeline validation state tracker auditing transitions, bindings, and UAV overlaps.
class GP_RHI_API Tracker final
{
public:
    static constexpr gp::UInt32 kMaxSimultaneousUAVs = 32u;

    explicit Tracker(sync::PipelineStage pipeline) noexcept;

    [[nodiscard]] GP_FORCEINLINE_HINT sync::PipelineStage getPipeline() const noexcept
    {
        return m_pipeline;
    }

    void rename(Resource* resource, gp::StringView newName) noexcept;
    void addOps(const OperationsList& list) noexcept;

    void assertAccess(const ResourceIdentity& identity, sync::ResourceAccess requiredAccess) noexcept;
    void assertUAV(view::UnorderedAccessView* uav, UAVMode mode, gp::UInt32 slot) noexcept;
    void assertTracked(Resource* resource, sync::ResourceAccess expectedAccess) noexcept;
    void setTrackedAccess(
        const ResourceIdentity& identity, sync::ResourceAccess access, sync::PipelineStage pipelines
    ) noexcept;

    void clearUAVState(UAVMode mode) noexcept;
    void dispatchOrDrawBarrier(UAVMode mode) noexcept;

    void allUAVsOverlap(bool allow) noexcept;
    void specificUAVOverlap(const ResourceIdentity& identity, bool allow) noexcept;

    /// @brief Extracts all recorded operations from this context tracker so `ValidationRHI::submitCommandLists` can
    ///        replay them in exact hardware submission order across Graphics and AsyncCompute queues.
    /// @return Move-transferred `OperationsList`.
    [[nodiscard]] OperationsList finalizeOps() noexcept;

    /// @brief Executes a single `Operation` immediately against live `Resource` state machines.
    /// @param[in] op Operation to execute.
    /// @param[in] executePipeline Pipeline executing `op`.
    /// @param[in,out] inOutAllowAllUAVsOverlap Tracks global UAV overlap state on `executePipeline`.
    /// @return True if execution completed; false if blocked waiting on an unsignaled cross-pipeline `Fence`.
    [[nodiscard]] static bool
        executeOp(const Operation& op, sync::PipelineStage executePipeline, bool& inOutAllowAllUAVsOverlap) noexcept;

    /// @brief Replays queued operation lists across pipelines, respecting cross-pipeline `Fence` waits.
    /// @param[in] pipeline Target pipeline receiving `ops`.
    /// @param[in,out] ops Operations submitted on `pipeline`.
    static void replayOpQueue(sync::PipelineStage pipeline, OperationsList&& ops) noexcept;

    /// @brief Resets static cross-pipeline replay queues (called on `ValidationRHI::shutdown()` and test resets).
    static void resetAllPipelines() noexcept;

private:
    struct BoundUAVSlot
    {
        ViewIdentity identity{};
        bool bound{ false };
    };

    sync::PipelineStage m_pipeline{ sync::PipelineStage::Graphics };
    OperationsList m_currentList{};
    BoundUAVSlot m_boundUAVs[static_cast<gp::UInt32>(UAVMode::Count)][kMaxSimultaneousUAVs]{};
};

}   // namespace gp::rhi::validation
