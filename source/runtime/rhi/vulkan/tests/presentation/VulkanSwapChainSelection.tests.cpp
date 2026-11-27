// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanSwapChainSelection.hpp"
#include "containers/arrays/Vector.hpp"       // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/color/PixelFormatEnums.hpp"   // IWYU pragma: keep
#include "presentation/VulkanPresentationEnums.hpp"
#include "presentation/VulkanPresentationTypes.hpp"
#include "RHIForward.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Builds surface capabilities with a free (undefined) current extent.
/// @return Capabilities accepting 1..4096 pixels and 2..8 images.
[[nodiscard]] VkSurfaceCapabilitiesKHR makeFreeExtentCapabilities() noexcept
{
    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.minImageCount = 2u;
    capabilities.maxImageCount = 8u;
    capabilities.currentExtent = VkExtent2D{ 0xFFFFFFFFu, 0xFFFFFFFFu };
    capabilities.minImageExtent = VkExtent2D{ 1u, 1u };
    capabilities.maxImageExtent = VkExtent2D{ 4096u, 4096u };
    capabilities.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR | VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    capabilities.currentTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    return capabilities;
}

}   // namespace

TEST(VulkanSwapChainSelectionTest, ColorSpaceFollowsDisplayOutput)
{
    EXPECT_EQ(selectVulkanSwapChainColorSpace(VulkanDisplayOutputFormat::SdrSrgb), VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    EXPECT_EQ(
        selectVulkanSwapChainColorSpace(VulkanDisplayOutputFormat::SdrRec709), VK_COLOR_SPACE_BT709_NONLINEAR_EXT
    );
    EXPECT_EQ(
        selectVulkanSwapChainColorSpace(VulkanDisplayOutputFormat::HdrAces1000NitSt2084),
        VK_COLOR_SPACE_HDR10_ST2084_EXT
    );
    EXPECT_EQ(
        selectVulkanSwapChainColorSpace(VulkanDisplayOutputFormat::HdrAces2000NitSt2084),
        VK_COLOR_SPACE_HDR10_ST2084_EXT
    );
    EXPECT_EQ(
        selectVulkanSwapChainColorSpace(VulkanDisplayOutputFormat::HdrLinearExr), VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    );
}

TEST(VulkanSwapChainSelectionTest, SurfaceFormatRejectsEmptyList)
{
    const gp::Vector<VkSurfaceFormatKHR> formats{};
    VulkanSurfaceFormatSelection selection{};

    EXPECT_FALSE(
        selectVulkanSurfaceFormat(formats.view(), PixelFormat::Unknown, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, selection)
    );
}

TEST(VulkanSwapChainSelectionTest, SurfaceFormatSingleUndefinedEntryAcceptsRequestedFormat)
{
    gp::Vector<VkSurfaceFormatKHR> formats{};
    formats.pushBack(VkSurfaceFormatKHR{ VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    VulkanSurfaceFormatSelection selection{};

    ASSERT_TRUE(selectVulkanSurfaceFormat(
        formats.view(), PixelFormat::R8G8B8A8_UNorm, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, selection
    ));
    EXPECT_EQ(selection.surfaceFormat.format, VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(selection.pixelFormat, PixelFormat::R8G8B8A8_UNorm);
    EXPECT_FALSE(selection.isFallback);
}

TEST(VulkanSwapChainSelectionTest, SurfaceFormatPrefersExactMatch)
{
    gp::Vector<VkSurfaceFormatKHR> formats{};
    formats.pushBack(VkSurfaceFormatKHR{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    formats.pushBack(VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    VulkanSurfaceFormatSelection selection{};

    ASSERT_TRUE(
        selectVulkanSurfaceFormat(formats.view(), PixelFormat::Unknown, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, selection)
    );
    EXPECT_EQ(selection.surfaceFormat.format, VK_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(selection.pixelFormat, kVulkanDefaultBackBufferFormat);
    EXPECT_FALSE(selection.isFallback);
}

TEST(VulkanSwapChainSelectionTest, SurfaceFormatFallsBackToFirstKnownFormat)
{
    gp::Vector<VkSurfaceFormatKHR> formats{};
    formats.pushBack(VkSurfaceFormatKHR{ VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    formats.pushBack(VkSurfaceFormatKHR{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    VulkanSurfaceFormatSelection selection{};

    ASSERT_TRUE(selectVulkanSurfaceFormat(
        formats.view(), PixelFormat::B8G8R8A8_UNorm, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, selection
    ));
    EXPECT_EQ(selection.surfaceFormat.format, VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(selection.pixelFormat, PixelFormat::R8G8B8A8_UNorm);
    EXPECT_TRUE(selection.isFallback);
}

TEST(VulkanSwapChainSelectionTest, SurfaceFormatFallsBackAcrossColorSpaces)
{
    gp::Vector<VkSurfaceFormatKHR> formats{};
    formats.pushBack(VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    VulkanSurfaceFormatSelection selection{};

    ASSERT_TRUE(selectVulkanSurfaceFormat(
        formats.view(), PixelFormat::B8G8R8A8_UNorm, VK_COLOR_SPACE_HDR10_ST2084_EXT, selection
    ));
    EXPECT_EQ(selection.surfaceFormat.colorSpace, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    EXPECT_EQ(selection.pixelFormat, PixelFormat::B8G8R8A8_UNorm);
}

TEST(VulkanSwapChainSelectionTest, SurfaceFormatFailsWhenNothingIsKnown)
{
    gp::Vector<VkSurfaceFormatKHR> formats{};
    formats.pushBack(VkSurfaceFormatKHR{ VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    VulkanSurfaceFormatSelection selection{};

    EXPECT_FALSE(
        selectVulkanSurfaceFormat(formats.view(), PixelFormat::Unknown, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, selection)
    );
}

TEST(VulkanSwapChainSelectionTest, PresentModeOrderOfPreference)
{
    gp::Vector<VkPresentModeKHR> modes{};
    modes.pushBack(VK_PRESENT_MODE_FIFO_KHR);
    modes.pushBack(VK_PRESENT_MODE_MAILBOX_KHR);
    modes.pushBack(VK_PRESENT_MODE_IMMEDIATE_KHR);

    EXPECT_EQ(selectVulkanPresentMode(modes.view(), false, -1), VK_PRESENT_MODE_IMMEDIATE_KHR);
    EXPECT_EQ(selectVulkanPresentMode(modes.view(), true, -1), VK_PRESENT_MODE_MAILBOX_KHR);

    gp::Vector<VkPresentModeKHR> fifoOnly{};
    fifoOnly.pushBack(VK_PRESENT_MODE_FIFO_KHR);
    EXPECT_EQ(selectVulkanPresentMode(fifoOnly.view(), false, -1), VK_PRESENT_MODE_FIFO_KHR);

    gp::Vector<VkPresentModeKHR> relaxedOnly{};
    relaxedOnly.pushBack(VK_PRESENT_MODE_FIFO_RELAXED_KHR);
    EXPECT_EQ(selectVulkanPresentMode(relaxedOnly.view(), true, -1), VK_PRESENT_MODE_FIFO_RELAXED_KHR);

    const gp::Vector<VkPresentModeKHR> none{};
    EXPECT_EQ(selectVulkanPresentMode(none.view(), false, -1), VK_PRESENT_MODE_FIFO_KHR);
}

TEST(VulkanSwapChainSelectionTest, PresentModeOverrideAppliesOnlyWhenSupported)
{
    gp::Vector<VkPresentModeKHR> modes{};
    modes.pushBack(VK_PRESENT_MODE_FIFO_KHR);
    modes.pushBack(VK_PRESENT_MODE_MAILBOX_KHR);

    EXPECT_EQ(
        selectVulkanPresentMode(modes.view(), true, static_cast<gp::Int32>(VK_PRESENT_MODE_FIFO_KHR)),
        VK_PRESENT_MODE_FIFO_KHR
    );
    EXPECT_EQ(
        selectVulkanPresentMode(modes.view(), true, static_cast<gp::Int32>(VK_PRESENT_MODE_IMMEDIATE_KHR)),
        VK_PRESENT_MODE_MAILBOX_KHR
    );
}

TEST(VulkanSwapChainSelectionTest, ImageCountIsClampedToSurfaceLimits)
{
    VkSurfaceCapabilitiesKHR capabilities = makeFreeExtentCapabilities();

    EXPECT_EQ(selectVulkanSwapChainImageCount(capabilities, 3u), 3u);
    EXPECT_EQ(selectVulkanSwapChainImageCount(capabilities, 1u), 2u);
    EXPECT_EQ(selectVulkanSwapChainImageCount(capabilities, 16u), 8u);

    capabilities.maxImageCount = 0u;
    EXPECT_EQ(selectVulkanSwapChainImageCount(capabilities, 16u), 16u);
}

TEST(VulkanSwapChainSelectionTest, TransformHonorsPreTransformSetting)
{
    VkSurfaceCapabilitiesKHR capabilities = makeFreeExtentCapabilities();

    EXPECT_EQ(selectVulkanSurfaceTransform(capabilities, true), VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR);
    EXPECT_EQ(selectVulkanSurfaceTransform(capabilities, false), VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);

    capabilities.supportedTransforms = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    EXPECT_EQ(selectVulkanSurfaceTransform(capabilities, false), VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR);
}

TEST(VulkanSwapChainSelectionTest, QuarterTurnTransformsAreDetected)
{
    EXPECT_FALSE(isVulkanQuarterTurnTransform(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR));
    EXPECT_FALSE(isVulkanQuarterTurnTransform(VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR));
    EXPECT_TRUE(isVulkanQuarterTurnTransform(VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR));
    EXPECT_TRUE(isVulkanQuarterTurnTransform(VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR));
    EXPECT_TRUE(isVulkanQuarterTurnTransform(VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR));
}

TEST(VulkanSwapChainSelectionTest, ExtentUsesImposedSurfaceExtent)
{
    VkSurfaceCapabilitiesKHR capabilities = makeFreeExtentCapabilities();
    capabilities.currentExtent = VkExtent2D{ 800u, 600u };

    const VkExtent2D extent =
        selectVulkanSwapChainExtent(capabilities, 1920u, 1080u, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);

    EXPECT_EQ(extent.width, 800u);
    EXPECT_EQ(extent.height, 600u);
}

TEST(VulkanSwapChainSelectionTest, FreeExtentIsClampedAndSwappedForQuarterTurns)
{
    const VkSurfaceCapabilitiesKHR capabilities = makeFreeExtentCapabilities();

    const VkExtent2D identity =
        selectVulkanSwapChainExtent(capabilities, 1920u, 1080u, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
    EXPECT_EQ(identity.width, 1920u);
    EXPECT_EQ(identity.height, 1080u);

    const VkExtent2D rotated =
        selectVulkanSwapChainExtent(capabilities, 1920u, 1080u, VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR);
    EXPECT_EQ(rotated.width, 1080u);
    EXPECT_EQ(rotated.height, 1920u);

    const VkExtent2D clamped =
        selectVulkanSwapChainExtent(capabilities, 10000u, 0u, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
    EXPECT_EQ(clamped.width, 4096u);
    EXPECT_EQ(clamped.height, 1u);
}

TEST(VulkanSwapChainSelectionTest, CompositeAlphaPreference)
{
    EXPECT_EQ(
        selectVulkanCompositeAlpha(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR),
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
    );
    EXPECT_EQ(
        selectVulkanCompositeAlpha(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR | VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR),
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    );
    EXPECT_EQ(
        selectVulkanCompositeAlpha(
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR | VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
        ),
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
    );
    EXPECT_EQ(selectVulkanCompositeAlpha(0u), VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
}

TEST(VulkanSwapChainSelectionTest, AcquireResultTranslation)
{
    EXPECT_EQ(translateVulkanAcquireResult(VK_SUCCESS), VulkanSwapChainStatus::Healthy);
    EXPECT_EQ(translateVulkanAcquireResult(VK_SUBOPTIMAL_KHR), VulkanSwapChainStatus::Healthy);
    EXPECT_EQ(translateVulkanAcquireResult(VK_ERROR_OUT_OF_DATE_KHR), VulkanSwapChainStatus::OutOfDate);
    EXPECT_EQ(
        translateVulkanAcquireResult(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT), VulkanSwapChainStatus::OutOfDate
    );
    EXPECT_EQ(translateVulkanAcquireResult(VK_ERROR_SURFACE_LOST_KHR), VulkanSwapChainStatus::SurfaceLost);
    EXPECT_EQ(translateVulkanAcquireResult(VK_NOT_READY), VulkanSwapChainStatus::NotReady);
    EXPECT_EQ(translateVulkanAcquireResult(VK_TIMEOUT), VulkanSwapChainStatus::NotReady);
    EXPECT_EQ(translateVulkanAcquireResult(VK_ERROR_DEVICE_LOST), VulkanSwapChainStatus::Failed);
}

TEST(VulkanSwapChainSelectionTest, PresentResultTranslation)
{
    EXPECT_EQ(translateVulkanPresentResult(VK_SUCCESS), VulkanSwapChainStatus::Healthy);
    EXPECT_EQ(translateVulkanPresentResult(VK_SUBOPTIMAL_KHR), VulkanSwapChainStatus::Healthy);
    EXPECT_EQ(translateVulkanPresentResult(VK_ERROR_OUT_OF_DATE_KHR), VulkanSwapChainStatus::OutOfDate);
    EXPECT_EQ(translateVulkanPresentResult(VK_ERROR_SURFACE_LOST_KHR), VulkanSwapChainStatus::SurfaceLost);
    EXPECT_EQ(translateVulkanPresentResult(VK_NOT_READY), VulkanSwapChainStatus::Failed);
    EXPECT_EQ(translateVulkanPresentResult(VK_ERROR_DEVICE_LOST), VulkanSwapChainStatus::Failed);
}

TEST(VulkanSwapChainSelectionTest, FramePacerSleepsWhenEarly)
{
    gp::UInt64 nextTarget = 0u;
    const gp::UInt64 sleepNs = computeVulkanFramePacerSleep(1000u, 4000u, 16000u, nextTarget);

    EXPECT_EQ(sleepNs, 3000u);
    EXPECT_EQ(nextTarget, 20000u);
}

TEST(VulkanSwapChainSelectionTest, FramePacerRestartsCadenceWhenLate)
{
    gp::UInt64 nextTarget = 0u;
    const gp::UInt64 sleepNs = computeVulkanFramePacerSleep(50000u, 4000u, 16000u, nextTarget);

    EXPECT_EQ(sleepNs, 0u);
    EXPECT_EQ(nextTarget, 66000u);

    // The first present has no target yet: no sleep, cadence starts now.
    const gp::UInt64 firstSleepNs = computeVulkanFramePacerSleep(7u, 0u, 10u, nextTarget);
    EXPECT_EQ(firstSleepNs, 0u);
    EXPECT_EQ(nextTarget, 17u);
}

TEST(VulkanSwapChainSelectionTest, PresentQueueSelection)
{
    // Queues are only compared by address here.
    gp::UInt8 graphicsStorage = 0u;
    gp::UInt8 computeStorage = 0u;
    VulkanQueue* const graphics = reinterpret_cast<VulkanQueue*>(&graphicsStorage);
    VulkanQueue* const compute = reinterpret_cast<VulkanQueue*>(&computeStorage);

    EXPECT_EQ(selectVulkanPresentQueue(graphics, true, compute, true, false), graphics);
    EXPECT_EQ(selectVulkanPresentQueue(graphics, true, compute, true, true), compute);
    EXPECT_EQ(selectVulkanPresentQueue(graphics, true, compute, false, true), graphics);
    EXPECT_EQ(selectVulkanPresentQueue(graphics, true, graphics, true, true), graphics);
    EXPECT_EQ(selectVulkanPresentQueue(graphics, true, nullptr, false, true), graphics);
    EXPECT_EQ(selectVulkanPresentQueue(graphics, false, compute, false, true), nullptr);
}

}   // namespace gp::rhi::vulkan::tests
