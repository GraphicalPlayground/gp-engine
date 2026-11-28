// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanLayoutManager.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanImageLayout.hpp"
#include "CoreMinimal.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

// Fake image handles for offline testing on 64-bit Windows where VkImage is a pointer type.
const VkImage kImageA = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x1000u));
const VkImage kImageB = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x2000u));
const VkImage kImageC = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x3000u));

VulkanBarrierResource
    makeColorTexture(const VkImage image, const gp::UInt32 numMips = 1u, const gp::UInt32 numArrayLayers = 1u)
{
    VulkanBarrierResource tex{};
    tex.type = VulkanBarrierResourceType::Texture;
    tex.image = image;
    tex.fullAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    tex.isDepthOrStencilAspect = false;
    tex.supportsSampling = true;
    tex.numMips = numMips;
    tex.numArrayLayers = numArrayLayers;
    return tex;
}

VulkanBarrierResource
    makeDepthStencilTexture(const VkImage image, const gp::UInt32 numMips = 1u, const gp::UInt32 numArrayLayers = 1u)
{
    VulkanBarrierResource tex{};
    tex.type = VulkanBarrierResourceType::Texture;
    tex.image = image;
    tex.fullAspectMask = static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    tex.isDepthOrStencilAspect = true;
    tex.supportsSampling = false;
    tex.numMips = numMips;
    tex.numArrayLayers = numArrayLayers;
    return tex;
}

}   // namespace

// Constructor and accessors

TEST(VulkanLayoutManagerTest, ConstructorAccessors)
{
    VulkanLayoutManager localManager(false, nullptr);
    EXPECT_FALSE(localManager.isWriteOnly());
    EXPECT_EQ(localManager.getFallback(), nullptr);
    EXPECT_EQ(localManager.getTrackedImageCount(), 0u);

    VulkanLayoutManager writeOnlyManager(true, &localManager);
    EXPECT_TRUE(writeOnlyManager.isWriteOnly());
    EXPECT_EQ(writeOnlyManager.getFallback(), &localManager);
    EXPECT_EQ(writeOnlyManager.getTrackedImageCount(), 0u);
}

// notifyDeletedImage tests

TEST(VulkanLayoutManagerTest, NotifyDeletedImageRemovesTrackedImage)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    const VulkanBarrierResource texB = makeColorTexture(kImageB);

    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_UNDEFINED);
    mgr.setFullLayout(texB, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(mgr.getTrackedImageCount(), 2u);

    mgr.notifyDeletedImage(kImageA);
    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);

    // getFullLayout should now return nullptr for the deleted image.
    EXPECT_EQ(mgr.getFullLayout(kImageA), nullptr);
    // The remaining image should still be accessible.
    ASSERT_NE(mgr.getFullLayout(kImageB), nullptr);
    EXPECT_EQ(mgr.getFullLayout(kImageB)->mainLayout, VK_IMAGE_LAYOUT_GENERAL);
}

TEST(VulkanLayoutManagerTest, NotifyDeletedImageUnknownImageIsNoOp)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);

    // Deleting an untracked image should not change the count.
    mgr.notifyDeletedImage(kImageB);
    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);
}

// getFullLayout(VkImage) tests

TEST(VulkanLayoutManagerTest, GetFullLayoutVkImageLocalHit)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    const VulkanImageLayout* const layout = mgr.getFullLayout(kImageA);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST(VulkanLayoutManagerTest, GetFullLayoutVkImageFallbackHit)
{
    VulkanLayoutManager fallback(false, nullptr);
    VulkanLayoutManager local(false, &fallback);

    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    fallback.setFullLayout(texA, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // Local has not seen kImageA yet; fallback should answer.
    const VulkanImageLayout* const layout = local.getFullLayout(kImageA);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

TEST(VulkanLayoutManagerTest, GetFullLayoutVkImageUnknownReturnsNull)
{
    VulkanLayoutManager mgr(false, nullptr);

    EXPECT_EQ(mgr.getFullLayout(kImageA), nullptr);
}

// getFullLayout(texture, addIfNotFound) tests

TEST(VulkanLayoutManagerTest, GetFullLayoutTextureLocalHit)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA, 4u, 2u);
    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_GENERAL);

    const VulkanImageLayout* const layout = mgr.getFullLayout(texA);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(layout->numMips, 4u);
    EXPECT_EQ(layout->numLayers, 2u);
}

TEST(VulkanLayoutManagerTest, GetFullLayoutTextureAddIfNotFoundFalseReturnsNull)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA);

    EXPECT_EQ(mgr.getFullLayout(texA, false), nullptr);
}

TEST(VulkanLayoutManagerTest, GetFullLayoutTextureAddIfNotFoundTrueCreatesEntry)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA, 2u, 3u);
    EXPECT_EQ(mgr.getTrackedImageCount(), 0u);

    const VulkanImageLayout* const layout = mgr.getFullLayout(texA, true, VK_IMAGE_LAYOUT_UNDEFINED);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(layout->numMips, 2u);
    EXPECT_EQ(layout->numLayers, 3u);
}

TEST(VulkanLayoutManagerTest, GetFullLayoutTextureImportsFromFallback)
{
    VulkanLayoutManager fallback(false, nullptr);
    VulkanLayoutManager local(false, &fallback);

    const VulkanBarrierResource texA = makeColorTexture(kImageA, 2u, 1u);
    fallback.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(local.getTrackedImageCount(), 0u);

    const VulkanImageLayout* const layout = local.getFullLayout(texA);
    ASSERT_NE(layout, nullptr);
    // Import should copy the layout into local.
    EXPECT_EQ(local.getTrackedImageCount(), 1u);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST(VulkanLayoutManagerTest, GetFullLayoutTextureImportedCopyIsIndependentOfFallback)
{
    VulkanLayoutManager fallback(false, nullptr);
    VulkanLayoutManager local(false, &fallback);

    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    fallback.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Import the layout into local.
    const VulkanImageLayout* const localLayout = local.getFullLayout(texA);
    ASSERT_NE(localLayout, nullptr);

    // Update fallback's copy of the image.
    fallback.setFullLayout(texA, VK_IMAGE_LAYOUT_GENERAL);

    // Local copy should still reflect the imported state.
    const VulkanImageLayout* const localLayoutAfter = local.getFullLayout(texA);
    ASSERT_NE(localLayoutAfter, nullptr);
    EXPECT_EQ(localLayoutAfter->mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// setFullLayout(VkImage, layout) tests

TEST(VulkanLayoutManagerTest, SetFullLayoutVkImageCreatesNewEntry)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanImageLayout newLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2u, 1u, VK_IMAGE_ASPECT_COLOR_BIT);

    mgr.setFullLayout(kImageA, newLayout);

    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);
    const VulkanImageLayout* const retrieved = mgr.getFullLayout(kImageA);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->mainLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(retrieved->numMips, 2u);
}

TEST(VulkanLayoutManagerTest, SetFullLayoutVkImageReplacesExistingEntry)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanImageLayout original(VK_IMAGE_LAYOUT_UNDEFINED, 1u, 1u, VK_IMAGE_ASPECT_COLOR_BIT);
    mgr.setFullLayout(kImageA, original);

    const VulkanImageLayout replacement(VK_IMAGE_LAYOUT_GENERAL, 3u, 2u, VK_IMAGE_ASPECT_COLOR_BIT);
    mgr.setFullLayout(kImageA, replacement);

    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);
    const VulkanImageLayout* const retrieved = mgr.getFullLayout(kImageA);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->mainLayout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(retrieved->numMips, 3u);
    EXPECT_EQ(retrieved->numLayers, 2u);
}

// setFullLayout(texture, layout, onlyIfNotFound) tests

TEST(VulkanLayoutManagerTest, SetFullLayoutTextureCreatesNewEntry)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA, 4u, 2u);

    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_GENERAL);

    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);
    const VulkanImageLayout* const layout = mgr.getFullLayout(kImageA);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(layout->numMips, 4u);
}

TEST(VulkanLayoutManagerTest, SetFullLayoutTextureReplacesExistingEntry)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA);

    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_UNDEFINED);
    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);
    const VulkanImageLayout* const layout = mgr.getFullLayout(kImageA);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST(VulkanLayoutManagerTest, SetFullLayoutTextureOnlyIfNotFoundKeepsExisting)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA);

    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_GENERAL);
    // onlyIfNotFound=true: existing entry should not change.
    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);

    const VulkanImageLayout* const layout = mgr.getFullLayout(kImageA);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_GENERAL);
}

// setLayout tests

TEST(VulkanLayoutManagerTest, SetLayoutNewImageStartsUndefined)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA, 2u, 2u);

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0u;
    range.levelCount = 1u;
    range.baseArrayLayer = 0u;
    range.layerCount = 1u;
    mgr.setLayout(texA, range, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EXPECT_EQ(mgr.getTrackedImageCount(), 1u);
    const VulkanImageLayout* const layout = mgr.getFullLayout(kImageA);
    ASSERT_NE(layout, nullptr);
    // Partial set should expand the table; mip=0, layer=0 is SHADER_READ_ONLY_OPTIMAL.
    EXPECT_EQ(layout->getSubresLayout(0u, 0u, 0u), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // Other subresources are UNDEFINED (the initial value used for a new entry).
    EXPECT_EQ(layout->getSubresLayout(1u, 0u, 0u), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST(VulkanLayoutManagerTest, SetLayoutExistingImagePartialUpdate)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA, 2u, 1u);
    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_GENERAL);

    VkImageSubresourceRange mip0{};
    mip0.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    mip0.baseMipLevel = 0u;
    mip0.levelCount = 1u;
    mip0.baseArrayLayer = 0u;
    mip0.layerCount = 1u;
    mgr.setLayout(texA, mip0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    const VulkanImageLayout* const layout = mgr.getFullLayout(kImageA);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->getSubresLayout(0u, 0u, 0u), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(layout->getSubresLayout(0u, 1u, 0u), VK_IMAGE_LAYOUT_GENERAL);
}

// transferTo tests

TEST(VulkanLayoutManagerTest, TransferToMovesEntriesAndClearsSource)
{
    VulkanLayoutManager source(false, nullptr);
    VulkanLayoutManager destination(false, nullptr);

    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    const VulkanBarrierResource texB = makeColorTexture(kImageB);
    source.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    source.setFullLayout(texB, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(source.getTrackedImageCount(), 2u);

    source.transferTo(destination);

    EXPECT_EQ(source.getTrackedImageCount(), 0u);
    EXPECT_EQ(destination.getTrackedImageCount(), 2u);

    const VulkanImageLayout* const layoutA = destination.getFullLayout(kImageA);
    ASSERT_NE(layoutA, nullptr);
    EXPECT_EQ(layoutA->mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    const VulkanImageLayout* const layoutB = destination.getFullLayout(kImageB);
    ASSERT_NE(layoutB, nullptr);
    EXPECT_EQ(layoutB->mainLayout, VK_IMAGE_LAYOUT_GENERAL);
}

TEST(VulkanLayoutManagerTest, TransferToOverwritesSameImageInDestination)
{
    VulkanLayoutManager source(false, nullptr);
    VulkanLayoutManager destination(false, nullptr);

    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    destination.setFullLayout(texA, VK_IMAGE_LAYOUT_UNDEFINED);
    source.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    source.transferTo(destination);

    EXPECT_EQ(destination.getTrackedImageCount(), 1u);
    const VulkanImageLayout* const layout = destination.getFullLayout(kImageA);
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST(VulkanLayoutManagerTest, TransferToEmptySourceIsNoOp)
{
    VulkanLayoutManager source(false, nullptr);
    VulkanLayoutManager destination(false, nullptr);

    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    destination.setFullLayout(texA, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(destination.getTrackedImageCount(), 1u);

    source.transferTo(destination);

    EXPECT_EQ(destination.getTrackedImageCount(), 1u);
    EXPECT_EQ(source.getTrackedImageCount(), 0u);
}

// Write-only manager tests

TEST(VulkanLayoutManagerTest, WriteOnlyManagerAcceptsWrites)
{
    VulkanLayoutManager writeOnly(true, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA);

    writeOnly.setFullLayout(texA, VK_IMAGE_LAYOUT_GENERAL);

    EXPECT_EQ(writeOnly.getTrackedImageCount(), 1u);
}

TEST(VulkanLayoutManagerTest, WriteOnlyManagerCanTransferToReadWriteManager)
{
    VulkanLayoutManager writeOnly(true, nullptr);
    VulkanLayoutManager readWrite(false, nullptr);

    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    const VulkanBarrierResource texB = makeColorTexture(kImageB);
    writeOnly.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    writeOnly.setFullLayout(texB, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    writeOnly.transferTo(readWrite);

    EXPECT_EQ(writeOnly.getTrackedImageCount(), 0u);
    EXPECT_EQ(readWrite.getTrackedImageCount(), 2u);

    const VulkanImageLayout* const layoutA = readWrite.getFullLayout(kImageA);
    ASSERT_NE(layoutA, nullptr);
    EXPECT_EQ(layoutA->mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST(VulkanLayoutManagerTest, MultipleImagesSetAndRetrieved)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA, 1u, 1u);
    const VulkanBarrierResource texB = makeColorTexture(kImageB, 2u, 4u);
    const VulkanBarrierResource texC = makeDepthStencilTexture(kImageC, 3u, 1u);

    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    mgr.setFullLayout(texB, VK_IMAGE_LAYOUT_GENERAL);
    mgr.setFullLayout(texC, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(mgr.getTrackedImageCount(), 3u);

    const VulkanImageLayout* const a = mgr.getFullLayout(kImageA);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->mainLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(a->numMips, 1u);

    const VulkanImageLayout* const b = mgr.getFullLayout(kImageB);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->mainLayout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(b->numMips, 2u);
    EXPECT_EQ(b->numLayers, 4u);

    const VulkanImageLayout* const c = mgr.getFullLayout(kImageC);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->mainLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(c->numPlanes, 2u);
}

TEST(VulkanLayoutManagerTest, NotifyDeletedImageDoesNotAffectOtherImages)
{
    VulkanLayoutManager mgr(false, nullptr);
    const VulkanBarrierResource texA = makeColorTexture(kImageA);
    const VulkanBarrierResource texB = makeColorTexture(kImageB);
    const VulkanBarrierResource texC = makeColorTexture(kImageC);

    mgr.setFullLayout(texA, VK_IMAGE_LAYOUT_GENERAL);
    mgr.setFullLayout(texB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    mgr.setFullLayout(texC, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    mgr.notifyDeletedImage(kImageB);

    EXPECT_EQ(mgr.getTrackedImageCount(), 2u);
    EXPECT_EQ(mgr.getFullLayout(kImageB), nullptr);
    ASSERT_NE(mgr.getFullLayout(kImageA), nullptr);
    ASSERT_NE(mgr.getFullLayout(kImageC), nullptr);
}

}   // namespace gp::rhi::vulkan::tests
