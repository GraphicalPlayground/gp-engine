// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanViewableResource.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "CoreMinimal.hpp"
#include "resources/IVulkanResourceRenameListener.hpp"
#include "resources/VulkanLinkedView.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Shared event log so tests can check notification order.
struct UpdateLog final
{
    gp::Int32 events[8]{};
    gp::Int32 count{ 0 };

    void push(const gp::Int32 id) noexcept
    {
        if (count < 8)
        {
            events[count] = id;
        }
        ++count;
    }
};

class RecordingView final : public VulkanLinkedView
{
public:
    RecordingView(UpdateLog& log, const gp::Int32 id) noexcept
        : m_log(&log)
        , m_id(id)
    {}

    virtual ~RecordingView() noexcept override = default;

    virtual void updateView([[maybe_unused]] const allocator::ContextArray& contexts) noexcept override
    {
        ++updateCount;
        m_log->push(m_id);
    }

    gp::Int32 updateCount{ 0 };

private:
    UpdateLog* m_log{ nullptr };
    gp::Int32 m_id{ 0 };
};

class RecordingListener final : public IVulkanResourceRenameListener
{
public:
    explicit RecordingListener(UpdateLog& log, const gp::Int32 id) noexcept
        : m_log(&log)
        , m_id(id)
    {}

    virtual void resourceRenamed(
        [[maybe_unused]] const allocator::ContextArray& contexts, VulkanViewableResource* const renamedResource
    ) noexcept override
    {
        lastResource = renamedResource;
        m_log->push(m_id);
    }

    VulkanViewableResource* lastResource{ nullptr };

private:
    UpdateLog* m_log{ nullptr };
    gp::Int32 m_id{ 0 };
};

}   // namespace

TEST(VulkanLinkedViewTest, UnlinkedByDefaultAndUnlinkIsNoOp)
{
    UpdateLog log{};
    RecordingView view(log, 1);

    EXPECT_FALSE(view.isLinked());
    EXPECT_EQ(view.getNext(), nullptr);

    view.unlink();
    EXPECT_FALSE(view.isLinked());
}

TEST(VulkanLinkedViewTest, LinkHeadPushesFrontAndUnlinkRepairsList)
{
    UpdateLog log{};
    RecordingView first(log, 1);
    RecordingView second(log, 2);
    RecordingView third(log, 3);

    VulkanLinkedView* head = nullptr;
    first.linkHead(head);
    second.linkHead(head);
    third.linkHead(head);

    ASSERT_EQ(head, &third);
    EXPECT_EQ(third.getNext(), &second);
    EXPECT_EQ(second.getNext(), &first);
    EXPECT_EQ(first.getNext(), nullptr);

    // Middle removal.
    second.unlink();
    EXPECT_FALSE(second.isLinked());
    EXPECT_EQ(head, &third);
    EXPECT_EQ(third.getNext(), &first);

    // Head removal.
    third.unlink();
    EXPECT_EQ(head, &first);
    EXPECT_EQ(first.getNext(), nullptr);

    // Tail removal empties the list.
    first.unlink();
    EXPECT_EQ(head, nullptr);
}

TEST(VulkanLinkedViewTest, DestructorUnlinksFromList)
{
    UpdateLog log{};
    RecordingView survivor(log, 1);
    VulkanLinkedView* head = nullptr;
    survivor.linkHead(head);

    {
        RecordingView temporary(log, 2);
        temporary.linkHead(head);
        EXPECT_EQ(head, &temporary);
    }

    EXPECT_EQ(head, &survivor);
    EXPECT_EQ(survivor.getNext(), nullptr);
    survivor.unlink();
}

TEST(VulkanViewableResourceTest, UpdateRefreshesViewsBeforeListeners)
{
    UpdateLog log{};
    VulkanViewableResource resource{};
    RecordingView viewA(log, 1);
    RecordingView viewB(log, 2);
    RecordingListener listener(log, 10);

    EXPECT_FALSE(resource.hasLinkedViews());
    resource.linkView(viewA);
    resource.linkView(viewB);
    resource.addRenameListener(&listener);
    EXPECT_TRUE(resource.hasLinkedViews());
    EXPECT_EQ(resource.getNumRenameListeners(), 1u);

    const allocator::ContextArray contexts{};
    resource.updateLinkedViews(contexts);

    EXPECT_EQ(viewA.updateCount, 1);
    EXPECT_EQ(viewB.updateCount, 1);
    EXPECT_EQ(listener.lastResource, &resource);
    ASSERT_EQ(log.count, 3);
    EXPECT_EQ(log.events[2], 10);

    resource.removeRenameListener(&listener);
    viewA.unlink();
    viewB.unlink();
    EXPECT_FALSE(resource.hasLinkedViews());
    EXPECT_EQ(resource.getNumRenameListeners(), 0u);
}

TEST(VulkanViewableResourceTest, RemovingListenerStopsNotifications)
{
    UpdateLog log{};
    VulkanViewableResource resource{};
    RecordingListener first(log, 1);
    RecordingListener second(log, 2);

    resource.addRenameListener(&first);
    resource.addRenameListener(&second);
    EXPECT_EQ(resource.getNumRenameListeners(), 2u);

    resource.removeRenameListener(&first);
    EXPECT_EQ(resource.getNumRenameListeners(), 1u);

    const allocator::ContextArray contexts{};
    resource.updateLinkedViews(contexts);
    EXPECT_EQ(first.lastResource, nullptr);
    EXPECT_EQ(second.lastResource, &resource);

    resource.removeRenameListener(&second);
}

TEST(VulkanViewableResourceTest, UpdateWithoutViewsOrListenersIsNoOp)
{
    VulkanViewableResource resource{};
    const allocator::ContextArray contexts{};

    resource.updateLinkedViews(contexts);

    EXPECT_FALSE(resource.hasLinkedViews());
    EXPECT_EQ(resource.getNumRenameListeners(), 0u);
}

}   // namespace gp::rhi::vulkan::tests

