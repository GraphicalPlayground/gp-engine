// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/roles/NamedThreadDispatcher.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/roles/NamedThreadDispatcherDesc.hpp"
#include "threading/roles/ScopedThreadRole.hpp"
#include "threading/roles/ThreadRole.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "threading/roles/ThreadRoleTypes.hpp"
#include "threading/sync/JobCounter.hpp"
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace
{

/// @brief Records the order in which tasks ran and the role mask held by the executing thread.
struct ExecutionLog
{
    static constexpr gp::UInt32 kMaxEntries = 64u;

    gp::UInt32 values[kMaxEntries]{};
    ThreadRole observedRoles[kMaxEntries]{};
    gp::UInt32 count{ 0u };
};

/// @brief Payload for `recordValueTask`.
struct RecordItem
{
    ExecutionLog* log{ nullptr };
    gp::UInt32 value{ 0u };
};

/// @brief Appends `item.value` and the caller's role mask to `item.log`.
void recordValueTask(void* rawItem)
{
    RecordItem& item = *static_cast<RecordItem*>(rawItem);
    if (item.log != nullptr && item.log->count < ExecutionLog::kMaxEntries)
    {
        const gp::UInt32 idx = item.log->count++;
        item.log->values[idx] = item.value;
        item.log->observedRoles[idx] = ThreadRoleRegistry::getCurrentRoles();
    }
}

/// @brief Payload for a task that re-enqueues itself onto the same queue while being pumped.
struct RequeueContext
{
    NamedThreadDispatcher* dispatcher{ nullptr };
    NamedThread target{ NamedThread::Render };
    gp::UInt32 remaining{ 0u };
    gp::UInt32 executed{ 0u };
};

/// @brief Increments `executed` and re-dispatches itself until `remaining` reaches zero.
void requeueSelfTask(void* rawContext)
{
    RequeueContext& ctx = *static_cast<RequeueContext*>(rawContext);
    ++ctx.executed;
    if (ctx.remaining > 0u)
    {
        --ctx.remaining;
        (void)ctx.dispatcher->dispatch(ctx.target, &requeueSelfTask, &ctx);
    }
}

/// @brief Shared state for multi-producer single-consumer cross-thread dispatch tests.
struct CrossThreadDispatchContext
{
    static constexpr gp::UInt32 kProducerCount = 4u;
    static constexpr gp::UInt32 kTasksPerProducer = 250u;
    static constexpr gp::UInt32 kTotalTasks = kProducerCount * kTasksPerProducer;

    NamedThreadDispatcher* dispatcher{ nullptr };
    JobCounter completionCounter{};
    Atomic<gp::UInt32> executedSum{ 0u };
    Atomic<gp::UInt32> executedCount{ 0u };
    Atomic<gp::UInt32> producersReady{ 0u };
    Atomic<gp::UInt32> producersStart{ 0u };
    Atomic<gp::UInt32> dispatchFailures{ 0u };
    Atomic<gp::UInt32> workerDone{ 0u };
};

/// @brief Task run on the render thread during the cross-thread dispatch test.
void accumulateAtomicTask(void* rawContext)
{
    CrossThreadDispatchContext& ctx = *static_cast<CrossThreadDispatchContext*>(rawContext);
    (void)ctx.executedSum.fetchAdd(1u, gp::MemoryOrder::Relaxed);
    (void)ctx.executedCount.fetchAdd(1u, gp::MemoryOrder::AcqRel);
}

/// @brief Producer thread entry that pushes `kTasksPerProducer` tasks onto `NamedThread::Render`.
gp::UInt32 producerThreadEntry(void* rawContext)
{
    CrossThreadDispatchContext& ctx = *static_cast<CrossThreadDispatchContext*>(rawContext);
    (void)ctx.producersReady.fetchAdd(1u, gp::MemoryOrder::AcqRel);

    while (ctx.producersStart.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    for (gp::UInt32 i = 0u; i < CrossThreadDispatchContext::kTasksPerProducer; ++i)
    {
        ThreadRoleResult res = ThreadRoleResult::QueueFull;
        do
        {
            res = ctx.dispatcher->dispatch(NamedThread::Render, &accumulateAtomicTask, &ctx, &ctx.completionCounter);
            if (res == ThreadRoleResult::QueueFull)
            {
                gp::platform::Thread::yieldExecution();
            }
        }
        while (res == ThreadRoleResult::QueueFull);

        if (res != ThreadRoleResult::Success)
        {
            (void)ctx.dispatchFailures.fetchAdd(1u, gp::MemoryOrder::AcqRel);
        }
    }

    return 0u;
}

/// @brief Consumer thread entry owning `NamedThread::Render` and pumping until all tasks finish.
gp::UInt32 renderConsumerThreadEntry(void* rawContext)
{
    CrossThreadDispatchContext& ctx = *static_cast<CrossThreadDispatchContext*>(rawContext);
    const ScopedNamedThread renderRole(NamedThread::Render);
    if (!renderRole.isAttached())
    {
        return 1u;
    }

    while (ctx.workerDone.load(gp::MemoryOrder::Acquire) == 0u ||
           ctx.executedCount.load(gp::MemoryOrder::Acquire) < CrossThreadDispatchContext::kTotalTasks)
    {
        gp::UInt32 executed = 0u;
        if (ctx.dispatcher->pumpUntilIdle(NamedThread::Render, executed) != ThreadRoleResult::Success)
        {
            return 2u;
        }
        if (executed == 0u)
        {
            gp::platform::Thread::yieldExecution();
        }
    }

    return 0u;
}

}   // namespace

TEST(NamedThreadDispatcherTest, UninitializedStateRejectsOperations)
{
    NamedThreadDispatcher dispatcher;
    EXPECT_FALSE(dispatcher.isInitialized());
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Game), 0u);
    EXPECT_EQ(dispatcher.getPendingTaskCountApprox(NamedThread::Game), 0u);

    EXPECT_EQ(
        dispatcher.dispatch(
            NamedThread::Game,
            [](void*)
    {
    }
        ),
        ThreadRoleResult::NotInitialized
    );

    gp::UInt32 executedCount = 99u;
    EXPECT_EQ(dispatcher.pump(NamedThread::Game, kPumpAllTasks, executedCount), ThreadRoleResult::NotInitialized);
    EXPECT_EQ(executedCount, 0u);

    EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::Game, executedCount), ThreadRoleResult::NotInitialized);
    EXPECT_EQ(executedCount, 0u);

    executedCount = 77u;
    EXPECT_EQ(dispatcher.pump(NamedThread::Count, kPumpAllTasks, executedCount), ThreadRoleResult::NotInitialized);
    EXPECT_EQ(executedCount, 0u);

    // Calling shutdown before initialize is a safe no-op.
    dispatcher.shutdown();
    EXPECT_FALSE(dispatcher.isInitialized());
}

TEST(NamedThreadDispatcherTest, InitializeAndShutdownLifecycle)
{
    NamedThreadDispatcher dispatcher;
    NamedThreadDispatcherDesc desc{};
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::Game)] = 8u;
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::Render)] = 16u;
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::RHI)] = 32u;

    ASSERT_EQ(dispatcher.initialize(desc), ThreadRoleResult::Success);
    EXPECT_TRUE(dispatcher.isInitialized());
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Game), 8u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Render), 16u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::RHI), 32u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Count), 0u);
    EXPECT_EQ(dispatcher.getPendingTaskCountApprox(NamedThread::Count), 0u);

    // Re-initializing while active is rejected.
    EXPECT_EQ(dispatcher.initialize(desc), ThreadRoleResult::AlreadyInitialized);

    dispatcher.shutdown();
    EXPECT_FALSE(dispatcher.isInitialized());
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Game), 0u);

    // Second shutdown is a safe no-op, and re-initialization afterwards succeeds.
    dispatcher.shutdown();
    ASSERT_EQ(dispatcher.initialize(desc), ThreadRoleResult::Success);
    EXPECT_TRUE(dispatcher.isInitialized());
    dispatcher.shutdown();
}

TEST(NamedThreadDispatcherTest, ZeroQueueCapacityIsRejectedAndLeavesUninitialized)
{
    NamedThreadDispatcher dispatcher;
    NamedThreadDispatcherDesc desc{};
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::Render)] = 0u;

    EXPECT_EQ(dispatcher.initialize(desc), ThreadRoleResult::InvalidArguments);
    EXPECT_FALSE(dispatcher.isInitialized());
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Game), 0u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Render), 0u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::RHI), 0u);
}

TEST(NamedThreadDispatcherTest, DispatchAndPumpRejectInvalidArguments)
{
    NamedThreadDispatcher dispatcher;
    ASSERT_EQ(dispatcher.initialize(), ThreadRoleResult::Success);

    EXPECT_EQ(
        dispatcher.dispatch(
            NamedThread::Count,
            [](void*)
    {
    }
        ),
        ThreadRoleResult::InvalidArguments
    );
    EXPECT_EQ(dispatcher.dispatch(NamedThread::Game, nullptr), ThreadRoleResult::InvalidArguments);

    NamedThreadTask invalidTask{};
    EXPECT_EQ(dispatcher.dispatch(NamedThread::Game, invalidTask), ThreadRoleResult::InvalidArguments);

    gp::UInt32 executedCount = 42u;
    EXPECT_EQ(dispatcher.pump(NamedThread::Count, kPumpAllTasks, executedCount), ThreadRoleResult::InvalidArguments);
    EXPECT_EQ(executedCount, 0u);
}

TEST(NamedThreadDispatcherTest, PumpEnforcesTargetThreadRoleOwnership)
{
    NamedThreadDispatcher dispatcher;
    ASSERT_EQ(dispatcher.initialize(), ThreadRoleResult::Success);

    gp::UInt32 ranCount = 0u;
    ASSERT_EQ(
        dispatcher.dispatch(
            NamedThread::Render,
            [](void* rawCount)
    {
        ++(*static_cast<gp::UInt32*>(rawCount));
    },
            &ranCount
        ),
        ThreadRoleResult::Success
    );

    gp::UInt32 executedCount = 123u;
    // Unattached thread cannot pump Render.
    EXPECT_EQ(dispatcher.pump(NamedThread::Render, kPumpAllTasks, executedCount), ThreadRoleResult::WrongThread);
    EXPECT_EQ(executedCount, 0u);
    EXPECT_EQ(ranCount, 0u);

    // ParallelRender is a shared scoped role, not the Render thread owner.
    {
        const ScopedThreadRole parallelGuard(ThreadRole::ParallelRender);
        EXPECT_EQ(dispatcher.pump(NamedThread::Render, kPumpAllTasks, executedCount), ThreadRoleResult::WrongThread);
        EXPECT_EQ(ranCount, 0u);
    }

    // Holding Game does not allow pumping Render.
    {
        const ScopedNamedThread gameGuard(NamedThread::Game);
        EXPECT_EQ(dispatcher.pump(NamedThread::Render, kPumpAllTasks, executedCount), ThreadRoleResult::WrongThread);
        EXPECT_EQ(ranCount, 0u);
    }

    // Holding Render succeeds and drains the task.
    {
        const ScopedNamedThread renderGuard(NamedThread::Render);
        EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::Render, executedCount), ThreadRoleResult::Success);
        EXPECT_EQ(executedCount, 1u);
        EXPECT_EQ(ranCount, 1u);
    }
}

TEST(NamedThreadDispatcherTest, FifoOrderAndBoundedPumpCount)
{
    NamedThreadDispatcher dispatcher;
    ASSERT_EQ(dispatcher.initialize(), ThreadRoleResult::Success);

    const ScopedNamedThread rhiGuard(NamedThread::RHI);
    ASSERT_TRUE(rhiGuard.isAttached());

    ExecutionLog log{};
    RecordItem items[5]{
        { &log, 10u },
        { &log, 20u },
        { &log, 30u },
        { &log, 40u },
        { &log, 50u },
    };

    for (RecordItem& item: items)
    {
        ASSERT_EQ(dispatcher.dispatch(NamedThread::RHI, &recordValueTask, &item), ThreadRoleResult::Success);
    }
    EXPECT_EQ(dispatcher.getPendingTaskCountApprox(NamedThread::RHI), 5u);

    // Zero maxTaskCount runs nothing.
    gp::UInt32 executedCount = 99u;
    EXPECT_EQ(dispatcher.pump(NamedThread::RHI, 0u, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 0u);
    EXPECT_EQ(log.count, 0u);

    // Pump first 2 tasks.
    EXPECT_EQ(dispatcher.pump(NamedThread::RHI, 2u, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 2u);
    EXPECT_EQ(log.count, 2u);
    EXPECT_EQ(log.values[0], 10u);
    EXPECT_EQ(log.values[1], 20u);
    EXPECT_EQ(log.observedRoles[0], ThreadRole::RHI);
    EXPECT_EQ(dispatcher.getPendingTaskCountApprox(NamedThread::RHI), 3u);

    // Pump remaining 3 tasks with pumpUntilIdle.
    EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::RHI, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 3u);
    ASSERT_EQ(log.count, 5u);
    EXPECT_EQ(log.values[2], 30u);
    EXPECT_EQ(log.values[3], 40u);
    EXPECT_EQ(log.values[4], 50u);
    EXPECT_EQ(dispatcher.getPendingTaskCountApprox(NamedThread::RHI), 0u);

    // Pumping an empty queue succeeds with 0 executed.
    EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::RHI, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 0u);
}

TEST(NamedThreadDispatcherTest, BoundedPumpCapsSelfRequeueingTasks)
{
    NamedThreadDispatcher dispatcher;
    ASSERT_EQ(dispatcher.initialize(), ThreadRoleResult::Success);

    const ScopedNamedThread renderGuard(NamedThread::Render);
    ASSERT_TRUE(renderGuard.isAttached());

    RequeueContext ctx{};
    ctx.dispatcher = &dispatcher;
    ctx.target = NamedThread::Render;
    ctx.remaining = 10u;

    ASSERT_EQ(dispatcher.dispatch(NamedThread::Render, &requeueSelfTask, &ctx), ThreadRoleResult::Success);

    gp::UInt32 executedCount = 0u;
    EXPECT_EQ(dispatcher.pump(NamedThread::Render, 4u, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 4u);
    EXPECT_EQ(ctx.executed, 4u);
    EXPECT_EQ(dispatcher.getPendingTaskCountApprox(NamedThread::Render), 1u);

    EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::Render, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 7u);
    EXPECT_EQ(ctx.executed, 11u);
}

TEST(NamedThreadDispatcherTest, QueueFullRollsBackCompletionCounter)
{
    NamedThreadDispatcher dispatcher;
    NamedThreadDispatcherDesc desc{};
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::Game)] = 2u;
    ASSERT_EQ(dispatcher.initialize(desc), ThreadRoleResult::Success);

    JobCounter counter;
    ASSERT_EQ(
        dispatcher.dispatch(
            NamedThread::Game,
            [](void*)
    {
    },
            nullptr,
            &counter
        ),
        ThreadRoleResult::Success
    );
    ASSERT_EQ(
        dispatcher.dispatch(
            NamedThread::Game,
            [](void*)
    {
    },
            nullptr,
            &counter
        ),
        ThreadRoleResult::Success
    );
    EXPECT_EQ(counter.getCount(), 2u);

    // Third dispatch overflows capacity 2 and must roll the counter back to 2.
    EXPECT_EQ(
        dispatcher.dispatch(
            NamedThread::Game,
            [](void*)
    {
    },
            nullptr,
            &counter
        ),
        ThreadRoleResult::QueueFull
    );
    EXPECT_EQ(counter.getCount(), 2u);

    const ScopedNamedThread gameGuard(NamedThread::Game);
    gp::UInt32 executedCount = 0u;
    EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::Game, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 2u);
    EXPECT_TRUE(counter.isComplete());
}

TEST(NamedThreadDispatcherTest, MultiProducerCrossThreadDispatchAndWait)
{
    NamedThreadDispatcher dispatcher;
    NamedThreadDispatcherDesc desc{};
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::Render)] = 128u;
    ASSERT_EQ(dispatcher.initialize(desc), ThreadRoleResult::Success);

    CrossThreadDispatchContext ctx{};
    ctx.dispatcher = &dispatcher;

    gp::platform::Thread consumer{};
    gp::platform::ThreadDesc consumerDesc{};
    consumerDesc.name = "GP_RenderConsumerTestThread";
    consumerDesc.entry = &renderConsumerThreadEntry;
    consumerDesc.userData = &ctx;
    ASSERT_EQ(consumer.start(consumerDesc), gp::platform::ThreadResult::Success);

    gp::platform::Thread producers[CrossThreadDispatchContext::kProducerCount]{};
    for (gp::UInt32 i = 0u; i < CrossThreadDispatchContext::kProducerCount; ++i)
    {
        gp::platform::ThreadDesc prodDesc{};
        prodDesc.name = "GP_DispatchProducerTestThread";
        prodDesc.entry = &producerThreadEntry;
        prodDesc.userData = &ctx;
        ASSERT_EQ(producers[i].start(prodDesc), gp::platform::ThreadResult::Success);
    }

    while (ctx.producersReady.load(gp::MemoryOrder::Acquire) < CrossThreadDispatchContext::kProducerCount)
    {
        gp::platform::Thread::spinWait();
    }
    ctx.producersStart.store(1u, gp::MemoryOrder::Release);

    for (gp::platform::Thread& producer: producers)
    {
        gp::UInt32 exitCode = 1u;
        EXPECT_EQ(producer.join(&exitCode), gp::platform::ThreadResult::Success);
        EXPECT_EQ(exitCode, 0u);
    }

    // Wait on the JobCounter until the render consumer finishes every dispatched task.
    EXPECT_EQ(ctx.completionCounter.wait(0u, JobWaitMode::Auto), JobSyncResult::Success);
    ctx.workerDone.store(1u, gp::MemoryOrder::Release);

    gp::UInt32 consumerExit = 1u;
    EXPECT_EQ(consumer.join(&consumerExit), gp::platform::ThreadResult::Success);
    EXPECT_EQ(consumerExit, 0u);

    EXPECT_EQ(ctx.dispatchFailures.load(gp::MemoryOrder::Acquire), 0u);
    EXPECT_EQ(ctx.executedCount.load(gp::MemoryOrder::Acquire), CrossThreadDispatchContext::kTotalTasks);
    EXPECT_EQ(ctx.executedSum.load(gp::MemoryOrder::Acquire), CrossThreadDispatchContext::kTotalTasks);
    EXPECT_TRUE(ctx.completionCounter.isComplete());

    dispatcher.shutdown();
}

TEST(NamedThreadDispatcherTest, CapacityRoundingAndExcessiveCapacityRollback)
{
    NamedThreadDispatcher dispatcher;
    NamedThreadDispatcherDesc desc{};
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::Game)] = 1u;
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::Render)] = 5u;
    desc.queueCapacities[static_cast<gp::USize>(NamedThread::RHI)] = ~static_cast<gp::USize>(0u);

    // RHI fails on excessive capacity after Game and Render were initialized; both must roll back.
    EXPECT_EQ(dispatcher.initialize(desc), ThreadRoleResult::InvalidArguments);
    EXPECT_FALSE(dispatcher.isInitialized());
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Game), 0u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Render), 0u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::RHI), 0u);

    desc.queueCapacities[static_cast<gp::USize>(NamedThread::RHI)] = 17u;
    ASSERT_EQ(dispatcher.initialize(desc), ThreadRoleResult::Success);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Game), 2u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::Render), 8u);
    EXPECT_EQ(dispatcher.getQueueCapacity(NamedThread::RHI), 32u);
    dispatcher.shutdown();
}

TEST(NamedThreadDispatcherTest, QueuesAreIsolatedAndStructOverloadTracksCounter)
{
    NamedThreadDispatcher dispatcher;
    ASSERT_EQ(dispatcher.initialize(), ThreadRoleResult::Success);

    const ScopedNamedThread gameGuard(NamedThread::Game);
    const ScopedNamedThread renderGuard(NamedThread::Render);
    const ScopedNamedThread rhiGuard(NamedThread::RHI);

    ExecutionLog gameLog{};
    ExecutionLog renderLog{};
    ExecutionLog rhiLog{};
    RecordItem gameItem{ &gameLog, 1u };
    RecordItem renderItem{ &renderLog, 2u };
    RecordItem rhiItem{ &rhiLog, 3u };

    JobCounter counter;
    NamedThreadTask gameTask{};
    gameTask.entry = &recordValueTask;
    gameTask.userData = &gameItem;
    gameTask.counter = &counter;

    NamedThreadTask renderTask{};
    renderTask.entry = &recordValueTask;
    renderTask.userData = &renderItem;
    renderTask.counter = &counter;

    NamedThreadTask rhiTask{};
    rhiTask.entry = &recordValueTask;
    rhiTask.userData = &rhiItem;
    rhiTask.counter = &counter;

    ASSERT_EQ(dispatcher.dispatch(NamedThread::Game, gameTask), ThreadRoleResult::Success);
    ASSERT_EQ(dispatcher.dispatch(NamedThread::Render, renderTask), ThreadRoleResult::Success);
    ASSERT_EQ(dispatcher.dispatch(NamedThread::RHI, rhiTask), ThreadRoleResult::Success);
    EXPECT_EQ(counter.getCount(), 3u);

    gp::UInt32 executedCount = 0u;
    EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::Game, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 1u);
    EXPECT_EQ(gameLog.count, 1u);
    EXPECT_EQ(gameLog.values[0], 1u);
    EXPECT_EQ(renderLog.count, 0u);
    EXPECT_EQ(rhiLog.count, 0u);
    EXPECT_EQ(counter.getCount(), 2u);

    EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::Render, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 1u);
    EXPECT_EQ(renderLog.count, 1u);
    EXPECT_EQ(renderLog.values[0], 2u);
    EXPECT_EQ(counter.getCount(), 1u);

    EXPECT_EQ(dispatcher.pumpUntilIdle(NamedThread::RHI, executedCount), ThreadRoleResult::Success);
    EXPECT_EQ(executedCount, 1u);
    EXPECT_EQ(rhiLog.count, 1u);
    EXPECT_EQ(rhiLog.values[0], 3u);
    EXPECT_TRUE(counter.isComplete());
}

}   // namespace gp::thread::tests
