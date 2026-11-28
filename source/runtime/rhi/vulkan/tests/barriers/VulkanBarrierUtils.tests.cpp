// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierUtils.hpp"
#include "barriers/VulkanBarrierTypes.hpp"   // IWYU pragma: keep
#include "containers/arrays/Vector.hpp"
#include "CoreMinimal.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

// setupImageBarrier and setupImageBarrierEntireRes

TEST(VulkanBarrierUtilsTest, SetupImageBarrierFillsAllFields)
{
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0xABCD1234u));

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 2u;
    range.levelCount = 3u;
    range.baseArrayLayer = 1u;
    range.layerCount = 4u;

    VkImageMemoryBarrier2 barrier{};
    setupImageBarrier(
        barrier,
        fakeImage,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        range
    );

    EXPECT_EQ(barrier.sType, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2);
    EXPECT_EQ(barrier.pNext, nullptr);
    EXPECT_EQ(barrier.srcStageMask, static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT));
    EXPECT_EQ(barrier.dstStageMask, static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));
    EXPECT_EQ(barrier.srcAccessMask, static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_READ_BIT));
    EXPECT_EQ(barrier.dstAccessMask, static_cast<VkAccessFlags2>(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
    EXPECT_EQ(barrier.oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(barrier.newLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(barrier.srcQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(barrier.dstQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(barrier.image, fakeImage);
    EXPECT_EQ(barrier.subresourceRange.aspectMask, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
    EXPECT_EQ(barrier.subresourceRange.baseMipLevel, 2u);
    EXPECT_EQ(barrier.subresourceRange.levelCount, 3u);
    EXPECT_EQ(barrier.subresourceRange.baseArrayLayer, 1u);
    EXPECT_EQ(barrier.subresourceRange.layerCount, 4u);
}

TEST(VulkanBarrierUtilsTest, SetupImageBarrierEntireResSetsRemainingCountsAndAspect)
{
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0xDEADBEEFu));

    VkImageMemoryBarrier2 barrier{};
    setupImageBarrierEntireRes(
        barrier,
        fakeImage,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    EXPECT_EQ(barrier.sType, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2);
    EXPECT_EQ(barrier.image, fakeImage);
    EXPECT_EQ(barrier.subresourceRange.aspectMask, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
    EXPECT_EQ(barrier.subresourceRange.baseMipLevel, 0u);
    EXPECT_EQ(barrier.subresourceRange.levelCount, VK_REMAINING_MIP_LEVELS);
    EXPECT_EQ(barrier.subresourceRange.baseArrayLayer, 0u);
    EXPECT_EQ(barrier.subresourceRange.layerCount, VK_REMAINING_ARRAY_LAYERS);
    EXPECT_EQ(barrier.oldLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(barrier.newLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(barrier.srcQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(barrier.dstQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
}

// mergeBarrierAccessMask

TEST(VulkanBarrierUtilsTest, MergeBarrierAccessMaskReadOnlySourceSkipsMasks)
{
    // Source is purely read-only: access masks must not be accumulated.
    VkMemoryBarrier legacyBarrier{};
    legacyBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    mergeBarrierAccessMask(legacyBarrier, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    EXPECT_EQ(legacyBarrier.srcAccessMask, 0u);
    EXPECT_EQ(legacyBarrier.dstAccessMask, 0u);

    VkMemoryBarrier2 syncBarrier{};
    syncBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    mergeBarrierAccessMask(syncBarrier, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    EXPECT_EQ(syncBarrier.srcAccessMask, 0u);
    EXPECT_EQ(syncBarrier.dstAccessMask, 0u);
}

TEST(VulkanBarrierUtilsTest, MergeBarrierAccessMaskWriteSourceOrsInBothMasks)
{
    // Source has a write bit: both access masks are ORed in.
    VkMemoryBarrier legacyBarrier{};
    legacyBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    mergeBarrierAccessMask(legacyBarrier, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    EXPECT_EQ(legacyBarrier.srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_WRITE_BIT));
    EXPECT_EQ(legacyBarrier.dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT));

    // Second merge accumulates onto existing masks.
    mergeBarrierAccessMask(legacyBarrier, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
    EXPECT_EQ(
        legacyBarrier.srcAccessMask,
        static_cast<VkAccessFlags>(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
    );
    EXPECT_EQ(
        legacyBarrier.dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT)
    );

    VkMemoryBarrier2 syncBarrier{};
    syncBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    mergeBarrierAccessMask(syncBarrier, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    EXPECT_NE(syncBarrier.srcAccessMask, 0u);
    EXPECT_NE(syncBarrier.dstAccessMask, 0u);
}

// downgradeBarrier - three overloads

TEST(VulkanBarrierUtilsTest, DowngradeMemoryBarrierCopiesToLegacyFormat)
{
    VkMemoryBarrier2 src2{};
    src2.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    src2.pNext = nullptr;
    src2.srcAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_READ_BIT);
    src2.dstAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    VkMemoryBarrier out{};
    downgradeBarrier(out, src2);

    EXPECT_EQ(out.sType, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    EXPECT_EQ(out.pNext, nullptr);
    EXPECT_EQ(out.srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT));
    EXPECT_EQ(out.dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
}

TEST(VulkanBarrierUtilsTest, DowngradeBufferBarrierCopiesToLegacyFormat)
{
    const VkBuffer fakeBuffer = reinterpret_cast<VkBuffer>(static_cast<gp::UIntPtr>(0xCAFEu));

    VkBufferMemoryBarrier2 src2{};
    src2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    src2.pNext = nullptr;
    src2.srcAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_TRANSFER_WRITE_BIT);
    src2.dstAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_READ_BIT);
    src2.srcQueueFamilyIndex = 0u;
    src2.dstQueueFamilyIndex = 1u;
    src2.buffer = fakeBuffer;
    src2.offset = 128u;
    src2.size = 512u;

    VkBufferMemoryBarrier out{};
    downgradeBarrier(out, src2);

    EXPECT_EQ(out.sType, VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
    EXPECT_EQ(out.pNext, nullptr);
    EXPECT_EQ(out.srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT));
    EXPECT_EQ(out.dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT));
    EXPECT_EQ(out.srcQueueFamilyIndex, 0u);
    EXPECT_EQ(out.dstQueueFamilyIndex, 1u);
    EXPECT_EQ(out.buffer, fakeBuffer);
    EXPECT_EQ(out.offset, 128u);
    EXPECT_EQ(out.size, 512u);
}

TEST(VulkanBarrierUtilsTest, DowngradeImageBarrierCopiesToLegacyFormat)
{
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0xFACEu));

    VkImageSubresourceRange range{};
    range.aspectMask = static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    range.baseMipLevel = 0u;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = 0u;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkImageMemoryBarrier2 src2{};
    src2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    src2.pNext = nullptr;
    src2.srcAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    src2.dstAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_READ_BIT);
    src2.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    src2.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    src2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src2.image = fakeImage;
    src2.subresourceRange = range;

    VkImageMemoryBarrier out{};
    downgradeBarrier(out, src2);

    EXPECT_EQ(out.sType, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    EXPECT_EQ(out.pNext, nullptr);
    EXPECT_EQ(out.srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT));
    EXPECT_EQ(out.dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT));
    EXPECT_EQ(out.oldLayout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(out.newLayout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    EXPECT_EQ(out.srcQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(out.dstQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(out.image, fakeImage);
    EXPECT_EQ(out.subresourceRange.aspectMask, range.aspectMask);
    EXPECT_EQ(out.subresourceRange.levelCount, VK_REMAINING_MIP_LEVELS);
    EXPECT_EQ(out.subresourceRange.layerCount, VK_REMAINING_ARRAY_LAYERS);
}

// Template downgradeBarrier(array, barrier, src, dst)

TEST(VulkanBarrierUtilsTest, TemplateDowngradeBarrierAppendsToArrayAndMergesStages)
{
    VkMemoryBarrier2 src2{};
    src2.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    src2.srcStageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
    src2.dstStageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    src2.srcAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_WRITE_BIT);
    src2.dstAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_READ_BIT);

    gp::Vector<VkMemoryBarrier> outArray;
    VkPipelineStageFlags mergedSrc = 0u;
    VkPipelineStageFlags mergedDst = 0u;

    downgradeBarrier(outArray, src2, mergedSrc, mergedDst);

    EXPECT_EQ(outArray.size(), 1u);
    EXPECT_EQ(outArray[0].srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_WRITE_BIT));
    EXPECT_EQ(outArray[0].dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT));
    EXPECT_EQ(mergedSrc, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
    EXPECT_EQ(mergedDst, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    // Second barrier: stage masks accumulate.
    VkMemoryBarrier2 src2b{};
    src2b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    src2b.srcStageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_TRANSFER_BIT);
    src2b.dstStageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    src2b.srcAccessMask = 0u;
    src2b.dstAccessMask = 0u;

    downgradeBarrier(outArray, src2b, mergedSrc, mergedDst);

    EXPECT_EQ(outArray.size(), 2u);
    EXPECT_EQ(
        mergedSrc,
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT)
    );
    EXPECT_EQ(
        mergedDst,
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
    );
}

TEST(VulkanBarrierUtilsTest, DowngradeBarrierArrayConvertsAllElementsAndMergesStages)
{
    gp::Vector<VkMemoryBarrier2> srcArray;

    VkMemoryBarrier2 b1{};
    b1.srcStageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
    b1.dstStageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    b1.srcAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_WRITE_BIT);
    b1.dstAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_SHADER_READ_BIT);
    srcArray.pushBack(b1);

    VkMemoryBarrier2 b2{};
    b2.srcStageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_TRANSFER_BIT);
    b2.dstStageMask = static_cast<VkPipelineStageFlags2>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    b2.srcAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_TRANSFER_WRITE_BIT);
    b2.dstAccessMask = static_cast<VkAccessFlags2>(VK_ACCESS_UNIFORM_READ_BIT);
    srcArray.pushBack(b2);

    gp::Vector<VkMemoryBarrier> dstArray;
    VkPipelineStageFlags mergedSrc = 0u;
    VkPipelineStageFlags mergedDst = 0u;

    downgradeBarrierArray(dstArray, srcArray, mergedSrc, mergedDst);

    EXPECT_EQ(dstArray.size(), 2u);
    EXPECT_EQ(dstArray[0].srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_SHADER_WRITE_BIT));
    EXPECT_EQ(dstArray[1].srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT));
    EXPECT_EQ(dstArray[1].dstAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_UNIFORM_READ_BIT));
    EXPECT_EQ(
        mergedSrc,
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT)
    );
    EXPECT_EQ(
        mergedDst,
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
    );
}

// mergeDepthStencilLayouts

TEST(VulkanBarrierUtilsTest, MergeDepthStencilLayoutsPatchesDepthStencilAspectBarriers)
{
    gp::Vector<VkImageMemoryBarrier> barriers;

    // Barrier with depth+stencil aspect: READ_ONLY -> DEPTH_STENCIL_READ_ONLY, ATTACHMENT -> DS_ATTACHMENT
    {
        VkImageMemoryBarrier b{};
        b.subresourceRange.aspectMask =
            static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        b.oldLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        barriers.pushBack(b);
    }

    // Barrier with depth aspect only: same patching rules
    {
        VkImageMemoryBarrier b{};
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        barriers.pushBack(b);
    }

    // Barrier with color aspect only: untouched
    {
        VkImageMemoryBarrier b{};
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        barriers.pushBack(b);
    }

    // Barrier with depth aspect and a non-patchable layout: untouched
    {
        VkImageMemoryBarrier b{};
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers.pushBack(b);
    }

    mergeDepthStencilLayouts(barriers);

    // Barrier 0: depth+stencil aspect
    EXPECT_EQ(barriers[0].oldLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    EXPECT_EQ(barriers[0].newLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // Barrier 1: depth aspect
    EXPECT_EQ(barriers[1].oldLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(barriers[1].newLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    // Barrier 2: color aspect - unchanged
    EXPECT_EQ(barriers[2].oldLayout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(barriers[2].newLayout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    // Barrier 3: transfer layouts - unchanged
    EXPECT_EQ(barriers[3].oldLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(barriers[3].newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

}   // namespace gp::rhi::vulkan::tests
