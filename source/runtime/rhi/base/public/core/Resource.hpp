// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/ContainerForward.hpp"
#include "containers/strings/Name.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceForward.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerEnums.hpp"   // IWYU pragma: keep
#include "memory/pointers/PointerTypes.hpp"
#include "RHIBase.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::resource
{

/// @brief Intrusively reference-counted base class for all GPU resources managed by the RHI.
/// @details Models Unreal Engine's `FRHIResource`. Reference counts and deferred-deletion state bits live in a single
///          32-bit `gp::AtomicRefStateCounter` (30-bit ref count plus `MarkedForDelete` and `Deleting` flags). When
///          the last CPU reference drops to zero, `release()` pushes the resource onto a lock-free intrusive deletion
///          queue instead of calling `delete` inline, because the GPU or RHI thread may still be executing commands
///          that reference the resource.
/// @note Subclasses must be destroyed through `ResourceDeletionQueue` (or friend RHI executors). Direct `delete`
///       triggers a `GP_CHECK` failure in checked builds unless exit bypass is enabled.
class GP_RHI_API Resource
{
    friend class ::gp::rhi::DynamicRHI;
    friend class ::gp::rhi::command::CommandListExecutor;
    friend class ::gp::rhi::command::CommandListImmediate;
    friend class ResourceDeletionQueue;
    friend struct ResourceInternalAccess;

public:
    /// @brief Constructs an RHI resource tagged with `resourceType` and an initial reference count of zero.
    /// @details Defaults `isCommitted()` and `allowsLifetimeExtension()` to `true`. Registers the instance with the
    ///          global tracking list when `GP_RHI_ENABLE_RESOURCE_INFO` is active and tracking is turned on.
    /// @param[in] resourceType Concrete runtime type tag for this resource.
    explicit Resource(const ResourceType resourceType) noexcept;

    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    Resource(Resource&&) = delete;
    Resource& operator=(Resource&&) = delete;

protected:
    /// @brief Protected virtual destructor that enforces teardown through the deferred deletion queue.
    /// @details Asserts that `getRefCount() == 0` and that this instance is the active target of `deleteResources()`
    ///          (unless engine exit bypass is enabled). Unlinks the resource from tracking when enabled.
    virtual ~Resource() noexcept;

public:
    /// @brief Increments the intrusive reference count.
    /// @note Triggers an assertion if the resource has entered the terminal `Deleting` state or overflows 30 bits.
    /// @return Updated reference count after incrementing.
    GP_FORCEINLINE_HINT gp::UInt32 addRef() const noexcept
    {
        const gp::UInt32 newCount = m_atomicFlags.addRef();
        GP_CHECK_SLOW(newCount > 0u);
        return newCount;
    }

    /// @brief Decrements the intrusive reference count and schedules deferred deletion when it hits zero.
    /// @details Uses `MemoryOrder::Release` on every decrement and issues an `Acquire` fence on the final `1 -> 0`
    ///          transition before calling `markForDelete()`.
    /// @note Triggers an assertion if the resource is already deleting or if the count underflows.
    /// @return Remaining reference count after decrementing.
    GP_FORCEINLINE_HINT gp::UInt32 release() const noexcept
    {
        GP_CHECKF(
            !m_atomicFlags.isDeleting(gp::memory::MemoryOrder::Relaxed),
            "Cannot release an RHI resource that is currently being deleted."
        );

        const gp::UInt32 newCount = m_atomicFlags.release();
        if (newCount == 0u)
        {
            markForDelete();
        }
        return newCount;
    }

    /// @brief Attempts to increment the reference count as long as final deletion has not started.
    /// @details Intended for RHI caches (such as PSO or sampler caches) that hold non-owning pointers and may safely
    ///          resurrect a resource waiting in the deferred deletion queue before `deleteResources()` runs.
    /// @return True if the reference count was incremented; false if `RefStateFlags::Deleting` is already set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool tryAddRefIfNotDeleting() const noexcept
    {
        return m_atomicFlags.tryAddRefIfNotDeleting();
    }

    /// @brief Attempts to increment the reference count only when the resource is live and unmarked.
    /// @details Fails if the count is zero or if either `MarkedForDelete` or `Deleting` is set.
    /// @return True if the reference count was incremented from a live state; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool tryAddRefIfLive() const noexcept
    {
        return m_atomicFlags.tryAddRefIfLive();
    }

    /// @brief Reads the current reference count with relaxed memory ordering.
    /// @return Active reference count in `[0, AtomicRefStateCounter::kMaxRefCount]`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getRefCount() const noexcept
    {
        return m_atomicFlags.getRefCount(gp::memory::MemoryOrder::Relaxed);
    }

    /// @brief Reports whether this resource holds live references and has not been marked for deletion.
    /// @return True when `getRefCount() > 0` and neither `MarkedForDelete` nor `Deleting` is set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        const gp::UInt32 packed = m_atomicFlags.getPackedValue(gp::memory::MemoryOrder::Relaxed);
        return ((packed & gp::AtomicRefStateCounter::kFlagsMask) == 0u) &&
               ((packed & gp::AtomicRefStateCounter::kRefCountMask) != 0u);
    }

    /// @brief Reports whether `RefStateFlags::MarkedForDelete` is currently set on this resource.
    /// @return True if the resource has been queued for deferred deletion; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isMarkedForDelete() const noexcept
    {
        return m_atomicFlags.isMarkedForDelete(gp::memory::MemoryOrder::Relaxed);
    }

    /// @brief Opts this resource out of render-thread lifetime extensions.
    /// @details Once disabled, releasing the last reference routes the resource to the immediate pending-delete queue
    ///          so it is reclaimed on the next deletion flush even while a lifetime extension scope is active.
    /// @note Must be called while holding a live reference (`isValid() == true`).
    GP_FORCEINLINE_HINT void disableLifetimeExtension() noexcept
    {
        GP_ENSURE_MSG(
            isValid(),
            "Resource is already marked for deletion. disableLifetimeExtension() must be called while holding a live " "reference."
        );
        m_allowExtendLifetime = 0u;
    }

    /// @brief Reports whether this resource obeys render-thread lifetime extension scopes.
    /// @return True if deferred deletion waits for active lifetime extension scopes to end; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool allowsLifetimeExtension() const noexcept
    {
        return m_allowExtendLifetime != 0u;
    }

    /// @brief Reports whether the resource's backing GPU memory is currently committed.
    /// @return True if committed; false for uncommitted reserved or tiled resources.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isCommitted() const noexcept
    {
        return m_isCommitted != 0u;
    }

    /// @brief Returns the runtime resource type tag assigned at construction.
    /// @return `ResourceType` enumerator identifying the concrete subclass.
    [[nodiscard]] GP_FORCEINLINE_HINT ResourceType getType() const noexcept
    {
        return m_resourceType;
    }

    /// @brief Returns the diagnostic owner name attached to this resource.
    /// @return Assigned `gp::Name`, or `"None"` when unset or when `GP_RHI_ENABLE_RESOURCE_INFO` is disabled.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Name getOwnerName() const noexcept
    {
#if GP_RHI_ENABLE_RESOURCE_INFO
        return m_ownerName;
#else
        return gp::Name{};
#endif
    }

    /// @brief Assigns a diagnostic owner name for memory profiling and leak reports.
    /// @param[in] ownerName Owner identifier to associate with this resource.
    GP_FORCEINLINE_HINT void setOwnerName(const gp::Name ownerName) noexcept
    {
#if GP_RHI_ENABLE_RESOURCE_INFO
        m_ownerName = ownerName;
#else
        static_cast<void>(ownerName);
#endif
    }

    /// @brief Returns the cumulative number of pipeline state resources destroyed by `deleteResources()`.
    /// @details Uses `MemoryOrder::Acquire` so callers observe shader reference releases completed by the deleted
    ///          pipeline state destructors.
    /// @return Monotonic count of deleted graphics, compute, ray tracing, and work graph pipeline states.
    [[nodiscard]] static gp::UInt64 getNumDeletedPipelineStates() noexcept;

#if GP_RHI_ENABLE_RESOURCE_INFO
    /// @brief Populates diagnostic metadata for this resource when supported by the concrete subclass.
    /// @details The base implementation resets `outResourceInfo` to defaults and returns `false`.
    /// @param[out] outResourceInfo Receives the resource's name, type, VRAM range, and residency flags.
    /// @return True if `outResourceInfo` was populated with backend data; false otherwise.
    virtual bool getResourceInfo(ResourceInfo& outResourceInfo) const noexcept;

    /// @brief Reports whether this instance is currently linked into the global tracked resource list.
    /// @return True if actively tracked; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isBeingTracked() const noexcept
    {
        return m_isBeingTracked.load(gp::memory::MemoryOrder::Acquire);
    }

    /// @brief Links `inResource` into the global tracked resource list if global tracking is enabled.
    /// @param[in,out] inResource Resource instance to begin tracking (ignored if null).
    static void beginTrackingResource(Resource* inResource) noexcept;

    /// @brief Unlinks `inResource` from the global tracked resource list if it is currently tracked.
    /// @param[in,out] inResource Resource instance to stop tracking (ignored if null).
    static void endTrackingResource(Resource* inResource) noexcept;

    /// @brief Enables global tracking for all subsequently created RHI resources.
    static void startTrackingAllResources() noexcept;

    /// @brief Disables global tracking and unlinks every currently tracked RHI resource.
    static void stopTrackingAllResources() noexcept;

    /// @brief Reports whether global RHI resource tracking is currently active.
    /// @return True if newly created resources are automatically tracked; false otherwise.
    [[nodiscard]] static bool isTrackingAllResources() noexcept;

    /// @brief Returns the number of RHI resources currently registered in the global tracking list.
    /// @return Count of live tracked resources.
    [[nodiscard]] static gp::USize getTrackedResourceCount() noexcept;

    /// @brief Appends pointers to all currently tracked RHI resources into `outResources`.
    /// @note Callers must coordinate with deferred deletion if they dereference pointers after this call returns.
    /// @param[in,out] outResources Destination vector receiving tracked resource pointers.
    static void gatherTrackedResources(gp::Vector<Resource*>& outResources) noexcept;
#endif

protected:
    /// @brief Updates the committed-memory flag for reserved or tiled GPU resources.
    /// @param[in] isCommitted True when backing physical memory is committed; false when uncommitted.
    GP_FORCEINLINE_HINT void setCommitted(const bool isCommitted) noexcept
    {
        m_isCommitted = isCommitted ? 1u : 0u;
    }

private:
    /// @brief Marks this resource for deferred deletion and enqueues it on the matching lock-free pending stack.
    /// @details Kept out-of-line so `release()` stays compact at every smart-pointer destruction site.
    void markForDelete() const noexcept;

    /// @brief Drains pending deletion stacks into `outResources` in LIFO order.
    /// @param[in,out] outResources Destination vector receiving resources queued for deletion.
    /// @param[in] includeExtendedLifetimeResources When true, also drains the lifetime-extended pending queue.
    static void
        gatherResourcesToDelete(gp::Vector<Resource*>& outResources, bool includeExtendedLifetimeResources) noexcept;

    /// @brief Destroys each resource in `resources` whose reference count is still zero.
    /// @details If a cache resurrected a resource (`getRefCount() > 0`) while it waited in the queue, clears
    ///          `RefStateFlags::MarkedForDelete` and leaves the resource alive.
    /// @param[in] resources Batch of candidate resources previously gathered from the pending stacks.
    static void deleteResources(gp::ConstVectorView<Resource*> resources) noexcept;

    /// @brief Controls whether `~Resource()` skips the deletion-queue ownership check during engine shutdown.
    /// @param[in] bypass True during final process teardown; false during normal runtime.
    static void setBypassDeletionQueueCheckOnExit(bool bypass) noexcept;

    /// @brief Reports whether the deletion-queue destructor guard is currently bypassed for shutdown.
    /// @return True if exit bypass is enabled; false otherwise.
    [[nodiscard]] static bool isBypassingDeletionQueueCheckOnExit() noexcept;

private:
    /// @brief Intrusive link used by the lock-free pending-delete stack while `MarkedForDelete` is set.
    mutable Resource* m_nextPendingDelete{ nullptr };

    /// @brief Packed 30-bit reference count plus `MarkedForDelete` (bit 30) and `Deleting` (bit 31) flags.
    mutable gp::AtomicRefStateCounter m_atomicFlags{};

    /// @brief Immutable runtime type tag identifying the concrete resource subclass.
    const ResourceType m_resourceType{ ResourceType::None };

    /// @brief True when backing GPU memory is committed.
    gp::UInt8 m_isCommitted : 1 { 1u };

    /// @brief True when deferred deletion respects render-thread lifetime extension scopes.
    gp::UInt8 m_allowExtendLifetime : 1 { 1u };

#if GP_RHI_ENABLE_RESOURCE_INFO
    /// @brief Lock-free flag indicating whether this resource is linked into the global tracking list.
    gp::thread::Atomic<bool> m_isBeingTracked{ false };

    /// @brief Intrusive previous pointer in the global tracking doubly-linked list.
    Resource* m_trackingPrev{ nullptr };

    /// @brief Intrusive next pointer in the global tracking doubly-linked list.
    Resource* m_trackingNext{ nullptr };

    /// @brief Diagnostic owner identifier assigned by the creating subsystem or asset.
    gp::Name m_ownerName{};
#endif
};

}   // namespace gp::rhi::resource
