// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "containers/strings/Name.hpp"
#include "core/GPUMask.hpp"
#include "core/Resource.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "sync/GPUFence.hpp"
#include "sync/RenderQuery.hpp"
#include "sync/RenderQueryPool.hpp"
#include "sync/SyncForward.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>
#include <utility>

namespace gp::rhi::sync::tests
{

static_assert(gp::concepts::IsRefCounted<GPUFence>, "GPUFence must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<RenderQuery>, "RenderQuery must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<RenderQueryPool>, "RenderQueryPool must satisfy IsRefCounted.");

static_assert(sizeof(GPUFenceRef) == sizeof(GPUFence*), "GPUFenceRef must remain a single pointer.");
static_assert(sizeof(RenderQueryRef) == sizeof(RenderQuery*), "RenderQueryRef must remain a single pointer.");
static_assert(
    sizeof(RenderQueryPoolRef) == sizeof(RenderQueryPool*), "RenderQueryPoolRef must remain a single pointer."
);

static_assert(!std::is_default_constructible_v<GPUFence>, "GPUFence must not be default constructible.");
static_assert(!std::is_copy_constructible_v<GPUFence>, "GPUFence must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<GPUFence>, "GPUFence must not be copy assignable.");
static_assert(!std::is_move_constructible_v<GPUFence>, "GPUFence must not be move constructible.");
static_assert(!std::is_move_assignable_v<GPUFence>, "GPUFence must not be move assignable.");

static_assert(!std::is_copy_constructible_v<RenderQuery>, "RenderQuery must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<RenderQuery>, "RenderQuery must not be copy assignable.");
static_assert(!std::is_move_constructible_v<RenderQuery>, "RenderQuery must not be move constructible.");
static_assert(!std::is_move_assignable_v<RenderQuery>, "RenderQuery must not be move assignable.");

static_assert(!std::is_copy_constructible_v<RenderQueryPool>, "RenderQueryPool must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<RenderQueryPool>, "RenderQueryPool must not be copy assignable.");
static_assert(!std::is_move_constructible_v<RenderQueryPool>, "RenderQueryPool must not be move constructible.");
static_assert(!std::is_move_assignable_v<RenderQueryPool>, "RenderQueryPool must not be move assignable.");

static_assert(std::is_default_constructible_v<PooledRenderQuery>, "PooledRenderQuery must be default constructible.");
static_assert(!std::is_copy_constructible_v<PooledRenderQuery>, "PooledRenderQuery must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<PooledRenderQuery>, "PooledRenderQuery must not be copy assignable.");
static_assert(std::is_move_constructible_v<PooledRenderQuery>, "PooledRenderQuery must be move constructible.");
static_assert(std::is_move_assignable_v<PooledRenderQuery>, "PooledRenderQuery must be move assignable.");

namespace
{

class MockGPUFence final : public GPUFence
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    explicit MockGPUFence(const gp::Name name) noexcept
        : GPUFence(name)
    {}

    virtual ~MockGPUFence() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc GPUFence::clear()
    virtual void clear() noexcept override
    {
        m_signaled.store(false, gp::memory::MemoryOrder::Release);
        m_clearCallCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc GPUFence::poll()
    [[nodiscard]] virtual bool poll() const noexcept override
    {
        return m_signaled.load(gp::memory::MemoryOrder::Acquire);
    }

    /// @copydoc GPUFence::wait()
    virtual void wait(command::CommandListImmediate& cmdList, const GPUMask gpuMask) const noexcept override
    {
        m_lastWaitCmdList = static_cast<const void*>(&cmdList);
        m_lastWaitMask = gpuMask;
        m_waitCallCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        m_signaled.store(true, gp::memory::MemoryOrder::Release);
    }

    void signal() noexcept
    {
        m_signaled.store(true, gp::memory::MemoryOrder::Release);
    }

    [[nodiscard]] gp::UInt32 getClearCallCount() const noexcept
    {
        return m_clearCallCount.load(gp::memory::MemoryOrder::Acquire);
    }

    [[nodiscard]] gp::UInt32 getWaitCallCount() const noexcept
    {
        return m_waitCallCount.load(gp::memory::MemoryOrder::Acquire);
    }

    [[nodiscard]] const void* getLastWaitCmdList() const noexcept
    {
        return m_lastWaitCmdList;
    }

    [[nodiscard]] GPUMask getLastWaitMask() const noexcept
    {
        return m_lastWaitMask;
    }

private:
    mutable gp::thread::Atomic<bool> m_signaled{ false };
    gp::thread::Atomic<gp::UInt32> m_clearCallCount{ 0u };
    mutable gp::thread::Atomic<gp::UInt32> m_waitCallCount{ 0u };
    mutable const void* m_lastWaitCmdList{ nullptr };
    mutable GPUMask m_lastWaitMask{ 0u };
};

class MockRenderQuery final : public RenderQuery
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_constructCount{ 0u };
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    explicit MockRenderQuery(const gp::UInt32 id = 0u) noexcept
        : m_id(id)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    virtual ~MockRenderQuery() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    [[nodiscard]] gp::UInt32 getId() const noexcept
    {
        return m_id;
    }

private:
    gp::UInt32 m_id{ 0u };
};

class MockRenderQueryPool final : public RenderQueryPool
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    MockRenderQueryPool() noexcept = default;

    virtual ~MockRenderQueryPool() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc RenderQueryPool::allocateQuery()
    [[nodiscard]] virtual PooledRenderQuery allocateQuery() noexcept override
    {
        ++m_allocateCalls;
        if (!m_freeQueries.isEmpty())
        {
            RenderQueryRef reused = std::move(m_freeQueries.back());
            m_freeQueries.popBack();
            ++m_reuseCount;
            return PooledRenderQuery(this, std::move(reused));
        }

        RenderQueryRef fresh(new MockRenderQuery(++m_nextQueryId));
        return PooledRenderQuery(this, std::move(fresh));
    }

    [[nodiscard]] gp::USize getFreeCount() const noexcept
    {
        return m_freeQueries.size();
    }

    [[nodiscard]] gp::UInt32 getAllocateCallCount() const noexcept
    {
        return m_allocateCalls;
    }

    [[nodiscard]] gp::UInt32 getReleaseCallCount() const noexcept
    {
        return m_releaseCalls;
    }

    [[nodiscard]] gp::UInt32 getReuseCount() const noexcept
    {
        return m_reuseCount;
    }

    [[nodiscard]] gp::UInt32 getCreatedQueryCount() const noexcept
    {
        return m_nextQueryId;
    }

private:
    /// @copydoc RenderQueryPool::releaseQuery()
    virtual void releaseQuery(RenderQueryRef&& query) noexcept override
    {
        ++m_releaseCalls;
        m_freeQueries.pushBack(std::move(query));
    }

private:
    gp::Vector<RenderQueryRef> m_freeQueries{};
    gp::UInt32 m_nextQueryId{ 0u };
    gp::UInt32 m_allocateCalls{ 0u };
    gp::UInt32 m_releaseCalls{ 0u };
    gp::UInt32 m_reuseCount{ 0u };
};

class GPUFenceAndQueryTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        MockGPUFence::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        MockRenderQuery::s_constructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        MockRenderQuery::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        MockRenderQueryPool::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }
};

}   // namespace

TEST_F(GPUFenceAndQueryTestFixture, GPUFenceReportsTypeNameAndSignalLifecycle)
{
    const gp::Name fenceName("ShadowPassFence");
    MockGPUFence* const rawFence = new MockGPUFence(fenceName);
    GPUFenceRef fence(rawFence);

    EXPECT_EQ(fence->getType(), resource::ResourceType::GPUFence);
    EXPECT_EQ(fence->getName(), fenceName);
    EXPECT_FALSE(fence->poll());
    EXPECT_FALSE(fence->poll(GPUMask::gpu0()));

    rawFence->signal();
    EXPECT_TRUE(fence->poll());
    EXPECT_TRUE(fence->poll(GPUMask::gpu0()));

    fence->clear();
    EXPECT_EQ(rawFence->getClearCallCount(), 1u);
    EXPECT_FALSE(fence->poll());
    EXPECT_FALSE(fence->poll(GPUMask::gpu0()));

    alignas(8) gp::UInt8 dummyCmdListStorage[8]{};
    command::CommandListImmediate& dummyCmdList =
        *reinterpret_cast<command::CommandListImmediate*>(dummyCmdListStorage);
    const GPUMask waitMask = GPUMask::fromIndex(2u);

    fence->wait(dummyCmdList, waitMask);
    EXPECT_EQ(rawFence->getWaitCallCount(), 1u);
    EXPECT_EQ(rawFence->getLastWaitCmdList(), static_cast<const void*>(dummyCmdListStorage));
    EXPECT_EQ(rawFence->getLastWaitMask(), waitMask);
    EXPECT_TRUE(fence->poll());
    EXPECT_TRUE(fence->poll(GPUMask::gpu0()));

    // Upcast to ResourceRef exercises shared intrusive reference counting.
    resource::ResourceRef baseRef = fence;
    EXPECT_EQ(fence->getRefCount(), 2u);
    baseRef.reset();
    EXPECT_EQ(fence->getRefCount(), 1u);
}

TEST_F(GPUFenceAndQueryTestFixture, GPUFenceDefaultNoneNameAndPendingWriteCounterSingleThreaded)
{
    GPUFenceRef fence(new MockGPUFence(gp::Name{}));

    EXPECT_TRUE(fence->getName().isNone());
    EXPECT_EQ(fence->getNumPendingWriteCommands(), 0);
    EXPECT_FALSE(fence->hasPendingWriteCommands());

    EXPECT_EQ(fence->incrementPendingWriteCommands(), 1);
    EXPECT_EQ(fence->getNumPendingWriteCommands(), 1);
    EXPECT_TRUE(fence->hasPendingWriteCommands());

    EXPECT_EQ(fence->incrementPendingWriteCommands(), 2);
    EXPECT_EQ(fence->incrementPendingWriteCommands(), 3);
    EXPECT_EQ(fence->getNumPendingWriteCommands(), 3);
    EXPECT_TRUE(fence->hasPendingWriteCommands());

    EXPECT_EQ(fence->decrementPendingWriteCommands(), 2);
    EXPECT_EQ(fence->decrementPendingWriteCommands(), 1);
    EXPECT_TRUE(fence->hasPendingWriteCommands());

    EXPECT_EQ(fence->decrementPendingWriteCommands(), 0);
    EXPECT_EQ(fence->getNumPendingWriteCommands(), 0);
    EXPECT_FALSE(fence->hasPendingWriteCommands());
}

TEST_F(GPUFenceAndQueryTestFixture, GPUFencePendingWriteCounterMultiThreaded)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::Int32 kIterationsPerThread = 5000;

    GPUFenceRef fence(new MockGPUFence(gp::Name("ThreadedFence")));
    gp::thread::Atomic<gp::UInt32> readyCount{ 0u };
    gp::thread::Atomic<bool> startFlag{ false };

    // Phase 1: concurrent increments across all threads.
    {
        std::thread workers[kThreadCount];
        for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
        {
            workers[t] = std::thread([&fence, &readyCount, &startFlag]()
            {
                readyCount.fetchAdd(1u, gp::memory::MemoryOrder::Release);
                while (!startFlag.load(gp::memory::MemoryOrder::Acquire))
                {
                    std::this_thread::yield();
                }

                for (gp::Int32 i = 0; i < kIterationsPerThread; ++i)
                {
                    fence->incrementPendingWriteCommands();
                }
            });
        }

        while (readyCount.load(gp::memory::MemoryOrder::Acquire) < kThreadCount)
        {
            std::this_thread::yield();
        }
        startFlag.store(true, gp::memory::MemoryOrder::Release);

        for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
        {
            workers[t].join();
        }
    }

    const gp::Int32 expectedTotal = static_cast<gp::Int32>(kThreadCount) * kIterationsPerThread;
    EXPECT_EQ(fence->getNumPendingWriteCommands(), expectedTotal);
    EXPECT_TRUE(fence->hasPendingWriteCommands());

    // Phase 2: concurrent decrements across all threads returning the counter to zero.
    readyCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    startFlag.store(false, gp::memory::MemoryOrder::SeqCst);
    {
        std::thread workers[kThreadCount];
        for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
        {
            workers[t] = std::thread([&fence, &readyCount, &startFlag]()
            {
                readyCount.fetchAdd(1u, gp::memory::MemoryOrder::Release);
                while (!startFlag.load(gp::memory::MemoryOrder::Acquire))
                {
                    std::this_thread::yield();
                }

                for (gp::Int32 i = 0; i < kIterationsPerThread; ++i)
                {
                    fence->decrementPendingWriteCommands();
                }
            });
        }

        while (readyCount.load(gp::memory::MemoryOrder::Acquire) < kThreadCount)
        {
            std::this_thread::yield();
        }
        startFlag.store(true, gp::memory::MemoryOrder::Release);

        for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
        {
            workers[t].join();
        }
    }

    EXPECT_EQ(fence->getNumPendingWriteCommands(), 0);
    EXPECT_FALSE(fence->hasPendingWriteCommands());
}

TEST_F(GPUFenceAndQueryTestFixture, GPUFenceDeferredDeletionLifecycle)
{
    {
        GPUFenceRef fence(new MockGPUFence(gp::Name("DeferredFence")));
        EXPECT_EQ(MockGPUFence::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);
    }

    // Releasing the final GPUFenceRef queues deletion instead of destroying inline.
    EXPECT_EQ(MockGPUFence::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);

    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockGPUFence::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 1u);
}

TEST_F(GPUFenceAndQueryTestFixture, RenderQueryTypeRefCountingAndDeferredDeletion)
{
    {
        RenderQueryRef query(new RenderQuery());
        EXPECT_EQ(query->getType(), resource::ResourceType::RenderQuery);
        EXPECT_EQ(query->getRefCount(), 1u);

        resource::ResourceRef baseRef = query;
        EXPECT_EQ(query->getRefCount(), 2u);
        baseRef.reset();
        EXPECT_EQ(query->getRefCount(), 1u);
    }

    {
        RenderQueryRef mockQuery(new MockRenderQuery(42u));
        EXPECT_EQ(mockQuery->getType(), resource::ResourceType::RenderQuery);
        EXPECT_EQ(MockRenderQuery::s_constructCount.load(gp::memory::MemoryOrder::Acquire), 1u);
        EXPECT_EQ(MockRenderQuery::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);
    }

    EXPECT_EQ(MockRenderQuery::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);

    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockRenderQuery::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 1u);
}

TEST_F(GPUFenceAndQueryTestFixture, PooledRenderQueryDefaultAndUnpooledRelease)
{
    PooledRenderQuery emptyHandle{};
    EXPECT_FALSE(emptyHandle.isValid());
    EXPECT_EQ(emptyHandle.getQuery(), nullptr);

    // Calling releaseQuery() on an empty handle is a safe no-op.
    emptyHandle.releaseQuery();
    EXPECT_FALSE(emptyHandle.isValid());
    EXPECT_EQ(emptyHandle.getQuery(), nullptr);

    // Wrapping a query with a null pool still releases its reference cleanly.
    {
        RenderQueryRef standalone(new MockRenderQuery(7u));
        PooledRenderQuery unpooled(nullptr, std::move(standalone));
        EXPECT_FALSE(standalone.isValid());
        EXPECT_TRUE(unpooled.isValid());
        ASSERT_NE(unpooled.getQuery(), nullptr);

        unpooled.releaseQuery();
        EXPECT_FALSE(unpooled.isValid());
        EXPECT_EQ(unpooled.getQuery(), nullptr);
    }

    EXPECT_EQ(MockRenderQuery::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockRenderQuery::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 1u);
}

TEST_F(GPUFenceAndQueryTestFixture, RenderQueryPoolAllocateReuseAndRAIIAutoReturn)
{
    MockRenderQueryPool* const rawPool = new MockRenderQueryPool();
    RenderQueryPoolRef pool(rawPool);

    EXPECT_EQ(pool->getType(), resource::ResourceType::RenderQueryPool);
    EXPECT_EQ(rawPool->getFreeCount(), 0u);

    RenderQuery* firstPtr = nullptr;
    {
        PooledRenderQuery leased = pool->allocateQuery();
        EXPECT_TRUE(leased.isValid());
        ASSERT_NE(leased.getQuery(), nullptr);
        firstPtr = leased.getQuery();
        EXPECT_EQ(firstPtr->getType(), resource::ResourceType::RenderQuery);
        EXPECT_EQ(rawPool->getAllocateCallCount(), 1u);
        EXPECT_EQ(rawPool->getReleaseCallCount(), 0u);
        EXPECT_EQ(rawPool->getFreeCount(), 0u);
    }

    // Exiting scope returned the leased query to the pool automatically.
    EXPECT_EQ(rawPool->getReleaseCallCount(), 1u);
    EXPECT_EQ(rawPool->getFreeCount(), 1u);

    // Allocating again reuses the recycled RenderQuery instead of creating a second one.
    PooledRenderQuery secondLease = pool->allocateQuery();
    EXPECT_TRUE(secondLease.isValid());
    EXPECT_EQ(secondLease.getQuery(), firstPtr);
    EXPECT_EQ(rawPool->getCreatedQueryCount(), 1u);
    EXPECT_EQ(rawPool->getReuseCount(), 1u);
    EXPECT_EQ(rawPool->getFreeCount(), 0u);

    // Explicit releaseQuery() returns immediately and is idempotent on repeat calls.
    secondLease.releaseQuery();
    EXPECT_FALSE(secondLease.isValid());
    EXPECT_EQ(secondLease.getQuery(), nullptr);
    EXPECT_EQ(rawPool->getReleaseCallCount(), 2u);
    EXPECT_EQ(rawPool->getFreeCount(), 1u);

    secondLease.releaseQuery();
    EXPECT_EQ(rawPool->getReleaseCallCount(), 2u);
    EXPECT_EQ(rawPool->getFreeCount(), 1u);
}

TEST_F(GPUFenceAndQueryTestFixture, PooledRenderQueryMoveConstructorAndMoveAssignment)
{
    MockRenderQueryPool* const rawPool = new MockRenderQueryPool();
    RenderQueryPoolRef pool(rawPool);

    PooledRenderQuery first = pool->allocateQuery();
    ASSERT_TRUE(first.isValid());
    RenderQuery* const firstPtr = first.getQuery();

    // Move construction transfers ownership without releasing back to the pool.
    PooledRenderQuery movedConstructed(std::move(first));
    EXPECT_FALSE(first.isValid());
    EXPECT_EQ(first.getQuery(), nullptr);
    EXPECT_TRUE(movedConstructed.isValid());
    EXPECT_EQ(movedConstructed.getQuery(), firstPtr);
    EXPECT_EQ(rawPool->getReleaseCallCount(), 0u);

    // Allocate a second distinct query while the first is still leased.
    PooledRenderQuery second = pool->allocateQuery();
    ASSERT_TRUE(second.isValid());
    RenderQuery* const secondPtr = second.getQuery();
    EXPECT_NE(secondPtr, firstPtr);
    EXPECT_EQ(rawPool->getCreatedQueryCount(), 2u);
    EXPECT_EQ(rawPool->getFreeCount(), 0u);

    // Move assignment must return movedConstructed's old query before taking over second.
    movedConstructed = std::move(second);
    EXPECT_FALSE(second.isValid());
    EXPECT_EQ(second.getQuery(), nullptr);
    EXPECT_TRUE(movedConstructed.isValid());
    EXPECT_EQ(movedConstructed.getQuery(), secondPtr);
    EXPECT_EQ(rawPool->getReleaseCallCount(), 1u);
    EXPECT_EQ(rawPool->getFreeCount(), 1u);

    // Re-allocating retrieves firstPtr that was released by the move assignment.
    PooledRenderQuery recycled = pool->allocateQuery();
    EXPECT_EQ(recycled.getQuery(), firstPtr);
    EXPECT_EQ(rawPool->getFreeCount(), 0u);

    // Self-move assignment leaves the handle valid and does not release the query.
    PooledRenderQuery& selfAlias = movedConstructed;
    movedConstructed = std::move(selfAlias);
    EXPECT_TRUE(movedConstructed.isValid());
    EXPECT_EQ(movedConstructed.getQuery(), secondPtr);
    EXPECT_EQ(rawPool->getReleaseCallCount(), 1u);

    // Move-assigning an empty handle releases the currently held query.
    PooledRenderQuery empty{};
    movedConstructed = std::move(empty);
    EXPECT_FALSE(movedConstructed.isValid());
    EXPECT_EQ(movedConstructed.getQuery(), nullptr);
    EXPECT_EQ(rawPool->getReleaseCallCount(), 2u);
    EXPECT_EQ(rawPool->getFreeCount(), 1u);

    recycled.releaseQuery();
    EXPECT_EQ(rawPool->getReleaseCallCount(), 3u);
    EXPECT_EQ(rawPool->getFreeCount(), 2u);
}

TEST_F(GPUFenceAndQueryTestFixture, RenderQueryPoolDeferredDeletionCascadesToCachedQueries)
{
    {
        RenderQueryPoolRef pool(new MockRenderQueryPool());
        {
            PooledRenderQuery q0 = pool->allocateQuery();
            PooledRenderQuery q1 = pool->allocateQuery();
            ASSERT_TRUE(q0.isValid());
            ASSERT_TRUE(q1.isValid());
        }

        EXPECT_EQ(MockRenderQuery::s_constructCount.load(gp::memory::MemoryOrder::Acquire), 2u);
        EXPECT_EQ(MockRenderQuery::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);
        EXPECT_EQ(MockRenderQueryPool::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);
    }

    // Releasing the pool defers destruction until ResourceDeletionQueue flushes.
    EXPECT_EQ(MockRenderQueryPool::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);
    EXPECT_EQ(MockRenderQuery::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 0u);

    // Flush drains the pool first, then drains the cached RenderQuery references released by ~MockRenderQueryPool.
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockRenderQueryPool::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 1u);
    EXPECT_EQ(MockRenderQuery::s_destructCount.load(gp::memory::MemoryOrder::Acquire), 2u);
}

}   // namespace gp::rhi::sync::tests
