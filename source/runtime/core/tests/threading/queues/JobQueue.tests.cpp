// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "concepts/Memory.hpp"   // IWYU pragma: keep
#include "memory/allocators/FiberScratchAllocator.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/Fiber.hpp"
#include "threading/queues/JobQueueDesc.hpp"
#include "threading/queues/JobQueueEnums.hpp"
#include "threading/queues/JobQueueForward.hpp"
#include "threading/queues/JobQueueTypes.hpp"   // IWYU pragma: keep
#include "threading/queues/MPMCRingQueue.hpp"
#include "threading/queues/WorkStealingDeque.hpp"
#include "threading/sync/JobCounter.hpp"
#include "threading/sync/JobFence.hpp"
#include "threading/sync/JobWaitNode.hpp"
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace
{

template <typename InType>
concept CanInstantiateWorkStealingDeque = requires { typename WorkStealingDeque<InType>; };

template <typename InType>
concept CanInstantiateMPMCRingQueue = requires { typename MPMCRingQueue<InType>; };

struct NonTrivialQueuePayload
{
    gp::UInt32 value{ 0u };

    NonTrivialQueuePayload() noexcept = default;

    NonTrivialQueuePayload(const NonTrivialQueuePayload& other) noexcept
        : value(other.value)
    {}

    ~NonTrivialQueuePayload() noexcept
    {}
};

static_assert(JobQueueElement<gp::UInt32>, "JobQueueElement must accept UInt32.");
static_assert(JobQueueElement<JobDecl>, "JobQueueElement must accept JobDecl.");
static_assert(JobQueueElement<JobQueueItem>, "JobQueueElement must accept JobQueueItem.");
static_assert(!JobQueueElement<const gp::UInt32>, "JobQueueElement must reject const types.");
static_assert(!JobQueueElement<NonTrivialQueuePayload>, "JobQueueElement must reject non-trivially-copyable types.");

static_assert(CanInstantiateWorkStealingDeque<gp::UInt32>, "WorkStealingDeque must accept UInt32.");
static_assert(CanInstantiateWorkStealingDeque<JobDecl>, "WorkStealingDeque must accept JobDecl.");
static_assert(CanInstantiateWorkStealingDeque<JobQueueItem>, "WorkStealingDeque must accept JobQueueItem.");
static_assert(!CanInstantiateWorkStealingDeque<const gp::UInt32>, "WorkStealingDeque must reject const types.");
static_assert(
    !CanInstantiateWorkStealingDeque<NonTrivialQueuePayload>,
    "WorkStealingDeque must reject non-trivially-copyable types."
);

static_assert(CanInstantiateMPMCRingQueue<gp::UInt32>, "MPMCRingQueue must accept UInt32.");
static_assert(CanInstantiateMPMCRingQueue<JobDecl>, "MPMCRingQueue must accept JobDecl.");
static_assert(CanInstantiateMPMCRingQueue<JobQueueItem>, "MPMCRingQueue must accept JobQueueItem.");
static_assert(!CanInstantiateMPMCRingQueue<const gp::UInt32>, "MPMCRingQueue must reject const types.");
static_assert(
    !CanInstantiateMPMCRingQueue<NonTrivialQueuePayload>, "MPMCRingQueue must reject non-trivially-copyable types."
);

static void incrementCounterJobEntry(void* const userData) noexcept
{
    gp::UInt32* const target = static_cast<gp::UInt32*>(userData);
    if (target != nullptr)
    {
        ++(*target);
    }
}

constexpr void dummyConstexprJobEntry(void* const userData) noexcept
{
    (void)userData;
}

constexpr bool testConstexprWorkStealingDeque() noexcept
{
    gp::UInt32 backing[4]{};
    WorkStealingDeque<gp::UInt32> deque(backing, 4u);
    if (!deque.isValid() || deque.capacity() != 4u || !deque.isEmptyApprox() || deque.isFullApprox())
    {
        return false;
    }

    const gp::UInt32 batch[3] = { 10u, 20u, 30u };
    if (deque.tryPushBottomRange(batch, 3u) != JobQueueResult::Success)
    {
        return false;
    }

    gp::UInt32 stolen = 0u;
    if (deque.tryStealTop(stolen) != JobQueueResult::Success || stolen != 10u)
    {
        return false;
    }

    gp::UInt32 popped = 0u;
    if (deque.tryPopBottom(popped) != JobQueueResult::Success || popped != 30u)
    {
        return false;
    }

    if (deque.tryPopBottom(popped) != JobQueueResult::Success || popped != 20u)
    {
        return false;
    }

    return deque.tryPopBottom(popped) == JobQueueResult::Empty && deque.tryStealTop(stolen) == JobQueueResult::Empty;
}

static_assert(testConstexprWorkStealingDeque(), "Constexpr WorkStealingDeque evaluation failed.");

constexpr bool testConstexprMPMCRingQueue() noexcept
{
    using QueueType = MPMCRingQueue<JobQueueItem>;
    QueueType::SlotType slots[4]{};
    QueueType queue(slots, 4u);
    if (!queue.isValid() || queue.capacity() != 4u || !queue.isEmptyApprox() || queue.isFullApprox())
    {
        return false;
    }

    const JobDecl decl = JobDecl::make(&dummyConstexprJobEntry, nullptr, nullptr, JobPriority::High);
    const JobQueueItem inItem = JobQueueItem::fromJob(decl);

    if (queue.tryPush(inItem) != JobQueueResult::Success)
    {
        return false;
    }

    JobQueueItem outItem{};
    if (queue.tryPop(outItem) != JobQueueResult::Success)
    {
        return false;
    }

    return outItem.isJobEntry() && outItem.isValid() && outItem.entry == &dummyConstexprJobEntry &&
           outItem.priority == JobPriority::High && queue.isEmptyApprox();
}

static_assert(testConstexprMPMCRingQueue(), "Constexpr MPMCRingQueue evaluation failed.");

struct SpmcWorkStealingStressContext
{
    static constexpr gp::UInt32 kThiefCount = 3u;
    static constexpr gp::UInt32 kTotalItems = 16384u;
    static constexpr gp::USize kDequeCapacity = 256u;

    WorkStealingDeque<gp::UInt32> deque{};
    Atomic<gp::UInt32> readyThreads{ 0u };
    JobFence startFence{ false };
    Atomic<gp::UInt8> producerDone{ 0u };
    Atomic<gp::UInt32> totalConsumed{ 0u };
    Atomic<gp::UInt32> ownerPoppedCount{ 0u };
    Atomic<gp::UInt32> thiefStolenCount{ 0u };
    Atomic<gp::UInt32> contendedStealCount{ 0u };
    Atomic<gp::UInt8> visited[kTotalItems]{};
};

static gp::UInt32 spmcThiefThreadEntry(void* const rawData) noexcept
{
    SpmcWorkStealingStressContext* const ctx = static_cast<SpmcWorkStealingStressContext*>(rawData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::SpinOnly);

    while (ctx->totalConsumed.load(gp::MemoryOrder::Acquire) < SpmcWorkStealingStressContext::kTotalItems)
    {
        gp::UInt32 item = 0u;
        const JobQueueResult res = ctx->deque.tryStealTop(item);
        if (res == JobQueueResult::Success)
        {
            if (item < SpmcWorkStealingStressContext::kTotalItems)
            {
                ctx->visited[item].fetchAdd(1u, gp::MemoryOrder::AcqRel);
            }
            ctx->thiefStolenCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
            ctx->totalConsumed.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        }
        else if (res == JobQueueResult::Contended)
        {
            ctx->contendedStealCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
            gp::platform::Thread::spinWait();
        }
        else
        {
            if (ctx->producerDone.load(gp::MemoryOrder::Acquire) != 0u && ctx->deque.isEmptyApprox())
            {
                break;
            }
            gp::platform::Thread::spinWait();
        }
    }

    return 0u;
}

struct MpmcContendedRingStressContext
{
    static constexpr gp::UInt32 kProducerCount = 4u;
    static constexpr gp::UInt32 kConsumerCount = 4u;
    static constexpr gp::UInt32 kItemsPerProducer = 4096u;
    static constexpr gp::UInt32 kTotalItems = kProducerCount * kItemsPerProducer;
    static constexpr gp::USize kQueueCapacity = 64u;

    MPMCRingQueue<gp::UInt32> queue{};
    Atomic<gp::UInt32> nextProducerIndex{ 0u };
    Atomic<gp::UInt32> readyThreads{ 0u };
    JobFence startFence{ false };
    Atomic<gp::UInt32> totalDequeued{ 0u };
    Atomic<gp::UInt8> visited[kTotalItems]{};
};

static gp::UInt32 mpmcProducerThreadEntry(void* const rawData) noexcept
{
    MpmcContendedRingStressContext* const ctx = static_cast<MpmcContendedRingStressContext*>(rawData);
    const gp::UInt32 producerId = ctx->nextProducerIndex.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    const gp::UInt32 baseItem = producerId * MpmcContendedRingStressContext::kItemsPerProducer;

    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::SpinOnly);

    for (gp::UInt32 i = 0u; i < MpmcContendedRingStressContext::kItemsPerProducer; ++i)
    {
        const gp::UInt32 itemValue = baseItem + i;
        while (ctx->queue.tryPush(itemValue) != JobQueueResult::Success)
        {
            gp::platform::Thread::spinWait();
        }
    }

    return 0u;
}

static gp::UInt32 mpmcConsumerThreadEntry(void* const rawData) noexcept
{
    MpmcContendedRingStressContext* const ctx = static_cast<MpmcContendedRingStressContext*>(rawData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::SpinOnly);

    while (ctx->totalDequeued.load(gp::MemoryOrder::Acquire) < MpmcContendedRingStressContext::kTotalItems)
    {
        gp::UInt32 itemValue = 0u;
        if (ctx->queue.tryPop(itemValue) == JobQueueResult::Success)
        {
            if (itemValue < MpmcContendedRingStressContext::kTotalItems)
            {
                ctx->visited[itemValue].fetchAdd(1u, gp::MemoryOrder::AcqRel);
            }
            ctx->totalDequeued.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        }
        else
        {
            gp::platform::Thread::spinWait();
        }
    }

    return 0u;
}

struct Layer2FiberWorkStealingIntegrationContext
{
    static constexpr gp::UInt32 kChildJobCount = 16u;

    WorkStealingDeque<JobQueueItem> ownerDeque{};
    MPMCRingQueue<JobQueueItem> readyFiberQueue{};
    JobCounter childCounter{ kChildJobCount };
    Fiber* ownerAnchorFiber{ nullptr };
    Fiber parentFiber{};
    Atomic<gp::UInt32> childAccumulator{ 0u };
    Atomic<gp::UInt32> stolenChildCount{ 0u };
    Atomic<gp::UInt32> localChildCount{ 0u };
    Atomic<gp::UInt8> thiefReady{ 0u };
    Atomic<gp::UInt8> parentCompleted{ 0u };
    bool parentScratchCanaryIntact{ false };
};

struct ChildTaskPayload
{
    Layer2FiberWorkStealingIntegrationContext* ctx{ nullptr };
    gp::UInt32 value{ 0u };
};

static void childWorkStealingJobEntry(void* const rawData) noexcept
{
    const ChildTaskPayload* const payload = static_cast<const ChildTaskPayload*>(rawData);
    payload->ctx->childAccumulator.fetchAdd(payload->value, gp::MemoryOrder::AcqRel);
    (void)payload->ctx->childCounter.decrement(1u);
}

static JobSyncResult integrationYieldCallback(Fiber& currentFiber, void* const rawData) noexcept
{
    Layer2FiberWorkStealingIntegrationContext* const ctx =
        static_cast<Layer2FiberWorkStealingIntegrationContext*>(rawData);
    return (currentFiber.switchTo(*ctx->ownerAnchorFiber) == FiberResult::Success) ? JobSyncResult::Success
                                                                                   : JobSyncResult::YieldFailed;
}

static void integrationWakeCallback(Fiber* const waitingFiber, void* const rawData) noexcept
{
    Layer2FiberWorkStealingIntegrationContext* const ctx =
        static_cast<Layer2FiberWorkStealingIntegrationContext*>(rawData);
    const JobQueueItem wakeItem = JobQueueItem::fromResumedFiber(waitingFiber, JobPriority::High);
    while (ctx->readyFiberQueue.tryPush(wakeItem) != JobQueueResult::Success)
    {
        gp::platform::Thread::spinWait();
    }
}

static void parentFiberSpawningEntry(void* const rawData) noexcept
{
    Layer2FiberWorkStealingIntegrationContext* const ctx =
        static_cast<Layer2FiberWorkStealingIntegrationContext*>(rawData);

    gp::memory::ScopedFiberScratchMark scratchScope;
    gp::memory::FiberScratchAllocator scratchAlloc;
    gp::UInt64* const canary = static_cast<gp::UInt64*>(scratchAlloc.allocate(sizeof(gp::UInt64), alignof(gp::UInt64)));
    ASSERT_NE(canary, nullptr);
    *canary = 0xA5A55A5AF00DCAFEULL;

    ChildTaskPayload payloads[Layer2FiberWorkStealingIntegrationContext::kChildJobCount]{};
    JobQueueItem batchItems[Layer2FiberWorkStealingIntegrationContext::kChildJobCount]{};
    for (gp::UInt32 i = 0u; i < Layer2FiberWorkStealingIntegrationContext::kChildJobCount; ++i)
    {
        payloads[i].ctx = ctx;
        payloads[i].value = i + 1u;

        const JobDecl decl = JobDecl::make(
            &childWorkStealingJobEntry,
            &payloads[i],
            &ctx->childCounter,
            JobPriority::Normal,
            FiberStackTier::Standard64KiB
        );
        batchItems[i] = JobQueueItem::fromJob(decl);
    }

    ASSERT_EQ(
        ctx->ownerDeque.tryPushBottomRange(batchItems, Layer2FiberWorkStealingIntegrationContext::kChildJobCount),
        JobQueueResult::Success
    );

    // Yield on childCounter until all 16 child jobs are executed by the owner and thief threads!
    EXPECT_EQ(ctx->childCounter.wait(0u, JobWaitMode::Auto), JobSyncResult::Success);

    ctx->parentScratchCanaryIntact = (*canary == 0xA5A55A5AF00DCAFEULL);
    ctx->parentCompleted.store(1u, gp::MemoryOrder::Release);
}

static gp::UInt32 integrationThiefWorkerEntry(void* const rawData) noexcept
{
    Layer2FiberWorkStealingIntegrationContext* const ctx =
        static_cast<Layer2FiberWorkStealingIntegrationContext*>(rawData);
    ctx->thiefReady.store(1u, gp::MemoryOrder::Release);

    while (ctx->parentCompleted.load(gp::MemoryOrder::Acquire) == 0u)
    {
        JobQueueItem stolenItem{};
        const JobQueueResult res = ctx->ownerDeque.tryStealTop(stolenItem);
        if (res == JobQueueResult::Success && stolenItem.isJobEntry() && stolenItem.isValid())
        {
            ctx->stolenChildCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
            stolenItem.entry(stolenItem.userData);
        }
        else
        {
            gp::platform::Thread::spinWait();
        }
    }

    return 0u;
}

}   // namespace

TEST(JobQueueDescTest, JobDeclAndJobQueueItemLayoutAndRoundTripSemantics)
{
    JobDecl emptyDecl{};
    EXPECT_FALSE(emptyDecl.isValid());
    EXPECT_FALSE(emptyDecl.invoke());
    EXPECT_EQ(emptyDecl.priority, JobPriority::Normal);
    EXPECT_EQ(emptyDecl.stackTier, FiberStackTier::Standard64KiB);

    JobCounter dummyCounter(4u);
    gp::UInt32 payloadValue = 99u;

    const JobDecl populated = JobDecl::make(
        &incrementCounterJobEntry, &payloadValue, &dummyCounter, JobPriority::High, FiberStackTier::Large512KiB, 0x1234u
    );
    EXPECT_TRUE(populated.isValid());
    EXPECT_EQ(populated.entry, &incrementCounterJobEntry);
    EXPECT_EQ(populated.userData, &payloadValue);
    EXPECT_EQ(populated.counter, &dummyCounter);
    EXPECT_EQ(populated.priority, JobPriority::High);
    EXPECT_EQ(populated.stackTier, FiberStackTier::Large512KiB);
    EXPECT_EQ(populated.flags, 0x1234u);
    EXPECT_TRUE(populated.invoke());
    EXPECT_EQ(payloadValue, 100u);

    const JobQueueItem jobItem = JobQueueItem::fromJob(populated);
    EXPECT_TRUE(jobItem.isJobEntry());
    EXPECT_FALSE(jobItem.isResumedFiber());
    EXPECT_TRUE(jobItem.isValid());
    EXPECT_EQ(jobItem.getResumedFiber(), nullptr);
    EXPECT_EQ(jobItem.getJobCounter(), &dummyCounter);

    const JobDecl roundTrip = jobItem.toJobDecl();
    EXPECT_EQ(roundTrip.entry, populated.entry);
    EXPECT_EQ(roundTrip.userData, populated.userData);
    EXPECT_EQ(roundTrip.counter, populated.counter);
    EXPECT_EQ(roundTrip.priority, populated.priority);
    EXPECT_EQ(roundTrip.stackTier, populated.stackTier);
    EXPECT_EQ(roundTrip.flags, populated.flags);

    Fiber dummyFiber{};
    const JobQueueItem fiberItem = JobQueueItem::fromResumedFiber(&dummyFiber, JobPriority::High);
    EXPECT_FALSE(fiberItem.isJobEntry());
    EXPECT_TRUE(fiberItem.isResumedFiber());
    EXPECT_TRUE(fiberItem.isValid());
    EXPECT_EQ(fiberItem.getResumedFiber(), &dummyFiber);
    EXPECT_EQ(fiberItem.getJobCounter(), nullptr);
    EXPECT_EQ(fiberItem.toJobDecl().counter, nullptr);

    const JobQueueItem invalidFiberItem = JobQueueItem::fromResumedFiber(nullptr);
    EXPECT_FALSE(invalidFiberItem.isValid());
}

TEST(WorkStealingDequeTest, LifecycleValidationAndPowerOfTwoRounding)
{
    WorkStealingDeque<gp::UInt32> deque;
    EXPECT_FALSE(deque.isValid());
    EXPECT_EQ(deque.capacity(), 0u);
    EXPECT_TRUE(deque.isEmptyApprox());
    EXPECT_FALSE(deque.isFullApprox());

    gp::UInt32 dummy = 0u;
    gp::USize stolenCount = 0u;
    EXPECT_EQ(deque.tryPushBottom(1u), JobQueueResult::NotInitialized);
    EXPECT_EQ(deque.tryPushBottomRange(&dummy, 1u), JobQueueResult::NotInitialized);
    EXPECT_EQ(deque.tryPopBottom(dummy), JobQueueResult::NotInitialized);
    EXPECT_EQ(deque.tryStealTop(dummy), JobQueueResult::NotInitialized);
    EXPECT_EQ(deque.tryStealTopBatch(&dummy, 1u, stolenCount), JobQueueResult::NotInitialized);

    EXPECT_EQ(deque.initialize(0u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(deque.initialize(WorkStealingDeque<gp::UInt32>::kMaxCapacity + 1u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(deque.initialize(~static_cast<gp::USize>(0u)), JobQueueResult::InvalidArguments);

    EXPECT_EQ(deque.initialize(1u), JobQueueResult::Success);
    EXPECT_TRUE(deque.isValid());
    EXPECT_EQ(deque.capacity(), 2u);

    EXPECT_EQ(deque.initialize(5u), JobQueueResult::Success);
    EXPECT_EQ(deque.capacity(), 8u);

    alignas(16) gp::UInt8 rawExternal[sizeof(gp::UInt32) * 8u]{};
    gp::UInt32* const alignedExternal = reinterpret_cast<gp::UInt32*>(rawExternal);

    EXPECT_EQ(deque.initializeWithExternalBuffer(nullptr, 4u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(deque.initializeWithExternalBuffer(alignedExternal, 1u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(deque.initializeWithExternalBuffer(alignedExternal, 6u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(
        deque.initializeWithExternalBuffer(alignedExternal, WorkStealingDeque<gp::UInt32>::kMaxCapacity * 2u),
        JobQueueResult::InvalidArguments
    );

    gp::UInt32* const misalignedExternal = reinterpret_cast<gp::UInt32*>(rawExternal + 1u);
    EXPECT_EQ(deque.initializeWithExternalBuffer(misalignedExternal, 4u), JobQueueResult::InvalidArguments);

    EXPECT_EQ(deque.initializeWithExternalBuffer(alignedExternal, 4u), JobQueueResult::Success);
    EXPECT_TRUE(deque.isValid());
    EXPECT_EQ(deque.capacity(), 4u);

    ASSERT_EQ(deque.tryPushBottom(77u), JobQueueResult::Success);
    WorkStealingDeque<gp::UInt32> movedDeque(std::move(deque));
    EXPECT_FALSE(deque.isValid());
    EXPECT_TRUE(movedDeque.isValid());
    EXPECT_EQ(movedDeque.sizeApprox(), 1u);

    ASSERT_EQ(movedDeque.tryPopBottom(dummy), JobQueueResult::Success);
    EXPECT_EQ(dummy, 77u);
}

TEST(WorkStealingDequeTest, LifoOwnerPopFifoThiefStealBatchAndMultiLapWrapAround)
{
    WorkStealingDeque<gp::UInt32> deque;
    ASSERT_EQ(deque.initialize(8u), JobQueueResult::Success);
    ASSERT_EQ(deque.capacity(), 8u);

    // Validate batch push edge cases
    EXPECT_EQ(deque.tryPushBottomRange(nullptr, 0u), JobQueueResult::Success);
    EXPECT_EQ(deque.tryPushBottomRange(nullptr, 2u), JobQueueResult::InvalidArguments);

    const gp::UInt32 oversizedBatch[9] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u };
    EXPECT_EQ(deque.tryPushBottomRange(oversizedBatch, 9u), JobQueueResult::Full);
    EXPECT_TRUE(deque.isEmptyApprox());

    // Push all 8 slots via tryPushBottomRange: [10, 20, 30, 40, 50, 60, 70, 80]
    const gp::UInt32 batch8[8] = { 10u, 20u, 30u, 40u, 50u, 60u, 70u, 80u };
    ASSERT_EQ(deque.tryPushBottomRange(batch8, 8u), JobQueueResult::Success);
    EXPECT_EQ(deque.sizeApprox(), 8u);
    EXPECT_TRUE(deque.isFullApprox());

    // Any additional single or batch push on a full deque must return Full without modifying cursors
    EXPECT_EQ(deque.tryPushBottom(90u), JobQueueResult::Full);
    EXPECT_EQ(deque.tryPushBottomRange(batch8, 1u), JobQueueResult::Full);

    // Thief batch steal validation: requesting up to 8 items steals half of the 8 available items (4 items: 10..40)
    gp::UInt32 stolenBatch[8]{};
    gp::USize stolenCount = 0u;
    EXPECT_EQ(deque.tryStealTopBatch(nullptr, 4u, stolenCount), JobQueueResult::InvalidArguments);
    EXPECT_EQ(deque.tryStealTopBatch(stolenBatch, 0u, stolenCount), JobQueueResult::InvalidArguments);

    ASSERT_EQ(deque.tryStealTopBatch(stolenBatch, 8u, stolenCount), JobQueueResult::Success);
    ASSERT_EQ(stolenCount, 4u);
    EXPECT_EQ(stolenBatch[0], 10u);
    EXPECT_EQ(stolenBatch[1], 20u);
    EXPECT_EQ(stolenBatch[2], 30u);
    EXPECT_EQ(stolenBatch[3], 40u);
    EXPECT_EQ(deque.sizeApprox(), 4u);

    // Owner pops from bottom in LIFO order -> 80, 70, 60
    gp::UInt32 outVal = 0u;
    ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 80u);
    ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 70u);
    ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 60u);

    // Exactly 1 item remains (50): tryStealTopBatch must still steal that 1 remaining item!
    ASSERT_EQ(deque.tryStealTopBatch(stolenBatch, 4u, stolenCount), JobQueueResult::Success);
    ASSERT_EQ(stolenCount, 1u);
    EXPECT_EQ(stolenBatch[0], 50u);

    EXPECT_TRUE(deque.isEmptyApprox());
    EXPECT_EQ(deque.tryStealTopBatch(stolenBatch, 4u, stolenCount), JobQueueResult::Empty);
    EXPECT_EQ(stolenCount, 0u);
    EXPECT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Empty);
    EXPECT_EQ(deque.tryStealTop(outVal), JobQueueResult::Empty);
    EXPECT_EQ(deque.getBottomCursor(), deque.getTopCursor());

    // Multi-lap wrap-around verification across 128 iterations on an 8-element ring
    for (gp::UInt32 lap = 0u; lap < 128u; ++lap)
    {
        const gp::UInt32 a = lap * 10u + 1u;
        const gp::UInt32 b = lap * 10u + 2u;
        const gp::UInt32 c = lap * 10u + 3u;

        ASSERT_EQ(deque.tryPushBottom(a), JobQueueResult::Success);
        ASSERT_EQ(deque.tryPushBottom(b), JobQueueResult::Success);
        ASSERT_EQ(deque.tryPushBottom(c), JobQueueResult::Success);

        ASSERT_EQ(deque.tryStealTop(outVal), JobQueueResult::Success);
        EXPECT_EQ(outVal, a);

        ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
        EXPECT_EQ(outVal, c);

        // Last-item owner pop path (top == bottom)
        ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
        EXPECT_EQ(outVal, b);

        EXPECT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Empty);
    }

    // Verify clear() resets cursors to 0
    ASSERT_EQ(deque.tryPushBottom(42u), JobQueueResult::Success);
    deque.clear();
    EXPECT_TRUE(deque.isEmptyApprox());
    EXPECT_EQ(deque.getBottomCursor(), 0);
    EXPECT_EQ(deque.getTopCursor(), 0);
}

TEST(MPMCRingQueueTest, LifecycleValidationFifoBatchOperationsAndMultiLapWrapAround)
{
    MPMCRingQueue<gp::UInt32> queue;
    EXPECT_FALSE(queue.isValid());
    EXPECT_EQ(queue.capacity(), 0u);
    EXPECT_FALSE(queue.isFullApprox());

    gp::UInt32 outVal = 0u;
    gp::USize batchCount = 0u;
    EXPECT_EQ(queue.tryPush(1u), JobQueueResult::NotInitialized);
    EXPECT_EQ(queue.tryPushBatch(&outVal, 1u, batchCount), JobQueueResult::NotInitialized);
    EXPECT_EQ(queue.tryPop(outVal), JobQueueResult::NotInitialized);
    EXPECT_EQ(queue.tryPopBatch(&outVal, 1u, batchCount), JobQueueResult::NotInitialized);
    EXPECT_EQ(queue.initialize(0u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.initialize(MPMCRingQueue<gp::UInt32>::kMaxCapacity + 1u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.initialize(~static_cast<gp::USize>(0u)), JobQueueResult::InvalidArguments);

    using SlotType = MPMCRingQueue<gp::UInt32>::SlotType;
    alignas(64) gp::UInt8 rawSlotBytes[sizeof(SlotType) * 8u]{};
    SlotType* const alignedSlots = reinterpret_cast<SlotType*>(rawSlotBytes);
    SlotType* const misalignedSlots = reinterpret_cast<SlotType*>(rawSlotBytes + 1u);

    EXPECT_EQ(queue.initializeWithExternalBuffer(nullptr, 4u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.initializeWithExternalBuffer(alignedSlots, 1u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.initializeWithExternalBuffer(alignedSlots, 6u), JobQueueResult::InvalidArguments);
    EXPECT_EQ(
        queue.initializeWithExternalBuffer(alignedSlots, MPMCRingQueue<gp::UInt32>::kMaxCapacity * 2u),
        JobQueueResult::InvalidArguments
    );
    EXPECT_EQ(queue.initializeWithExternalBuffer(misalignedSlots, 4u), JobQueueResult::InvalidArguments);

    ASSERT_EQ(queue.initialize(4u), JobQueueResult::Success);
    EXPECT_EQ(queue.capacity(), 4u);

    // Validate batch push & pop edge cases
    EXPECT_EQ(queue.tryPushBatch(nullptr, 0u, batchCount), JobQueueResult::Success);
    EXPECT_EQ(batchCount, 0u);
    EXPECT_EQ(queue.tryPushBatch(nullptr, 2u, batchCount), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.tryPopBatch(nullptr, 2u, batchCount), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.tryPopBatch(&outVal, 0u, batchCount), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.tryPopBatch(&outVal, 2u, batchCount), JobQueueResult::Empty);

    // Push 6 items into a 4-slot queue via tryPushBatch -> pushes first 4 and returns Full with batchCount == 4
    const gp::UInt32 sixItems[6] = { 11u, 22u, 33u, 44u, 55u, 66u };
    EXPECT_EQ(queue.tryPushBatch(sixItems, 6u, batchCount), JobQueueResult::Full);
    EXPECT_EQ(batchCount, 4u);
    EXPECT_TRUE(queue.isFullApprox());

    // Pop all 4 items via tryPopBatch
    gp::UInt32 poppedBatch[6]{};
    gp::USize poppedCount = 0u;
    ASSERT_EQ(queue.tryPopBatch(poppedBatch, 6u, poppedCount), JobQueueResult::Success);
    ASSERT_EQ(poppedCount, 4u);
    EXPECT_EQ(poppedBatch[0], 11u);
    EXPECT_EQ(poppedBatch[1], 22u);
    EXPECT_EQ(poppedBatch[2], 33u);
    EXPECT_EQ(poppedBatch[3], 44u);
    EXPECT_TRUE(queue.isEmptyApprox());

    queue.clear();
    EXPECT_EQ(queue.getEnqueueCursor(), 0u);
    EXPECT_EQ(queue.getDequeueCursor(), 0u);

    // Multi-lap full-buffer cycle testing per-slot sequence ticket progression (pos + mask + 1)
    for (gp::UInt32 lap = 0u; lap < 64u; ++lap)
    {
        for (gp::UInt32 i = 0u; i < 4u; ++i)
        {
            ASSERT_EQ(queue.tryPush(lap * 100u + i), JobQueueResult::Success);
        }
        EXPECT_EQ(queue.tryPush(999999u), JobQueueResult::Full);
        EXPECT_EQ(queue.sizeApprox(), 4u);

        for (gp::UInt32 i = 0u; i < 4u; ++i)
        {
            ASSERT_EQ(queue.tryPop(outVal), JobQueueResult::Success);
            EXPECT_EQ(outVal, lap * 100u + i);
        }
        EXPECT_EQ(queue.tryPop(outVal), JobQueueResult::Empty);
        EXPECT_TRUE(queue.isEmptyApprox());
    }

    // Move semantics verification
    ASSERT_EQ(queue.tryPush(42u), JobQueueResult::Success);
    MPMCRingQueue<gp::UInt32> movedQueue;
    movedQueue = std::move(queue);
    EXPECT_FALSE(queue.isValid());
    EXPECT_TRUE(movedQueue.isValid());
    ASSERT_EQ(movedQueue.tryPop(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 42u);
}

TEST(WorkStealingDequeTest, SmpSingleOwnerMultipleThievesLastElementRaceStress)
{
    SpmcWorkStealingStressContext ctx{};
    ASSERT_EQ(ctx.deque.initialize(SpmcWorkStealingStressContext::kDequeCapacity), JobQueueResult::Success);

    gp::platform::Thread thieves[SpmcWorkStealingStressContext::kThiefCount]{};
    for (gp::UInt32 t = 0u; t < SpmcWorkStealingStressContext::kThiefCount; ++t)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_WorkStealThief";
        desc.entry = &spmcThiefThreadEntry;
        desc.userData = &ctx;
        ASSERT_EQ(thieves[t].start(desc), gp::platform::ThreadResult::Success);
    }

    while (ctx.readyThreads.load(gp::MemoryOrder::Acquire) < SpmcWorkStealingStressContext::kThiefCount)
    {
        gp::platform::Thread::spinWait();
    }

    ctx.startFence.signal();

    gp::UInt32 nextToPush = 0u;
    while (nextToPush < SpmcWorkStealingStressContext::kTotalItems)
    {
        // Push a small burst of 4 items, then pop 1 locally from the bottom to constantly provoke 1-item (top ==
        // bottom) races between the Owner's tryPopBottom() and the 3 Thieves' tryStealTop()!
        for (gp::UInt32 b = 0u; b < 4u && nextToPush < SpmcWorkStealingStressContext::kTotalItems; ++b)
        {
            if (ctx.deque.tryPushBottom(nextToPush) == JobQueueResult::Success)
            {
                ++nextToPush;
            }
            else
            {
                gp::UInt32 popped = 0u;
                if (ctx.deque.tryPopBottom(popped) == JobQueueResult::Success)
                {
                    ctx.visited[popped].fetchAdd(1u, gp::MemoryOrder::AcqRel);
                    ctx.ownerPoppedCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
                    ctx.totalConsumed.fetchAdd(1u, gp::MemoryOrder::AcqRel);
                }
            }
        }

        gp::UInt32 localItem = 0u;
        if (ctx.deque.tryPopBottom(localItem) == JobQueueResult::Success)
        {
            ctx.visited[localItem].fetchAdd(1u, gp::MemoryOrder::AcqRel);
            ctx.ownerPoppedCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
            ctx.totalConsumed.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        }
    }

    // Drain any remaining items from the bottom alongside the thieves
    gp::UInt32 remainingItem = 0u;
    while (ctx.deque.tryPopBottom(remainingItem) == JobQueueResult::Success)
    {
        ctx.visited[remainingItem].fetchAdd(1u, gp::MemoryOrder::AcqRel);
        ctx.ownerPoppedCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
        ctx.totalConsumed.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }

    ctx.producerDone.store(1u, gp::MemoryOrder::Release);

    for (gp::UInt32 t = 0u; t < SpmcWorkStealingStressContext::kThiefCount; ++t)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(thieves[t].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_EQ(ctx.totalConsumed.load(gp::MemoryOrder::Acquire), SpmcWorkStealingStressContext::kTotalItems);
    EXPECT_EQ(
        ctx.ownerPoppedCount.load(gp::MemoryOrder::Acquire) + ctx.thiefStolenCount.load(gp::MemoryOrder::Acquire),
        SpmcWorkStealingStressContext::kTotalItems
    );

    // Every single item in [0..kTotalItems) must have been processed exactly once (zero lost, zero duplicated)!
    for (gp::UInt32 i = 0u; i < SpmcWorkStealingStressContext::kTotalItems; ++i)
    {
        ASSERT_EQ(ctx.visited[i].load(gp::MemoryOrder::Acquire), 1u) << "Mismatch at item index " << i;
    }
}

TEST(MPMCRingQueueTest, SmpMultiProducerMultiConsumerContendedWrapAroundStress)
{
    MpmcContendedRingStressContext ctx{};
    ASSERT_EQ(ctx.queue.initialize(MpmcContendedRingStressContext::kQueueCapacity), JobQueueResult::Success);

    gp::platform::Thread producers[MpmcContendedRingStressContext::kProducerCount]{};
    gp::platform::Thread consumers[MpmcContendedRingStressContext::kConsumerCount]{};

    for (gp::UInt32 p = 0u; p < MpmcContendedRingStressContext::kProducerCount; ++p)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_MPMCProducer";
        desc.entry = &mpmcProducerThreadEntry;
        desc.userData = &ctx;
        ASSERT_EQ(producers[p].start(desc), gp::platform::ThreadResult::Success);
    }

    for (gp::UInt32 c = 0u; c < MpmcContendedRingStressContext::kConsumerCount; ++c)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_MPMCConsumer";
        desc.entry = &mpmcConsumerThreadEntry;
        desc.userData = &ctx;
        ASSERT_EQ(consumers[c].start(desc), gp::platform::ThreadResult::Success);
    }

    const gp::UInt32 expectedThreads =
        MpmcContendedRingStressContext::kProducerCount + MpmcContendedRingStressContext::kConsumerCount;
    while (ctx.readyThreads.load(gp::MemoryOrder::Acquire) < expectedThreads)
    {
        gp::platform::Thread::spinWait();
    }

    ctx.startFence.signal();

    for (gp::UInt32 p = 0u; p < MpmcContendedRingStressContext::kProducerCount; ++p)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(producers[p].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    for (gp::UInt32 c = 0u; c < MpmcContendedRingStressContext::kConsumerCount; ++c)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(consumers[c].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_EQ(ctx.totalDequeued.load(gp::MemoryOrder::Acquire), MpmcContendedRingStressContext::kTotalItems);
    EXPECT_TRUE(ctx.queue.isEmptyApprox());

    for (gp::UInt32 i = 0u; i < MpmcContendedRingStressContext::kTotalItems; ++i)
    {
        ASSERT_EQ(ctx.visited[i].load(gp::MemoryOrder::Acquire), 1u) << "MPMC item count mismatch at " << i;
    }
}

TEST(JobQueueIntegrationTest, FiberParentSpawnsToWorkStealingDequeThiefStealsAndWakesViaMPMCQueue)
{
    ASSERT_TRUE(gp::memory::FiberScratchArena::initializePool(4096u, 16u));

    Layer2FiberWorkStealingIntegrationContext ctx{};
    ASSERT_EQ(ctx.ownerDeque.initialize(32u), JobQueueResult::Success);
    ASSERT_EQ(ctx.readyFiberQueue.initialize(16u), JobQueueResult::Success);

    ScopedThreadFiber ownerAnchor(4000u, "OwnerWorkerAnchor");
    ASSERT_TRUE(ownerAnchor.isValid());
    ctx.ownerAnchorFiber = &ownerAnchor.getFiber();

    FiberDesc parentDesc{};
    parentDesc.name = "ParentSpawningFiber";
    parentDesc.id = 4001u;
    parentDesc.entry = &parentFiberSpawningEntry;
    parentDesc.userData = &ctx;
    parentDesc.returnFiber = &ownerAnchor.getFiber();
    ASSERT_EQ(ctx.parentFiber.create(parentDesc), FiberResult::Success);

    JobFiberParker parker{};
    parker.yieldCallback = &integrationYieldCallback;
    parker.wakeCallback = &integrationWakeCallback;
    parker.userData = &ctx;
    ScopedJobFiberParker parkerGuard(parker);

    gp::platform::Thread thiefWorker{};
    gp::platform::ThreadDesc thiefDesc{};
    thiefDesc.name = "GP_IntegrationThief";
    thiefDesc.entry = &integrationThiefWorkerEntry;
    thiefDesc.userData = &ctx;
    ASSERT_EQ(thiefWorker.start(thiefDesc), gp::platform::ThreadResult::Success);

    while (ctx.thiefReady.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    // Switch into parentFiber: it pushes 16 child jobs into ownerDeque and yields on childCounter.wait(0)!
    ASSERT_EQ(ownerAnchor.getFiber().switchTo(ctx.parentFiber), FiberResult::Success);

    // Owner worker loop: pops child jobs from bottom of ownerDeque (LIFO) while thief steals from top (FIFO),
    // and monitors readyFiberQueue (MPMC) for the woken parentFiber!
    while (ctx.parentCompleted.load(gp::MemoryOrder::Acquire) == 0u)
    {
        JobQueueItem readyItem{};
        if (ctx.readyFiberQueue.tryPop(readyItem) == JobQueueResult::Success && readyItem.isResumedFiber())
        {
            Fiber* const wokenFiber = readyItem.getResumedFiber();
            ASSERT_NE(wokenFiber, nullptr);
            ASSERT_EQ(ownerAnchor.getFiber().switchTo(*wokenFiber), FiberResult::Success);
            continue;
        }

        JobQueueItem localJob{};
        if (ctx.ownerDeque.tryPopBottom(localJob) == JobQueueResult::Success && localJob.isJobEntry())
        {
            ctx.localChildCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
            localJob.entry(localJob.userData);
        }
        else
        {
            gp::platform::Thread::spinWait();
        }
    }

    gp::UInt32 thiefExit = 1u;
    EXPECT_EQ(thiefWorker.join(&thiefExit), gp::platform::ThreadResult::Success);
    EXPECT_EQ(thiefExit, 0u);

    // Sum of 1..16 = 136
    EXPECT_EQ(ctx.childAccumulator.load(gp::MemoryOrder::Acquire), 136u);
    EXPECT_EQ(
        ctx.localChildCount.load(gp::MemoryOrder::Acquire) + ctx.stolenChildCount.load(gp::MemoryOrder::Acquire),
        Layer2FiberWorkStealingIntegrationContext::kChildJobCount
    );
    EXPECT_TRUE(ctx.parentScratchCanaryIntact);
    EXPECT_TRUE(ctx.parentFiber.isCompleted());

    EXPECT_EQ(ctx.parentFiber.destroy(), FiberResult::Success);
    EXPECT_EQ(gp::memory::FiberScratchArena::getSharedPagePool().getLeasedPageCount(), 0u);
    gp::memory::FiberScratchArena::shutdownPool();
}

}   // namespace gp::thread::tests
