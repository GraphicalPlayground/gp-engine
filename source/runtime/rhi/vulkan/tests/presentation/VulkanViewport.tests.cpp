// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanViewport.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"       // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceTypes.hpp"
#include "maths/color/PixelFormatEnums.hpp"   // IWYU pragma: keep
#include "platform/VulkanPlatform.hpp"
#include "presentation/PresentArgs.hpp"
#include "presentation/PresentationForward.hpp"
#include "presentation/VulkanPresentationDesc.hpp"
#include "presentation/VulkanPresentationForward.hpp"
#include "presentation/VulkanPresentationTypes.hpp"
#include "presentation/VulkanSwapChain.hpp"
#include "presentation/VulkanSwapChainSelection.hpp"
#include "presentation/VulkanViewportRHI.hpp"
#include "RHIForward.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include "window/NativeWindow.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Requested viewport size.
constexpr gp::UInt32 kViewportSize = 128u;

/// @brief Live fixture exercising presentation objects without a window.
class VulkanViewportLiveTest : public VulkanResourceLiveTest
{
protected:
    /// @brief Builds viewport services around the fixture's resource services.
    /// @return Services without queues (no presentation is possible).
    [[nodiscard]] VulkanViewportServices makeServices() const noexcept
    {
        VulkanViewportServices services{};
        services.resourceServices = m_services;
        return services;
    }
};

}   // namespace

TEST(VulkanViewportServicesTest, ValidityRequiresGraphicsQueue)
{
    const VulkanViewportServices services{};
    EXPECT_FALSE(services.isValid());
}

TEST(VulkanViewportEntryPointTest, SupportedResolutionsAreUnavailable)
{
    gp::Vector<ScreenResolution> resolutions{};
    resolutions.pushBack(ScreenResolution{});

    EXPECT_FALSE(getVulkanSupportedResolutions(resolutions, true));
    EXPECT_TRUE(resolutions.isEmpty());

    gp::UInt32 width = 1280u;
    gp::UInt32 height = 720u;
    getVulkanSupportedResolution(width, height);
    EXPECT_EQ(width, 1280u);
    EXPECT_EQ(height, 720u);
}

TEST(VulkanViewportEntryPointTest, NullViewportCallsAreIgnored)
{
    advanceVulkanViewportFrame(nullptr);
    SUCCEED();
}

TEST_F(VulkanViewportLiveTest, PlatformSurfaceCreationFailsWithoutWindow)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const VkResult result =
        VulkanPlatform::createSurface(VulkanPlatformWindowContext{}, m_instance.getHandle(), &surface);

    EXPECT_NE(result, VK_SUCCESS);
    EXPECT_EQ(surface, static_cast<VkSurfaceKHR>(VK_NULL_HANDLE));
}

TEST_F(VulkanViewportLiveTest, SwapChainInitializeFailsWithoutWindow)
{
    VulkanSwapChainDesc desc{};
    desc.device = m_device;
    desc.width = kViewportSize;
    desc.height = kViewportSize;
    VulkanSwapChain swapChain(desc);

    EXPECT_FALSE(swapChain.initialize(VulkanPlatformWindowContext{}, nullptr));
    EXPECT_EQ(swapChain.getHandle(), static_cast<VkSwapchainKHR>(VK_NULL_HANDLE));
    EXPECT_EQ(swapChain.getSurface(), static_cast<VkSurfaceKHR>(VK_NULL_HANDLE));
    EXPECT_EQ(swapChain.getNumImages(), 0u);
    EXPECT_EQ(swapChain.getCurrentImageIndex(), -1);

    gp::UInt32 imageIndex = 0u;
    VulkanSemaphoreRef semaphore{};
    EXPECT_EQ(swapChain.acquireImageIndex(imageIndex, semaphore), VulkanSwapChainStatus::OutOfDate);
    EXPECT_FALSE(semaphore);
    EXPECT_EQ(swapChain.getNumAcquireCalls(), 0u);

    swapChain.destroy(nullptr);
    EXPECT_EQ(swapChain.getHandle(), static_cast<VkSwapchainKHR>(VK_NULL_HANDLE));
}

TEST_F(VulkanViewportLiveTest, SwapChainDestroyWithRecreateInfoHandsOverNothingWhenEmpty)
{
    VulkanSwapChainDesc desc{};
    desc.device = m_device;
    VulkanSwapChain swapChain(desc);

    VulkanSwapChainRecreateInfo recreateInfo{};
    swapChain.destroy(&recreateInfo);

    EXPECT_EQ(recreateInfo.swapChain, static_cast<VkSwapchainKHR>(VK_NULL_HANDLE));
    EXPECT_EQ(recreateInfo.surface, static_cast<VkSurfaceKHR>(VK_NULL_HANDLE));
}

TEST_F(VulkanViewportLiveTest, ConstructionStoresRequestedParameters)
{
    const VulkanViewportRef viewport(new VulkanViewport(
        makeServices(), VulkanPlatformWindowContext{}, kViewportSize, kViewportSize * 2u, true, PixelFormat::Unknown
    ));

    EXPECT_EQ(viewport->getSizeX(), kViewportSize);
    EXPECT_EQ(viewport->getSizeY(), kViewportSize * 2u);
    EXPECT_TRUE(viewport->isFullscreen());
    EXPECT_EQ(viewport->getPixelFormat(), kVulkanDefaultBackBufferFormat);
    EXPECT_EQ(viewport->getBackBuffer(), nullptr);
    EXPECT_EQ(viewport->getSwapChain(), nullptr);
    EXPECT_EQ(viewport->getNativeSwapChain(), nullptr);
    EXPECT_EQ(viewport->getNativeBackBufferTexture(), nullptr);
    EXPECT_EQ(viewport->getNativeBackBufferRT(), nullptr);
    EXPECT_EQ(viewport->getAcquiredImageIndex(), -1);
    EXPECT_EQ(viewport->getPresentCount(), 0u);
    EXPECT_EQ(viewport->getRecreateCount(), 0u);
}

TEST_F(VulkanViewportLiveTest, NativeWindowReportsHandleAndClearsAuxiliaryParameter)
{
    gp::hal::NativeWindowHandle nativeHandle{};
    nativeHandle.backend = gp::hal::NativeWindowBackend::Win32;
    gp::UInt8 fakeWindow = 0u;
    nativeHandle.win32.hwnd = &fakeWindow;

    const VulkanViewportRef viewport(new VulkanViewport(
        makeServices(),
        VulkanPlatformWindowContext(nativeHandle),
        kViewportSize,
        kViewportSize,
        false,
        PixelFormat::B8G8R8A8_UNorm
    ));

    gp::UInt8 sentinel = 0u;
    void* addParam = &sentinel;
    EXPECT_EQ(viewport->getNativeWindow(&addParam), static_cast<void*>(&fakeWindow));
    EXPECT_EQ(addParam, nullptr);
    EXPECT_EQ(viewport->getNativeWindow(), static_cast<void*>(&fakeWindow));
}

TEST_F(VulkanViewportLiveTest, InitializeFailsWithoutWindow)
{
    // `initialize()` requires a graphics queue; the swapchain fails (no surface) before the queue is ever used, so an
    // address standing for it is enough.
    gp::UInt8 queueStorage = 0u;
    VulkanViewportServices services = makeServices();
    services.graphicsQueue = reinterpret_cast<VulkanQueue*>(&queueStorage);
    ASSERT_TRUE(services.isValid());

    const VulkanViewportRef viewport(new VulkanViewport(
        services, VulkanPlatformWindowContext{}, kViewportSize, kViewportSize, false, PixelFormat::Unknown
    ));

    EXPECT_FALSE(viewport->initialize(cmdList()));
    EXPECT_EQ(viewport->getBackBuffer(), nullptr);
    EXPECT_EQ(viewport->getSwapChain(), nullptr);
}

TEST_F(VulkanViewportLiveTest, CustomPresentDefaultsToNone)
{
    const VulkanViewportRef viewport(new VulkanViewport(
        makeServices(), VulkanPlatformWindowContext{}, kViewportSize, kViewportSize, false, PixelFormat::Unknown
    ));

    EXPECT_EQ(viewport->getCustomPresent(), nullptr);
    viewport->setCustomPresent(nullptr);
    EXPECT_EQ(viewport->getCustomPresent(), nullptr);
}

TEST_F(VulkanViewportLiveTest, FrameEventWaitWithoutPresentReturns)
{
    const VulkanViewportRef viewport(new VulkanViewport(
        makeServices(), VulkanPlatformWindowContext{}, kViewportSize, kViewportSize, false, PixelFormat::Unknown
    ));

    viewport->issueFrameEvent();
    viewport->waitForFrameEventCompletion();
    viewport->advanceBackBufferFrame();
    SUCCEED();
}

TEST_F(VulkanViewportLiveTest, CreateViewportRejectsMissingOrUnknownWindow)
{
    const VulkanViewportServices services = makeServices();

    EXPECT_FALSE(
        createVulkanViewport(cmdList(), services, nullptr, kViewportSize, kViewportSize, false, PixelFormat::Unknown)
    );

    const gp::hal::NativeWindowHandle unknownHandle{};
    EXPECT_FALSE(createVulkanViewport(
        cmdList(), services, &unknownHandle, kViewportSize, kViewportSize, false, PixelFormat::Unknown
    ));
}

TEST_F(VulkanViewportLiveTest, CreateViewportRejectsInvalidServices)
{
    gp::hal::NativeWindowHandle nativeHandle{};
    nativeHandle.backend = gp::hal::NativeWindowBackend::Win32;
    gp::UInt8 fakeWindow = 0u;
    nativeHandle.win32.hwnd = &fakeWindow;

    // No graphics queue: the services cannot present.
    EXPECT_FALSE(createVulkanViewport(
        cmdList(), makeServices(), &nativeHandle, kViewportSize, kViewportSize, false, PixelFormat::Unknown
    ));
}

TEST_F(VulkanViewportLiveTest, EntryPointsIgnoreNullViewport)
{
    resizeVulkanViewport(cmdList(), nullptr, kViewportSize, kViewportSize, false);
    beginDrawingVulkanViewport(cmdList(), nullptr, nullptr);
    endDrawingVulkanViewport(cmdList(), nullptr, presentation::PresentArgsWithStats{});

    EXPECT_FALSE(getVulkanViewportBackBuffer(cmdList(), nullptr));
}

TEST_F(VulkanViewportLiveTest, BackBufferEntryPointNeedsInitializedViewport)
{
    const VulkanViewportRef viewport(new VulkanViewport(
        makeServices(), VulkanPlatformWindowContext{}, kViewportSize, kViewportSize, false, PixelFormat::Unknown
    ));

    EXPECT_FALSE(getVulkanViewportBackBuffer(cmdList(), viewport.get()));
}

}   // namespace gp::rhi::vulkan::tests
