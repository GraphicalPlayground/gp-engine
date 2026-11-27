// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanCopyCommands.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "commands/CommandTypes.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "RHIDefinitions.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Fake non-null image handle (never dereferenced by the fake recorder).
const VkImage kSourceImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x1000u));

/// @brief Fake non-null image handle (never dereferenced by the fake recorder).
const VkImage kDestImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x2000u));

/// @brief Fake non-null buffer handle (never dereferenced by the fake recorder).
const VkBuffer kSourceBuffer = reinterpret_cast<VkBuffer>(static_cast<gp::UIntPtr>(0x3000u));

/// @brief Fake non-null buffer handle (never dereferenced by the fake recorder).
const VkBuffer kDestBuffer = reinterpret_cast<VkBuffer>(static_cast<gp::UIntPtr>(0x4000u));

}   // namespace

TEST(VulkanCopyCommandsTest, WholeMipCopyUsesSourceMipExtent)
{
    command::CopyTextureInfo info{};
    info.sourceMipIndex = 1u;
    info.destMipIndex = 2u;
    info.numMips = 1u;

    gp::Vector<VkImageCopy> regions{};
    buildVulkanImageCopyRegions(
        info, resource::IntVector(16, 8, 1), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, regions
    );

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].extent.width, 8u);
    EXPECT_EQ(regions[0].extent.height, 4u);
    EXPECT_EQ(regions[0].extent.depth, 1u);
    EXPECT_EQ(regions[0].srcSubresource.mipLevel, 1u);
    EXPECT_EQ(regions[0].dstSubresource.mipLevel, 2u);
    EXPECT_EQ(regions[0].srcSubresource.aspectMask, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
}

TEST(VulkanCopyCommandsTest, MipChainCopyHalvesExtentAndOffsetPerRegion)
{
    command::CopyTextureInfo info{};
    info.size = resource::IntVector(6, 4, 1);
    info.sourcePosition = resource::IntVector(2, 2, 0);
    info.destPosition = resource::IntVector(4, 0, 0);
    info.sourceSliceIndex = 1u;
    info.destSliceIndex = 3u;
    info.numSlices = 2u;
    info.numMips = 2u;

    gp::Vector<VkImageCopy> regions{};
    buildVulkanImageCopyRegions(
        info, resource::IntVector(64, 64, 1), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, regions
    );

    ASSERT_EQ(regions.size(), 2u);
    EXPECT_EQ(regions[0].extent.width, 6u);
    EXPECT_EQ(regions[0].extent.height, 4u);
    EXPECT_EQ(regions[0].srcOffset.x, 2);
    EXPECT_EQ(regions[0].dstOffset.x, 4);
    EXPECT_EQ(regions[0].srcSubresource.baseArrayLayer, 1u);
    EXPECT_EQ(regions[0].dstSubresource.baseArrayLayer, 3u);
    EXPECT_EQ(regions[0].srcSubresource.layerCount, 2u);

    EXPECT_EQ(regions[1].extent.width, 3u);
    EXPECT_EQ(regions[1].extent.height, 2u);
    EXPECT_EQ(regions[1].extent.depth, 1u);
    EXPECT_EQ(regions[1].srcOffset.x, 1);
    EXPECT_EQ(regions[1].srcOffset.y, 1);
    EXPECT_EQ(regions[1].dstOffset.x, 2);
    EXPECT_EQ(regions[1].srcSubresource.mipLevel, 1u);
    EXPECT_EQ(regions[1].dstSubresource.mipLevel, 1u);
}

TEST(VulkanCopyCommandsTest, ZeroMipsAndSlicesAreTreatedAsOne)
{
    command::CopyTextureInfo info{};
    info.numMips = 0u;
    info.numSlices = 0u;

    gp::Vector<VkImageCopy> regions{};
    regions.pushBack(VkImageCopy{});
    regions.pushBack(VkImageCopy{});
    buildVulkanImageCopyRegions(
        info, resource::IntVector(4, 4, 1), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, regions
    );

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].srcSubresource.layerCount, 1u);
}

TEST(VulkanCopyCommandsTest, ExtentsClampToOneTexel)
{
    command::CopyTextureInfo info{};
    info.sourceMipIndex = 5u;

    gp::Vector<VkImageCopy> regions{};
    buildVulkanImageCopyRegions(
        info, resource::IntVector(4, 2, 1), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, regions
    );

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].extent.width, 1u);
    EXPECT_EQ(regions[0].extent.height, 1u);
    EXPECT_EQ(regions[0].extent.depth, 1u);
}

TEST(VulkanCopyCommandsTest, ReadbackRegionsTargetPackedMipOffsets)
{
    VulkanCpuReadbackBuffer readback{};
    readback.mipOffsets[0] = 0u;
    readback.mipOffsets[1] = 256u;

    command::CopyTextureInfo info{};
    info.numMips = 2u;
    info.sourceSliceIndex = 2u;

    gp::Vector<VkBufferImageCopy> regions{};
    buildVulkanReadbackCopyRegions(info, resource::IntVector(8, 8, 1), VK_IMAGE_ASPECT_COLOR_BIT, readback, regions);

    ASSERT_EQ(regions.size(), 2u);
    EXPECT_EQ(regions[0].bufferOffset, 0u);
    EXPECT_EQ(regions[0].bufferRowLength, 8u);
    EXPECT_EQ(regions[0].bufferImageHeight, 8u);
    EXPECT_EQ(regions[0].imageSubresource.baseArrayLayer, 2u);
    EXPECT_EQ(regions[0].imageSubresource.layerCount, 1u);
    EXPECT_EQ(regions[1].bufferOffset, 256u);
    EXPECT_EQ(regions[1].bufferRowLength, 4u);
    EXPECT_EQ(regions[1].imageExtent.width, 4u);
    EXPECT_EQ(regions[1].imageSubresource.mipLevel, 1u);
}

TEST(VulkanCopyCommandsTest, ReadbackRegionsStopAtTheLastMip)
{
    VulkanCpuReadbackBuffer readback{};
    command::CopyTextureInfo info{};
    info.destMipIndex = kMaxTextureMipCount - 1u;
    info.numMips = 3u;

    gp::Vector<VkBufferImageCopy> regions{};
    buildVulkanReadbackCopyRegions(info, resource::IntVector(8, 8, 1), VK_IMAGE_ASPECT_COLOR_BIT, readback, regions);

    EXPECT_EQ(regions.size(), 1u);
}

TEST(VulkanCopyCommandsTest, BufferRegionFoldsSubAllocationOffsets)
{
    VkBufferCopy region{};
    ASSERT_TRUE(buildVulkanBufferCopyRegion(256u, 16u, 512u, 32u, 64u, region));
    EXPECT_EQ(region.srcOffset, 272u);
    EXPECT_EQ(region.dstOffset, 544u);
    EXPECT_EQ(region.size, 64u);
}

TEST(VulkanCopyCommandsTest, EmptyBufferRegionIsRejected)
{
    VkBufferCopy region{ 7u, 7u, 7u };
    EXPECT_FALSE(buildVulkanBufferCopyRegion(0u, 0u, 0u, 0u, 0u, region));
    EXPECT_EQ(region.size, 7u);
}

TEST(VulkanCopyCommandsTest, ImageCopyRecordsTransferLayouts)
{
    FakeTransferRecorder recorder{};
    VkImageCopy region{};
    region.extent = VkExtent3D{ 4u, 4u, 1u };
    recordVulkanImageCopy(recorder, kSourceImage, kDestImage, gp::ConstVectorView<VkImageCopy>(&region, 1u));

    ASSERT_EQ(recorder.imageCopyCount, 1u);
    EXPECT_EQ(recorder.imageCopies[0].srcImage, kSourceImage);
    EXPECT_EQ(recorder.imageCopies[0].dstImage, kDestImage);
    EXPECT_EQ(recorder.imageCopies[0].srcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(recorder.imageCopies[0].dstLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(recorder.imageCopies[0].regionCount, 1u);
}

TEST(VulkanCopyCommandsTest, EmptyImageCopiesRecordNothing)
{
    FakeTransferRecorder recorder{};
    recordVulkanImageCopy(recorder, kSourceImage, kDestImage, gp::ConstVectorView<VkImageCopy>());
    recordVulkanReadbackCopy(recorder, kSourceImage, kDestBuffer, gp::ConstVectorView<VkBufferImageCopy>());
    EXPECT_EQ(recorder.totalCommandCount(), 0u);
}

TEST(VulkanCopyCommandsTest, ReadbackCopyPublishesWritesToTheHost)
{
    FakeTransferRecorder recorder{};
    VkBufferImageCopy region{};
    recordVulkanReadbackCopy(recorder, kSourceImage, kDestBuffer, gp::ConstVectorView<VkBufferImageCopy>(&region, 1u));

    ASSERT_EQ(recorder.imageToBufferCount, 1u);
    EXPECT_EQ(recorder.imageToBufferCopies[0].image, kSourceImage);
    EXPECT_EQ(recorder.imageToBufferCopies[0].buffer, kDestBuffer);
    EXPECT_EQ(recorder.imageToBufferCopies[0].layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    ASSERT_EQ(recorder.barrierCount, 1u);
    EXPECT_EQ(recorder.barriers[0].dstStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_HOST_BIT));
    EXPECT_EQ(recorder.barriers[0].srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT));
    EXPECT_EQ(recorder.barriers[0].dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_HOST_READ_BIT));
}

TEST(VulkanCopyCommandsTest, BufferCopyIsFencedOnBothSides)
{
    FakeTransferRecorder recorder{};
    const VkBufferCopy region{ 8u, 16u, 32u };
    recordVulkanBufferCopy(recorder, kSourceBuffer, kDestBuffer, region);

    ASSERT_EQ(recorder.copyCount, 1u);
    EXPECT_EQ(recorder.copies[0].srcBuffer, kSourceBuffer);
    EXPECT_EQ(recorder.copies[0].dstBuffer, kDestBuffer);
    EXPECT_EQ(recorder.copies[0].region.srcOffset, 8u);
    EXPECT_EQ(recorder.copies[0].region.dstOffset, 16u);
    EXPECT_EQ(recorder.copies[0].region.size, 32u);

    ASSERT_EQ(recorder.barrierCount, 2u);
    EXPECT_EQ(recorder.barriers[0].dstStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT));
    EXPECT_EQ(recorder.barriers[1].srcStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT));
    EXPECT_EQ(recorder.barriers[1].dstStageMask, static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
}

}   // namespace gp::rhi::vulkan::tests
