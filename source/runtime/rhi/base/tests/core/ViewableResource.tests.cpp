// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ViewableResource.hpp"
#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::rhi::resource::tests
{

static_assert(std::is_base_of_v<Resource, ViewableResource>, "ViewableResource must inherit publicly from Resource.");
static_assert(
    gp::concepts::IsRefCounted<ViewableResource>, "ViewableResource must satisfy gp::concepts::IsRefCounted."
);
static_assert(
    gp::concepts::HasRefCountQuery<gp::DefaultRefCountPolicy<ViewableResource>, ViewableResource>,
    "ViewableResource must satisfy gp::concepts::HasRefCountQuery."
);
static_assert(
    sizeof(ViewableResourceRef) == sizeof(ViewableResource*), "ViewableResourceRef must remain a single pointer."
);
static_assert(!std::is_copy_constructible_v<ViewableResource>, "ViewableResource must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<ViewableResource>, "ViewableResource must not be copy assignable.");
static_assert(!std::is_move_constructible_v<ViewableResource>, "ViewableResource must not be move constructible.");
static_assert(!std::is_move_assignable_v<ViewableResource>, "ViewableResource must not be move assignable.");

namespace
{

class MockViewableResource : public ViewableResource
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_constructCount{ 0u };
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    static void resetCounters() noexcept
    {
        s_constructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    MockViewableResource(
        const ResourceType type,
        const sync::ResourceAccess initialAccess,
        const gp::StringView debugName = gp::StringView{},
        const gp::Name ownerName = gp::Name{}
    ) noexcept
        : ViewableResource(type, initialAccess, debugName, ownerName)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    virtual ~MockViewableResource() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    void updateTrackedAccessForTest(const sync::TrackedAccess trackedAccess) noexcept
    {
        setTrackedAccessFromContext(trackedAccess);
    }

    void takeOwnershipForTest(ViewableResource& other) noexcept
    {
        takeOwnership(other);
    }

    void releaseOwnershipForTest() noexcept
    {
        releaseOwnership();
    }

    void renameForTest(const gp::Name newName) noexcept
    {
        setName(newName);
    }
};

class ForwardingViewableResource final : public MockViewableResource
{
public:
    explicit ForwardingViewableResource(MockViewableResource* const target) noexcept
        : MockViewableResource(ResourceType::TextureReference, sync::ResourceAccess::Unknown)
        , m_target(target)
    {}

    virtual ~ForwardingViewableResource() noexcept override = default;

    /// @copydoc ViewableResource::setTrackedAccessFromContext()
    virtual void setTrackedAccessFromContext(const sync::TrackedAccess trackedAccess) noexcept override
    {
        ViewableResource::setTrackedAccessFromContext(trackedAccess);
        if (m_target != nullptr)
        {
            m_target->updateTrackedAccessForTest(trackedAccess);
        }
    }

private:
    MockViewableResource* m_target{ nullptr };
};

class ViewableResourceTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
#if GP_RHI_ENABLE_RESOURCE_INFO
        Resource::stopTrackingAllResources();
#endif
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
        ResourceDeletionQueue::flushPendingDeletes(true);
        MockViewableResource::resetCounters();
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

TEST_F(ViewableResourceTestFixture, InitialStateAndDefaultGraphicsPipeline)
{
    MockViewableResource* const raw = new MockViewableResource(
        ResourceType::Texture, sync::ResourceAccess::SRVGraphics, "GBufferAlbedo", gp::Name("DeferredRenderer")
    );
    ViewableResourceRef handle(raw);

    EXPECT_EQ(handle->getType(), ResourceType::Texture);
    EXPECT_EQ(handle->getTrackedAccessUnsafe(), sync::ResourceAccess::SRVGraphics);
    EXPECT_EQ(handle->getTrackedPipelinesUnsafe(), sync::PipelineStage::Graphics);
    EXPECT_EQ(handle->getRefCount(), 1u);
    EXPECT_TRUE(handle->isValid());

#if GP_RHI_ENABLE_RESOURCE_INFO
    EXPECT_EQ(handle->getName(), gp::Name("GBufferAlbedo"));
    EXPECT_EQ(handle->getOwnerName(), gp::Name("DeferredRenderer"));
#else
    EXPECT_TRUE(handle->getName().isNone());
    EXPECT_TRUE(handle->getOwnerName().isNone());
#endif

    // Converting ViewableResourceRef to base ResourceRef preserves reference counting.
    ResourceRef baseHandle = handle;
    EXPECT_EQ(handle->getRefCount(), 2u);

    handle.reset();
    baseHandle.reset();
    EXPECT_EQ(MockViewableResource::s_destructCount.load(), 0u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockViewableResource::s_constructCount.load(), 1u);
    EXPECT_EQ(MockViewableResource::s_destructCount.load(), 1u);
}

TEST_F(ViewableResourceTestFixture, NullAndEmptyDebugNameLeaveNameAsNone)
{
    ViewableResourceRef nullNamed(
        new MockViewableResource(ResourceType::Buffer, sync::ResourceAccess::CopyDest, gp::StringView(nullptr))
    );
    ViewableResourceRef emptyNamed(new MockViewableResource(ResourceType::Buffer, sync::ResourceAccess::CopyDest, ""));

    EXPECT_TRUE(nullNamed->getName().isNone());
    EXPECT_TRUE(emptyNamed->getName().isNone());
    EXPECT_TRUE(nullNamed->getOwnerName().isNone());

    static_cast<MockViewableResource*>(emptyNamed.get())->renameForTest(gp::Name("DynamicVertexBuffer"));
    EXPECT_EQ(emptyNamed->getName(), gp::Name("DynamicVertexBuffer"));
}

TEST_F(ViewableResourceTestFixture, ContextUpdatesAndVirtualForwarding)
{
    MockViewableResource* const target =
        new MockViewableResource(ResourceType::Texture, sync::ResourceAccess::Unknown, "TargetTexture");
    ForwardingViewableResource* const wrapper = new ForwardingViewableResource(target);

    ViewableResourceRef targetRef(target);
    ViewableResourceRef wrapperRef(wrapper);

    const sync::TrackedAccess computeUav(sync::ResourceAccess::UAVCompute, sync::PipelineStage::AsyncCompute);
    wrapper->updateTrackedAccessForTest(computeUav);

    EXPECT_EQ(wrapper->getTrackedAccessUnsafe(), sync::ResourceAccess::UAVCompute);
    EXPECT_EQ(wrapper->getTrackedPipelinesUnsafe(), sync::PipelineStage::AsyncCompute);
    EXPECT_EQ(target->getTrackedAccessUnsafe(), sync::ResourceAccess::UAVCompute);
    EXPECT_EQ(target->getTrackedPipelinesUnsafe(), sync::PipelineStage::AsyncCompute);

    // TrackedAccessInfo binds cleanly to ViewableResource pointers.
    const sync::TrackedAccessInfo info(target, sync::ResourceAccess::SRVCompute, sync::PipelineStage::AsyncCompute);
    EXPECT_EQ(info.resource, target);
    EXPECT_EQ(info.access, sync::ResourceAccess::SRVCompute);
    EXPECT_EQ(info.pipelines, sync::PipelineStage::AsyncCompute);
}

TEST_F(ViewableResourceTestFixture, TakeOwnershipAndReleaseOwnershipTransferTrackedState)
{
    MockViewableResource* const source =
        new MockViewableResource(ResourceType::Buffer, sync::ResourceAccess::VertexOrIndexBuffer, "SourceBuffer");
    MockViewableResource* const destination =
        new MockViewableResource(ResourceType::Buffer, sync::ResourceAccess::Unknown, "DestinationBuffer");

    ViewableResourceRef srcRef(source);
    ViewableResourceRef dstRef(destination);

    source->updateTrackedAccessForTest(
        sync::TrackedAccess(sync::ResourceAccess::UAVCompute, sync::PipelineStage::AsyncCompute)
    );

    destination->takeOwnershipForTest(*source);
    EXPECT_EQ(destination->getTrackedAccessUnsafe(), sync::ResourceAccess::UAVCompute);
    EXPECT_EQ(destination->getTrackedPipelinesUnsafe(), sync::PipelineStage::AsyncCompute);

    source->releaseOwnershipForTest();
    EXPECT_EQ(source->getTrackedAccessUnsafe(), sync::ResourceAccess::Unknown);
    EXPECT_EQ(source->getTrackedPipelinesUnsafe(), sync::PipelineStage::Graphics);
}

}   // namespace gp::rhi::resource::tests
