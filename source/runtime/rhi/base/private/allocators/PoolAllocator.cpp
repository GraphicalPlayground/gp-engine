// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/PoolAllocator.hpp"
#include "allocators/MemoryPool.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "maths/base/Scalar.hpp"
#include "platforms/base/system/Thread.hpp"
#include "profiling/Profiler.hpp"
#include "templates/NumericLimits.hpp"
#include "threading/jobs/JobSystem.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::allocator
{

namespace
{

gp::Float32 g_poolAllocatorDefragSizeFraction = 0.9f;
gp::console::AutoConsoleVariableRef g_cvarPoolAllocatorDefragSizeFraction(
    "rhi.PoolAllocator.DefragSizeFraction",
    g_poolAllocatorDefragSizeFraction,
    "Skip defrag of pool if usage is more than given fraction (default: 0.9f)."
);

gp::Int32 g_poolAllocatorDefragMaxPoolsToClear = 1;
gp::console::AutoConsoleVariableRef g_cvarPoolAllocatorDefragMaxPoolsToClear(
    "rhi.PoolAllocator.DefragMaxPoolsToClear",
    g_poolAllocatorDefragMaxPoolsToClear,
    "Maximum number of pools to try and clear during a single defrag call (default: 1; negative disables time slicing)."
);

gp::Int32 g_poolAllocatorValidateLinkedList = 0;
gp::console::AutoConsoleVariableRef g_cvarPoolAllocatorValidateLinkedList(
    "rhi.PoolAllocator.ValidateLinkedList",
    g_poolAllocatorValidateLinkedList,
    "Validate internal linked list invariants of all RHI pool allocators during every operation."
);

struct AsyncPoolAllocationPayload
{
    PoolAllocator* allocator{ nullptr };
    gp::UInt32 sizeInBytes{ 0u };
    gp::UInt32 allocationAlignment{ 0u };
    PoolResourceTypes resourceType{ PoolResourceTypes::None };
    gp::thread::Atomic<gp::UInt32>* inFlightCounter{ nullptr };
    void (*entryFn)(PoolAllocator*, gp::UInt32, gp::UInt32, PoolResourceTypes) noexcept { nullptr };
};

void executeAsyncPoolAllocationJob(void* const userData) noexcept
{
    AsyncPoolAllocationPayload* const payload = static_cast<AsyncPoolAllocationPayload*>(userData);
    payload->entryFn(payload->allocator, payload->sizeInBytes, payload->allocationAlignment, payload->resourceType);
    payload->inFlightCounter->fetchSub(1u, gp::MemoryOrder::Release);
    delete payload;
}

gp::UInt32 executeAsyncPoolAllocationThread(void* const userData) noexcept
{
    executeAsyncPoolAllocationJob(userData);
    return 0u;
}

}   // namespace

gp::Float32 PoolAllocator::getDefragSizeFraction() noexcept
{
    return g_poolAllocatorDefragSizeFraction;
}

void PoolAllocator::setDefragSizeFraction(const gp::Float32 fraction) noexcept
{
    g_poolAllocatorDefragSizeFraction = fraction;
}

gp::Int32 PoolAllocator::getDefragMaxPoolsToClear() noexcept
{
    return g_poolAllocatorDefragMaxPoolsToClear;
}

void PoolAllocator::setDefragMaxPoolsToClear(const gp::Int32 maxPoolsToClear) noexcept
{
    g_poolAllocatorDefragMaxPoolsToClear = maxPoolsToClear;
}

bool PoolAllocator::isLinkedListValidationEnabled() noexcept
{
    return g_poolAllocatorValidateLinkedList > 0;
}

void PoolAllocator::setLinkedListValidationEnabled(const bool enabled) noexcept
{
    g_poolAllocatorValidateLinkedList = enabled ? 1 : 0;
}

PoolAllocator::PoolAllocator(
    const gp::UInt64 defaultPoolSize,
    const gp::UInt32 poolAlignment,
    const gp::UInt32 maxAllocationSize,
    const PoolFreeListOrder freeListOrder,
    const bool defragEnabled,
    const PoolAllocationStrategy poolAllocationStrategy,
    const gp::UInt16 overflowPoolsCount
) noexcept
    : m_defaultPoolSize(defaultPoolSize)
    , m_poolAlignment(poolAlignment)
    , m_maxAllocationSize(maxAllocationSize)
    , m_freeListOrder(freeListOrder)
    , m_defragEnabled(defragEnabled)
    , m_lastDefragPoolIndex(0)
    , m_totalAllocatedBlocks(0u)
    , m_poolAllocationStrategy(poolAllocationStrategy)
    , m_overflowPoolsCount(static_cast<gp::Int16>(overflowPoolsCount))
{}

PoolAllocator::~PoolAllocator() noexcept
{
    destroy();
}

void PoolAllocator::scheduleAsyncPoolAllocation(
    const gp::UInt32 sizeInBytes, const gp::UInt32 allocationAlignment, const PoolResourceTypes allocationResourceType
) noexcept
{
    AsyncPoolAllocationPayload* const payload =
        new AsyncPoolAllocationPayload{ this,
                                        sizeInBytes,
                                        allocationAlignment,
                                        allocationResourceType,
                                        &m_inFlightAsyncTasks,
                                        [](PoolAllocator* const allocator,
                                           const gp::UInt32 size,
                                           const gp::UInt32 alignment,
                                           const PoolResourceTypes resourceType) noexcept -> void
    {
        allocator->poolAsyncAllocate(size, alignment, resourceType);
    } };

    // Caller has already incremented m_inFlightAsyncTasks before releasing m_mutex so destroy() cannot race.
    gp::thread::JobSystem* const jobSystem = gp::thread::JobSystem::getCurrentJobSystem();
    if (jobSystem != nullptr && jobSystem->isInitialized())
    {
        const gp::thread::JobSystemResult result =
            jobSystem->dispatch(&executeAsyncPoolAllocationJob, payload, nullptr, gp::thread::JobPriority::High);
        if (result == gp::thread::JobSystemResult::Success)
        {
            return;
        }
    }

    // Fall back to a detached worker thread when no global job system is bound to this thread.
    gp::platform::Thread workerThread;
    gp::platform::ThreadDesc threadDesc{};
    threadDesc.name = "RHIPoolAsyncAlloc";
    threadDesc.entry = &executeAsyncPoolAllocationThread;
    threadDesc.userData = payload;
    threadDesc.priority = gp::platform::ThreadPriority::AboveNormal;
    if (workerThread.start(threadDesc) == gp::platform::ThreadResult::Success)
    {
        (void)workerThread.detach();
        return;
    }

    // Last resort: execute synchronously so the task counter and allocation status never stall.
    executeAsyncPoolAllocationJob(payload);
}

void PoolAllocator::initialize(const PoolResourceTypes allocationResourceType) noexcept
{
    if (m_poolAllocationStrategy == PoolAllocationStrategy::Async)
    {
        GP_CHECK(m_pools.isEmpty());

        m_pools.pushBack(createNewPool(0, 16u, allocationResourceType));
        m_poolAllocationOrder.pushBack(0u);

        GP_CHECK(m_overflowPoolsCount > 0);

        m_poolAllocationStatus = PoolAllocationStatus::Allocating;
        m_inFlightAsyncTasks.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        scheduleAsyncPoolAllocation(static_cast<gp::UInt32>(m_defaultPoolSize), 16u, allocationResourceType);
    }
}

void PoolAllocator::destroy() noexcept
{
    if (m_poolAllocationStrategy == PoolAllocationStrategy::Async)
    {
        {
            const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);
            m_poolAllocationStatus = PoolAllocationStatus::ShutDown;
        }

        // Wait for any in-flight background pool allocation to observe ShutDown and exit before deleting pools.
        while (m_inFlightAsyncTasks.load(gp::MemoryOrder::Acquire) != 0u)
        {
            gp::platform::Thread::yieldExecution();
        }
    }

    for (gp::ISize index = static_cast<gp::ISize>(m_pools.size()) - 1; index >= 0; --index)
    {
        MemoryPool* const pool = m_pools[static_cast<gp::USize>(index)];
        if (pool != nullptr)
        {
            pool->destroy();
            delete pool;
        }
    }

    for (gp::USize index = 0u; index < m_overflowPools.size(); ++index)
    {
        MemoryPool* const pool = m_overflowPools[index];
        if (pool != nullptr)
        {
            pool->destroy();
            delete pool;
        }
    }

    m_pools.clear();
    m_poolAllocationOrder.clear();
    m_overflowPools.clear();
}

bool PoolAllocator::tryAllocateInternal(
    const gp::UInt32 sizeInBytes,
    const gp::UInt32 allocationAlignment,
    const PoolResourceTypes allocationResourceType,
    PoolAllocationData& allocationData
) noexcept
{
    GP_PROFILE_SCOPE_N("PoolAllocator::tryAllocateInternal");

    if (m_poolAllocationStrategy == PoolAllocationStrategy::Async)
    {
        m_mutex.lock();

        GP_CHECK(m_poolAllocationOrder.size() == m_pools.size());

        bool resourceAllocated = false;

        for (const gp::UInt32 poolIndex: m_poolAllocationOrder)
        {
            MemoryPool* const pool = m_pools[poolIndex];
            if (pool != nullptr && pool->isResourceTypeSupported(allocationResourceType) && !pool->isFull() &&
                pool->tryAllocate(sizeInBytes, allocationAlignment, allocationResourceType, allocationData))
            {
                resourceAllocated = true;
                break;
            }
        }

        gp::UInt32 alignedSize = 0u;

        // No existing pool could satisfy the request; promote an overflow pool or create one inline.
        if (!resourceAllocated)
        {
            gp::Int16 newPoolIndex = MemoryPool::kInvalidPoolIndex;
            for (gp::USize poolIndex = 0u; poolIndex < m_pools.size(); ++poolIndex)
            {
                if (m_pools[poolIndex] == nullptr)
                {
                    newPoolIndex = static_cast<gp::Int16>(poolIndex);
                    break;
                }
            }

            const gp::Int16 poolIndexForInit =
                (newPoolIndex >= 0) ? newPoolIndex : static_cast<gp::Int16>(m_pools.size());

            alignedSize = MemoryPool::getAlignedSize(sizeInBytes, m_poolAlignment, allocationAlignment);

            MemoryPool* readyPool = nullptr;
            for (gp::USize overflowIndex = 0u; overflowIndex < m_overflowPools.size(); ++overflowIndex)
            {
                MemoryPool* const candidate = m_overflowPools[overflowIndex];
                if (candidate->isResourceTypeSupported(allocationResourceType) &&
                    candidate->getPoolAlignment() == m_poolAlignment && candidate->getPoolSize() >= alignedSize)
                {
                    readyPool = candidate;

                    GP_CHECK(poolIndexForInit != MemoryPool::kInvalidPoolIndex);

                    readyPool->setPoolIndex(poolIndexForInit);
                    readyPool->initFreeBlocks();

                    GP_CHECK(m_overflowPools[overflowIndex]->isEmpty());
                    m_overflowPools.removeAtSwap(overflowIndex);
                    break;
                }
            }

            if (readyPool == nullptr)
            {
                readyPool = createNewPool(poolIndexForInit, alignedSize, allocationResourceType);
            }

            if (newPoolIndex >= 0)
            {
                m_pools[static_cast<gp::USize>(newPoolIndex)] = readyPool;
            }
            else
            {
                newPoolIndex = static_cast<gp::Int16>(m_pools.size());
                m_pools.pushBack(readyPool);
                m_poolAllocationOrder.pushBack(static_cast<gp::UInt32>(newPoolIndex));
            }

            GP_CHECK(newPoolIndex == readyPool->getPoolIndex());

            resourceAllocated = m_pools[static_cast<gp::USize>(newPoolIndex)]->tryAllocate(
                sizeInBytes, allocationAlignment, allocationResourceType, allocationData
            );
        }

        // Check whether the reserve of matching overflow pools needs to be replenished.
        gp::Int32 freePoolsCount = 0;
        for (gp::USize overflowIndex = 0u; overflowIndex < m_overflowPools.size(); ++overflowIndex)
        {
            const MemoryPool* const candidate = m_overflowPools[overflowIndex];
            if (candidate->isResourceTypeSupported(allocationResourceType) &&
                candidate->getPoolAlignment() == m_poolAlignment && candidate->getPoolSize() >= alignedSize)
            {
                ++freePoolsCount;
            }
        }

        if ((freePoolsCount < m_overflowPoolsCount) && (m_poolAllocationStatus == PoolAllocationStatus::Idle))
        {
            m_poolAllocationStatus = PoolAllocationStatus::Allocating;
            m_inFlightAsyncTasks.fetchAdd(1u, gp::MemoryOrder::AcqRel);
            m_mutex.unlock();

            scheduleAsyncPoolAllocation(
                static_cast<gp::UInt32>(m_defaultPoolSize), m_poolAlignment, allocationResourceType
            );
        }
        else
        {
            m_mutex.unlock();
        }

        return resourceAllocated;
    }
    else
    {
        const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);

        GP_CHECK(m_poolAllocationOrder.size() == m_pools.size());

        for (const gp::UInt32 poolIndex: m_poolAllocationOrder)
        {
            MemoryPool* const pool = m_pools[poolIndex];
            if (pool != nullptr && pool->isResourceTypeSupported(allocationResourceType) && !pool->isFull() &&
                pool->tryAllocate(sizeInBytes, allocationAlignment, allocationResourceType, allocationData))
            {
                return true;
            }
        }

        gp::Int16 newPoolIndex = MemoryPool::kInvalidPoolIndex;
        for (gp::USize poolIndex = 0u; poolIndex < m_pools.size(); ++poolIndex)
        {
            if (m_pools[poolIndex] == nullptr)
            {
                newPoolIndex = static_cast<gp::Int16>(poolIndex);
                break;
            }
        }

        const gp::UInt32 alignedSize = MemoryPool::getAlignedSize(sizeInBytes, m_poolAlignment, allocationAlignment);
        if (newPoolIndex >= 0)
        {
            m_pools[static_cast<gp::USize>(newPoolIndex)] =
                createNewPool(newPoolIndex, alignedSize, allocationResourceType);
        }
        else
        {
            newPoolIndex = static_cast<gp::Int16>(m_pools.size());
            m_pools.pushBack(createNewPool(newPoolIndex, alignedSize, allocationResourceType));
            m_poolAllocationOrder.pushBack(static_cast<gp::UInt32>(newPoolIndex));
        }

        return m_pools[static_cast<gp::USize>(newPoolIndex)]->tryAllocate(
            sizeInBytes, allocationAlignment, allocationResourceType, allocationData
        );
    }
}

void PoolAllocator::poolAsyncAllocate(
    const gp::UInt32 sizeInBytes, const gp::UInt32 allocationAlignment, const PoolResourceTypes allocationResourceType
) noexcept
{
    GP_PROFILE_SCOPE_N("PoolAllocator::poolAsyncAllocate");

    const gp::UInt32 alignedSize = MemoryPool::getAlignedSize(sizeInBytes, m_poolAlignment, allocationAlignment);
    MemoryPool* const newPool = createNewPool(MemoryPool::kInvalidPoolIndex, alignedSize, allocationResourceType);

    {
        const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);
        GP_CHECK(m_poolAllocationStrategy == PoolAllocationStrategy::Async);

        if (m_poolAllocationStatus == PoolAllocationStatus::Allocating)
        {
            // Free blocks will be initialized when this overflow pool is assigned a real pool index.
            m_overflowPools.pushBack(newPool);
        }
        else
        {
            newPool->destroy();
            delete newPool;
        }

        if (m_poolAllocationStatus != PoolAllocationStatus::ShutDown)
        {
            m_poolAllocationStatus = PoolAllocationStatus::Idle;
        }
    }
}

void PoolAllocator::deallocateInternal(PoolAllocationData& allocationData) noexcept
{
    const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);

    GP_CHECK(allocationData.isAllocated());
    GP_CHECK(allocationData.getPoolIndex() >= 0);
    GP_CHECK(static_cast<gp::USize>(allocationData.getPoolIndex()) < m_pools.size());

    m_pools[static_cast<gp::USize>(allocationData.getPoolIndex())]->deallocate(allocationData);
}

void PoolAllocator::sortPools() noexcept
{
    m_poolAllocationOrder.sort(
        [this](const gp::UInt32 lhs, const gp::UInt32 rhs) noexcept -> bool
    {
        // When defragmentation is enabled, pack new allocations into the fullest pools first.
        // Otherwise prefer smaller pools with a stable index tie-breaker.
        if (m_defragEnabled)
        {
            const gp::UInt32 lhsUsedSize = (m_pools[lhs] != nullptr)
                                               ? static_cast<gp::UInt32>(m_pools[lhs]->getUsedSize())
                                               : gp::NumericLimits<gp::UInt32>::max();
            const gp::UInt32 rhsUsedSize = (m_pools[rhs] != nullptr)
                                               ? static_cast<gp::UInt32>(m_pools[rhs]->getUsedSize())
                                               : gp::NumericLimits<gp::UInt32>::max();
            return lhsUsedSize > rhsUsedSize;
        }
        else
        {
            const gp::UInt32 lhsPoolSize = (m_pools[lhs] != nullptr)
                                               ? static_cast<gp::UInt32>(m_pools[lhs]->getPoolSize())
                                               : gp::NumericLimits<gp::UInt32>::max();
            const gp::UInt32 rhsPoolSize = (m_pools[rhs] != nullptr)
                                               ? static_cast<gp::UInt32>(m_pools[rhs]->getPoolSize())
                                               : gp::NumericLimits<gp::UInt32>::max();
            if (lhsPoolSize != rhsPoolSize)
            {
                return lhsPoolSize < rhsPoolSize;
            }

            const gp::UInt32 lhsPoolIndex = (m_pools[lhs] != nullptr)
                                                ? static_cast<gp::UInt32>(m_pools[lhs]->getPoolIndex())
                                                : gp::NumericLimits<gp::UInt32>::max();
            const gp::UInt32 rhsPoolIndex = (m_pools[rhs] != nullptr)
                                                ? static_cast<gp::UInt32>(m_pools[rhs]->getPoolIndex())
                                                : gp::NumericLimits<gp::UInt32>::max();
            return lhsPoolIndex < rhsPoolIndex;
        }
    }
    );
}

void PoolAllocator::defrag(
    const ContextArray& contexts, const gp::UInt32 maxCopySize, gp::UInt32& currentCopySize
) noexcept
{
    if (!m_defragEnabled)
    {
        return;
    }

    GP_PROFILE_SCOPE_N("PoolAllocator::defrag");

    const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);

    gp::Vector<MemoryPool*> sortedTargetPools;
    sortedTargetPools.reserve(m_pools.size());
    gp::UInt32 maxPoolsToDefrag = 0u;

    for (MemoryPool* const pool: m_pools)
    {
        if (pool != nullptr && !pool->isFull())
        {
            sortedTargetPools.pushBack(pool);
        }

        const gp::Float32 usage =
            (pool != nullptr)
                ? (static_cast<gp::Float32>(pool->getUsedSize()) / static_cast<gp::Float32>(pool->getPoolSize()))
                : 1.0f;
        if (usage < g_poolAllocatorDefragSizeFraction)
        {
            ++maxPoolsToDefrag;
        }
    }

    if (maxPoolsToDefrag == 0u || sortedTargetPools.size() < 2u)
    {
        return;
    }

    sortedTargetPools.sort(
        [](const MemoryPool* const lhs, const MemoryPool* const rhs) noexcept -> bool
    {
        return lhs->getUsedSize() < rhs->getUsedSize();
    }
    );

    gp::Vector<MemoryPool*> targetPools;
    if (g_poolAllocatorDefragMaxPoolsToClear < 0)
    {
        // Order destination pools from fullest to emptiest, skipping index 0 which is the first pool to clear.
        targetPools.reserve(sortedTargetPools.size());
        for (gp::ISize poolIndex = static_cast<gp::ISize>(sortedTargetPools.size()) - 1; poolIndex > 0; --poolIndex)
        {
            targetPools.pushBack(sortedTargetPools[static_cast<gp::USize>(poolIndex)]);
        }

        for (gp::USize poolIndex = 0u; poolIndex < sortedTargetPools.size() - 1u; ++poolIndex)
        {
            MemoryPool* const poolToClear = sortedTargetPools[poolIndex];
            poolToClear->tryClear(contexts, this, maxCopySize, currentCopySize, targetPools);

            // Drop the next pool to clear from the target list.
            targetPools.popBack();

            if (currentCopySize >= maxCopySize)
            {
                break;
            }
        }
    }
    else
    {
        if (static_cast<gp::USize>(m_lastDefragPoolIndex) >= m_pools.size())
        {
            m_lastDefragPoolIndex = 0;
        }

        gp::Int32 defraggedPoolCount = 0;
        for (; static_cast<gp::USize>(m_lastDefragPoolIndex) < m_pools.size(); ++m_lastDefragPoolIndex)
        {
            MemoryPool* const poolToClear = m_pools[static_cast<gp::USize>(m_lastDefragPoolIndex)];

            const gp::Float32 usage = (poolToClear != nullptr) ? (static_cast<gp::Float32>(poolToClear->getUsedSize()) /
                                                                  static_cast<gp::Float32>(poolToClear->getPoolSize()))
                                                               : 1.0f;
            if (usage >= g_poolAllocatorDefragSizeFraction)
            {
                continue;
            }

            targetPools.clear();
            targetPools.reserve(sortedTargetPools.size());
            for (gp::ISize sortedIndex = static_cast<gp::ISize>(sortedTargetPools.size()) - 1; sortedIndex >= 0;
                 --sortedIndex)
            {
                MemoryPool* const targetPool = sortedTargetPools[static_cast<gp::USize>(sortedIndex)];
                if (poolToClear == targetPool)
                {
                    break;
                }
                targetPools.pushBack(targetPool);
            }

            if (targetPools.isEmpty())
            {
                continue;
            }

            poolToClear->tryClear(contexts, this, maxCopySize, currentCopySize, targetPools);
            ++defraggedPoolCount;

            if (currentCopySize >= maxCopySize || (defraggedPoolCount >= g_poolAllocatorDefragMaxPoolsToClear))
            {
                break;
            }
        }
    }
}

void PoolAllocator::updateMemoryStats(
    gp::UInt32& ioMemoryAllocated,
    gp::UInt32& ioMemoryUsed,
    gp::UInt32& ioMemoryFree,
    gp::UInt32& ioMemoryEndFree,
    gp::UInt32& ioAlignmentWaste,
    gp::UInt32& ioAllocatedPageCount,
    gp::UInt32& ioFullPageCount
) noexcept
{
    const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);

    m_totalAllocatedBlocks = 0u;

    gp::UInt32 maxEndFree = 0u;
    for (const MemoryPool* const pool: m_pools)
    {
        if (pool != nullptr)
        {
            ioMemoryAllocated += static_cast<gp::UInt32>(pool->getPoolSize());
            ioMemoryUsed += static_cast<gp::UInt32>(pool->getUsedSize() - pool->getAlignmentWaste());
            ioMemoryFree += static_cast<gp::UInt32>(pool->getFreeSize());
            maxEndFree = gp::math::max(maxEndFree, static_cast<gp::UInt32>(pool->getFreeSize()));
            ioAlignmentWaste += pool->getAlignmentWaste();
            ++ioAllocatedPageCount;
            if (pool->isFull())
            {
                ++ioFullPageCount;
            }

            m_totalAllocatedBlocks += pool->getAllocatedBlocks();
        }
    }

    ioMemoryEndFree += maxEndFree;
}

}   // namespace gp::rhi::allocator
