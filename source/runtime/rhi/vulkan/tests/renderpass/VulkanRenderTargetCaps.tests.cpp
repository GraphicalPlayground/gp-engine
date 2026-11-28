// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanRenderTargetCapsTest, DefaultCapsSupportNothing)
{
    const VulkanRenderTargetCaps caps{};

    EXPECT_FALSE(caps.hasKHRRenderPass2);
    EXPECT_FALSE(caps.hasKHRDynamicRendering);
    EXPECT_FALSE(caps.hasValidShadingRateDataType());
    EXPECT_EQ(caps.getShadingRateImageLayout(), VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_FALSE(caps.usesSeparateDepthStencilLayouts());
}

TEST(VulkanRenderTargetCapsTest, PaletteShadingRateUsesShadingRateAttachmentLayout)
{
    VulkanRenderTargetCaps caps{};
    caps.vrsImageDataType = pipeline::VRSImageDataType::Palette;

    EXPECT_TRUE(caps.hasValidShadingRateDataType());
    EXPECT_EQ(caps.getShadingRateImageLayout(), VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR);
}

TEST(VulkanRenderTargetCapsTest, FractionalShadingRateUsesFragmentDensityLayout)
{
    VulkanRenderTargetCaps caps{};
    caps.vrsImageDataType = pipeline::VRSImageDataType::Fractional;

    EXPECT_TRUE(caps.hasValidShadingRateDataType());
    EXPECT_EQ(caps.getShadingRateImageLayout(), VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT);
}

TEST(VulkanRenderTargetCapsTest, SeparateLayoutsNeedParallelRenderingWithoutMergedQuirk)
{
    VulkanRenderTargetCaps caps{};

    caps.supportsParallelRendering = true;
    EXPECT_TRUE(caps.usesSeparateDepthStencilLayouts());

    caps.requiresMergedDepthStencilLayout = true;
    EXPECT_FALSE(caps.usesSeparateDepthStencilLayouts());

    caps.supportsParallelRendering = false;
    EXPECT_FALSE(caps.usesSeparateDepthStencilLayouts());
}

}   // namespace gp::rhi::vulkan::tests
