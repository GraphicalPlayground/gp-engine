// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTextureUsage.hpp"
#include "CoreMinimal.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanTextureEnums.hpp"
#include "resources/VulkanTextureStats.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

using resource::TextureCreateFlags;

constexpr VkImageUsageFlags kCopyAndSampleUsage =
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

[[nodiscard]] resource::TextureDesc make2D(const PixelFormat format, const gp::Int32 size) noexcept
{
    return resource::TextureCreateDesc::create2D("Usage", size, size, format);
}

}   // namespace

TEST(VulkanTextureUsageTest, PlainTexturesCanBeCopiedAndSampled)
{
    EXPECT_EQ(getVulkanImageUsageFlags(TextureCreateFlags::None, false), kCopyAndSampleUsage);
}

TEST(VulkanTextureUsageTest, TargetsGetTheirAttachmentUsage)
{
    EXPECT_EQ(
        getVulkanImageUsageFlags(TextureCreateFlags::RenderTargetable, false),
        kCopyAndSampleUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    );
    EXPECT_EQ(
        getVulkanImageUsageFlags(TextureCreateFlags::DepthStencilTargetable, false),
        kCopyAndSampleUsage | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    EXPECT_EQ(
        getVulkanImageUsageFlags(TextureCreateFlags::ResolveTargetable, false),
        kCopyAndSampleUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    );
    EXPECT_EQ(
        getVulkanImageUsageFlags(TextureCreateFlags::UAV, false), kCopyAndSampleUsage | VK_IMAGE_USAGE_STORAGE_BIT
    );
}

TEST(VulkanTextureUsageTest, MemorylessTargetsDropCopiesOnlyWhenSupported)
{
    const TextureCreateFlags flags = TextureCreateFlags::RenderTargetable | TextureCreateFlags::Memoryless;

    const VkImageUsageFlags tileUsage = getVulkanImageUsageFlags(flags, true);
    EXPECT_EQ(tileUsage, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);

    const VkImageUsageFlags fallbackUsage = getVulkanImageUsageFlags(flags, false);
    EXPECT_EQ(fallbackUsage, kCopyAndSampleUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
}

TEST(VulkanTextureUsageTest, FormatFeaturesFilterUnsupportedUsage)
{
    bool needsExtendedUsage = true;
    const VkImageUsageFlags usage = kCopyAndSampleUsage | VK_IMAGE_USAGE_STORAGE_BIT;

    const VkImageUsageFlags filtered = filterVulkanImageUsageByFormatFeatures(
        usage, VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT, false, needsExtendedUsage
    );
    EXPECT_EQ(
        filtered, static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    );
    EXPECT_FALSE(needsExtendedUsage);
}

TEST(VulkanTextureUsageTest, SeparateUAVFormatKeepsStorageWithExtendedUsage)
{
    bool needsExtendedUsage = false;
    const VkImageUsageFlags filtered = filterVulkanImageUsageByFormatFeatures(
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
        true,
        needsExtendedUsage
    );
    EXPECT_EQ(filtered, static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
    EXPECT_TRUE(needsExtendedUsage);
}

TEST(VulkanTextureUsageTest, InitialLayoutFollowsTheAccess)
{
    using sync::ResourceAccess;
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::RTV, false, true, false), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::RTV, false, true, true), VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ
    );
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::Present, false, true, false), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::DSVWrite, true, true, false), VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
    );
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::DSVRead, true, true, false), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::SRVGraphicsPixel, false, true, false),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(getVulkanInitialImageLayout(ResourceAccess::SRVCompute, false, false, false), VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::SRVCompute, true, true, false), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
    );
    EXPECT_EQ(getVulkanInitialImageLayout(ResourceAccess::UAVCompute, false, true, false), VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::CopySrc, false, true, false), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );
    EXPECT_EQ(
        getVulkanInitialImageLayout(ResourceAccess::CopyDest, false, true, false), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
}

TEST(VulkanTextureUsageTest, AccessWithoutLayoutStartsUndefined)
{
    EXPECT_EQ(
        getVulkanInitialImageLayout(sync::ResourceAccess::Unknown, false, true, false), VK_IMAGE_LAYOUT_UNDEFINED
    );
    EXPECT_EQ(
        getVulkanInitialImageLayout(sync::ResourceAccess::Discard, false, true, false), VK_IMAGE_LAYOUT_UNDEFINED
    );
    EXPECT_EQ(
        getVulkanInitialImageLayout(sync::ResourceAccess::CPURead, false, true, false), VK_IMAGE_LAYOUT_UNDEFINED
    );
}

TEST(VulkanTextureUsageTest, LayerCountCoversFacesAndSlices)
{
    EXPECT_EQ(getVulkanTextureLayerCount(make2D(PixelFormat::R8G8B8A8_UNorm, 4)), 1u);
    EXPECT_EQ(
        getVulkanTextureLayerCount(
            resource::TextureCreateDesc::create2DArray("Array", 4, 4, 5u, PixelFormat::R8_UNorm)
        ),
        5u
    );
    EXPECT_EQ(
        getVulkanTextureLayerCount(resource::TextureCreateDesc::createCube("Cube", 4u, PixelFormat::R8_UNorm)), 6u
    );
    EXPECT_EQ(
        getVulkanTextureLayerCount(
            resource::TextureCreateDesc::createCubeArray("CubeArray", 4u, 3u, PixelFormat::R8_UNorm)
        ),
        18u
    );
    EXPECT_EQ(
        getVulkanTextureLayerCount(resource::TextureCreateDesc::create3D("Volume", 4, 4, 8, PixelFormat::R8_UNorm)), 1u
    );
}

TEST(VulkanTextureUsageTest, MutableFormatsNeedAViewFormatList)
{
    resource::TextureDesc desc = make2D(PixelFormat::R8G8B8A8_UNorm, 4);
    EXPECT_FALSE(isVulkanTextureFormatMutable(desc));

    desc.flags = TextureCreateFlags::SRGB;
    EXPECT_TRUE(isVulkanTextureFormatMutable(desc));

    desc.flags = TextureCreateFlags::None;
    desc.uavFormat = PixelFormat::R32_UInt;
    EXPECT_TRUE(isVulkanTextureFormatMutable(desc));

    desc.uavFormat = desc.format;
    EXPECT_FALSE(isVulkanTextureFormatMutable(desc));
}

TEST(VulkanTextureUsageTest, SubresourcesAreMipMajor)
{
    // 4x4 RGBA8, 2 slices, 2 mips: mip 0 slices are 64 bytes each, then the 2x2 mip 1 slices of 16 bytes.
    resource::TextureDesc desc =
        resource::TextureCreateDesc::create2DArray("Layout", 4, 4, 2u, PixelFormat::R8G8B8A8_UNorm);
    desc.numMips = 2u;

    gp::UInt64 rowStride = 0u;
    gp::UInt64 size = 0u;
    EXPECT_EQ(calculateVulkanSubresourceOffset(desc, 0u, 0u, rowStride, size), 0u);
    EXPECT_EQ(rowStride, 16u);
    EXPECT_EQ(size, 64u);

    EXPECT_EQ(calculateVulkanSubresourceOffset(desc, 0u, 1u, rowStride, size), 64u);
    EXPECT_EQ(calculateVulkanSubresourceOffset(desc, 1u, 0u, rowStride, size), 128u);
    EXPECT_EQ(rowStride, 8u);
    EXPECT_EQ(size, 16u);
    EXPECT_EQ(calculateVulkanSubresourceOffset(desc, 1u, 1u, rowStride, size), 144u);
}

TEST(VulkanTextureUsageTest, SubresourceOffsetUsesBlocksForCompressedFormats)
{
    // 8x8 BC1 is 2x2 blocks of 8 bytes.
    const resource::TextureDesc desc = make2D(PixelFormat::BC1_UNorm, 8);
    gp::UInt64 rowStride = 0u;
    gp::UInt64 size = 0u;
    EXPECT_EQ(calculateVulkanSubresourceOffset(desc, 0u, 0u, rowStride, size), 0u);
    EXPECT_EQ(rowStride, 16u);
    EXPECT_EQ(size, 32u);
}

TEST(VulkanTextureUsageTest, OutOfRangeSubresourceGivesZeros)
{
    const resource::TextureDesc desc = make2D(PixelFormat::R8G8B8A8_UNorm, 4);
    gp::UInt64 rowStride = 7u;
    gp::UInt64 size = 7u;
    EXPECT_EQ(calculateVulkanSubresourceOffset(desc, 1u, 0u, rowStride, size), 0u);
    EXPECT_EQ(rowStride, 0u);
    EXPECT_EQ(size, 0u);
    EXPECT_EQ(calculateVulkanSubresourceOffset(desc, 0u, 1u, rowStride, size), 0u);
    EXPECT_EQ(size, 0u);
}

TEST(VulkanTextureStatsTest, CategoryPrefersReadbackThenTargets)
{
    resource::TextureDesc desc = make2D(PixelFormat::R8G8B8A8_UNorm, 4);
    EXPECT_EQ(getVulkanTextureStatCategory(desc), VulkanTextureStatCategory::Texture2D);

    desc.flags = TextureCreateFlags::UAV;
    EXPECT_EQ(getVulkanTextureStatCategory(desc), VulkanTextureStatCategory::RenderTarget);

    desc.flags = TextureCreateFlags::CPUReadback | TextureCreateFlags::RenderTargetable;
    EXPECT_EQ(getVulkanTextureStatCategory(desc), VulkanTextureStatCategory::CPUReadback);

    EXPECT_EQ(
        getVulkanTextureStatCategory(resource::TextureCreateDesc::createCube("Cube", 4u, PixelFormat::R8_UNorm)),
        VulkanTextureStatCategory::TextureCube
    );
    EXPECT_EQ(
        getVulkanTextureStatCategory(resource::TextureCreateDesc::create3D("Volume", 4, 4, 4, PixelFormat::R8_UNorm)),
        VulkanTextureStatCategory::Texture3D
    );
}

TEST(VulkanTextureStatsTest, AllocationsAndFreesBalance)
{
    constexpr gp::UInt64 kSize = 4096u;
    const gp::USize category = static_cast<gp::USize>(VulkanTextureStatCategory::Texture3D);
    const VulkanTextureMemoryStats before = getVulkanTextureMemoryStats();

    updateVulkanTextureStats(VulkanTextureStatCategory::Texture3D, kSize, true);
    const VulkanTextureMemoryStats allocated = getVulkanTextureMemoryStats();
    EXPECT_EQ(allocated.categoryBytes[category] - before.categoryBytes[category], static_cast<gp::Int64>(kSize));
    EXPECT_EQ(allocated.categoryCounts[category] - before.categoryCounts[category], 1);
    EXPECT_EQ(allocated.totalTextureBytes - before.totalTextureBytes, static_cast<gp::Int64>(kSize));

    updateVulkanTextureStats(VulkanTextureStatCategory::Texture3D, kSize, false);
    const VulkanTextureMemoryStats freed = getVulkanTextureMemoryStats();
    EXPECT_EQ(freed.categoryBytes[category], before.categoryBytes[category]);
    EXPECT_EQ(freed.categoryCounts[category], before.categoryCounts[category]);
    EXPECT_EQ(freed.totalTextureBytes, before.totalTextureBytes);
}

}   // namespace gp::rhi::vulkan::tests
