// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/Resource.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceTypes.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "profiling/Profiler.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/roles/ThreadRole.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::resource
{

namespace
{

/// @brief Lock-free, zero-allocation intrusive MPSC/MPMC stack drained in one atomic exchange.
/// @details Pushing only updates `m_head` with a CAS loop and consuming steals the entire chain via a single
///          wait-free `exchange(nullptr, AcqRel)`. Neither operation suffers from Treiber-stack pop ABA races, and
///          each `Resource` enters at most once while `MarkedForDelete` is set.
class alignas(gp::memory::HardwareConstants::cacheLineSize) PendingDeleteStack final
{
public:
    constexpr PendingDeleteStack() noexcept = default;
    ~PendingDeleteStack() noexcept = default;

    PendingDeleteStack(const PendingDeleteStack&) = delete;
    PendingDeleteStack& operator=(const PendingDeleteStack&) = delete;
    PendingDeleteStack(PendingDeleteStack&&) = delete;
    PendingDeleteStack& operator=(PendingDeleteStack&&) = delete;

    /// @brief Pushes `resource` onto the head of the pending-delete chain.
    /// @param[in,out] resource Resource that has newly latched `MarkedForDelete`.
    void push(Resource* const resource) noexcept
    {
        GP_CHECK(resource != nullptr);

        Resource* oldHead = m_head.load(gp::memory::MemoryOrder::Relaxed);
        for (;;)
        {
            setNextPendingDelete(resource, oldHead);
            if (m_head.compareExchangeWeak(
                    oldHead, resource, gp::memory::MemoryOrder::Release, gp::memory::MemoryOrder::Relaxed
                )) [[likely]]
            {
                return;
            }
        }
    }

    /// @brief Atomically steals the entire chain and appends its items to `outResources` in LIFO order.
    /// @param[in,out] outResources Destination vector receiving the detached resource pointers.
    void consumeAllLifo(gp::Vector<Resource*>& outResources) noexcept
    {
        Resource* current = m_head.exchange(nullptr, gp::memory::MemoryOrder::AcqRel);
        while (current != nullptr)
        {
            Resource* const next = detachNextPendingDelete(current);
            outResources.emplaceBack(current);
            current = next;
        }
    }

private:
    static void setNextPendingDelete(Resource* resource, Resource* next) noexcept;
    static Resource* detachNextPendingDelete(Resource* resource) noexcept;

private:
    gp::thread::Atomic<Resource*> m_head{ nullptr };
};

PendingDeleteStack g_pendingDeletes{};
PendingDeleteStack g_pendingDeletesWithLifetimeExtension{};

gp::thread::Atomic<gp::UInt64> g_numDeletedPipelineStates{ 0u };
gp::thread::Atomic<gp::Int32> g_resourceLifetimeRefCount{ 0 };
gp::thread::Atomic<bool> g_bypassDeletionQueueCheckOnExit{ false };

#if GP_DO_CHECK
/// @brief Tracks the resource currently being destroyed by `Resource::deleteResources()` on this thread.
thread_local const Resource* t_currentlyDeleting = nullptr;
#endif

#if GP_RHI_ENABLE_RESOURCE_INFO
gp::thread::CacheAlignedMutex g_trackingMutex{};
gp::thread::Atomic<bool> g_isTrackingResources{ false };
Resource* g_trackedListHead = nullptr;
gp::USize g_trackedResourceCount = 0u;
#endif

}   // namespace

/// @brief Friend helper granting file-local queue and tracking code access to `Resource` intrusive link fields.
struct ResourceInternalAccess final
{
    static void setNextPendingDelete(Resource* const resource, Resource* const next) noexcept
    {
        resource->m_nextPendingDelete = next;
    }

    static Resource* detachNextPendingDelete(Resource* const resource) noexcept
    {
        Resource* const next = resource->m_nextPendingDelete;
        resource->m_nextPendingDelete = nullptr;
        return next;
    }

#if GP_RHI_ENABLE_RESOURCE_INFO
    static void linkTrackedResourceLocked(Resource* const resource) noexcept
    {
        resource->m_trackingPrev = nullptr;
        resource->m_trackingNext = g_trackedListHead;
        if (g_trackedListHead != nullptr)
        {
            g_trackedListHead->m_trackingPrev = resource;
        }
        g_trackedListHead = resource;
        ++g_trackedResourceCount;
    }

    static void unlinkTrackedResourceLocked(Resource* const resource) noexcept
    {
        Resource* const prev = resource->m_trackingPrev;
        Resource* const next = resource->m_trackingNext;

        if (prev != nullptr)
        {
            prev->m_trackingNext = next;
        }
        else if (g_trackedListHead == resource)
        {
            g_trackedListHead = next;
        }

        if (next != nullptr)
        {
            next->m_trackingPrev = prev;
        }

        resource->m_trackingPrev = nullptr;
        resource->m_trackingNext = nullptr;

        GP_CHECK(g_trackedResourceCount > 0u);
        --g_trackedResourceCount;
    }

    static Resource* getTrackingNext(Resource* const resource) noexcept
    {
        return resource->m_trackingNext;
    }

    static void clearTrackingLinksLocked(Resource* const resource) noexcept
    {
        resource->m_trackingPrev = nullptr;
        resource->m_trackingNext = nullptr;
        resource->m_isBeingTracked.store(false, gp::memory::MemoryOrder::Release);
    }

    static bool isMarkedTrackedRelaxed(const Resource* const resource) noexcept
    {
        return resource->m_isBeingTracked.load(gp::memory::MemoryOrder::Relaxed);
    }

    static void setMarkedTrackedRelease(Resource* const resource, const bool tracked) noexcept
    {
        resource->m_isBeingTracked.store(tracked, gp::memory::MemoryOrder::Release);
    }
#endif
};

namespace
{

void PendingDeleteStack::setNextPendingDelete(Resource* const resource, Resource* const next) noexcept
{
    ResourceInternalAccess::setNextPendingDelete(resource, next);
}

Resource* PendingDeleteStack::detachNextPendingDelete(Resource* const resource) noexcept
{
    return ResourceInternalAccess::detachNextPendingDelete(resource);
}

}   // namespace

Resource::Resource(const ResourceType resourceType) noexcept
    : m_resourceType(resourceType)
    , m_isCommitted(1u)
    , m_allowExtendLifetime(1u)
{
#if GP_RHI_ENABLE_RESOURCE_INFO
    beginTrackingResource(this);
#endif
}

Resource::~Resource() noexcept
{
#if GP_DO_CHECK
    GP_CHECKF(
        g_bypassDeletionQueueCheckOnExit.load(gp::memory::MemoryOrder::Acquire) || (t_currentlyDeleting == this),
        "RHI Resource must be destroyed through ResourceDeletionQueue::deleteResources()."
    );
    GP_CHECKF(
        m_atomicFlags.getRefCount(gp::memory::MemoryOrder::Relaxed) == 0u,
        "RHI Resource destroyed while outstanding references still exist."
    );
    t_currentlyDeleting = nullptr;
#endif

#if GP_RHI_ENABLE_RESOURCE_INFO
    endTrackingResource(this);
#endif
}

void Resource::markForDelete() const noexcept
{
    if (m_atomicFlags.tryMarkForDelete())
    {
        Resource* const mutableSelf = const_cast<Resource*>(this);
        if (m_allowExtendLifetime != 0u)
        {
            g_pendingDeletesWithLifetimeExtension.push(mutableSelf);
        }
        else
        {
            g_pendingDeletes.push(mutableSelf);
        }
    }
}

void Resource::gatherResourcesToDelete(
    gp::Vector<Resource*>& outResources, const bool includeExtendedLifetimeResources
) noexcept
{
    GP_PROFILE_SCOPE_N("RHI::GatherResourcesToDelete");

    if (includeExtendedLifetimeResources)
    {
        g_pendingDeletesWithLifetimeExtension.consumeAllLifo(outResources);
    }

    g_pendingDeletes.consumeAllLifo(outResources);
}

void Resource::deleteResources(const gp::ConstVectorView<Resource*> resources) noexcept
{
    GP_PROFILE_SCOPE_N("RHI::DeleteResources");

    gp::UInt64 deletedPipelineStateCount = 0u;
    for (Resource* const resource: resources)
    {
        if (resource == nullptr)
        {
            continue;
        }

        if (resource->m_atomicFlags.tryBeginDeleting())
        {
            if (isPipelineStateResourceType(resource->getType()))
            {
                ++deletedPipelineStateCount;
            }

#if GP_DO_CHECK
            t_currentlyDeleting = resource;
#endif
            delete resource;

#if GP_DO_CHECK
            GP_CHECKF(
                t_currentlyDeleting == nullptr,
                "Derived RHI resource destructor failed to execute Resource::~Resource()."
            );
#endif
        }
    }

    // Publish the pipeline state deletion count after destructors release shader references.
    if (deletedPipelineStateCount > 0u)
    {
        g_numDeletedPipelineStates.fetchAdd(deletedPipelineStateCount, gp::memory::MemoryOrder::Release);
    }
}

gp::UInt64 Resource::getNumDeletedPipelineStates() noexcept
{
    return g_numDeletedPipelineStates.load(gp::memory::MemoryOrder::Acquire);
}

void Resource::setBypassDeletionQueueCheckOnExit(const bool bypass) noexcept
{
    g_bypassDeletionQueueCheckOnExit.store(bypass, gp::memory::MemoryOrder::Release);
}

bool Resource::isBypassingDeletionQueueCheckOnExit() noexcept
{
    return g_bypassDeletionQueueCheckOnExit.load(gp::memory::MemoryOrder::Acquire);
}

#if GP_RHI_ENABLE_RESOURCE_INFO

bool Resource::getResourceInfo(ResourceInfo& outResourceInfo) const noexcept
{
    outResourceInfo = ResourceInfo{};
    return false;
}

void Resource::beginTrackingResource(Resource* const inResource) noexcept
{
    if (inResource == nullptr)
    {
        return;
    }

    if (!g_isTrackingResources.load(gp::memory::MemoryOrder::Acquire))
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::CacheAlignedMutex> lock(g_trackingMutex);
    if (g_isTrackingResources.load(gp::memory::MemoryOrder::Relaxed) &&
        !ResourceInternalAccess::isMarkedTrackedRelaxed(inResource))
    {
        ResourceInternalAccess::linkTrackedResourceLocked(inResource);
        ResourceInternalAccess::setMarkedTrackedRelease(inResource, true);
    }
}

void Resource::endTrackingResource(Resource* const inResource) noexcept
{
    if (inResource == nullptr)
    {
        return;
    }

    if (!inResource->isBeingTracked())
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::CacheAlignedMutex> lock(g_trackingMutex);
    if (ResourceInternalAccess::isMarkedTrackedRelaxed(inResource))
    {
        ResourceInternalAccess::unlinkTrackedResourceLocked(inResource);
        ResourceInternalAccess::setMarkedTrackedRelease(inResource, false);
    }
}

void Resource::startTrackingAllResources() noexcept
{
    gp::thread::UniqueLock<gp::thread::CacheAlignedMutex> lock(g_trackingMutex);
    g_isTrackingResources.store(true, gp::memory::MemoryOrder::Release);
}

void Resource::stopTrackingAllResources() noexcept
{
    gp::thread::UniqueLock<gp::thread::CacheAlignedMutex> lock(g_trackingMutex);
    g_isTrackingResources.store(false, gp::memory::MemoryOrder::Release);

    Resource* current = g_trackedListHead;
    while (current != nullptr)
    {
        Resource* const next = ResourceInternalAccess::getTrackingNext(current);
        ResourceInternalAccess::clearTrackingLinksLocked(current);
        current = next;
    }

    g_trackedListHead = nullptr;
    g_trackedResourceCount = 0u;
}

bool Resource::isTrackingAllResources() noexcept
{
    return g_isTrackingResources.load(gp::memory::MemoryOrder::Acquire);
}

gp::USize Resource::getTrackedResourceCount() noexcept
{
    gp::thread::UniqueLock<gp::thread::CacheAlignedMutex> lock(g_trackingMutex);
    return g_trackedResourceCount;
}

void Resource::gatherTrackedResources(gp::Vector<Resource*>& outResources) noexcept
{
    gp::thread::UniqueLock<gp::thread::CacheAlignedMutex> lock(g_trackingMutex);
    outResources.reserve(outResources.size() + g_trackedResourceCount);

    Resource* current = g_trackedListHead;
    while (current != nullptr)
    {
        outResources.emplaceBack(current);
        current = ResourceInternalAccess::getTrackingNext(current);
    }
}

#endif   // GP_RHI_ENABLE_RESOURCE_INFO

void ResourceDeletionQueue::gatherResourcesToDelete(
    gp::Vector<Resource*>& outResources, const bool includeExtendedLifetimeResources
) noexcept
{
    Resource::gatherResourcesToDelete(outResources, includeExtendedLifetimeResources);
}

void ResourceDeletionQueue::deleteResources(const gp::ConstVectorView<Resource*> resources) noexcept
{
    Resource::deleteResources(resources);
}

void ResourceDeletionQueue::flushPendingDeletes(const bool includeExtendedLifetimeResources) noexcept
{
    gp::Vector<Resource*> pendingBatch{};
    for (;;)
    {
        pendingBatch.clear();
        Resource::gatherResourcesToDelete(pendingBatch, includeExtendedLifetimeResources);
        if (pendingBatch.isEmpty())
        {
            break;
        }
        Resource::deleteResources(pendingBatch);
    }
}

void ResourceDeletionQueue::addLifetimeRef(const gp::Int32 numRefs) noexcept
{
    GP_CHECKF(gp::thread::isInRenderThread(), "ResourceDeletionQueue::addLifetimeRef() must run on the render thread.");
    GP_CHECKF(numRefs > 0, "ResourceDeletionQueue::addLifetimeRef() requires a positive reference delta.");

    g_resourceLifetimeRefCount.fetchAdd(numRefs, gp::memory::MemoryOrder::AcqRel);
}

bool ResourceDeletionQueue::releaseLifetimeRef(const gp::Int32 numRefs) noexcept
{
    GP_CHECKF(
        gp::thread::isInRenderThread(), "ResourceDeletionQueue::releaseLifetimeRef() must run on the render thread."
    );
    GP_CHECKF(numRefs > 0, "ResourceDeletionQueue::releaseLifetimeRef() requires a positive reference delta.");

    const gp::Int32 prevCount = g_resourceLifetimeRefCount.fetchSub(numRefs, gp::memory::MemoryOrder::AcqRel);
    const gp::Int32 newCount = prevCount - numRefs;
    GP_CHECKF(newCount >= 0, "ResourceDeletionQueue lifetime reference count underflowed.");

    return newCount == 0;
}

gp::Int32 ResourceDeletionQueue::getLifetimeRefCount() noexcept
{
    return g_resourceLifetimeRefCount.load(gp::memory::MemoryOrder::Acquire);
}

bool ResourceDeletionQueue::shouldIncludeExtendedLifetimeResources() noexcept
{
    return getLifetimeRefCount() == 0;
}

void ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(const bool bypass) noexcept
{
    Resource::setBypassDeletionQueueCheckOnExit(bypass);
}

bool ResourceDeletionQueue::isBypassingDeletionQueueCheckOnExit() noexcept
{
    return Resource::isBypassingDeletionQueueCheckOnExit();
}

ScopedResourceLifetimeExtension::ScopedResourceLifetimeExtension() noexcept
{
    ResourceDeletionQueue::addLifetimeRef(1);
}

ScopedResourceLifetimeExtension::~ScopedResourceLifetimeExtension() noexcept
{
    static_cast<void>(ResourceDeletionQueue::releaseLifetimeRef(1));
}

}   // namespace gp::rhi::resource
