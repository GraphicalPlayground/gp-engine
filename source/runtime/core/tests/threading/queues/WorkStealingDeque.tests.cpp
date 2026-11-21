// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/queues/WorkStealingDeque.hpp"
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
concept CanInstantiateWorkStealingDeque = requires { typename WorkStealingDeque<InType>; };

struct NonTrivialDequePayload
{
    gp::UInt32 value{ 0u };

    NonTrivialDequePayload() noexcept = default;

    NonTrivialDequePayload(const NonTrivialDequePayload& other) noexcept
        : value(other.value)
    {}

    ~NonTrivialDequePayload() noexcept
    {}
};

static_assert(CanInstantiateWorkStealingDeque<gp::UInt32>, "WorkStealingDeque must accept UInt32.");
static_assert(CanInstantiateWorkStealingDeque<gp::UInt64>, "WorkStealingDeque must accept UInt64.");
static_assert(CanInstantiateWorkStealingDeque<JobDecl>, "WorkStealingDeque must accept JobDecl.");
static_assert(CanInstantiateWorkStealingDeque<JobQueueItem>, "WorkStealingDeque must accept JobQueueItem.");
static_assert(!CanInstantiateWorkStealingDeque<const gp::UInt32>, "WorkStealingDeque must reject const types.");
static_assert(
    !CanInstantiateWorkStealingDeque<NonTrivialDequePayload>,
    "WorkStealingDeque must reject non-trivially-copyable types."
);

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

struct SpmcBatchStealStressContext
{
    static constexpr gp::UInt32 kThiefCount = 3u;
    static constexpr gp::UInt32 kTotalItems = 8192u;
    static constexpr gp::USize kDequeCapacity = 256u;

    WorkStealingDeque<gp::UInt32> deque{};
    Atomic<gp::UInt32> readyThreads{ 0u };
    JobFence startFence{ false };
    Atomic<gp::UInt8> producerDone{ 0u };
    Atomic<gp::UInt32> totalConsumed{ 0u };
    Atomic<gp::UInt8> visited[kTotalItems]{};
};

static gp::UInt32 spmcBatchThiefThreadEntry(void* const rawData) noexcept
{
    SpmcBatchStealStressContext* const ctx = static_cast<SpmcBatchStealStressContext*>(rawData);
    ctx->readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    (void)ctx->startFence.wait(JobWaitMode::SpinOnly);

    gp::UInt32 stolenBatch[16]{};
    while (ctx->totalConsumed.load(gp::MemoryOrder::Acquire) < SpmcBatchStealStressContext::kTotalItems)
    {
        gp::USize stolenCount = 0u;
        const JobQueueResult res = ctx->deque.tryStealTopBatch(stolenBatch, 16u, stolenCount);
        if (res == JobQueueResult::Success && stolenCount > 0u)
        {
            for (gp::USize i = 0u; i < stolenCount; ++i)
            {
                const gp::UInt32 item = stolenBatch[i];
                if (item < SpmcBatchStealStressContext::kTotalItems)
                {
                    ctx->visited[item].fetchAdd(1u, gp::MemoryOrder::AcqRel);
                }
            }
            ctx->totalConsumed.fetchAdd(static_cast<gp::UInt32>(stolenCount), gp::MemoryOrder::AcqRel);
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

}   // namespace

template <typename InType>
class WorkStealingDequeTypedTest : public ::testing::Test
{};

using WorkStealingElementTypes = ::testing::Types<gp::UInt32, gp::UInt64, JobDecl, JobQueueItem>;
TYPED_TEST_SUITE(WorkStealingDequeTypedTest, WorkStealingElementTypes);

TYPED_TEST(WorkStealingDequeTypedTest, CacheLineLayoutAndDefaultState)
{
    using DequeType = WorkStealingDeque<TypeParam>;
    EXPECT_EQ(sizeof(DequeType), 3u * kJobQueueCacheLineSize);
    EXPECT_EQ(alignof(DequeType), kJobQueueCacheLineSize);

    DequeType deque;
    EXPECT_FALSE(deque.isValid());
    EXPECT_EQ(deque.capacity(), 0u);
    EXPECT_EQ(deque.sizeApprox(), 0u);
    EXPECT_TRUE(deque.isEmptyApprox());
    EXPECT_FALSE(deque.isFullApprox());
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

    ASSERT_EQ(movedDeque.tryPushBottom(88u), JobQueueResult::Success);
    WorkStealingDeque<gp::UInt32> assignedDeque;
    assignedDeque = std::move(movedDeque);
    EXPECT_FALSE(movedDeque.isValid());
    EXPECT_TRUE(assignedDeque.isValid());
    ASSERT_EQ(assignedDeque.tryPopBottom(dummy), JobQueueResult::Success);
    EXPECT_EQ(dummy, 88u);
}

TEST(WorkStealingDequeTest, LifoOwnerPopFifoThiefStealBatchAndMultiLapWrapAround)
{
    WorkStealingDeque<gp::UInt32> deque;
    ASSERT_EQ(deque.initialize(8u), JobQueueResult::Success);
    ASSERT_EQ(deque.capacity(), 8u);

    EXPECT_EQ(deque.tryPushBottomRange(nullptr, 0u), JobQueueResult::Success);
    EXPECT_EQ(deque.tryPushBottomRange(nullptr, 2u), JobQueueResult::InvalidArguments);

    const gp::UInt32 oversizedBatch[9] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u };
    EXPECT_EQ(deque.tryPushBottomRange(oversizedBatch, 9u), JobQueueResult::Full);
    EXPECT_TRUE(deque.isEmptyApprox());

    const gp::UInt32 batch8[8] = { 10u, 20u, 30u, 40u, 50u, 60u, 70u, 80u };
    ASSERT_EQ(deque.tryPushBottomRange(batch8, 8u), JobQueueResult::Success);
    EXPECT_EQ(deque.sizeApprox(), 8u);
    EXPECT_TRUE(deque.isFullApprox());

    EXPECT_EQ(deque.tryPushBottom(90u), JobQueueResult::Full);
    EXPECT_EQ(deque.tryPushBottomRange(batch8, 1u), JobQueueResult::Full);

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

    gp::UInt32 outVal = 0u;
    ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 80u);
    ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 70u);
    ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
    EXPECT_EQ(outVal, 60u);

    ASSERT_EQ(deque.tryStealTopBatch(stolenBatch, 4u, stolenCount), JobQueueResult::Success);
    ASSERT_EQ(stolenCount, 1u);
    EXPECT_EQ(stolenBatch[0], 50u);

    EXPECT_TRUE(deque.isEmptyApprox());
    EXPECT_EQ(deque.tryStealTopBatch(stolenBatch, 4u, stolenCount), JobQueueResult::Empty);
    EXPECT_EQ(stolenCount, 0u);
    EXPECT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Empty);
    EXPECT_EQ(deque.tryStealTop(outVal), JobQueueResult::Empty);
    EXPECT_EQ(deque.getBottomCursor(), deque.getTopCursor());

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

        ASSERT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Success);
        EXPECT_EQ(outVal, b);

        EXPECT_EQ(deque.tryPopBottom(outVal), JobQueueResult::Empty);
    }

    ASSERT_EQ(deque.tryPushBottom(42u), JobQueueResult::Success);
    deque.clear();
    EXPECT_TRUE(deque.isEmptyApprox());
    EXPECT_EQ(deque.getBottomCursor(), 0);
    EXPECT_EQ(deque.getTopCursor(), 0);
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

    for (gp::UInt32 i = 0u; i < SpmcWorkStealingStressContext::kTotalItems; ++i)
    {
        ASSERT_EQ(ctx.visited[i].load(gp::MemoryOrder::Acquire), 1u) << "Mismatch at item index " << i;
    }
}

TEST(WorkStealingDequeTest, SmpBatchRangePushAndBatchThiefStealStress)
{
    SpmcBatchStealStressContext ctx{};
    ASSERT_EQ(ctx.deque.initialize(SpmcBatchStealStressContext::kDequeCapacity), JobQueueResult::Success);

    gp::platform::Thread thieves[SpmcBatchStealStressContext::kThiefCount]{};
    for (gp::UInt32 t = 0u; t < SpmcBatchStealStressContext::kThiefCount; ++t)
    {
        gp::platform::ThreadDesc desc{};
        desc.name = "GP_BatchStealThief";
        desc.entry = &spmcBatchThiefThreadEntry;
        desc.userData = &ctx;
        ASSERT_EQ(thieves[t].start(desc), gp::platform::ThreadResult::Success);
    }

    while (ctx.readyThreads.load(gp::MemoryOrder::Acquire) < SpmcBatchStealStressContext::kThiefCount)
    {
        gp::platform::Thread::spinWait();
    }

    ctx.startFence.signal();

    gp::UInt32 nextToPush = 0u;
    while (nextToPush < SpmcBatchStealStressContext::kTotalItems)
    {
        gp::UInt32 batch[8]{};
        gp::USize batchSize = 0u;
        while (batchSize < 8u &&
               (nextToPush + static_cast<gp::UInt32>(batchSize)) < SpmcBatchStealStressContext::kTotalItems)
        {
            batch[batchSize] = nextToPush + static_cast<gp::UInt32>(batchSize);
            ++batchSize;
        }

        if (ctx.deque.tryPushBottomRange(batch, batchSize) == JobQueueResult::Success)
        {
            nextToPush += static_cast<gp::UInt32>(batchSize);
        }
        else
        {
            gp::UInt32 localItem = 0u;
            if (ctx.deque.tryPopBottom(localItem) == JobQueueResult::Success)
            {
                ctx.visited[localItem].fetchAdd(1u, gp::MemoryOrder::AcqRel);
                ctx.totalConsumed.fetchAdd(1u, gp::MemoryOrder::AcqRel);
            }
        }
    }

    gp::UInt32 remainingItem = 0u;
    while (ctx.deque.tryPopBottom(remainingItem) == JobQueueResult::Success)
    {
        ctx.visited[remainingItem].fetchAdd(1u, gp::MemoryOrder::AcqRel);
        ctx.totalConsumed.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }

    ctx.producerDone.store(1u, gp::MemoryOrder::Release);

    for (gp::UInt32 t = 0u; t < SpmcBatchStealStressContext::kThiefCount; ++t)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(thieves[t].join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    EXPECT_EQ(ctx.totalConsumed.load(gp::MemoryOrder::Acquire), SpmcBatchStealStressContext::kTotalItems);
    for (gp::UInt32 i = 0u; i < SpmcBatchStealStressContext::kTotalItems; ++i)
    {
        ASSERT_EQ(ctx.visited[i].load(gp::MemoryOrder::Acquire), 1u) << "Batch stress mismatch at index " << i;
    }
}

}   // namespace gp::thread::tests
