// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQueryPool.hpp"
#include "../queries/VulkanQueryTestFixture.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "queries/VulkanQueryEnums.hpp"
#include "queries/VulkanQuerySettings.hpp"
#include "queries/VulkanRenderQuery.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

/// @brief Live fixture for single query pools.
class VulkanQueryPoolLiveTest : public VulkanQueryLiveTest
{
protected:
    /// @brief Reports whether pools can be reset from the host.
    /// @return True when `VK_EXT_host_query_reset` is usable.
    [[nodiscard]] bool supportsHostQueryReset() const noexcept
    {
        return (m_device->getOptionalExtensions().hasEXTHostQueryReset != 0u) &&
               (dynamic_api::vkResetQueryPoolEXT != nullptr);
    }
};

TEST_F(VulkanQueryPoolLiveTest, CreatesAnEmptyPoolOfTheRequestedType)
{
    const VulkanQueryPool pool(*m_device, 8u, VulkanQueryPoolType::Occlusion);

    ASSERT_TRUE(pool.isValid());
    EXPECT_NE(pool.getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(pool.getMaxQueries(), 8u);
    EXPECT_EQ(pool.getPoolType(), VulkanQueryPoolType::Occlusion);
    EXPECT_EQ(&pool.getDevice(), m_device);
    EXPECT_EQ(pool.getQueryStride(), 1u);
    EXPECT_EQ(pool.getCurrentQueryCount(), 0u);
    EXPECT_FALSE(pool.isFull());
    EXPECT_EQ(pool.getUnusedFrameCount(), 0);
}

TEST_F(VulkanQueryPoolLiveTest, HostResetClearsReservationsAndAppliesTheStride)
{
    if (!supportsHostQueryReset())
    {
        GTEST_SKIP() << "VK_EXT_host_query_reset unavailable; skipping host reset test.";
    }

    VulkanQueryPool pool(*m_device, 8u, VulkanQueryPoolType::Occlusion);
    ASSERT_TRUE(pool.isValid());

    pool.reset(VK_NULL_HANDLE, 2u);
    EXPECT_FALSE(pool.isDirty());
    EXPECT_EQ(pool.getQueryStride(), 2u);
    EXPECT_EQ(pool.getCurrentQueryCount(), 0u);

    {
        const gp::RefCountedPtr<VulkanRenderQuery> first(new VulkanRenderQuery(sync::RenderQueryType::Occlusion));
        const gp::RefCountedPtr<VulkanRenderQuery> second(new VulkanRenderQuery(sync::RenderQueryType::Occlusion));

        // Each query spans one slot per view.
        pool.reserveQuery(*first);
        EXPECT_EQ(first->indexInPool, 0u);
        EXPECT_TRUE(pool.isDirty());
        pool.reserveQuery(*second);
        EXPECT_EQ(second->indexInPool, 2u);
        EXPECT_EQ(pool.getCurrentQueryCount(), 2u);
        EXPECT_FALSE(pool.isFull());

        // The pool holds a reference until the results are resolved or the pool is reset.
        EXPECT_GE(first->getRefCount(), 2u);
        pool.reset(VK_NULL_HANDLE, 2u);
        EXPECT_EQ(first->getRefCount(), 1u);
        EXPECT_EQ(pool.getCurrentQueryCount(), 0u);
    }
}

TEST_F(VulkanQueryPoolLiveTest, TimestampSlotsFillThePool)
{
    if (!supportsHostQueryReset())
    {
        GTEST_SKIP() << "VK_EXT_host_query_reset unavailable; skipping host reset test.";
    }

    VulkanQueryPool pool(*m_device, 4u, VulkanQueryPoolType::Timestamp);
    ASSERT_TRUE(pool.isValid());
    pool.reset(VK_NULL_HANDLE);

    gp::UInt64 results[4]{};
    for (gp::UInt32 index = 0u; index < 4u; ++index)
    {
        EXPECT_FALSE(pool.isFull());
        EXPECT_EQ(pool.reserveQuery(&results[index]), index);
    }
    EXPECT_TRUE(pool.isFull());
    EXPECT_EQ(pool.getCurrentQueryCount(), 4u);
}

TEST_F(VulkanQueryPoolLiveTest, ResolvingAnEmptyPoolSucceedsWithoutReadback)
{
    VulkanQueryPool pool(*m_device, 4u, VulkanQueryPoolType::Timestamp);
    ASSERT_TRUE(pool.isValid());
    EXPECT_TRUE(pool.resolveResults());
}

TEST_F(VulkanQueryPoolLiveTest, UnusedFramesMakeThePoolStale)
{
    VulkanQueryPool pool(*m_device, 4u, VulkanQueryPoolType::Occlusion);
    ASSERT_TRUE(pool.isValid());

    const gp::Int32 deletionDelay = getVulkanQueryPoolDeletionDelay();
    for (gp::Int32 frame = 0; frame < deletionDelay; ++frame)
    {
        EXPECT_FALSE(pool.isStale());
        pool.incrementUnusedFrameCount();
    }
    EXPECT_EQ(pool.getUnusedFrameCount(), deletionDelay);
    EXPECT_TRUE(pool.isStale());

    // Resetting puts the pool back in use.
    if (supportsHostQueryReset())
    {
        pool.reset(VK_NULL_HANDLE);
        EXPECT_EQ(pool.getUnusedFrameCount(), 0);
        EXPECT_FALSE(pool.isStale());
    }
}

TEST_F(VulkanQueryPoolLiveTest, DirtyFlagTracksPendingResets)
{
    VulkanQueryPool pool(*m_device, 4u, VulkanQueryPoolType::Occlusion);
    ASSERT_TRUE(pool.isValid());

    VulkanCommandBuffer* const cmdBuffer = beginCommandBuffer();
    ASSERT_NE(cmdBuffer, nullptr);
    pool.reset(cmdBuffer->getHandle());
    EXPECT_FALSE(pool.isDirty());

    pool.setDirty();
    EXPECT_TRUE(pool.isDirty());

    VulkanPayload* const payload = new VulkanPayload(*m_queue);
    EXPECT_TRUE(submitAndWait(cmdBuffer, payload));
}

}   // namespace gp::rhi::vulkan::tests
