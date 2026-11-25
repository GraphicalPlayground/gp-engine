// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanImageLayout.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

// Constructor tests

TEST(VulkanImageLayoutConstructorTest, ColorImageHasOnePlane)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 2u, 4u, VK_IMAGE_ASPECT_COLOR_BIT);

    EXPECT_EQ(layout.numMips, 2u);
    EXPECT_EQ(layout.numLayers, 4u);
    EXPECT_EQ(layout.numPlanes, 1u);
    EXPECT_EQ(layout.mainLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_TRUE(layout.areAllSubresourcesSameLayout());
}

TEST(VulkanImageLayoutConstructorTest, DepthOnlyImageHasOnePlane)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1u, 1u, VK_IMAGE_ASPECT_DEPTH_BIT);

    EXPECT_EQ(layout.numPlanes, 1u);
    EXPECT_EQ(layout.mainLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

TEST(VulkanImageLayoutConstructorTest, DepthStencilImageHasTwoPlanes)
{
    const VkImageAspectFlags depthStencilAspect =
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 3u, 2u, depthStencilAspect);

    EXPECT_EQ(layout.numMips, 3u);
    EXPECT_EQ(layout.numLayers, 2u);
    EXPECT_EQ(layout.numPlanes, 2u);
}

// areAllSubresourcesSameLayout tests

TEST(VulkanImageLayoutSubresTest, InitiallyAllSubresourcesSameLayout)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 4u, 2u, VK_IMAGE_ASPECT_COLOR_BIT);

    EXPECT_TRUE(layout.areAllSubresourcesSameLayout());
    EXPECT_TRUE(layout.subresLayouts.isEmpty());
}

TEST(VulkanImageLayoutSubresTest, AfterPartialSetNotAllSameLayout)
{
    const VkImageAspectFlags colorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 2u, 2u, colorAspect);

    // Set only the first mip of the first layer.
    VkImageSubresourceRange range{};
    range.aspectMask = colorAspect;
    range.baseMipLevel = 0u;
    range.levelCount = 1u;
    range.baseArrayLayer = 0u;
    range.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

    EXPECT_FALSE(layout.areAllSubresourcesSameLayout());
}

// getSubresLayout tests

TEST(VulkanImageLayoutGetSubresTest, WithoutTableReturnMainLayout)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 4u, 3u, VK_IMAGE_ASPECT_COLOR_BIT);

    // Any valid combination should return mainLayout when no table is present.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, 0u), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(layout.getSubresLayout(2u, 3u, 0u), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

TEST(VulkanImageLayoutGetSubresTest, LayerTildeZeroMapsToLayerZero)
{
    const VkImageAspectFlags colorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 2u, 3u, colorAspect);

    // Expand the table by partial set on a specific subresource.
    VkImageSubresourceRange range{};
    range.aspectMask = colorAspect;
    range.baseMipLevel = 0u;
    range.levelCount = 1u;
    range.baseArrayLayer = 0u;
    range.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

    // ~0u should map to layer 0.
    EXPECT_EQ(layout.getSubresLayout(~0u, 0u, 0u), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // Layer 0 directly should match.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, 0u), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // Layer 1 was not updated and should remain UNDEFINED.
    EXPECT_EQ(layout.getSubresLayout(1u, 0u, 0u), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST(VulkanImageLayoutGetSubresTest, StencilAspectMapsToLastPlane)
{
    const VkImageAspectFlags depthStencilAspect =
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 1u, 1u, depthStencilAspect);

    // Set depth to SHADER_READ_ONLY_OPTIMAL (partial, aspect depth only).
    VkImageSubresourceRange depthRange{};
    depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthRange.baseMipLevel = 0u;
    depthRange.levelCount = 1u;
    depthRange.baseArrayLayer = 0u;
    depthRange.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, depthRange);

    // Depth aspect (plane 0) got updated.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, VK_IMAGE_ASPECT_DEPTH_BIT), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // Stencil aspect (plane numPlanes-1 = 1) was not changed.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, VK_IMAGE_ASPECT_STENCIL_BIT), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST(VulkanImageLayoutGetSubresTest, IndexMathPlaneLayerMip)
{
    // numPlanes=2, numLayers=2, numMips=3: total 12 entries.
    // index = plane*(numLayers*numMips) + layer*numMips + mip
    const VkImageAspectFlags depthStencilAspect =
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 3u, 2u, depthStencilAspect);

    // Set a single entry: plane=0, layer=1, mip=2 -> index = 0*(2*3)+1*3+2 = 5
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    range.baseMipLevel = 2u;
    range.levelCount = 1u;
    range.baseArrayLayer = 1u;
    range.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, range);

    EXPECT_EQ(layout.getSubresLayout(1u, 2u, 0u), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // Neighboring entry (plane=0, layer=1, mip=1 -> index=4) should be unchanged.
    EXPECT_EQ(layout.getSubresLayout(1u, 1u, 0u), VK_IMAGE_LAYOUT_UNDEFINED);
    // Stencil plane (plane=1, layer=1, mip=2 -> index=11) should be unchanged.
    EXPECT_EQ(layout.getSubresLayout(1u, 2u, 1u), VK_IMAGE_LAYOUT_UNDEFINED);
}

// areSubresourcesSameLayout tests

TEST(VulkanImageLayoutSubresRangeTest, NoTableCompareWithMainLayout)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_GENERAL, 2u, 2u, VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0u;
    range.levelCount = 2u;
    range.baseArrayLayer = 0u;
    range.layerCount = 2u;

    EXPECT_TRUE(layout.areSubresourcesSameLayout(VK_IMAGE_LAYOUT_GENERAL, range));
    EXPECT_FALSE(layout.areSubresourcesSameLayout(VK_IMAGE_LAYOUT_UNDEFINED, range));
}

TEST(VulkanImageLayoutSubresRangeTest, WithTableMixedLayouts)
{
    const VkImageAspectFlags colorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 2u, 1u, colorAspect);

    // Set only mip 0 to SHADER_READ_ONLY_OPTIMAL.
    VkImageSubresourceRange mip0Range{};
    mip0Range.aspectMask = colorAspect;
    mip0Range.baseMipLevel = 0u;
    mip0Range.levelCount = 1u;
    mip0Range.baseArrayLayer = 0u;
    mip0Range.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mip0Range);

    VkImageSubresourceRange mip1Range{};
    mip1Range.aspectMask = colorAspect;
    mip1Range.baseMipLevel = 1u;
    mip1Range.levelCount = 1u;
    mip1Range.baseArrayLayer = 0u;
    mip1Range.layerCount = 1u;

    // Mip 0 matches.
    EXPECT_TRUE(layout.areSubresourcesSameLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mip0Range));
    // Mip 1 is still UNDEFINED.
    EXPECT_TRUE(layout.areSubresourcesSameLayout(VK_IMAGE_LAYOUT_UNDEFINED, mip1Range));

    // Both mips together: not uniform SHADER_READ_ONLY_OPTIMAL.
    VkImageSubresourceRange fullRange{};
    fullRange.aspectMask = colorAspect;
    fullRange.baseMipLevel = 0u;
    fullRange.levelCount = VK_REMAINING_MIP_LEVELS;
    fullRange.baseArrayLayer = 0u;
    fullRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    EXPECT_FALSE(layout.areSubresourcesSameLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fullRange));
}

// getSubresRangeLayerCount and getSubresRangeMipCount tests

TEST(VulkanImageLayoutRangeCountTest, RemainingLayersExpands)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 1u, 6u, VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageSubresourceRange range{};
    range.baseArrayLayer = 2u;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    EXPECT_EQ(layout.getSubresRangeLayerCount(range), 4u);
}

TEST(VulkanImageLayoutRangeCountTest, ExplicitLayerCountPassedThrough)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 1u, 6u, VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageSubresourceRange range{};
    range.baseArrayLayer = 0u;
    range.layerCount = 3u;

    EXPECT_EQ(layout.getSubresRangeLayerCount(range), 3u);
}

TEST(VulkanImageLayoutRangeCountTest, RemainingMipsExpands)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 5u, 1u, VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageSubresourceRange range{};
    range.baseMipLevel = 1u;
    range.levelCount = VK_REMAINING_MIP_LEVELS;

    EXPECT_EQ(layout.getSubresRangeMipCount(range), 4u);
}

TEST(VulkanImageLayoutRangeCountTest, ExplicitMipCountPassedThrough)
{
    const VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 5u, 1u, VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageSubresourceRange range{};
    range.baseMipLevel = 2u;
    range.levelCount = 2u;

    EXPECT_EQ(layout.getSubresRangeMipCount(range), 2u);
}

// collapseSubresLayoutsIfSame tests

TEST(VulkanImageLayoutCollapseTest, AlreadyCollapsedIsNoOp)
{
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_GENERAL, 2u, 2u, VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_TRUE(layout.areAllSubresourcesSameLayout());

    layout.collapseSubresLayoutsIfSame();

    EXPECT_TRUE(layout.areAllSubresourcesSameLayout());
    EXPECT_EQ(layout.mainLayout, VK_IMAGE_LAYOUT_GENERAL);
}

TEST(VulkanImageLayoutCollapseTest, AllSameAfterExpandCollapses)
{
    const VkImageAspectFlags colorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 2u, 1u, colorAspect);

    // Expand table with one mip different, then set it to match.
    VkImageSubresourceRange mip0{};
    mip0.aspectMask = colorAspect;
    mip0.baseMipLevel = 0u;
    mip0.levelCount = 1u;
    mip0.baseArrayLayer = 0u;
    mip0.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mip0);
    EXPECT_FALSE(layout.areAllSubresourcesSameLayout());

    VkImageSubresourceRange mip1{};
    mip1.aspectMask = colorAspect;
    mip1.baseMipLevel = 1u;
    mip1.levelCount = 1u;
    mip1.baseArrayLayer = 0u;
    mip1.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mip1);

    // set() calls collapseSubresLayoutsIfSame() internally.
    EXPECT_TRUE(layout.areAllSubresourcesSameLayout());
    EXPECT_EQ(layout.mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST(VulkanImageLayoutCollapseTest, DifferentLayoutsDoNotCollapse)
{
    const VkImageAspectFlags colorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 2u, 1u, colorAspect);

    VkImageSubresourceRange mip0{};
    mip0.aspectMask = colorAspect;
    mip0.baseMipLevel = 0u;
    mip0.levelCount = 1u;
    mip0.baseArrayLayer = 0u;
    mip0.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mip0);

    // Mip 1 stays UNDEFINED so the table is not uniform.
    layout.collapseSubresLayoutsIfSame();

    EXPECT_FALSE(layout.areAllSubresourcesSameLayout());
}

// set() tests

TEST(VulkanImageLayoutSetTest, WholeImageResetsClearsTable)
{
    const VkImageAspectFlags colorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 2u, 2u, colorAspect);

    // Expand the table first.
    VkImageSubresourceRange partial{};
    partial.aspectMask = colorAspect;
    partial.baseMipLevel = 0u;
    partial.levelCount = 1u;
    partial.baseArrayLayer = 0u;
    partial.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, partial);
    EXPECT_FALSE(layout.areAllSubresourcesSameLayout());

    // Now set the whole image.
    VkImageSubresourceRange wholeImage{};
    wholeImage.aspectMask = colorAspect;
    wholeImage.baseMipLevel = 0u;
    wholeImage.levelCount = VK_REMAINING_MIP_LEVELS;
    wholeImage.baseArrayLayer = 0u;
    wholeImage.layerCount = VK_REMAINING_ARRAY_LAYERS;
    layout.set(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, wholeImage);

    EXPECT_TRUE(layout.areAllSubresourcesSameLayout());
    EXPECT_EQ(layout.mainLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_TRUE(layout.subresLayouts.isEmpty());
}

TEST(VulkanImageLayoutSetTest, PartialSetExpandsTable)
{
    const VkImageAspectFlags colorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 2u, 2u, colorAspect);
    EXPECT_TRUE(layout.subresLayouts.isEmpty());

    VkImageSubresourceRange partial{};
    partial.aspectMask = colorAspect;
    partial.baseMipLevel = 0u;
    partial.levelCount = 1u;
    partial.baseArrayLayer = 0u;
    partial.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_GENERAL, partial);

    // Table is now expanded to numPlanes * numLayers * numMips = 1 * 2 * 2 = 4 entries.
    EXPECT_EQ(layout.subresLayouts.size(), 4u);
    // Entry at plane=0, layer=0, mip=0 is GENERAL.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, 0u), VK_IMAGE_LAYOUT_GENERAL);
    // Other entries remain UNDEFINED.
    EXPECT_EQ(layout.getSubresLayout(0u, 1u, 0u), VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(layout.getSubresLayout(1u, 0u, 0u), VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(layout.getSubresLayout(1u, 1u, 0u), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST(VulkanImageLayoutSetTest, StencilOnlyTouchesStencilPlane)
{
    // numPlanes=2, numLayers=1, numMips=1 -> table size = 2
    const VkImageAspectFlags depthStencilAspect =
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 1u, 1u, depthStencilAspect);

    VkImageSubresourceRange stencilRange{};
    stencilRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    stencilRange.baseMipLevel = 0u;
    stencilRange.levelCount = 1u;
    stencilRange.baseArrayLayer = 0u;
    stencilRange.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stencilRange);

    // Stencil plane (plane=1) got updated.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, 1u), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // Depth plane (plane=0) is still UNDEFINED.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, 0u), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST(VulkanImageLayoutSetTest, DepthOnlyTouchesDepthPlane)
{
    const VkImageAspectFlags depthStencilAspect =
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 1u, 1u, depthStencilAspect);

    VkImageSubresourceRange depthRange{};
    depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthRange.baseMipLevel = 0u;
    depthRange.levelCount = 1u;
    depthRange.baseArrayLayer = 0u;
    depthRange.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depthRange);

    // Depth plane (plane=0) got updated.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, 0u), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // Stencil plane (plane=1) is still UNDEFINED.
    EXPECT_EQ(layout.getSubresLayout(0u, 0u, 1u), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST(VulkanImageLayoutSetTest, SettingRemainingToMatchCollapsesTable)
{
    // Start with D+S image, set depth plane to X, then set stencil plane to X.
    // After the second set, all entries are X, so the table should collapse.
    const VkImageAspectFlags depthStencilAspect =
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    VulkanImageLayout layout(VK_IMAGE_LAYOUT_UNDEFINED, 1u, 1u, depthStencilAspect);

    VkImageSubresourceRange depthRange{};
    depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthRange.baseMipLevel = 0u;
    depthRange.levelCount = 1u;
    depthRange.baseArrayLayer = 0u;
    depthRange.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, depthRange);
    EXPECT_FALSE(layout.areAllSubresourcesSameLayout());

    VkImageSubresourceRange stencilRange{};
    stencilRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    stencilRange.baseMipLevel = 0u;
    stencilRange.levelCount = 1u;
    stencilRange.baseArrayLayer = 0u;
    stencilRange.layerCount = 1u;
    layout.set(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, stencilRange);

    EXPECT_TRUE(layout.areAllSubresourcesSameLayout());
    EXPECT_EQ(layout.mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

}   // namespace gp::rhi::vulkan::tests
