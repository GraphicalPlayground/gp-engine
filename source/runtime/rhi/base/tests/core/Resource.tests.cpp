// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/Resource.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "containers/strings/Name.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceTypes.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace gp::rhi::resource::tests
{

static_assert(gp::concepts::IsRefCounted<Resource>, "Resource must satisfy gp::concepts::IsRefCounted.");
static_assert(
    gp::concepts::HasRefCountQuery<gp::DefaultRefCountPolicy<Resource>, Resource>,
    "Resource must satisfy gp::concepts::HasRefCountQuery."
);
static_assert(sizeof(ResourceRef) == sizeof(Resource*), "ResourceRef must remain a single pointer.");
static_assert(!std::is_copy_constructible_v<Resource>, "Resource must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<Resource>, "Resource must not be copy assignable.");
static_assert(!std::is_move_constructible_v<Resource>, "Resource must not be move constructible.");
static_assert(!std::is_move_assignable_v<Resource>, "Resource must not be move assignable.");

namespace
{

class TestResource final : public Resource
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_constructCount{ 0u };
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    static void resetCounters() noexcept
    {
        s_constructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    explicit TestResource(const ResourceType type = ResourceType::Buffer, const gp::UInt32 id = 0u) noexcept
        : Resource(type)
        , m_id(id)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    virtual ~TestResource() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    void setCommittedForTest(const bool committed) noexcept
    {
        setCommitted(committed);
    }

    [[nodiscard]] gp::UInt32 getId() const noexcept
    {
        return m_id;
    }

#if GP_RHI_ENABLE_RESOURCE_INFO
    void setCustomResourceInfo(const ResourceInfo& info) noexcept
    {
        m_customInfo = info;
        m_hasCustomInfo = true;
    }

    virtual bool getResourceInfo(ResourceInfo& outResourceInfo) const noexcept override
    {
        if (!m_hasCustomInfo)
        {
            return Resource::getResourceInfo(outResourceInfo);
        }

        outResourceInfo = m_customInfo;
        return true;
    }
#endif

private:
    gp::UInt32 m_id{ 0u };
#if GP_RHI_ENABLE_RESOURCE_INFO
    ResourceInfo m_customInfo{};
    bool m_hasCustomInfo{ false };
#endif
};

class ResourceTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
#if GP_RHI_ENABLE_RESOURCE_INFO
        Resource::stopTrackingAllResources();
#endif
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
        ResourceDeletionQueue::flushPendingDeletes(true);
        TestResource::resetCounters();
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

TEST_F(ResourceTestFixture, InitialStateMatchesUnrealDefaults)
{
    TestResource* const raw = new TestResource(ResourceType::Texture, 7u);

    EXPECT_EQ(raw->getType(), ResourceType::Texture);
    EXPECT_EQ(raw->getRefCount(), 0u);
    EXPECT_FALSE(raw->isValid());
    EXPECT_FALSE(raw->isMarkedForDelete());
    EXPECT_TRUE(raw->isCommitted());
    EXPECT_TRUE(raw->allowsLifetimeExtension());
    EXPECT_TRUE(raw->getOwnerName().isNone());

    raw->setCommittedForTest(false);
    EXPECT_FALSE(raw->isCommitted());

    // Acquire and release once so teardown runs through the deferred deletion queue.
    ResourceRef ref(raw);
    EXPECT_TRUE(raw->isValid());
    EXPECT_EQ(raw->getRefCount(), 1u);
    ref.reset();

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(TestResource::s_constructCount.load(), 1u);
    EXPECT_EQ(TestResource::s_destructCount.load(), 1u);
}

TEST_F(ResourceTestFixture, RefCountedPtrRetainCopyMoveAndDeferredDelete)
{
    TestResource* const raw = new TestResource(ResourceType::Buffer, 42u);
    {
        ResourceRef first(raw);
        EXPECT_EQ(raw->getRefCount(), 1u);
        EXPECT_TRUE(raw->isValid());

        ResourceRef second = first;
        EXPECT_EQ(raw->getRefCount(), 2u);

        ResourceRef moved(std::move(second));
        EXPECT_FALSE(second.isValid());
        EXPECT_EQ(moved.get(), raw);
        EXPECT_EQ(raw->getRefCount(), 2u);

        first.reset();
        EXPECT_EQ(raw->getRefCount(), 1u);
        EXPECT_FALSE(raw->isMarkedForDelete());
    }

    // Releasing the last reference queues the resource without running the destructor inline.
    EXPECT_EQ(TestResource::s_destructCount.load(), 0u);
    EXPECT_EQ(raw->getRefCount(), 0u);
    EXPECT_FALSE(raw->isValid());
    EXPECT_TRUE(raw->isMarkedForDelete());

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(TestResource::s_destructCount.load(), 1u);
}

TEST_F(ResourceTestFixture, CacheResurrectionPreventsDeletionAndClearsMark)
{
    TestResource* const raw = new TestResource(ResourceType::SamplerState, 11u);

    {
        ResourceRef ref(raw);
        EXPECT_EQ(raw->getRefCount(), 1u);
    }

    EXPECT_TRUE(raw->isMarkedForDelete());
    EXPECT_FALSE(raw->isValid());

    // Strict live-only promotion must reject a resource that is already queued for deletion.
    EXPECT_FALSE(raw->tryAddRefIfLive());
    EXPECT_EQ(raw->getRefCount(), 0u);

    // Cache resurrection succeeds before deleteResources() latches Deleting.
    ASSERT_TRUE(raw->tryAddRefIfNotDeleting());
    ResourceRef resurrected(raw, gp::kAdoptRef);
    EXPECT_EQ(raw->getRefCount(), 1u);
    EXPECT_TRUE(raw->isMarkedForDelete());
    EXPECT_FALSE(raw->isValid());

    // Draining and processing the queue must clear MarkedForDelete instead of deleting the resource.
    gp::Vector<Resource*> pending{};
    ResourceDeletionQueue::gatherResourcesToDelete(pending, true);
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0], raw);

    ResourceDeletionQueue::deleteResources(pending);
    EXPECT_EQ(TestResource::s_destructCount.load(), 0u);
    EXPECT_FALSE(raw->isMarkedForDelete());
    EXPECT_TRUE(raw->isValid());
    EXPECT_EQ(raw->getRefCount(), 1u);

    // Once unmarked and live, tryAddRefIfLive() works again.
    ASSERT_TRUE(raw->tryAddRefIfLive());
    EXPECT_EQ(raw->getRefCount(), 2u);
    EXPECT_EQ(raw->release(), 1u);

    // Releasing the resurrected reference later must enqueue and delete cleanly.
    resurrected.reset();
    EXPECT_TRUE(raw->isMarkedForDelete());

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(TestResource::s_destructCount.load(), 1u);
}

TEST_F(ResourceTestFixture, TransientResurrectionCyclesDoNotDuplicateQueueEntries)
{
    TestResource* const raw = new TestResource(ResourceType::BlendState, 3u);

    {
        ResourceRef initialRef(raw);
    }
    EXPECT_TRUE(raw->isMarkedForDelete());

    // Repeated 0 -> 1 -> 0 cycles while MarkedForDelete is set must not push duplicates onto the stack.
    for (gp::UInt32 cycle = 0u; cycle < 5u; ++cycle)
    {
        ASSERT_TRUE(raw->tryAddRefIfNotDeleting());
        EXPECT_EQ(raw->getRefCount(), 1u);
        EXPECT_EQ(raw->release(), 0u);
    }

    gp::Vector<Resource*> pending{};
    ResourceDeletionQueue::gatherResourcesToDelete(pending, true);
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0], raw);

    ResourceDeletionQueue::deleteResources(pending);
    EXPECT_EQ(TestResource::s_destructCount.load(), 1u);
}

TEST_F(ResourceTestFixture, PipelineStateDeletionCounterTracksOnlyPipelineStates)
{
    const gp::UInt64 baseline = Resource::getNumDeletedPipelineStates();

    TestResource* const gfxPso = new TestResource(ResourceType::GraphicsPipelineState);
    TestResource* const computePso = new TestResource(ResourceType::ComputePipelineState);
    TestResource* const rtPso = new TestResource(ResourceType::RayTracingPipelineState);
    TestResource* const wgPso = new TestResource(ResourceType::WorkGraphPipelineState);
    TestResource* const resurrectedPso = new TestResource(ResourceType::GraphicsPipelineState);
    TestResource* const buffer = new TestResource(ResourceType::Buffer);
    TestResource* const texture = new TestResource(ResourceType::Texture);

    {
        ResourceRef r1(gfxPso);
        ResourceRef r2(computePso);
        ResourceRef r3(rtPso);
        ResourceRef r4(wgPso);
        ResourceRef r5(resurrectedPso);
        ResourceRef r6(buffer);
        ResourceRef r7(texture);
    }

    // Keep one PSO alive across the batch to confirm resurrected PSOs do not bump the counter.
    ASSERT_TRUE(resurrectedPso->tryAddRefIfNotDeleting());
    ResourceRef survivor(resurrectedPso, gp::kAdoptRef);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(Resource::getNumDeletedPipelineStates(), baseline + 4u);
    EXPECT_EQ(TestResource::s_destructCount.load(), 6u);

    survivor.reset();
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(Resource::getNumDeletedPipelineStates(), baseline + 5u);
    EXPECT_EQ(TestResource::s_destructCount.load(), 7u);
}

#if GP_RHI_ENABLE_RESOURCE_INFO

TEST_F(ResourceTestFixture, OwnerNameAndGetResourceInfoBehavior)
{
    TestResource* const raw = new TestResource(ResourceType::Texture, 9u);
    ResourceRef ref(raw);

    const gp::Name owner("ShadowMapAtlas");
    raw->setOwnerName(owner);
    EXPECT_EQ(raw->getOwnerName(), owner);

    // Base getResourceInfo() resets the output struct and returns false.
    ResourceInfo queried{};
    queried.name = gp::Name("StaleName");
    queried.type = ResourceType::Buffer;
    queried.vramAllocation = VRamAllocation(128u, 512u);
    queried.isTransient = true;
    queried.isValid = false;
    queried.isResident = false;

    EXPECT_FALSE(raw->getResourceInfo(queried));
    EXPECT_EQ(queried, ResourceInfo{});

    // Subclass override populates metadata and returns true.
    ResourceInfo expected{};
    expected.name = gp::Name("ShadowMapAtlas_0");
    expected.type = ResourceType::Texture;
    expected.vramAllocation = VRamAllocation(4096u, 16777216u);
    expected.isTransient = false;
    expected.isValid = true;
    expected.isResident = true;
    raw->setCustomResourceInfo(expected);

    EXPECT_TRUE(raw->getResourceInfo(queried));
    EXPECT_EQ(queried, expected);
}

TEST_F(ResourceTestFixture, GlobalTrackingLifecycleAndGather)
{
    EXPECT_FALSE(Resource::isTrackingAllResources());
    EXPECT_EQ(Resource::getTrackedResourceCount(), 0u);

    // Resources created before tracking starts are not tracked.
    ResourceRef untracked(new TestResource(ResourceType::Buffer, 1u));
    EXPECT_FALSE(untracked->isBeingTracked());
    EXPECT_EQ(Resource::getTrackedResourceCount(), 0u);

    Resource::startTrackingAllResources();
    EXPECT_TRUE(Resource::isTrackingAllResources());

    ResourceRef trackedA(new TestResource(ResourceType::Buffer, 2u));
    ResourceRef trackedB(new TestResource(ResourceType::Texture, 3u));
    ResourceRef trackedC(new TestResource(ResourceType::UniformBuffer, 4u));

    EXPECT_TRUE(trackedA->isBeingTracked());
    EXPECT_TRUE(trackedB->isBeingTracked());
    EXPECT_TRUE(trackedC->isBeingTracked());
    EXPECT_EQ(Resource::getTrackedResourceCount(), 3u);

    // Duplicate begin and null calls are no-ops.
    Resource::beginTrackingResource(nullptr);
    Resource::endTrackingResource(nullptr);
    Resource::beginTrackingResource(trackedA.get());
    EXPECT_EQ(Resource::getTrackedResourceCount(), 3u);

    gp::Vector<Resource*> gathered{};
    Resource::gatherTrackedResources(gathered);
    EXPECT_EQ(gathered.size(), 3u);

    // Deleting the middle element unlinks it cleanly on destruction.
    trackedB.reset();
    EXPECT_EQ(Resource::getTrackedResourceCount(), 3u);
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(Resource::getTrackedResourceCount(), 2u);

    // Stopping tracking clears every remaining link and resets the count.
    Resource::stopTrackingAllResources();
    EXPECT_FALSE(Resource::isTrackingAllResources());
    EXPECT_EQ(Resource::getTrackedResourceCount(), 0u);
    EXPECT_FALSE(trackedA->isBeingTracked());
    EXPECT_FALSE(trackedC->isBeingTracked());

    // Subsequent endTrackingResource() calls on unlinked resources are safe no-ops.
    Resource::endTrackingResource(trackedA.get());
    EXPECT_EQ(Resource::getTrackedResourceCount(), 0u);
}

#endif   // GP_RHI_ENABLE_RESOURCE_INFO

}   // namespace gp::rhi::resource::tests
