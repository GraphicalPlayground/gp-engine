// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanReadbackStagingBuffer.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/StagingBuffer.hpp"   // IWYU pragma: keep
#include "resources/VulkanBuffer.hpp"
#include "VulkanResourceTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

using VulkanBufferRef = gp::RefCountedPtr<VulkanBuffer>;

}   // namespace

class VulkanReadbackStagingBufferLiveTest : public VulkanResourceLiveTest
{
protected:
    [[nodiscard]] VulkanBufferRef makeSource(const gp::UInt32 size) noexcept
    {
        const resource::BufferCreateDesc desc = resource::BufferCreateDesc::create(
            "ReadbackSource", size, 0u, resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static
        );
        return VulkanBufferRef(new VulkanBuffer(m_services, desc));
    }
};

TEST_F(VulkanReadbackStagingBufferLiveTest, NewStagingBufferHasNoGPUMemory)
{
    const resource::StagingBufferRef staging = createVulkanStagingBuffer();
    ASSERT_NE(staging.get(), nullptr);

    const VulkanReadbackStagingBuffer* const readback = static_cast<VulkanReadbackStagingBuffer*>(staging.get());
    EXPECT_EQ(readback->getGPUSizeBytes(), 0u);
    EXPECT_EQ(readback->getQueuedNumBytes(), 0u);
}

TEST_F(VulkanReadbackStagingBufferLiveTest, CopyRecordsTransferAndHostBarrier)
{
    const VulkanBufferRef source = makeSource(512u);
    const resource::StagingBufferRef staging = createVulkanStagingBuffer();
    VulkanReadbackStagingBuffer* const readback = static_cast<VulkanReadbackStagingBuffer*>(staging.get());

    FakeTransferRecorder recorder{};
    readback->copyFromBuffer(recorder, *source, 32u, 128u);

    EXPECT_EQ(readback->getQueuedNumBytes(), 128u);
    EXPECT_GE(readback->getGPUSizeBytes(), 128u);

    ASSERT_EQ(recorder.copyCount, 1u);
    EXPECT_EQ(recorder.copies[0].srcBuffer, source->getHandle());
    EXPECT_NE(recorder.copies[0].dstBuffer, VK_NULL_HANDLE);
    EXPECT_EQ(recorder.copies[0].region.srcOffset, 32u + static_cast<VkDeviceSize>(source->getOffset()));
    EXPECT_EQ(recorder.copies[0].region.dstOffset, 0u);
    EXPECT_EQ(recorder.copies[0].region.size, 128u);

    ASSERT_EQ(recorder.barrierCount, 1u);
    EXPECT_EQ(recorder.barriers[0].srcStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT));
    EXPECT_EQ(recorder.barriers[0].dstStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_HOST_BIT));
    EXPECT_EQ(recorder.barriers[0].srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT));
    EXPECT_EQ(recorder.barriers[0].dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_HOST_READ_BIT));
}

TEST_F(VulkanReadbackStagingBufferLiveTest, ReusesStagingWhenLargeEnoughAndGrowsOtherwise)
{
    const VulkanBufferRef source = makeSource(1024u);
    const resource::StagingBufferRef staging = createVulkanStagingBuffer();
    VulkanReadbackStagingBuffer* const readback = static_cast<VulkanReadbackStagingBuffer*>(staging.get());

    FakeTransferRecorder recorder{};
    readback->copyFromBuffer(recorder, *source, 0u, 256u);
    const VkBuffer firstStaging = recorder.copies[0].dstBuffer;

    readback->copyFromBuffer(recorder, *source, 0u, 64u);
    EXPECT_EQ(recorder.copies[1].dstBuffer, firstStaging);
    EXPECT_EQ(readback->getQueuedNumBytes(), 64u);

    readback->copyFromBuffer(recorder, *source, 0u, 1024u);
    EXPECT_GE(readback->getGPUSizeBytes(), 1024u);
    EXPECT_EQ(readback->getQueuedNumBytes(), 1024u);
}

TEST_F(VulkanReadbackStagingBufferLiveTest, LockReturnsMappedPointerAtOffset)
{
    const VulkanBufferRef source = makeSource(256u);
    const resource::StagingBufferRef staging = createVulkanStagingBuffer();
    VulkanReadbackStagingBuffer* const readback = static_cast<VulkanReadbackStagingBuffer*>(staging.get());

    FakeTransferRecorder recorder{};
    readback->copyFromBuffer(recorder, *source, 0u, 256u);

    gp::UInt8* const base = static_cast<gp::UInt8*>(lockVulkanStagingBuffer(staging.get(), nullptr, 0u, 256u));
    ASSERT_NE(base, nullptr);
    unlockVulkanStagingBuffer(staging.get());

    gp::UInt8* const offsetPointer = static_cast<gp::UInt8*>(lockVulkanStagingBuffer(staging.get(), nullptr, 16u, 64u));
    EXPECT_EQ(offsetPointer, base + 16u);
    unlockVulkanStagingBuffer(staging.get());
}

}   // namespace gp::rhi::vulkan::tests

