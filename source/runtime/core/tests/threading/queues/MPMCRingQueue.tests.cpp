// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/queues/MPMCRingQueue.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/queues/JobQueueDesc.hpp"
#include "threading/queues/JobQueueEnums.hpp"
#include "threading/queues/JobQueueForward.hpp"
#include "threading/queues/JobQueueTypes.hpp"   // IWYU pragma: keep
#include "threading/sync/JobFence.hpp"
#include "threading/sync/JobSyncEnums.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::thread::tests
{

namespace
{

template <typename InType>
concept CanInstantiateMPMCRingQueue = requires { typename MPMCRingQueue<InType>; };

struct NonTrivialMpmcPayload
{
    gp::UInt32 value{ 0u };

    NonTrivialMpmcPayload() noexcept = default;

    NonTrivialMpmcPayload(const NonTrivialMpmcPayload& other) noexcept
        : value(other.value)
    {}

    ~NonTrivialMpmcPayload() noexcept
    {}
};

static_assert(CanInstantiateMPMCRingQueue<gp::UInt32>, "MPMCRingQueue must accept UInt32.");
static_assert(CanInstantiateMPMCRingQueue<gp::UInt64>, "MPMCRingQueue must accept UInt64.");
static_assert(CanInstantiateMPMCRingQueue<JobDecl>, "MPMCRingQueue must accept JobDecl.");
static_assert(CanInstantiateMPMCRingQueue<JobQueueItem>, "MPMCRingQueue must accept JobQueueItem.");
static_assert(!CanInstantiateMPMCRingQueue<const gp::UInt32>, "MPMCRingQueue must reject const types.");
static_assert(
    !CanInstantiateMPMCRingQueue<NonTrivialMpmcPayload>, "MPMCRingQueue must reject non-trivially-copyable types."
);

constexpr void dummyConstexprMpmcJobEntry(void* const userData) noexcept
{
    (void)userData;
}

constexpr bool testConstexprMPMCRingQueue() noexcept
{
    using QueueType = MPMCRingQueue<JobQueueItem>;
    QueueType::SlotType slots[4]{};
    QueueType queue(slots, 4u);
    if (!queue.isValid() || queue.capacity() != 4u || !queue.isEmptyApprox() || queue.isFullApprox())
    {
        return false;
    }

    const JobDecl decl = JobDecl::make(&dummyConstexprMpmcJobEntry, nullptr, nullptr, JobPriority::High);
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

    return outItem.isJobEntry() && outItem.isValid() && outItem.entry == &dummyConstexprMpmcJobEntry &&
           outItem.priority == JobPriority::High && queue.isEmptyApprox();
}

static_assert(testConstexprMPMCRingQueue(), "Constexpr MPMCRingQueue evaluation failed.");

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

static gp::UInt32 mpmcBatchProducerThreadEntry(void* const rawData) noexcept
{
    MpmcContendedRingStressContext* const ctx = static_cast<MpmcContendedRingStressContext*>(rawData);
    const gp::UInt32 producerId = ctx->nextProducerIndex.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    const gp::UInt32 baseItem = producerId * MpmcContendedRingStressContext::kItemsPerProducer;

    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::SpinOnly);

    gp::UInt32 pushedSoFar = 0u;
    while (pushedSoFar < MpmcContendedRingStressContext::kItemsPerProducer)
    {
        gp::UInt32 batch[8]{};
        gp::USize countToPush = 0u;
        while (countToPush < 8u &&
               (pushedSoFar + static_cast<gp::UInt32>(countToPush)) < MpmcContendedRingStressContext::kItemsPerProducer)
        {
            batch[countToPush] = baseItem + pushedSoFar + static_cast<gp::UInt32>(countToPush);
            ++countToPush;
        }

        gp::USize pushedCount = 0u;
        const JobQueueResult res = ctx->queue.tryPushBatch(batch, countToPush, pushedCount);
        if (pushedCount > 0u)
        {
            pushedSoFar += static_cast<gp::UInt32>(pushedCount);
        }
        if (res != JobQueueResult::Success)
        {
            gp::platform::Thread::spinWait();
        }
    }

    return 0u;
}

static gp::UInt32 mpmcBatchConsumerThreadEntry(void* const rawData) noexcept
{
    MpmcContendedRingStressContext* const ctx = static_cast<MpmcContendedRingStressContext*>(rawData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::SpinOnly);

    gp::UInt32 poppedBatch[8]{};
    while (ctx->totalDequeued.load(gp::MemoryOrder::Acquire) < MpmcContendedRingStressContext::kTotalItems)
    {
        gp::USize poppedCount = 0u;
        if (ctx->queue.tryPopBatch(poppedBatch, 8u, poppedCount) == JobQueueResult::Success && poppedCount > 0u)
        {
            for (gp::USize i = 0u; i < poppedCount; ++i)
            {
                const gp::UInt32 itemValue = poppedBatch[i];
                if (itemValue < MpmcContendedRingStressContext::kTotalItems)
                {
                    ctx->visited[itemValue].fetchAdd(1u, gp::MemoryOrder::AcqRel);
                }
            }
            ctx->totalDequeued.fetchAdd(static_cast<gp::UInt32>(poppedCount), gp::MemoryOrder::AcqRel);
        }
        else
        {
            gp::platform::Thread::spinWait();
        }
    }

    return 0u;
}

}   // namespace

template <typename InType>
class MPMCRingQueueTypedTest : public ::testing::Test
{};

using MPMCElementTypes = ::testing::Types<gp::UInt32, gp::UInt64, JobDecl, JobQueueItem>;
TYPED_TEST_SUITE(MPMCRingQueueTypedTest, MPMCElementTypes);

TYPED_TEST(MPMCRingQueueTypedTest, CacheLineAndSlotIsolationLayout)
{
    using QueueType = MPMCRingQueue<TypeParam>;
    using SlotType = typename QueueType::SlotType;

    EXPECT_EQ(sizeof(QueueType), 3u * kJobQueueCacheLineSize);
    EXPECT_EQ(alignof(QueueType), kJobQueueCacheLineSize);
    EXPECT_EQ(alignof(SlotType), kJobQueueCacheLineSize);
    EXPECT_EQ(sizeof(SlotType), kJobQueueCacheLineSize);

    QueueType queue;
    EXPECT_FALSE(queue.isValid());
    EXPECT_EQ(queue.capacity(), 0u);
    EXPECT_EQ(queue.sizeApprox(), 0u);
    EXPECT_TRUE(queue.isEmptyApprox());
    EXPECT_FALSE(queue.isFullApprox());
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

    EXPECT_EQ(queue.tryPushBatch(nullptr, 0u, batchCount), JobQueueResult::Success);
    EXPECT_EQ(batchCount, 0u);
    EXPECT_EQ(queue.tryPushBatch(nullptr, 2u, batchCount), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.tryPopBatch(nullptr, 2u, batchCount), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.tryPopBatch(&outVal, 0u, batchCount), JobQueueResult::InvalidArguments);
    EXPECT_EQ(queue.tryPopBatch(&outVal, 2u, batchCount), JobQueueResult::Empty);

    const gp::UInt32 sixItems[6] = { 11u, 22u, 33u, 44u, 55u, 66u };
    EXPECT_EQ(queue.tryPushBatch(sixItems, 6u, batchCount), JobQueueResult::Full);
    EXPECT_EQ(batchCount, 4u);
    EXPECT_TRUE(queue.isFullApprox());

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

    ASSERT_EQ(queue.tryPush(42u), JobQueueResult::Success);
    MPMCRingQueue<gp::UInt32> movedConstructed(std::move(queue));
    EXPECT_FALSE(queue.isValid());
    EXPECT_TRUE(movedConstructed.isValid());
    ASSERT_EQ(movedConstructed.tryPop(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 42u);

    ASSERT_EQ(movedConstructed.tryPush(99u), JobQueueResult::Success);
    MPMCRingQueue<gp::UInt32> movedAssigned;
    movedAssigned = std::move(movedConstructed);
    EXPECT_FALSE(movedConstructed.isValid());
    EXPECT_TRUE(movedAssigned.isValid());
    ASSERT_EQ(movedAssigned.tryPop(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 99u);
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

TEST(MPMCRingQueueTest, SmpMultiProducerMultiConsumerBatchOperationsStress)
{
    MpmcContendedRingStressContext ctx{};
    ASSERT_EQ(ctx.queue.initialize(MpmcContendedRingStressContext::kQueueCapacity), JobQueueResult::Success);

    gp::platform::Thread producers[MpmcContendedRingStressContext::kProducerCount]{};
    gp::platform::Thread consumers[MpmcContendedRingStressContext::kConsumerCount]{};

    for (gp::UInt32 p = 0u; p < MpmcContendedRingStressContext::kProducerCount; ++p)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_MPMCBatchProd";
        desc.entry = &mpmcBatchProducerThreadEntry;
        desc.userData = &ctx;
        ASSERT_EQ(producers[p].start(desc), gp::platform::ThreadResult::Success);
    }

    for (gp::UInt32 c = 0u; c < MpmcContendedRingStressContext::kConsumerCount; ++c)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_MPMCBatchCons";
        desc.entry = &mpmcBatchConsumerThreadEntry;
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
        ASSERT_EQ(ctx.visited[i].load(gp::MemoryOrder::Acquire), 1u) << "MPMC batch item mismatch at " << i;
    }
}

}   // namespace gp::thread::tests
