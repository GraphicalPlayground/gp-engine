// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/FiberScratchAllocator.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/Fiber.hpp"
#include "threading/queues/JobQueueDesc.hpp"
#include "threading/queues/JobQueueEnums.hpp"
#include "threading/queues/JobQueueForward.hpp"
#include "threading/queues/JobQueueTypes.hpp"
#include "threading/queues/MPMCRingQueue.hpp"
#include "threading/queues/WorkStealingDeque.hpp"
#include "threading/sync/JobCounter.hpp"
#include "threading/sync/JobWaitNode.hpp"
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace
{

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
static_assert(JobQueueElement<gp::UInt64>, "JobQueueElement must accept UInt64.");
static_assert(JobQueueElement<JobDecl>, "JobQueueElement must accept JobDecl.");
static_assert(JobQueueElement<JobQueueItem>, "JobQueueElement must accept JobQueueItem.");
static_assert(!JobQueueElement<const gp::UInt32>, "JobQueueElement must reject const types.");
static_assert(!JobQueueElement<NonTrivialQueuePayload>, "JobQueueElement must reject non-trivially-copyable types.");

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

constexpr bool testConstexprJobDeclAndItem() noexcept
{
    const JobDecl decl = JobDecl::make(
        &dummyConstexprJobEntry, nullptr, nullptr, JobPriority::High, FiberStackTier::Large512KiB, 0x0042u
    );
    if (!decl.isValid() || decl.priority != JobPriority::High || decl.stackTier != FiberStackTier::Large512KiB ||
        decl.flags != 0x0042u)
    {
        return false;
    }

    const JobQueueItem item = JobQueueItem::fromJob(decl);
    if (!item.isValid() || !item.isJobEntry() || item.isResumedFiber())
    {
        return false;
    }

    const JobDecl roundTrip = item.toJobDecl();
    return roundTrip.entry == &dummyConstexprJobEntry && roundTrip.priority == JobPriority::High &&
           roundTrip.stackTier == FiberStackTier::Large512KiB && roundTrip.flags == 0x0042u;
}

static_assert(testConstexprJobDeclAndItem(), "Constexpr JobDecl and JobQueueItem evaluation failed.");

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

TEST(JobQueueTypesAndEnumsTest, ConstantsAndEnumValues)
{
    EXPECT_EQ(kJobQueueCacheLineSize, 64u);
    EXPECT_EQ(kMinJobQueueCapacity, 2u);
    EXPECT_EQ(kDefaultWorkStealingCapacity, 1024u);
    EXPECT_EQ(kDefaultMPMCQueueCapacity, 4096u);

    EXPECT_EQ(static_cast<gp::UInt8>(JobQueueResult::Success), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(JobPriority::Count), 3u);
    EXPECT_EQ(static_cast<gp::UInt8>(JobQueueItemKind::JobEntry), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(JobQueueItemKind::ResumedFiber), 1u);
}

TEST(JobQueueDescTest, JobDeclAndJobQueueItemLayoutAndRoundTripSemantics)
{
    EXPECT_EQ(sizeof(JobDecl), 32u);
    EXPECT_EQ(alignof(JobDecl), 16u);
    EXPECT_EQ(sizeof(JobQueueItem), 32u);
    EXPECT_EQ(alignof(JobQueueItem), 16u);

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

    ASSERT_EQ(ownerAnchor.getFiber().switchTo(ctx.parentFiber), FiberResult::Success);

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
