// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/jobs/JobSystem.hpp"
#include "memory/allocators/FiberScratchAllocator.hpp"
#include "memory/Memory.hpp"
#include "platforms/base/system/Time.hpp"
#include "threading/Fences.hpp"
#include "threading/jobs/ParallelFor.hpp"
#include "threading/sync/JobWaitNode.hpp"
#include <new>   // IWYU pragma: keep

namespace gp::thread
{

namespace detail
{

/// @brief Base `FiberId` bit-prefix identifying pooled `Standard64KiB` fibers (`0x00010000 | poolIndex`).
inline constexpr FiberId kStandardPoolFiberIdPrefix = 0x00010000u;

/// @brief Base `FiberId` bit-prefix identifying pooled `Large512KiB` fibers (`0x00020000 | poolIndex`).
inline constexpr FiberId kLargePoolFiberIdPrefix = 0x00020000u;

/// @brief Base `FiberId` bit-prefix identifying worker `ThreadAnchor` fibers (`0x00040000 | workerIndex`).
inline constexpr FiberId kWorkerAnchorFiberIdPrefix = 0x00040000u;

/// @brief Mask extracting the 16-bit prefix from a `JobSystem` `FiberId`.
inline constexpr FiberId kPoolFiberIdPrefixMask = 0xFFFF0000u;

/// @brief Mask extracting the 16-bit pool slot index from a `JobSystem` `FiberId`.
inline constexpr gp::UInt16 kPoolFiberIdIndexMask = 0xFFFFu;

/// @brief Thread-local pointer to the `JobSystem` bound to the current OS thread.
static thread_local JobSystem* tl_activeJobSystem{ nullptr };

/// @brief Thread-local worker slot index (`0` for the main thread, `1 .. N` for dedicated background workers).
static thread_local gp::UInt32 tl_workerSlotIndex{ kInvalidWorkerIndex };

/// @brief Thread-local pointer to the current OS worker thread's `ThreadAnchor` fiber.
static thread_local Fiber* tl_workerAnchorFiber{ nullptr };

/// @brief Thread-local pointer to a `FiberPoolSlot` that just finished executing and needs retirement on the anchor.
static thread_local void* tl_completedFiberSlot{ nullptr };

/// @brief Thread-local priority band of the job currently executing on this OS thread.
static thread_local JobPriority tl_activeJobPriority{ JobPriority::Normal };

/// @brief Thread-local stack tier of the job currently executing on this OS thread.
static thread_local FiberStackTier tl_activeJobStackTier{ FiberStackTier::Standard64KiB };

/// @brief Emits a compiler-level read/write memory barrier to prevent TLS address or value caching across a
///        cooperative fiber context switch.
static GP_FORCEINLINE void compilerMemoryBarrier() noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#else
    _ReadWriteBarrier();
#endif
}

/// @brief Reads `tl_activeJobSystem` through an out-of-line barrier so a resumed fiber that migrated to another OS
///        worker thread always observes the new host thread's `JobSystem*` ([ADR-0049], [ADR-0052]).
/// @return Pointer to the `JobSystem` bound to the current OS thread, or `nullptr`.
[[nodiscard]] static GP_FORCENOINLINE JobSystem* getActiveJobSystemTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_activeJobSystem;
}

/// @brief Reads `tl_workerSlotIndex` through an out-of-line barrier so a migrated fiber always observes its current
///        host OS thread's worker slot index.
/// @return Worker slot index (`0 .. N`), or `kInvalidWorkerIndex` if unbound.
[[nodiscard]] static GP_FORCENOINLINE gp::UInt32 getWorkerSlotIndexTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_workerSlotIndex;
}

/// @brief Binds or unbinds the calling OS thread's `JobSystem` pointer, worker slot index, and `ThreadAnchor` fiber.
/// @param[in] system Owning `JobSystem` pointer (or `nullptr` when unbinding).
/// @param[in] slotIndex Worker slot index in `[0, totalWorkerSlotCount - 1]` (or `kInvalidWorkerIndex`).
/// @param[in] anchorFiber Pointer to this OS thread's `ThreadAnchor` fiber (or `nullptr`).
static GP_FORCENOINLINE void
    bindWorkerThreadTLS(JobSystem* const system, const gp::UInt32 slotIndex, Fiber* const anchorFiber) noexcept
{
    compilerMemoryBarrier();
    tl_activeJobSystem = system;
    tl_workerSlotIndex = slotIndex;
    tl_workerAnchorFiber = anchorFiber;
    compilerMemoryBarrier();
}

/// @brief Reads `tl_workerAnchorFiber` through an out-of-line barrier so a yielding pooled fiber switches back to the
///        `ThreadAnchor` of whichever OS worker thread is currently hosting it.
/// @return Pointer to the current OS worker thread's `ThreadAnchor` fiber, or `nullptr`.
[[nodiscard]] static GP_FORCENOINLINE Fiber* getWorkerAnchorFiberTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_workerAnchorFiber;
}

/// @brief Records a completed `FiberPoolSlot*` in TLS immediately before switching from the finishing pooled fiber
///        back to the worker thread's `ThreadAnchor`.
/// @param[in] completedSlot Pointer to the `FiberPoolSlot` that completed its bound task.
static GP_FORCENOINLINE void setCompletedFiberSlotTLS(void* const completedSlot) noexcept
{
    compilerMemoryBarrier();
    tl_completedFiberSlot = completedSlot;
    compilerMemoryBarrier();
}

/// @brief Atomically reads and clears `tl_completedFiberSlot` on the worker's `ThreadAnchor` after a fiber switch.
/// @return Pointer to the `FiberPoolSlot` to retire and return to the free pool, or `nullptr`.
[[nodiscard]] static GP_FORCENOINLINE void* consumeCompletedFiberSlotTLS() noexcept
{
    compilerMemoryBarrier();
    void* const slot = tl_completedFiberSlot;
    tl_completedFiberSlot = nullptr;
    compilerMemoryBarrier();
    return slot;
}

/// @brief Updates the calling OS thread's active job priority and stack tier metadata prior to invoking a task.
/// @param[in] priority Priority band of the active task.
/// @param[in] stackTier Stack tier of the active task.
static GP_FORCENOINLINE void setActiveJobMetaTLS(const JobPriority priority, const FiberStackTier stackTier) noexcept
{
    compilerMemoryBarrier();
    tl_activeJobPriority = priority;
    tl_activeJobStackTier = stackTier;
    compilerMemoryBarrier();
}

/// @brief Reads `tl_activeJobPriority` through an out-of-line barrier.
/// @return Priority band of the job currently executing on the calling OS thread.
[[nodiscard]] static GP_FORCENOINLINE JobPriority getActiveJobPriorityTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_activeJobPriority;
}

/// @brief Reads `tl_activeJobStackTier` through an out-of-line barrier.
/// @return Stack tier of the job currently executing on the calling OS thread.
[[nodiscard]] static GP_FORCENOINLINE FiberStackTier getActiveJobStackTierTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_activeJobStackTier;
}

/// @brief Advances a 32-bit Marsaglia XorShift PRNG state used to randomize work-stealing victim selection.
/// @param[in,out] state Mutable 32-bit PRNG state (seeded with `0x9E3779B9u` if zero).
/// @return Next 32-bit pseudo-random integer.
[[nodiscard]] static constexpr gp::UInt32 nextXorShift32(gp::UInt32& state) noexcept
{
    gp::UInt32 x = (state == 0u) ? 0x9E3779B9u : state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    state = x;
    return x;
}

/// @brief Formats a zero-allocation worker thread debug name (`"GP_JobWorker_XX"`) into a 32-byte stack buffer.
/// @param[out] outBuffer Destination 32-byte character array.
/// @param[in] workerIndex One-based worker slot index (`1 .. 63`).
static void formatWorkerDebugName(char (&outBuffer)[32], const gp::UInt32 workerIndex) noexcept
{
    constexpr const char kPrefix[] = "GP_JobWorker_";
    gp::USize pos = 0u;
    while (kPrefix[pos] != '\0' && pos < 24u)
    {
        outBuffer[pos] = kPrefix[pos];
        ++pos;
    }

    const gp::UInt32 tens = (workerIndex / 10u) % 10u;
    const gp::UInt32 ones = workerIndex % 10u;
    outBuffer[pos++] = static_cast<char>('0' + tens);
    outBuffer[pos++] = static_cast<char>('0' + ones);
    outBuffer[pos] = '\0';
}

}   // namespace detail

JobSystem::~JobSystem() noexcept
{
    shutdown();
}

JobSystemResult JobSystem::initialize(const JobSystemDesc& desc) noexcept
{
    if (isInitialized())
    {
        return JobSystemResult::AlreadyInitialized;
    }

    if (desc.standardFiberCount == 0u || desc.largeFiberCount == 0u || desc.workerDequeCapacity == 0u ||
        desc.globalQueueCapacity == 0u || desc.workerThreadCount > kMaxJobSystemWorkers)
    {
        return JobSystemResult::InvalidArguments;
    }

    gp::memory::Malloc* const targetAllocator =
        (desc.backingMalloc != nullptr) ? desc.backingMalloc : gp::memory::getGlobalMalloc();
    if (targetAllocator == nullptr) [[unlikely]]
    {
        return JobSystemResult::OutOfMemory;
    }

    m_allocator = targetAllocator;
    m_ownsScratchPool = false;

    if (desc.initializeFiberScratchPool && gp::memory::FiberScratchArena::tryGetSharedPagePool() == nullptr)
    {
        const gp::USize pageSize =
            (desc.scratchPageSizeInBytes == 0u) ? (64ULL * 1024ULL) : desc.scratchPageSizeInBytes;
        const gp::UInt32 maxPages = (desc.maxScratchPages == 0u) ? 256u : desc.maxScratchPages;
        if (!gp::memory::FiberScratchArena::initializePool(pageSize, maxPages, m_allocator)) [[unlikely]]
        {
            return JobSystemResult::OutOfMemory;
        }
        m_ownsScratchPool = true;
    }

    gp::UInt32 resolvedWorkers = desc.workerThreadCount;
    if (resolvedWorkers == 0u)
    {
        const gp::UInt32 processCores = gp::platform::Thread::getProcessConcurrency();
        resolvedWorkers = (processCores > 1u) ? (processCores - 1u) : 1u;
        if (resolvedWorkers > kMaxJobSystemWorkers)
        {
            resolvedWorkers = kMaxJobSystemWorkers;
        }
    }

    m_workerThreadCount = resolvedWorkers;
    m_totalWorkerSlotCount = resolvedWorkers + 1u;
    m_standardFiberCount = desc.standardFiberCount;
    m_largeFiberCount = desc.largeFiberCount;
    m_spinIterationsBeforeYield = desc.spinIterationsBeforeYield;
    m_yieldIterationsBeforeSleep = desc.yieldIterationsBeforeSleep;
    m_idleSleepNanoseconds = (desc.idleSleepNanoseconds == 0ULL) ? 50'000ULL : desc.idleSleepNanoseconds;

    m_sleepingWorkerMask.store(0ULL, gp::MemoryOrder::Relaxed);
    m_workAvailableEpoch.store(0ULL, gp::MemoryOrder::Relaxed);
    m_shouldShutdown.store(0u, gp::MemoryOrder::Relaxed);
    m_totalJobsDispatched.store(0ULL, gp::MemoryOrder::Relaxed);
    m_totalJobsExecuted.store(0ULL, gp::MemoryOrder::Relaxed);
    m_totalJobsStolen.store(0ULL, gp::MemoryOrder::Relaxed);
    m_totalFiberSwitches.store(0ULL, gp::MemoryOrder::Relaxed);

    // Size the resumed-fiber queue to at least twice the total fiber pool so wakeups never overflow.
    const gp::USize totalFibers =
        static_cast<gp::USize>(m_standardFiberCount) + static_cast<gp::USize>(m_largeFiberCount);
    const gp::USize resumedQueueCap =
        gp::memory::roundUpToPowerOfTwo((totalFibers * 2u < 64u) ? 64u : (totalFibers * 2u));
    if (m_resumedFiberQueue.initialize(resumedQueueCap, m_allocator) != JobQueueResult::Success) [[unlikely]]
    {
        shutdown();
        return JobSystemResult::OutOfMemory;
    }

    // Initialize the global priority MPMC queues (High, Normal, Low).
    for (gp::USize p = 0u; p < static_cast<gp::USize>(JobPriority::Count); ++p)
    {
        if (m_globalQueues[p].initialize(desc.globalQueueCapacity, m_allocator) != JobQueueResult::Success) [[unlikely]]
        {
            shutdown();
            return JobSystemResult::OutOfMemory;
        }
    }

    // Initialize the lock-free index free-lists and two-tier Fiber pools.
    const gp::USize stdFreeListCap = gp::memory::roundUpToPowerOfTwo(
        (m_standardFiberCount < kMinJobQueueCapacity) ? kMinJobQueueCapacity
                                                      : static_cast<gp::USize>(m_standardFiberCount)
    );
    const gp::USize largeFreeListCap = gp::memory::roundUpToPowerOfTwo(
        (m_largeFiberCount < kMinJobQueueCapacity) ? kMinJobQueueCapacity : static_cast<gp::USize>(m_largeFiberCount)
    );

    if (m_freeStandardFibers.initialize(stdFreeListCap, m_allocator) != JobQueueResult::Success ||
        m_freeLargeFibers.initialize(largeFreeListCap, m_allocator) != JobQueueResult::Success) [[unlikely]]
    {
        shutdown();
        return JobSystemResult::OutOfMemory;
    }

    void* const rawStdFibers = m_allocator->tryAllocate(
        static_cast<gp::USize>(m_standardFiberCount) * sizeof(FiberPoolSlot), alignof(FiberPoolSlot)
    );
    void* const rawLargeFibers = m_allocator->tryAllocate(
        static_cast<gp::USize>(m_largeFiberCount) * sizeof(FiberPoolSlot), alignof(FiberPoolSlot)
    );
    if (rawStdFibers == nullptr || rawLargeFibers == nullptr) [[unlikely]]
    {
        if (rawStdFibers != nullptr)
        {
            m_allocator->deallocate(rawStdFibers);
        }
        if (rawLargeFibers != nullptr)
        {
            m_allocator->deallocate(rawLargeFibers);
        }
        shutdown();
        return JobSystemResult::OutOfMemory;
    }

    m_standardFibers = static_cast<FiberPoolSlot*>(rawStdFibers);
    for (gp::UInt16 i = 0u; i < m_standardFiberCount; ++i)
    {
        new (&m_standardFibers[i]) FiberPoolSlot();
        m_standardFibers[i].owner = this;
        m_standardFibers[i].poolIndex = i;
        m_standardFibers[i].tier = FiberStackTier::Standard64KiB;

        FiberDesc fDesc{};
        fDesc.name = "GP_JobFiber_64K";
        fDesc.id = detail::kStandardPoolFiberIdPrefix | static_cast<FiberId>(i);
        fDesc.entry = &JobSystem::fiberTaskEntryPoint;
        fDesc.userData = &m_standardFibers[i];
        fDesc.stackTier = FiberStackTier::Standard64KiB;
        fDesc.resetScratchOnTaskCompletion = true;

        if (m_standardFibers[i].fiber.create(fDesc) != FiberResult::Success ||
            m_freeStandardFibers.tryPush(i) != JobQueueResult::Success) [[unlikely]]
        {
            for (gp::UInt16 j = 0u; j <= i; ++j)
            {
                m_standardFibers[j].~FiberPoolSlot();
            }
            m_allocator->deallocate(m_standardFibers);
            m_standardFibers = nullptr;
            m_allocator->deallocate(rawLargeFibers);
            shutdown();
            return JobSystemResult::FiberPoolExhausted;
        }
    }

    m_largeFibers = static_cast<FiberPoolSlot*>(rawLargeFibers);
    for (gp::UInt16 i = 0u; i < m_largeFiberCount; ++i)
    {
        new (&m_largeFibers[i]) FiberPoolSlot();
        m_largeFibers[i].owner = this;
        m_largeFibers[i].poolIndex = i;
        m_largeFibers[i].tier = FiberStackTier::Large512KiB;

        FiberDesc fDesc{};
        fDesc.name = "GP_JobFiber_512K";
        fDesc.id = detail::kLargePoolFiberIdPrefix | static_cast<FiberId>(i);
        fDesc.entry = &JobSystem::fiberTaskEntryPoint;
        fDesc.userData = &m_largeFibers[i];
        fDesc.stackTier = FiberStackTier::Large512KiB;
        fDesc.resetScratchOnTaskCompletion = true;

        if (m_largeFibers[i].fiber.create(fDesc) != FiberResult::Success ||
            m_freeLargeFibers.tryPush(i) != JobQueueResult::Success) [[unlikely]]
        {
            for (gp::UInt16 j = 0u; j <= i; ++j)
            {
                m_largeFibers[j].~FiberPoolSlot();
            }
            m_allocator->deallocate(m_largeFibers);
            m_largeFibers = nullptr;
            shutdown();
            return JobSystemResult::FiberPoolExhausted;
        }
    }

    // Allocate and initialize per-worker slots (Slot 0 = Main Thread, Slots 1..N = Background Workers).
    void* const rawWorkerSlots = m_allocator->tryAllocate(
        static_cast<gp::USize>(m_totalWorkerSlotCount) * sizeof(WorkerSlot), alignof(WorkerSlot)
    );
    if (rawWorkerSlots == nullptr) [[unlikely]]
    {
        shutdown();
        return JobSystemResult::OutOfMemory;
    }

    m_workerSlots = static_cast<WorkerSlot*>(rawWorkerSlots);
    for (gp::UInt32 s = 0u; s < m_totalWorkerSlotCount; ++s)
    {
        new (&m_workerSlots[s]) WorkerSlot();
        m_workerSlots[s].owner = this;
        m_workerSlots[s].workerIndex = s;
        m_workerSlots[s].rngState = 0x9E3779B9u ^ ((s + 1u) * 0x85EBCA6Bu);
        detail::formatWorkerDebugName(m_workerSlots[s].debugName, s);

        if (m_workerSlots[s].localDeque.initialize(desc.workerDequeCapacity, m_allocator) != JobQueueResult::Success)
            [[unlikely]]
        {
            for (gp::UInt32 j = 0u; j <= s; ++j)
            {
                m_workerSlots[j].~WorkerSlot();
            }
            m_allocator->deallocate(m_workerSlots);
            m_workerSlots = nullptr;
            shutdown();
            return JobSystemResult::OutOfMemory;
        }

        if (gp::memory::PagePool* const sharedPool = gp::memory::FiberScratchArena::tryGetSharedPagePool();
            sharedPool != nullptr)
        {
            (void)m_workerSlots[s].emergencyScratchArena.initialize(
                sharedPool, m_allocator, true, "gp::WorkerEmergencyScratch"
            );
        }
    }

    // Bind Slot 0 to the initializing Main Thread.
    detail::bindWorkerThreadTLS(this, kMainThreadWorkerIndex, nullptr);
    m_isInitialized.store(1u, gp::MemoryOrder::Release);

    // Spawn dedicated background OS worker threads (Slots 1 .. m_workerThreadCount).
    const gp::UInt32 hardwareCores = gp::platform::Thread::getHardwareConcurrency();
    for (gp::UInt32 w = 1u; w <= m_workerThreadCount; ++w)
    {
        gp::platform::ThreadDesc tDesc{};
        tDesc.name = m_workerSlots[w].debugName;
        tDesc.entry = &JobSystem::workerThreadEntryPoint;
        tDesc.userData = &m_workerSlots[w];
        tDesc.priority = desc.workerThreadPriority;
        tDesc.failOnConfigError = false;

        if (desc.enableCoreAffinity && hardwareCores > 1u && hardwareCores <= 64u)
        {
            const gp::UInt32 targetCore = w % hardwareCores;
            tDesc.affinityMask = (1ULL << targetCore);
        }

        if (m_workerSlots[w].thread.start(tDesc) != gp::platform::ThreadResult::Success) [[unlikely]]
        {
            shutdown();
            return JobSystemResult::ThreadStartFailed;
        }

        while (m_workerSlots[w].threadStarted.load(gp::MemoryOrder::Acquire) == 0u)
        {
            gp::platform::Thread::spinWait();
        }
    }

    return JobSystemResult::Success;
}

void JobSystem::shutdown() noexcept
{
    m_shouldShutdown.store(1u, gp::MemoryOrder::Release);
    m_isInitialized.store(0u, gp::MemoryOrder::Release);
    wakeAllWorkers();

    if (m_workerSlots != nullptr)
    {
        for (gp::UInt32 w = 1u; w <= m_workerThreadCount; ++w)
        {
            if (m_workerSlots[w].thread.isJoinable())
            {
                (void)m_workerSlots[w].thread.join();
            }
        }

        for (gp::UInt32 s = 0u; s < m_totalWorkerSlotCount; ++s)
        {
            m_workerSlots[s].emergencyScratchArena.shutdown();
            m_workerSlots[s].localDeque.shutdown();
            m_workerSlots[s].~WorkerSlot();
        }

        if (m_allocator != nullptr)
        {
            m_allocator->deallocate(m_workerSlots);
        }
        m_workerSlots = nullptr;
    }

    if (m_standardFibers != nullptr)
    {
        for (gp::UInt16 i = 0u; i < m_standardFiberCount; ++i)
        {
            (void)m_standardFibers[i].fiber.destroy();
            m_standardFibers[i].~FiberPoolSlot();
        }
        if (m_allocator != nullptr)
        {
            m_allocator->deallocate(m_standardFibers);
        }
        m_standardFibers = nullptr;
    }

    if (m_largeFibers != nullptr)
    {
        for (gp::UInt16 i = 0u; i < m_largeFiberCount; ++i)
        {
            (void)m_largeFibers[i].fiber.destroy();
            m_largeFibers[i].~FiberPoolSlot();
        }
        if (m_allocator != nullptr)
        {
            m_allocator->deallocate(m_largeFibers);
        }
        m_largeFibers = nullptr;
    }

    m_freeStandardFibers.shutdown();
    m_freeLargeFibers.shutdown();
    for (gp::USize p = 0u; p < static_cast<gp::USize>(JobPriority::Count); ++p)
    {
        m_globalQueues[p].shutdown();
    }
    m_resumedFiberQueue.shutdown();

    if (detail::getActiveJobSystemTLS() == this)
    {
        detail::bindWorkerThreadTLS(nullptr, kInvalidWorkerIndex, nullptr);
    }

    if (m_ownsScratchPool)
    {
        gp::memory::FiberScratchArena::shutdownPool();
        m_ownsScratchPool = false;
    }

    m_workerThreadCount = 0u;
    m_totalWorkerSlotCount = 0u;
    m_standardFiberCount = 0u;
    m_largeFiberCount = 0u;
    m_allocator = nullptr;
}

void JobSystem::executeInlineJobItem(const JobDecl& decl) noexcept
{
    const JobPriority savedPriority = detail::getActiveJobPriorityTLS();
    const FiberStackTier savedTier = detail::getActiveJobStackTierTLS();
    detail::setActiveJobMetaTLS(decl.priority, decl.stackTier);

    gp::memory::ScopedFiberScratchMark inlineScratchScope;
    (void)decl.invoke();

    detail::setActiveJobMetaTLS(savedPriority, savedTier);
    m_totalJobsExecuted.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
    if (decl.counter != nullptr)
    {
        (void)decl.counter->decrement(1u);
    }
}

JobSystemResult
    JobSystem::enqueueItem(const JobQueueItem& item, const gp::UInt32 callerSlotIndex, const JobFlags flags) noexcept
{
    const bool canUseLocalDeque =
        (callerSlotIndex < m_totalWorkerSlotCount) && !gp::enums::hasAllFlags(flags, JobFlags::ForceGlobalQueue) &&
        (item.priority != JobPriority::High || gp::enums::hasAllFlags(flags, JobFlags::PreferLocalQueue));

    if (canUseLocalDeque)
    {
        if (m_workerSlots[callerSlotIndex].localDeque.tryPushBottom(item) == JobQueueResult::Success) [[likely]]
        {
            return JobSystemResult::Success;
        }
    }

    const gp::USize priorityIdx = (static_cast<gp::USize>(item.priority) < static_cast<gp::USize>(JobPriority::Count))
                                      ? static_cast<gp::USize>(item.priority)
                                      : static_cast<gp::USize>(JobPriority::Normal);

    for (gp::UInt32 attempt = 0u; attempt < 64u; ++attempt)
    {
        if (m_globalQueues[priorityIdx].tryPush(item) == JobQueueResult::Success) [[likely]]
        {
            return JobSystemResult::Success;
        }
        gp::platform::Thread::spinWait();
    }

    // When all queues are saturated, either run the job inline if permitted or help drain one queued item.
    while (m_globalQueues[priorityIdx].tryPush(item) != JobQueueResult::Success)
    {
        if (gp::enums::hasAllFlags(flags, JobFlags::AllowInlineOnOverflow) && item.isJobEntry())
        {
            executeInlineJobItem(item.toJobDecl());
            return JobSystemResult::Success;
        }

        if (!helpOneWorkItem())
        {
            gp::platform::Thread::spinWait();
        }
    }

    return JobSystemResult::Success;
}

JobSystemResult JobSystem::dispatch(const JobDecl& decl, const bool autoIncrementCounter) noexcept
{
    if (!isInitialized()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (!decl.isValid()) [[unlikely]]
    {
        return JobSystemResult::InvalidArguments;
    }

    if (autoIncrementCounter && decl.counter != nullptr)
    {
        if (decl.counter->increment() != JobSyncResult::Success) [[unlikely]]
        {
            return JobSystemResult::SyncError;
        }
    }

    const gp::UInt32 callerSlot =
        (detail::getActiveJobSystemTLS() == this) ? detail::getWorkerSlotIndexTLS() : kInvalidWorkerIndex;
    const JobFlags flags = static_cast<JobFlags>(decl.flags);
    const JobQueueItem item = JobQueueItem::fromJob(decl);

    m_totalJobsDispatched.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
    const JobSystemResult res = enqueueItem(item, callerSlot, flags);
    wakeSleepingWorkers(1u);
    return res;
}

JobSystemResult JobSystem::dispatchBatch(
    const JobDecl* const decls, const gp::UInt32 count, const bool autoIncrementCounters
) noexcept
{
    if (!isInitialized()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (count == 0u)
    {
        return JobSystemResult::Success;
    }

    if (decls == nullptr) [[unlikely]]
    {
        return JobSystemResult::InvalidArguments;
    }

    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        if (!decls[i].isValid()) [[unlikely]]
        {
            return JobSystemResult::InvalidArguments;
        }
    }

    if (autoIncrementCounters)
    {
        for (gp::UInt32 i = 0u; i < count; ++i)
        {
            if (decls[i].counter != nullptr)
            {
                if (decls[i].counter->increment() != JobSyncResult::Success) [[unlikely]]
                {
                    for (gp::UInt32 rollback = 0u; rollback < i; ++rollback)
                    {
                        if (decls[rollback].counter != nullptr)
                        {
                            (void)decls[rollback].counter->decrement(1u);
                        }
                    }
                    return JobSystemResult::SyncError;
                }
            }
        }
    }

    const gp::UInt32 callerSlot =
        (detail::getActiveJobSystemTLS() == this) ? detail::getWorkerSlotIndexTLS() : kInvalidWorkerIndex;

    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        const JobFlags flags = static_cast<JobFlags>(decls[i].flags);
        const JobQueueItem item = JobQueueItem::fromJob(decls[i]);
        (void)enqueueItem(item, callerSlot, flags);
    }

    m_totalJobsDispatched.fetchAdd(static_cast<gp::UInt64>(count), gp::MemoryOrder::Relaxed);
    wakeSleepingWorkers(count);
    return JobSystemResult::Success;
}

JobSystemResult JobSystem::dispatchRepeated(
    const JobDecl& decl, const gp::UInt32 repeatCount, const bool autoIncrementCounter
) noexcept
{
    if (!isInitialized()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (repeatCount == 0u)
    {
        return JobSystemResult::Success;
    }

    if (!decl.isValid()) [[unlikely]]
    {
        return JobSystemResult::InvalidArguments;
    }

    if (autoIncrementCounter && decl.counter != nullptr)
    {
        if (decl.counter->add(repeatCount) != JobSyncResult::Success) [[unlikely]]
        {
            return JobSystemResult::SyncError;
        }
    }

    const gp::UInt32 callerSlot =
        (detail::getActiveJobSystemTLS() == this) ? detail::getWorkerSlotIndexTLS() : kInvalidWorkerIndex;
    const JobFlags flags = static_cast<JobFlags>(decl.flags);
    const JobQueueItem item = JobQueueItem::fromJob(decl);

    for (gp::UInt32 i = 0u; i < repeatCount; ++i)
    {
        (void)enqueueItem(item, callerSlot, flags);
    }

    m_totalJobsDispatched.fetchAdd(static_cast<gp::UInt64>(repeatCount), gp::MemoryOrder::Relaxed);
    wakeSleepingWorkers(repeatCount);
    return JobSystemResult::Success;
}

JobSystemResult JobSystem::parallelFor(
    const gp::UInt32 elementCount,
    const gp::UInt32 minBatchSize,
    const ParallelForFunction callback,
    void* const userData,
    const JobPriority priority
) noexcept
{
    return ParallelFor::dispatch(*this, elementCount, minBatchSize, callback, userData, priority);
}

JobSystemResult JobSystem::parallelFor(
    const ParallelForDesc& desc, const ParallelForFunction callback, void* const userData
) noexcept
{
    return ParallelFor::dispatchWithDesc(*this, desc, callback, userData);
}

void JobSystem::wakeSleepingWorkers(const gp::UInt32 maxWorkersToWake) noexcept
{
    if (maxWorkersToWake == 0u)
    {
        return;
    }

    m_workAvailableEpoch.fetchAdd(1ULL, gp::MemoryOrder::Release);

    gp::UInt64 mask = m_sleepingWorkerMask.load(gp::MemoryOrder::Acquire);
    if (mask == 0ULL) [[likely]]
    {
        return;
    }

    if (maxWorkersToWake >= m_workerThreadCount)
    {
        (void)m_sleepingWorkerMask.exchange(0ULL, gp::MemoryOrder::AcqRel);
        return;
    }

    gp::UInt32 woken = 0u;
    while (mask != 0ULL && woken < maxWorkersToWake)
    {
        // Isolate and clear the lowest set bit to wake one sleeping worker.
        const gp::UInt64 lowestBit = mask & (~mask + 1ULL);
        const gp::UInt64 prev = m_sleepingWorkerMask.fetchAnd(~lowestBit, gp::MemoryOrder::AcqRel);
        if ((prev & lowestBit) != 0ULL)
        {
            ++woken;
        }
        mask = prev & ~lowestBit;
    }
}

void JobSystem::wakeAllWorkers() noexcept
{
    m_workAvailableEpoch.fetchAdd(1ULL, gp::MemoryOrder::Release);
    (void)m_sleepingWorkerMask.exchange(0ULL, gp::MemoryOrder::AcqRel);
}

bool JobSystem::hasAnyPendingWorkApprox() const noexcept
{
    if (!m_resumedFiberQueue.isEmptyApprox())
    {
        return true;
    }

    for (gp::USize p = 0u; p < static_cast<gp::USize>(JobPriority::Count); ++p)
    {
        if (!m_globalQueues[p].isEmptyApprox())
        {
            return true;
        }
    }

    if (m_workerSlots != nullptr)
    {
        for (gp::UInt32 s = 0u; s < m_totalWorkerSlotCount; ++s)
        {
            if (!m_workerSlots[s].localDeque.isEmptyApprox())
            {
                return true;
            }
        }
    }

    return false;
}

bool JobSystem::tryPopNextWorkItem(WorkerSlot& slot, JobQueueItem& outItem) noexcept
{
    // Drain woken fibers first because they already hold active stacks and FiberScratchArena pages.
    if (m_resumedFiberQueue.tryPop(outItem) == JobQueueResult::Success)
    {
        return true;
    }

    // Check the global High-Priority MPMC queue.
    if (m_globalQueues[static_cast<gp::USize>(JobPriority::High)].tryPop(outItem) == JobQueueResult::Success)
    {
        return true;
    }

    // Pop from the bottom of the calling worker's Chase-Lev deque in LIFO order for warm cache locality.
    if (slot.localDeque.tryPopBottom(outItem) == JobQueueResult::Success)
    {
        return true;
    }

    // Check the global Normal-Priority MPMC queue.
    if (m_globalQueues[static_cast<gp::USize>(JobPriority::Normal)].tryPop(outItem) == JobQueueResult::Success)
    {
        return true;
    }

    // Steal from the top of peer worker deques in FIFO order, starting from a pseudo-random victim index.
    const gp::UInt32 slotCount = m_totalWorkerSlotCount;
    if (slotCount > 1u)
    {
        const gp::UInt32 startOffset = detail::nextXorShift32(slot.rngState);
        for (gp::UInt32 i = 0u; i < slotCount; ++i)
        {
            const gp::UInt32 victimIndex = (startOffset + i) % slotCount;
            if (victimIndex == slot.workerIndex)
            {
                continue;
            }

            const JobQueueResult stealRes = m_workerSlots[victimIndex].localDeque.tryStealTop(outItem);
            if (stealRes == JobQueueResult::Success)
            {
                m_totalJobsStolen.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
                return true;
            }
        }
    }

    // Fall back to the global Low-Priority MPMC queue for background work.
    if (m_globalQueues[static_cast<gp::USize>(JobPriority::Low)].tryPop(outItem) == JobQueueResult::Success)
    {
        return true;
    }

    return false;
}

JobSystem::FiberPoolSlot* JobSystem::acquireFiberSlot(const FiberStackTier requestedTier) noexcept
{
    gp::UInt16 index = kInvalidFiberPoolIndex;
    if (requestedTier == FiberStackTier::Large512KiB)
    {
        if (m_freeLargeFibers.tryPop(index) == JobQueueResult::Success && index < m_largeFiberCount)
        {
            return &m_largeFibers[index];
        }
        return nullptr;
    }

    if (m_freeStandardFibers.tryPop(index) == JobQueueResult::Success && index < m_standardFiberCount) [[likely]]
    {
        return &m_standardFibers[index];
    }

    // When all Standard64KiB fibers are suspended, borrow an idle Large512KiB fiber if one is free.
    if (m_freeLargeFibers.tryPop(index) == JobQueueResult::Success && index < m_largeFiberCount)
    {
        return &m_largeFibers[index];
    }

    return nullptr;
}

void JobSystem::releaseFiberSlot(FiberPoolSlot& fiberSlot) noexcept
{
    if (fiberSlot.tier == FiberStackTier::Large512KiB)
    {
        [[maybe_unused]] const JobQueueResult res = m_freeLargeFibers.tryPush(fiberSlot.poolIndex);
        GP_ASSERT(res == JobQueueResult::Success, "Failed to return Large512KiB fiber index to free-list.");
    }
    else
    {
        [[maybe_unused]] const JobQueueResult res = m_freeStandardFibers.tryPush(fiberSlot.poolIndex);
        GP_ASSERT(res == JobQueueResult::Success, "Failed to return Standard64KiB fiber index to free-list.");
    }
}

JobSystem::FiberPoolSlot* JobSystem::resolveFiberPoolSlot(const Fiber* const fiber) const noexcept
{
    if (fiber == nullptr)
    {
        return nullptr;
    }

    const FiberId id = fiber->getId();
    const FiberId prefix = id & detail::kPoolFiberIdPrefixMask;
    const gp::UInt16 index = static_cast<gp::UInt16>(id & detail::kPoolFiberIdIndexMask);

    if (prefix == detail::kStandardPoolFiberIdPrefix && index < m_standardFiberCount)
    {
        return &m_standardFibers[index];
    }

    if (prefix == detail::kLargePoolFiberIdPrefix && index < m_largeFiberCount)
    {
        return &m_largeFibers[index];
    }

    return nullptr;
}

void JobSystem::retireCompletedFiberOnAnchor(FiberPoolSlot& completedSlot) noexcept
{
    JobCounter* const completionCounter = completedSlot.activeDecl.counter;
    completedSlot.activeDecl = JobDecl{};

    // Return the fiber slot to the free-list before decrementing completionCounter so any waiter unblocked by the
    // counter observes the fiber back in the pool right away.
    releaseFiberSlot(completedSlot);

    if (completionCounter != nullptr)
    {
        (void)completionCounter->decrement(1u);
    }
}

void JobSystem::executeWorkItem(WorkerSlot& slot, Fiber& anchorFiber, const JobQueueItem& item) noexcept
{
    if (!item.isValid()) [[unlikely]]
    {
        return;
    }

    const JobPriority savedPriority = detail::getActiveJobPriorityTLS();
    const FiberStackTier savedTier = detail::getActiveJobStackTierTLS();

    if (item.isResumedFiber())
    {
        Fiber* const resumedFiber = item.getResumedFiber();
        FiberPoolSlot* const poolSlot = resolveFiberPoolSlot(resumedFiber);
        if (poolSlot != nullptr)
        {
            detail::setActiveJobMetaTLS(poolSlot->activeDecl.priority, poolSlot->tier);
        }

        gp::memory::PagedLinearAllocator* const savedArena = gp::memory::FiberScratchArena::getBoundFiberArena();
        detail::setCompletedFiberSlotTLS(nullptr);
        m_totalFiberSwitches.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
        [[maybe_unused]] const FiberResult switchRes = anchorFiber.switchTo(*resumedFiber);
        GP_ASSERT(switchRes == FiberResult::Success, "Worker anchor failed to switch into resumed child fiber.");
        if (savedArena != nullptr)
        {
            (void)gp::memory::FiberScratchArena::bindFiberArena(savedArena);
        }
        detail::setActiveJobMetaTLS(savedPriority, savedTier);

        FiberPoolSlot* const completedSlot = static_cast<FiberPoolSlot*>(detail::consumeCompletedFiberSlotTLS());
        if (completedSlot != nullptr)
        {
            retireCompletedFiberOnAnchor(*completedSlot);
        }
        return;
    }

    const JobDecl decl = item.toJobDecl();
    FiberPoolSlot* fiberSlot = acquireFiberSlot(decl.stackTier);

    // If all pooled fibers are suspended, drain a resumed fiber first if one is waiting to finish.
    if (fiberSlot == nullptr) [[unlikely]]
    {
        JobQueueItem resumedItem{};
        if (m_resumedFiberQueue.tryPop(resumedItem) == JobQueueResult::Success && resumedItem.isResumedFiber())
        {
            executeWorkItem(slot, anchorFiber, resumedItem);
            fiberSlot = acquireFiberSlot(decl.stackTier);
        }
    }

    if (fiberSlot != nullptr) [[likely]]
    {
        fiberSlot->activeDecl = decl;
        [[maybe_unused]] const FiberResult bindRes =
            fiberSlot->fiber.bindTask(&JobSystem::fiberTaskEntryPoint, fiberSlot, &anchorFiber);
        GP_ASSERT(bindRes == FiberResult::Success, "Failed to bind JobDecl to acquired pooled Fiber.");

        gp::memory::PagedLinearAllocator* const savedArena = gp::memory::FiberScratchArena::getBoundFiberArena();
        detail::setActiveJobMetaTLS(decl.priority, fiberSlot->tier);
        detail::setCompletedFiberSlotTLS(nullptr);
        m_totalFiberSwitches.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);

        [[maybe_unused]] const FiberResult switchRes = anchorFiber.switchTo(fiberSlot->fiber);
        GP_ASSERT(switchRes == FiberResult::Success, "Worker anchor failed to switch into newly bound child fiber.");
        if (savedArena != nullptr)
        {
            (void)gp::memory::FiberScratchArena::bindFiberArena(savedArena);
        }
        detail::setActiveJobMetaTLS(savedPriority, savedTier);

        FiberPoolSlot* const completedSlot = static_cast<FiberPoolSlot*>(detail::consumeCompletedFiberSlotTLS());
        if (completedSlot != nullptr)
        {
            retireCompletedFiberOnAnchor(*completedSlot);
        }
        return;
    }

    // Emergency pool-exhaustion fallback: execute on the worker's ThreadAnchor with its emergency scratchpad bound
    // inside a ScopedFiberScratchMark so recursive emergency tasks unwind cleanly without wiping outer allocations.
    detail::setActiveJobMetaTLS(decl.priority, decl.stackTier);
    gp::memory::PagedLinearAllocator* const prevArena =
        gp::memory::FiberScratchArena::bindFiberArena(&slot.emergencyScratchArena);
    ++slot.emergencyExecutionDepth;
    {
        gp::memory::ScopedFiberScratchMark emergencyMark;
        (void)decl.invoke();
    }
    --slot.emergencyExecutionDepth;
    if (slot.emergencyExecutionDepth == 0u)
    {
        slot.emergencyScratchArena.reset();
    }
    (void)gp::memory::FiberScratchArena::bindFiberArena(prevArena);
    detail::setActiveJobMetaTLS(savedPriority, savedTier);

    m_totalJobsExecuted.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
    if (decl.counter != nullptr)
    {
        (void)decl.counter->decrement(1u);
    }
}

void JobSystem::fiberTaskEntryPoint(void* const rawFiberPoolSlot) noexcept
{
    FiberPoolSlot* const fiberSlot = static_cast<FiberPoolSlot*>(rawFiberPoolSlot);
    GP_ASSERT(fiberSlot != nullptr && fiberSlot->owner != nullptr, "Invalid FiberPoolSlot in fiberTaskEntryPoint.");

    const JobDecl decl = fiberSlot->activeDecl;
    detail::setActiveJobMetaTLS(decl.priority, fiberSlot->tier);

    (void)decl.invoke();

    fiberSlot->owner->m_totalJobsExecuted.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);

    // Record this slot on the finishing worker thread's TLS right before Fiber::fiberLoopTrampoline switches back to
    // the worker's ThreadAnchor, so only that worker retires the slot after m_isStackActive is cleared.
    detail::setCompletedFiberSlotTLS(fiberSlot);
}

JobSyncResult JobSystem::onFiberYield(Fiber& currentFiber, [[maybe_unused]] void* const rawJobSystem) noexcept
{
    Fiber* const workerAnchor = detail::getWorkerAnchorFiberTLS();
    if (workerAnchor != nullptr && workerAnchor->isValid()) [[likely]]
    {
        return (currentFiber.switchTo(*workerAnchor) == FiberResult::Success) ? JobSyncResult::Success
                                                                              : JobSyncResult::YieldFailed;
    }

    return (currentFiber.yieldToReturnFiber() == FiberResult::Success) ? JobSyncResult::Success
                                                                       : JobSyncResult::YieldFailed;
}

void JobSystem::onFiberWake(Fiber* const waitingFiber, void* const rawJobSystem) noexcept
{
    JobSystem* const system = static_cast<JobSystem*>(rawJobSystem);
    if (system == nullptr || waitingFiber == nullptr) [[unlikely]]
    {
        return;
    }

    const JobQueueItem wakeItem = JobQueueItem::fromResumedFiber(waitingFiber, JobPriority::High);
    while (system->m_resumedFiberQueue.tryPush(wakeItem) != JobQueueResult::Success)
    {
        gp::platform::Thread::spinWait();
    }

    system->wakeSleepingWorkers(1u);
}

bool JobSystem::helpOneWorkItem() noexcept
{
    if (!isInitialized()) [[unlikely]]
    {
        return false;
    }

    // When called from inside a child fiber, pop a fresh JobEntry and run it inline inside a scratch mark so we
    // never switch ThreadAnchor contexts from inside a child fiber.
    if (Fiber::isExecutingInChildFiber())
    {
        const gp::UInt32 slotIdx =
            (detail::getActiveJobSystemTLS() == this && detail::getWorkerSlotIndexTLS() < m_totalWorkerSlotCount)
                ? detail::getWorkerSlotIndexTLS()
                : kMainThreadWorkerIndex;

        JobQueueItem item{};
        if (m_workerSlots[slotIdx].localDeque.tryPopBottom(item) == JobQueueResult::Success)
        {
            if (item.isResumedFiber())
            {
                (void)m_resumedFiberQueue.tryPush(item);
                wakeSleepingWorkers(1u);
                return false;
            }

            executeInlineJobItem(item.toJobDecl());
            return true;
        }
        return false;
    }

    const gp::UInt32 callerSlotIdx =
        (detail::getActiveJobSystemTLS() == this && detail::getWorkerSlotIndexTLS() < m_totalWorkerSlotCount)
            ? detail::getWorkerSlotIndexTLS()
            : kMainThreadWorkerIndex;
    WorkerSlot& slot = m_workerSlots[callerSlotIdx];

    JobQueueItem item{};
    if (!tryPopNextWorkItem(slot, item))
    {
        return false;
    }

    Fiber* const existingFiber = Fiber::getCurrentFiber();
    const bool alreadyThreadAnchor = (existingFiber != nullptr) && existingFiber->isThreadAnchor();

    Fiber temporaryAnchor{};
    Fiber* activeAnchor = existingFiber;
    if (!alreadyThreadAnchor)
    {
        if (temporaryAnchor.convertCurrentThread(
                detail::kWorkerAnchorFiberIdPrefix | callerSlotIdx, "GP_HelpingThreadAnchor"
            ) != FiberResult::Success) [[unlikely]]
        {
            if (item.isResumedFiber())
            {
                (void)m_resumedFiberQueue.tryPush(item);
                wakeSleepingWorkers(1u);
                return false;
            }
            executeInlineJobItem(item.toJobDecl());
            return true;
        }
        activeAnchor = &temporaryAnchor;
    }

    JobSystem* const prevSystem = detail::getActiveJobSystemTLS();
    const gp::UInt32 prevSlot = detail::getWorkerSlotIndexTLS();
    Fiber* const prevAnchor = detail::getWorkerAnchorFiberTLS();
    detail::bindWorkerThreadTLS(this, callerSlotIdx, activeAnchor);

    JobFiberParker parker{};
    parker.yieldCallback = &JobSystem::onFiberYield;
    parker.wakeCallback = &JobSystem::onFiberWake;
    parker.userData = this;
    ScopedJobFiberParker parkerGuard(parker);

    executeWorkItem(slot, *activeAnchor, item);

    detail::bindWorkerThreadTLS(prevSystem, prevSlot, prevAnchor);
    if (!alreadyThreadAnchor && temporaryAnchor.isValid())
    {
        (void)temporaryAnchor.restoreCurrentThread();
    }

    return true;
}

JobSystemResult JobSystem::waitForCounter(JobCounter& counter, const JobCounterValue targetCount) noexcept
{
    if (counter.isAtOrBelow(targetCount, gp::MemoryOrder::Acquire))
    {
        return JobSystemResult::Success;
    }

    if (!isInitialized()) [[unlikely]]
    {
        return (counter.wait(targetCount, JobWaitMode::SpinOnly) == JobSyncResult::Success)
                   ? JobSystemResult::Success
                   : JobSystemResult::SyncError;
    }

    // When called from inside a pooled child Fiber, park on the counter's stack-resident wait list and switch back to
    // the worker thread's scheduler loop.
    if (Fiber::isExecutingInChildFiber() && detail::getActiveJobSystemTLS() == this)
    {
        return (counter.wait(targetCount, JobWaitMode::Auto) == JobSyncResult::Success) ? JobSystemResult::Success
                                                                                        : JobSystemResult::SyncError;
    }

    // External thread or ThreadAnchor path: convert the calling thread into a ThreadAnchor once for the wait loop and
    // help drain ready work items until the counter reaches targetCount.
    const gp::UInt32 callerSlotIdx =
        (detail::getActiveJobSystemTLS() == this && detail::getWorkerSlotIndexTLS() < m_totalWorkerSlotCount)
            ? detail::getWorkerSlotIndexTLS()
            : kMainThreadWorkerIndex;
    WorkerSlot& slot = m_workerSlots[callerSlotIdx];

    Fiber* const existingFiber = Fiber::getCurrentFiber();
    const bool alreadyThreadAnchor = (existingFiber != nullptr) && existingFiber->isThreadAnchor();

    Fiber temporaryAnchor{};
    Fiber* activeAnchor = existingFiber;
    if (!alreadyThreadAnchor)
    {
        if (temporaryAnchor.convertCurrentThread(
                detail::kWorkerAnchorFiberIdPrefix | callerSlotIdx, "GP_WaitAndHelpThreadAnchor"
            ) == FiberResult::Success)
        {
            activeAnchor = &temporaryAnchor;
        }
    }

    JobSystem* const prevSystem = detail::getActiveJobSystemTLS();
    const gp::UInt32 prevSlot = detail::getWorkerSlotIndexTLS();
    Fiber* const prevAnchor = detail::getWorkerAnchorFiberTLS();
    detail::bindWorkerThreadTLS(this, callerSlotIdx, activeAnchor);

    JobFiberParker parker{};
    parker.yieldCallback = &JobSystem::onFiberYield;
    parker.wakeCallback = &JobSystem::onFiberWake;
    parker.userData = this;
    ScopedJobFiberParker parkerGuard(parker);

    while (!counter.isAtOrBelow(targetCount, gp::MemoryOrder::Acquire))
    {
        JobQueueItem item{};
        if (tryPopNextWorkItem(slot, item))
        {
            if (activeAnchor != nullptr) [[likely]]
            {
                executeWorkItem(slot, *activeAnchor, item);
            }
            else if (item.isResumedFiber())
            {
                (void)m_resumedFiberQueue.tryPush(item);
                wakeSleepingWorkers(1u);
                gp::platform::Thread::spinWait();
            }
            else
            {
                executeInlineJobItem(item.toJobDecl());
            }
        }
        else
        {
            gp::platform::Thread::spinWait();
        }
    }

    detail::bindWorkerThreadTLS(prevSystem, prevSlot, prevAnchor);
    if (!alreadyThreadAnchor && temporaryAnchor.isValid())
    {
        (void)temporaryAnchor.restoreCurrentThread();
    }

    return JobSystemResult::Success;
}

JobSystemResult JobSystem::waitForFence(JobFence& fence) noexcept
{
    if (fence.isSignaled(gp::MemoryOrder::Acquire))
    {
        return JobSystemResult::Success;
    }

    if (!isInitialized()) [[unlikely]]
    {
        return (fence.wait(JobWaitMode::SpinOnly) == JobSyncResult::Success) ? JobSystemResult::Success
                                                                             : JobSystemResult::SyncError;
    }

    if (Fiber::isExecutingInChildFiber() && detail::getActiveJobSystemTLS() == this)
    {
        return (fence.wait(JobWaitMode::Auto) == JobSyncResult::Success) ? JobSystemResult::Success
                                                                         : JobSystemResult::SyncError;
    }

    const gp::UInt32 callerSlotIdx =
        (detail::getActiveJobSystemTLS() == this && detail::getWorkerSlotIndexTLS() < m_totalWorkerSlotCount)
            ? detail::getWorkerSlotIndexTLS()
            : kMainThreadWorkerIndex;
    WorkerSlot& slot = m_workerSlots[callerSlotIdx];

    Fiber* const existingFiber = Fiber::getCurrentFiber();
    const bool alreadyThreadAnchor = (existingFiber != nullptr) && existingFiber->isThreadAnchor();

    Fiber temporaryAnchor{};
    Fiber* activeAnchor = existingFiber;
    if (!alreadyThreadAnchor)
    {
        if (temporaryAnchor.convertCurrentThread(
                detail::kWorkerAnchorFiberIdPrefix | callerSlotIdx, "GP_FenceWaitThreadAnchor"
            ) == FiberResult::Success)
        {
            activeAnchor = &temporaryAnchor;
        }
    }

    JobSystem* const prevSystem = detail::getActiveJobSystemTLS();
    const gp::UInt32 prevSlot = detail::getWorkerSlotIndexTLS();
    Fiber* const prevAnchor = detail::getWorkerAnchorFiberTLS();
    detail::bindWorkerThreadTLS(this, callerSlotIdx, activeAnchor);

    JobFiberParker parker{};
    parker.yieldCallback = &JobSystem::onFiberYield;
    parker.wakeCallback = &JobSystem::onFiberWake;
    parker.userData = this;
    ScopedJobFiberParker parkerGuard(parker);

    while (!fence.isSignaled(gp::MemoryOrder::Acquire))
    {
        JobQueueItem item{};
        if (tryPopNextWorkItem(slot, item))
        {
            if (activeAnchor != nullptr) [[likely]]
            {
                executeWorkItem(slot, *activeAnchor, item);
            }
            else if (item.isResumedFiber())
            {
                (void)m_resumedFiberQueue.tryPush(item);
                wakeSleepingWorkers(1u);
                gp::platform::Thread::spinWait();
            }
            else
            {
                executeInlineJobItem(item.toJobDecl());
            }
        }
        else
        {
            gp::platform::Thread::spinWait();
        }
    }

    detail::bindWorkerThreadTLS(prevSystem, prevSlot, prevAnchor);
    if (!alreadyThreadAnchor && temporaryAnchor.isValid())
    {
        (void)temporaryAnchor.restoreCurrentThread();
    }

    return JobSystemResult::Success;
}

gp::UInt32 JobSystem::workerThreadEntryPoint(void* const rawWorkerSlot) noexcept
{
    WorkerSlot* const slot = static_cast<WorkerSlot*>(rawWorkerSlot);
    if (slot == nullptr || slot->owner == nullptr) [[unlikely]]
    {
        return 1u;
    }

    slot->owner->workerThreadLoop(*slot);
    return 0u;
}

void JobSystem::workerThreadLoop(WorkerSlot& slot) noexcept
{
    [[maybe_unused]] const FiberResult anchorRes =
        slot.anchorFiber.convertCurrentThread(detail::kWorkerAnchorFiberIdPrefix | slot.workerIndex, slot.debugName);
    GP_ASSERT(anchorRes == FiberResult::Success, "Background worker thread failed to convert into ThreadAnchor fiber.");

    detail::bindWorkerThreadTLS(this, slot.workerIndex, &slot.anchorFiber);

    JobFiberParker parker{};
    parker.yieldCallback = &JobSystem::onFiberYield;
    parker.wakeCallback = &JobSystem::onFiberWake;
    parker.userData = this;
    ScopedJobFiberParker parkerGuard(parker);

    slot.threadStarted.store(1u, gp::MemoryOrder::Release);

    const gp::UInt64 workerBit = (1ULL << slot.workerIndex);
    gp::UInt32 idleIterations = 0u;
    const gp::UInt32 spinLimit = m_spinIterationsBeforeYield;
    const gp::UInt32 yieldLimit = spinLimit + m_yieldIterationsBeforeSleep;

    while (m_shouldShutdown.load(gp::MemoryOrder::Acquire) == 0u)
    {
        JobQueueItem item{};
        if (tryPopNextWorkItem(slot, item))
        {
            idleIterations = 0u;
            executeWorkItem(slot, slot.anchorFiber, item);
            continue;
        }

        ++idleIterations;
        if (idleIterations <= spinLimit)
        {
            gp::platform::Thread::spinWait();
        }
        else if (idleIterations <= yieldLimit)
        {
            gp::platform::Thread::yieldExecution();
        }
        else
        {
            const gp::UInt64 observedEpoch = m_workAvailableEpoch.load(gp::MemoryOrder::Acquire);
            (void)m_sleepingWorkerMask.fetchOr(workerBit, gp::MemoryOrder::AcqRel);
            atomicThreadFence(gp::MemoryOrder::SeqCst);

            if (m_shouldShutdown.load(gp::MemoryOrder::Acquire) != 0u ||
                m_workAvailableEpoch.load(gp::MemoryOrder::Acquire) != observedEpoch || hasAnyPendingWorkApprox())
            {
                (void)m_sleepingWorkerMask.fetchAnd(~workerBit, gp::MemoryOrder::Release);
                idleIterations = 0u;
                continue;
            }

            if ((m_sleepingWorkerMask.load(gp::MemoryOrder::Acquire) & workerBit) != 0ULL)
            {
                gp::platform::Time::sleepNanoseconds(m_idleSleepNanoseconds);
            }

            (void)m_sleepingWorkerMask.fetchAnd(~workerBit, gp::MemoryOrder::Release);
        }
    }

    (void)m_sleepingWorkerMask.fetchAnd(~workerBit, gp::MemoryOrder::Release);
    detail::bindWorkerThreadTLS(nullptr, kInvalidWorkerIndex, nullptr);
    (void)slot.anchorFiber.restoreCurrentThread();
}

JobSystemStats JobSystem::getStats() const noexcept
{
    JobSystemStats stats{};
    stats.totalJobsDispatched = m_totalJobsDispatched.load(gp::MemoryOrder::Relaxed);
    stats.totalJobsExecuted = m_totalJobsExecuted.load(gp::MemoryOrder::Relaxed);
    stats.totalJobsStolen = m_totalJobsStolen.load(gp::MemoryOrder::Relaxed);
    stats.totalFiberSwitches = m_totalFiberSwitches.load(gp::MemoryOrder::Relaxed);
    stats.workerThreadCount = m_workerThreadCount;
    stats.totalWorkerSlotCount = m_totalWorkerSlotCount;
    stats.availableStandardFibers = static_cast<gp::UInt32>(m_freeStandardFibers.sizeApprox());
    stats.availableLargeFibers = static_cast<gp::UInt32>(m_freeLargeFibers.sizeApprox());

    gp::UInt64 mask = m_sleepingWorkerMask.load(gp::MemoryOrder::Relaxed);
    gp::UInt32 sleeping = 0u;
    while (mask != 0ULL)
    {
        mask &= (mask - 1ULL);
        ++sleeping;
    }
    stats.sleepingWorkerCount = sleeping;
    return stats;
}

gp::UInt32 JobSystem::getCurrentWorkerIndex() noexcept
{
    const gp::UInt32 slotIdx = detail::getWorkerSlotIndexTLS();
    return (slotIdx == kInvalidWorkerIndex) ? kMainThreadWorkerIndex : slotIdx;
}

JobSystem* JobSystem::getCurrentJobSystem() noexcept
{
    return detail::getActiveJobSystemTLS();
}

JobContext JobSystem::getCurrentJobContext() noexcept
{
    JobContext ctx{};
    ctx.jobSystem = detail::getActiveJobSystemTLS();
    const gp::UInt32 rawSlot = detail::getWorkerSlotIndexTLS();
    ctx.workerIndex = (rawSlot == kInvalidWorkerIndex) ? kMainThreadWorkerIndex : rawSlot;
    ctx.fiberId = Fiber::getCurrentFiberId();
    ctx.priority = detail::getActiveJobPriorityTLS();
    ctx.stackTier = detail::getActiveJobStackTierTLS();
    return ctx;
}

bool JobSystem::isWorkerThread() noexcept
{
    const gp::UInt32 rawSlot = detail::getWorkerSlotIndexTLS();
    return (rawSlot != kInvalidWorkerIndex) && (rawSlot > kMainThreadWorkerIndex);
}

}   // namespace gp::thread
