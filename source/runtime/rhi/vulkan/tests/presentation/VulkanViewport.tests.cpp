// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanViewport.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"       // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceTypes.hpp"
#include "loader/VulkanEntryPoints.hpp"
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

namespace
{

struct MockSwapChainApiGuard final
{
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR m_savedGetCaps{ nullptr };
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR m_savedGetFormats{ nullptr };
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR m_savedGetModes{ nullptr };
    PFN_vkCreateSwapchainKHR m_savedCreateSwapchain{ nullptr };
    PFN_vkDestroySwapchainKHR m_savedDestroySwapchain{ nullptr };
    PFN_vkGetSwapchainImagesKHR m_savedGetImages{ nullptr };
    PFN_vkDestroySurfaceKHR m_savedDestroySurface{ nullptr };

    static inline VkExtent2D s_surfaceExtent{ 128u, 128u };
    static inline VkSwapchainKHR s_lastOldSwapchain{ VK_NULL_HANDLE };
    static inline gp::Vector<VkSwapchainKHR> s_destroyedSwapchains{};
    static inline gp::Vector<VkSurfaceKHR> s_destroyedSurfaces{};

    static VkResult VKAPI_PTR
        mockGetCaps(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR* const outCaps) noexcept
    {
        *outCaps = VkSurfaceCapabilitiesKHR{};
        outCaps->minImageCount = 2u;
        outCaps->maxImageCount = 4u;
        outCaps->currentExtent = s_surfaceExtent;
        outCaps->minImageExtent = s_surfaceExtent;
        outCaps->maxImageExtent = s_surfaceExtent;
        outCaps->maxImageArrayLayers = 1u;
        outCaps->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        outCaps->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        outCaps->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        outCaps->supportedUsageFlags =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        return VK_SUCCESS;
    }

    static VkResult VKAPI_PTR mockGetFormats(
        VkPhysicalDevice, VkSurfaceKHR, gp::UInt32* const ioCount, VkSurfaceFormatKHR* const outFormats
    ) noexcept
    {
        if (outFormats == nullptr)
        {
            *ioCount = 1u;
            return VK_SUCCESS;
        }
        *ioCount = 1u;
        outFormats[0].format = VK_FORMAT_B8G8R8A8_UNORM;
        outFormats[0].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return VK_SUCCESS;
    }

    static VkResult VKAPI_PTR mockGetModes(
        VkPhysicalDevice, VkSurfaceKHR, gp::UInt32* const ioCount, VkPresentModeKHR* const outModes
    ) noexcept
    {
        if (outModes == nullptr)
        {
            *ioCount = 1u;
            return VK_SUCCESS;
        }
        *ioCount = 1u;
        outModes[0] = VK_PRESENT_MODE_FIFO_KHR;
        return VK_SUCCESS;
    }

    static VkResult VKAPI_PTR mockCreateSwapchain(
        VkDevice,
        const VkSwapchainCreateInfoKHR* const createInfo,
        const VkAllocationCallbacks*,
        VkSwapchainKHR* const outSwapchain
    ) noexcept
    {
        s_lastOldSwapchain = createInfo->oldSwapchain;
        *outSwapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<gp::UIntPtr>(0xBEEF0002u));
        return VK_SUCCESS;
    }

    static void VKAPI_PTR
        mockDestroySwapchain(VkDevice, const VkSwapchainKHR swapchain, const VkAllocationCallbacks*) noexcept
    {
        s_destroyedSwapchains.pushBack(swapchain);
    }

    static VkResult VKAPI_PTR
        mockGetImages(VkDevice, VkSwapchainKHR, gp::UInt32* const ioCount, VkImage* const outImages) noexcept
    {
        if (outImages == nullptr)
        {
            *ioCount = 2u;
            return VK_SUCCESS;
        }
        *ioCount = 2u;
        outImages[0] = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x101u));
        outImages[1] = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x102u));
        return VK_SUCCESS;
    }

    static void VKAPI_PTR
        mockDestroySurface(VkInstance, const VkSurfaceKHR surface, const VkAllocationCallbacks*) noexcept
    {
        s_destroyedSurfaces.pushBack(surface);
    }

    MockSwapChainApiGuard() noexcept
        : m_savedGetCaps(dynamic_api::vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
        , m_savedGetFormats(dynamic_api::vkGetPhysicalDeviceSurfaceFormatsKHR)
        , m_savedGetModes(dynamic_api::vkGetPhysicalDeviceSurfacePresentModesKHR)
        , m_savedCreateSwapchain(dynamic_api::vkCreateSwapchainKHR)
        , m_savedDestroySwapchain(dynamic_api::vkDestroySwapchainKHR)
        , m_savedGetImages(dynamic_api::vkGetSwapchainImagesKHR)
        , m_savedDestroySurface(dynamic_api::vkDestroySurfaceKHR)
    {
        s_surfaceExtent = VkExtent2D{ 128u, 128u };
        s_lastOldSwapchain = VK_NULL_HANDLE;
        s_destroyedSwapchains.clear();
        s_destroyedSurfaces.clear();

        dynamic_api::vkGetPhysicalDeviceSurfaceCapabilitiesKHR = &mockGetCaps;
        dynamic_api::vkGetPhysicalDeviceSurfaceFormatsKHR = &mockGetFormats;
        dynamic_api::vkGetPhysicalDeviceSurfacePresentModesKHR = &mockGetModes;
        dynamic_api::vkCreateSwapchainKHR = &mockCreateSwapchain;
        dynamic_api::vkDestroySwapchainKHR = &mockDestroySwapchain;
        dynamic_api::vkGetSwapchainImagesKHR = &mockGetImages;
        dynamic_api::vkDestroySurfaceKHR = &mockDestroySurface;
    }

    ~MockSwapChainApiGuard() noexcept
    {
        dynamic_api::vkGetPhysicalDeviceSurfaceCapabilitiesKHR = m_savedGetCaps;
        dynamic_api::vkGetPhysicalDeviceSurfaceFormatsKHR = m_savedGetFormats;
        dynamic_api::vkGetPhysicalDeviceSurfacePresentModesKHR = m_savedGetModes;
        dynamic_api::vkCreateSwapchainKHR = m_savedCreateSwapchain;
        dynamic_api::vkDestroySwapchainKHR = m_savedDestroySwapchain;
        dynamic_api::vkGetSwapchainImagesKHR = m_savedGetImages;
        dynamic_api::vkDestroySurfaceKHR = m_savedDestroySurface;
        s_destroyedSwapchains.clear();
        s_destroyedSurfaces.clear();
    }
};

}   // namespace

TEST_F(VulkanViewportLiveTest, SwapChainInitializeDestroysRetiredOldSwapChainOnSuccessAndFailure)
{
    const MockSwapChainApiGuard apiGuard;

    const VkSwapchainKHR retiredSwapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<gp::UIntPtr>(0xBEEF0001u));
    const VkSurfaceKHR reusedSurface = reinterpret_cast<VkSurfaceKHR>(static_cast<gp::UIntPtr>(0xCAFE0001u));

    VulkanSwapChainDesc desc{};
    desc.device = m_device;
    desc.width = kViewportSize;
    desc.height = kViewportSize;
    desc.pixelFormat = PixelFormat::B8G8R8A8_UNorm;

    // 1. Successful recreation passes oldSwapchain to vkCreateSwapchainKHR AND immediately destroys oldSwapchain.
    {
        VulkanSwapChainRecreateInfo recreateInfo{};
        recreateInfo.swapChain = retiredSwapchain;
        recreateInfo.surface = reusedSurface;

        VulkanSwapChain swapChain(desc);
        ASSERT_TRUE(swapChain.initialize(VulkanPlatformWindowContext{}, &recreateInfo));
        EXPECT_EQ(recreateInfo.swapChain, static_cast<VkSwapchainKHR>(VK_NULL_HANDLE));
        EXPECT_EQ(recreateInfo.surface, static_cast<VkSurfaceKHR>(VK_NULL_HANDLE));
        EXPECT_EQ(MockSwapChainApiGuard::s_lastOldSwapchain, retiredSwapchain);
        ASSERT_EQ(MockSwapChainApiGuard::s_destroyedSwapchains.size(), 1u);
        EXPECT_EQ(MockSwapChainApiGuard::s_destroyedSwapchains[0], retiredSwapchain);
        EXPECT_TRUE(MockSwapChainApiGuard::s_destroyedSurfaces.isEmpty());

        swapChain.destroy(nullptr);
        ASSERT_EQ(MockSwapChainApiGuard::s_destroyedSwapchains.size(), 2u);
        ASSERT_EQ(MockSwapChainApiGuard::s_destroyedSurfaces.size(), 1u);
        EXPECT_EQ(MockSwapChainApiGuard::s_destroyedSurfaces[0], reusedSurface);
    }

    // 2. Failed recreation (minimized zero-extent surface) still destroys both retired swapchain and surface.
    MockSwapChainApiGuard::s_destroyedSwapchains.clear();
    MockSwapChainApiGuard::s_destroyedSurfaces.clear();
    MockSwapChainApiGuard::s_surfaceExtent = VkExtent2D{ 0u, 0u };
    {
        VulkanSwapChainRecreateInfo recreateInfo{};
        recreateInfo.swapChain = retiredSwapchain;
        recreateInfo.surface = reusedSurface;

        VulkanSwapChain swapChain(desc);
        EXPECT_FALSE(swapChain.initialize(VulkanPlatformWindowContext{}, &recreateInfo));
        EXPECT_EQ(recreateInfo.swapChain, static_cast<VkSwapchainKHR>(VK_NULL_HANDLE));
        EXPECT_EQ(recreateInfo.surface, static_cast<VkSurfaceKHR>(VK_NULL_HANDLE));
        ASSERT_EQ(MockSwapChainApiGuard::s_destroyedSwapchains.size(), 1u);
        EXPECT_EQ(MockSwapChainApiGuard::s_destroyedSwapchains[0], retiredSwapchain);
        ASSERT_EQ(MockSwapChainApiGuard::s_destroyedSurfaces.size(), 1u);
        EXPECT_EQ(MockSwapChainApiGuard::s_destroyedSurfaces[0], reusedSurface);
    }
}

}   // namespace gp::rhi::vulkan::tests
