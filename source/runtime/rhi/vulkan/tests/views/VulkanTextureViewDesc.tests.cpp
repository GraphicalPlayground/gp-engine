// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanTextureViewDesc.hpp"
#include "CoreMinimal.hpp"
#include "RHIPixelFormat.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Builds a fully populated description.
[[nodiscard]] VulkanTextureViewDesc makeBaseDesc() noexcept
{
    VulkanTextureViewDesc desc{};
    desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
    desc.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.pixelFormat = PixelFormat::R8G8B8A8_UNorm;
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.firstMip = 0u;
    desc.numMips = 1u;
    desc.arraySliceIndex = 0u;
    desc.numArraySlices = 1u;
    desc.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.useIdentitySwizzle = false;
    return desc;
}

}   // namespace

TEST(VulkanTextureViewDescTest, DefaultsCoverTheWholeImage)
{
    const VulkanTextureViewDesc desc{};

    EXPECT_EQ(desc.viewType, VK_IMAGE_VIEW_TYPE_MAX_ENUM);
    EXPECT_EQ(desc.aspectFlags, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_NONE));
    EXPECT_EQ(desc.pixelFormat, PixelFormat::Unknown);
    EXPECT_EQ(desc.format, VK_FORMAT_UNDEFINED);
    EXPECT_EQ(desc.firstMip, 0u);
    EXPECT_EQ(desc.numMips, VK_REMAINING_MIP_LEVELS);
    EXPECT_EQ(desc.arraySliceIndex, 0u);
    EXPECT_EQ(desc.numArraySlices, VK_REMAINING_ARRAY_LAYERS);
    EXPECT_EQ(desc.imageUsageFlags, 0u);
    EXPECT_EQ(desc.samplerYcbcrConversion, static_cast<VkSamplerYcbcrConversion>(VK_NULL_HANDLE));
    EXPECT_FALSE(desc.useIdentitySwizzle);
}

TEST(VulkanTextureViewDescTest, EqualDescriptionsHashEqually)
{
    const VulkanTextureViewDesc first = makeBaseDesc();
    const VulkanTextureViewDesc second = makeBaseDesc();

    EXPECT_EQ(first.getHash(), second.getHash());
}

TEST(VulkanTextureViewDescTest, EveryFieldChangesTheHash)
{
    const gp::UInt64 baseHash = makeBaseDesc().getHash();

    VulkanTextureViewDesc desc = makeBaseDesc();
    desc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.pixelFormat = PixelFormat::R32_Float;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.format = VK_FORMAT_R8G8B8A8_SRGB;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.firstMip = 1u;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.numMips = 2u;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.arraySliceIndex = 1u;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.numArraySlices = 2u;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.imageUsageFlags = VK_IMAGE_USAGE_STORAGE_BIT;
    EXPECT_NE(desc.getHash(), baseHash);

    desc = makeBaseDesc();
    desc.useIdentitySwizzle = true;
    EXPECT_NE(desc.getHash(), baseHash);
}

TEST(VulkanTextureViewDescTest, MipAndLayerFieldsAreNotInterchangeable)
{
    VulkanTextureViewDesc mipDesc = makeBaseDesc();
    mipDesc.firstMip = 1u;
    VulkanTextureViewDesc layerDesc = makeBaseDesc();
    layerDesc.arraySliceIndex = 1u;

    EXPECT_NE(mipDesc.getHash(), layerDesc.getHash());
}

}   // namespace gp::rhi::vulkan::tests
