// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanUploadContext.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "commands/VulkanSubmission.hpp"
#include "contexts/VulkanCommandContextProvider.hpp"
#include "contexts/VulkanContextEnums.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "resources/BufferEnums.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanContextTestFixture.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

class VulkanUploadContextLiveTest : public VulkanContextLiveTest
{};

TEST_F(VulkanUploadContextLiveTest, UploadContextRecordsOnAPrimaryCommandBuffer)
{
    VulkanUploadContext context(m_contextServices);
    EXPECT_EQ(context.getCommandBufferType(), VulkanCommandBufferType::Primary);

    IVulkanTransferCommandRecorder* const recorder = context.getTransferRecorder();
    ASSERT_NE(recorder, nullptr);
    EXPECT_EQ(context.getTransferRecorder(), recorder);
    EXPECT_TRUE(context.hasActiveCommandBuffer());
}

TEST_F(VulkanUploadContextLiveTest, RecorderIsRebuiltAfterFinalize)
{
    VulkanUploadContext context(m_contextServices);
    ASSERT_NE(context.getTransferRecorder(), nullptr);

    VulkanPlatformCommandList commandList{};
    context.finalize(commandList);
    EXPECT_EQ(commandList.payloads.size(), 1u);
    EXPECT_FALSE(context.hasActiveCommandBuffer());

    ASSERT_NE(context.getTransferRecorder(), nullptr);
    EXPECT_TRUE(context.hasActiveCommandBuffer());
    discardUnsubmitted(commandList);
}

TEST_F(VulkanUploadContextLiveTest, RecordedFillIsExecutedByTheGPU)
{
    const VulkanViewTestBufferRef buffer =
        makeBuffer(resource::BufferUsageFlags::Static | resource::BufferUsageFlags::UnorderedAccess);
    ASSERT_NE(buffer->getHandle(), VK_NULL_HANDLE);

    VulkanUploadContext context(m_contextServices);
    IVulkanTransferCommandRecorder* const recorder = context.getTransferRecorder();
    ASSERT_NE(recorder, nullptr);
    recorder->fillBuffer(buffer->getHandle(), static_cast<VkDeviceSize>(buffer->getOffset()), 64u, 0xABCDu);

    EXPECT_TRUE(context.flushCommands(VulkanFlushFlags::WaitForCompletion));
    EXPECT_EQ(context.getNumPayloads(), 0u);
}

TEST_F(VulkanUploadContextLiveTest, PoolReusesReleasedContexts)
{
    VulkanUploadContextPool pool(m_contextServices);
    EXPECT_EQ(pool.getNumContexts(), 0u);

    VulkanUploadContext& first = pool.acquire();
    EXPECT_EQ(pool.getNumContexts(), 1u);
    EXPECT_EQ(pool.getNumFreeContexts(), 0u);

    VulkanUploadContext& second = pool.acquire();
    EXPECT_NE(&first, &second);
    EXPECT_EQ(pool.getNumContexts(), 2u);

    pool.release(first);
    EXPECT_EQ(pool.getNumFreeContexts(), 1u);

    VulkanUploadContext& reused = pool.acquire();
    EXPECT_EQ(&reused, &first);
    EXPECT_EQ(pool.getNumContexts(), 2u);
    EXPECT_EQ(pool.getNumFreeContexts(), 0u);

    pool.release(second);
    pool.release(reused);
    EXPECT_EQ(pool.getNumFreeContexts(), 2u);
}

TEST_F(VulkanUploadContextLiveTest, ProviderWithoutPoolHasNoUploadRecorder)
{
    VulkanCommandContextProvider provider(nullptr);
    EXPECT_EQ(provider.getUploadRecorder(cmdList()), nullptr);
    EXPECT_EQ(provider.getActiveUploadContext(), nullptr);

    VulkanPlatformCommandList commandList{};
    EXPECT_FALSE(provider.finalizeUploads(commandList));
    EXPECT_TRUE(commandList.payloads.isEmpty());
}

TEST_F(VulkanUploadContextLiveTest, ProviderSharesOneUploadContextUntilFinalized)
{
    VulkanUploadContextPool pool(m_contextServices);
    VulkanCommandContextProvider provider(&pool);

    IVulkanTransferCommandRecorder* const recorder = provider.getUploadRecorder(cmdList());
    ASSERT_NE(recorder, nullptr);
    VulkanUploadContext* const active = provider.getActiveUploadContext();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(provider.getUploadRecorder(cmdList()), recorder);
    EXPECT_EQ(pool.getNumContexts(), 1u);

    VulkanPlatformCommandList commandList{};
    EXPECT_TRUE(provider.finalizeUploads(commandList));
    EXPECT_EQ(commandList.payloads.size(), 1u);
    EXPECT_EQ(provider.getActiveUploadContext(), nullptr);
    EXPECT_EQ(pool.getNumFreeContexts(), 1u);

    // A second round reuses the pooled context.
    ASSERT_NE(provider.getUploadRecorder(cmdList()), nullptr);
    EXPECT_EQ(provider.getActiveUploadContext(), active);
    EXPECT_EQ(pool.getNumContexts(), 1u);

    VulkanPlatformCommandList secondList{};
    EXPECT_TRUE(provider.finalizeUploads(secondList));
    discardUnsubmitted(commandList);
    discardUnsubmitted(secondList);
}

TEST_F(VulkanUploadContextLiveTest, ProviderNullContextQueriesAreSafe)
{
    VulkanCommandContextProvider provider(nullptr);
    EXPECT_EQ(provider.getContextRecorder(nullptr), nullptr);
    EXPECT_EQ(provider.getTrackedPipelines(nullptr, nullptr), sync::PipelineStage::None);
    EXPECT_TRUE(provider.needsExtraTransitions(cmdList()));
}

}   // namespace gp::rhi::vulkan::tests
