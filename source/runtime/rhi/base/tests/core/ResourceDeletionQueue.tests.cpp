// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ResourceDeletionQueue.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/roles/ScopedThreadRole.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::rhi::resource::tests
{

namespace
{

class QueueTestResource : public Resource
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_constructCount{ 0u };
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    static void resetCounters() noexcept
    {
        s_constructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    explicit QueueTestResource(const ResourceType type = ResourceType::Buffer) noexcept
        : Resource(type)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    virtual ~QueueTestResource() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }
};

class CompositeTestResource final : public QueueTestResource
{
public:
    CompositeTestResource(const ResourceType type, ResourceRef child) noexcept
        : QueueTestResource(type)
        , m_child(static_cast<ResourceRef&&>(child))
    {}

    virtual ~CompositeTestResource() noexcept override = default;

private:
    ResourceRef m_child{};
};

class ResourceDeletionQueueTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
#if GP_RHI_ENABLE_RESOURCE_INFO
        Resource::stopTrackingAllResources();
#endif
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
        ResourceDeletionQueue::flushPendingDeletes(true);
        QueueTestResource::resetCounters();
    }

    virtual void TearDown() override
    {
        ResourceDeletionQueue::flushPendingDeletes(true);
#if GP_RHI_ENABLE_RESOURCE_INFO
        Resource::stopTrackingAllResources();
#endif
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
    }
};

}   // namespace

TEST_F(ResourceDeletionQueueTestFixture, SeparatesImmediateAndExtendedLifetimeQueues)
{
    QueueTestResource* const extendedRes = new QueueTestResource(ResourceType::Texture);
    QueueTestResource* const immediateRes = new QueueTestResource(ResourceType::Buffer);

    {
        ResourceRef extendedRef(extendedRes);
        ResourceRef immediateRef(immediateRes);

        EXPECT_TRUE(extendedRes->allowsLifetimeExtension());
        EXPECT_TRUE(immediateRes->allowsLifetimeExtension());

        immediateRes->disableLifetimeExtension();
        EXPECT_FALSE(immediateRes->allowsLifetimeExtension());
    }

    // Excluding extended-lifetime resources drains only immediateRes.
    gp::Vector<Resource*> firstBatch{};
    ResourceDeletionQueue::gatherResourcesToDelete(firstBatch, false);
    ASSERT_EQ(firstBatch.size(), 1u);
    EXPECT_EQ(firstBatch[0], immediateRes);

    ResourceDeletionQueue::deleteResources(firstBatch);
    EXPECT_EQ(QueueTestResource::s_destructCount.load(), 1u);

    // Including extended-lifetime resources drains extendedRes.
    gp::Vector<Resource*> secondBatch{};
    ResourceDeletionQueue::gatherResourcesToDelete(secondBatch, true);
    ASSERT_EQ(secondBatch.size(), 1u);
    EXPECT_EQ(secondBatch[0], extendedRes);

    ResourceDeletionQueue::deleteResources(secondBatch);
    EXPECT_EQ(QueueTestResource::s_destructCount.load(), 2u);
}

TEST_F(ResourceDeletionQueueTestFixture, GatherDrainsInLifoOrder)
{
    QueueTestResource* const first = new QueueTestResource(ResourceType::Buffer);
    QueueTestResource* const second = new QueueTestResource(ResourceType::Buffer);
    QueueTestResource* const third = new QueueTestResource(ResourceType::Buffer);

    {
        ResourceRef r1(first);
        r1->disableLifetimeExtension();
    }
    {
        ResourceRef r2(second);
        r2->disableLifetimeExtension();
    }
    {
        ResourceRef r3(third);
        r3->disableLifetimeExtension();
    }

    gp::Vector<Resource*> drained{};
    ResourceDeletionQueue::gatherResourcesToDelete(drained, false);
    ASSERT_EQ(drained.size(), 3u);
    EXPECT_EQ(drained[0], third);
    EXPECT_EQ(drained[1], second);
    EXPECT_EQ(drained[2], first);

    ResourceDeletionQueue::deleteResources(drained);
    EXPECT_EQ(QueueTestResource::s_destructCount.load(), 3u);
}

TEST_F(ResourceDeletionQueueTestFixture, DeleteResourcesIgnoresNullEntriesAndEmptyBatches)
{
    gp::Vector<Resource*> emptyBatch{};
    ResourceDeletionQueue::deleteResources(emptyBatch);

    gp::Vector<Resource*> nullBatch{};
    nullBatch.emplaceBack(nullptr);
    nullBatch.emplaceBack(nullptr);
    ResourceDeletionQueue::deleteResources(nullBatch);

    EXPECT_EQ(QueueTestResource::s_destructCount.load(), 0u);
}

TEST_F(ResourceDeletionQueueTestFixture, FlushPendingDeletesDrainsCascadingDestructors)
{
    ResourceRef leaf(new QueueTestResource(ResourceType::VertexShader));
    ResourceRef middle(new CompositeTestResource(ResourceType::BoundShaderState, leaf));
    ResourceRef root(new CompositeTestResource(ResourceType::GraphicsPipelineState, middle));

    leaf.reset();
    middle.reset();
    root.reset();

    // Only root is initially queued; middle and leaf are queued as each parent destructor runs.
    EXPECT_EQ(QueueTestResource::s_constructCount.load(), 3u);
    EXPECT_EQ(QueueTestResource::s_destructCount.load(), 0u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(QueueTestResource::s_destructCount.load(), 3u);

    gp::Vector<Resource*> leftover{};
    ResourceDeletionQueue::gatherResourcesToDelete(leftover, true);
    EXPECT_TRUE(leftover.isEmpty());
}

TEST_F(ResourceDeletionQueueTestFixture, RenderThreadLifetimeExtensionScopesTrackRefCount)
{
    gp::thread::ScopedNamedThread renderRole(gp::thread::NamedThread::Render);
    ASSERT_TRUE(renderRole.isAttached());

    EXPECT_EQ(ResourceDeletionQueue::getLifetimeRefCount(), 0);
    EXPECT_TRUE(ResourceDeletionQueue::shouldIncludeExtendedLifetimeResources());

    {
        ScopedResourceLifetimeExtension outerScope{};
        EXPECT_EQ(ResourceDeletionQueue::getLifetimeRefCount(), 1);
        EXPECT_FALSE(ResourceDeletionQueue::shouldIncludeExtendedLifetimeResources());

        {
            ScopedResourceLifetimeExtension innerScope{};
            EXPECT_EQ(ResourceDeletionQueue::getLifetimeRefCount(), 2);
            EXPECT_FALSE(ResourceDeletionQueue::shouldIncludeExtendedLifetimeResources());
        }

        EXPECT_EQ(ResourceDeletionQueue::getLifetimeRefCount(), 1);
        EXPECT_FALSE(ResourceDeletionQueue::shouldIncludeExtendedLifetimeResources());
    }

    EXPECT_EQ(ResourceDeletionQueue::getLifetimeRefCount(), 0);
    EXPECT_TRUE(ResourceDeletionQueue::shouldIncludeExtendedLifetimeResources());

    // Verify manual multi-ref add/release return signal.
    ResourceDeletionQueue::addLifetimeRef(3);
    EXPECT_EQ(ResourceDeletionQueue::getLifetimeRefCount(), 3);
    EXPECT_FALSE(ResourceDeletionQueue::releaseLifetimeRef(1));
    EXPECT_EQ(ResourceDeletionQueue::getLifetimeRefCount(), 2);
    EXPECT_TRUE(ResourceDeletionQueue::releaseLifetimeRef(2));
    EXPECT_EQ(ResourceDeletionQueue::getLifetimeRefCount(), 0);
    EXPECT_TRUE(ResourceDeletionQueue::shouldIncludeExtendedLifetimeResources());
}

TEST_F(ResourceDeletionQueueTestFixture, ExitBypassPermitsDirectDestructionDuringShutdown)
{
    class DirectDeleteResource final : public QueueTestResource
    {
    public:
        DirectDeleteResource() noexcept
            : QueueTestResource(ResourceType::Viewport)
        {}

        virtual ~DirectDeleteResource() noexcept override = default;
    };

    EXPECT_FALSE(ResourceDeletionQueue::isBypassingDeletionQueueCheckOnExit());
    ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(true);
    EXPECT_TRUE(ResourceDeletionQueue::isBypassingDeletionQueueCheckOnExit());

    DirectDeleteResource* const direct = new DirectDeleteResource();
    delete direct;

    EXPECT_EQ(QueueTestResource::s_constructCount.load(), 1u);
    EXPECT_EQ(QueueTestResource::s_destructCount.load(), 1u);

    ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
    EXPECT_FALSE(ResourceDeletionQueue::isBypassingDeletionQueueCheckOnExit());
}

TEST_F(ResourceDeletionQueueTestFixture, ConcurrencyStressMultiProducerDeferredDeletion)
{
    constexpr gp::UInt32 kProducerThreads = 6u;
    constexpr gp::UInt32 kResourcesPerThread = 2000u;
    constexpr gp::UInt32 kExpectedTotal = kProducerThreads * kResourcesPerThread;

    gp::thread::Atomic<bool> startLatch{ false };
    gp::thread::Atomic<gp::UInt32> finishedProducers{ 0u };

#if GP_RHI_ENABLE_RESOURCE_INFO
    Resource::startTrackingAllResources();
#endif

    std::vector<std::thread> producers;
    producers.reserve(kProducerThreads);

    for (gp::UInt32 threadIndex = 0u; threadIndex < kProducerThreads; ++threadIndex)
    {
        producers.emplace_back([threadIndex, &startLatch, &finishedProducers]()
        {
            while (!startLatch.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kResourcesPerThread; ++i)
            {
                const ResourceType type = ((i & 3u) == 0u) ? ResourceType::GraphicsPipelineState : ResourceType::Buffer;
                QueueTestResource* const raw = new QueueTestResource(type);
                ResourceRef handle(raw);

                if (((threadIndex + i) & 1u) == 0u)
                {
                    raw->disableLifetimeExtension();
                }

                // Exercise concurrent addRef/release pair before final release.
                ResourceRef alias = handle;
                alias.reset();
                handle.reset();
            }

            finishedProducers.fetchAdd(1u, gp::memory::MemoryOrder::Release);
        });
    }

    std::thread consumer([&startLatch, &finishedProducers]()
    {
        while (!startLatch.load(gp::memory::MemoryOrder::Acquire))
        {
            std::this_thread::yield();
        }

        gp::Vector<Resource*> batch{};
        while (finishedProducers.load(gp::memory::MemoryOrder::Acquire) < kProducerThreads)
        {
            batch.clear();
            ResourceDeletionQueue::gatherResourcesToDelete(batch, true);
            if (!batch.isEmpty())
            {
                ResourceDeletionQueue::deleteResources(batch);
            }
            else
            {
                std::this_thread::yield();
            }
        }

        ResourceDeletionQueue::flushPendingDeletes(true);
    });

    startLatch.store(true, gp::memory::MemoryOrder::Release);

    for (std::thread& producer: producers)
    {
        producer.join();
    }
    consumer.join();

    // Perform a final flush to guarantee every producer item was collected.
    ResourceDeletionQueue::flushPendingDeletes(true);

    EXPECT_EQ(QueueTestResource::s_constructCount.load(gp::memory::MemoryOrder::Acquire), kExpectedTotal);
    EXPECT_EQ(QueueTestResource::s_destructCount.load(gp::memory::MemoryOrder::Acquire), kExpectedTotal);

#if GP_RHI_ENABLE_RESOURCE_INFO
    EXPECT_EQ(Resource::getTrackedResourceCount(), 0u);
    Resource::stopTrackingAllResources();
#endif
}

}   // namespace gp::rhi::resource::tests
