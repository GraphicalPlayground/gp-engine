// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "loader/VulkanLoader.hpp"
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platform/VulkanGenericPlatform.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

struct ScopedLoaderReset final
{
    ScopedLoaderReset() noexcept
    {
        VulkanLoader::freeLibrary();
    }

    ~ScopedLoaderReset() noexcept
    {
        VulkanLoader::freeLibrary();
    }
};

struct ScopedCommandLine final
{
    bool m_wasInitialized{ false };
    gp::String m_savedExecutable{};
    gp::String m_savedArgs{};

    explicit ScopedCommandLine(const gp::StringView commandLine) noexcept
        : m_wasInitialized(gp::CommandLine::isInitialized())
    {
        if (m_wasInitialized)
        {
            m_savedExecutable = gp::String(gp::CommandLine::getExecutablePath());
            m_savedArgs = gp::String(gp::CommandLine::getOriginalCommandLine());
        }
        gp::CommandLine::initialize(commandLine);
    }

    ~ScopedCommandLine() noexcept
    {
        if (m_wasInitialized)
        {
            gp::String combined = m_savedExecutable;
            if (!m_savedArgs.isEmpty())
            {
                combined.pushBack(' ');
                combined.append(m_savedArgs.view());
            }
            gp::CommandLine::initialize(combined.view());
        }
        else
        {
            gp::CommandLine::shutdown();
        }
    }
};

bool stringsEqual(const char* const a, const char* const b) noexcept
{
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }
    const gp::USize lenA = gp::container::CharTraits<char>::length(a);
    const gp::USize lenB = gp::container::CharTraits<char>::length(b);
    return lenA == lenB && gp::container::CharTraits<char>::compare(a, b, lenA) == 0;
}

bool isPlatformInstanceProc(const char* const name) noexcept
{
    bool matched = false;
#define GP_CHECK_PLATFORM_PROC(Type, Func) \
    if (stringsEqual(name, #Func))         \
    {                                      \
        matched = true;                    \
    }
    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GP_CHECK_PLATFORM_PROC)
#undef GP_CHECK_PLATFORM_PROC
    return matched;
}

void VKAPI_PTR mockInstanceStub() noexcept
{}

void VKAPI_PTR mockDeviceStub() noexcept
{}

const char* g_missingProcName = nullptr;
bool g_omitSurfaceProcs = false;
bool g_omitPlatformInstanceProcs = false;
bool g_omitRayTracingProcs = false;

PFN_vkVoidFunction VKAPI_PTR mockGetInstanceProcAddr(const VkInstance instance, const char* const name) noexcept
{
    if (instance == VK_NULL_HANDLE || name == nullptr)
    {
        return nullptr;
    }
    if (g_missingProcName != nullptr && stringsEqual(name, g_missingProcName))
    {
        return nullptr;
    }
    if (g_omitSurfaceProcs &&
        (stringsEqual(name, "vkDestroySurfaceKHR") || stringsEqual(name, "vkGetPhysicalDeviceSurfaceSupportKHR")))
    {
        return nullptr;
    }
    if (g_omitPlatformInstanceProcs && isPlatformInstanceProc(name))
    {
        return nullptr;
    }
    if (g_omitRayTracingProcs && stringsEqual(name, "vkCmdTraceRaysKHR"))
    {
        return nullptr;
    }
    return &mockInstanceStub;
}

PFN_vkVoidFunction VKAPI_PTR mockGetDeviceProcAddr(const VkDevice device, const char* const name) noexcept
{
    if (device == VK_NULL_HANDLE || name == nullptr)
    {
        return nullptr;
    }
    // Simulate Vulkan spec behavior: instance/physical-device functions return nullptr from vkGetDeviceProcAddr,
    // while device/queue/command-buffer functions return direct ICD pointers.
    if (stringsEqual(name, "vkCmdDraw") || stringsEqual(name, "vkCreateBuffer") ||
        stringsEqual(name, "vkQueueSubmit") || stringsEqual(name, "vkCmdBeginRenderingKHR"))
    {
        return &mockDeviceStub;
    }
    return nullptr;
}

gp::UInt32 g_presentCalls = 0u;
gp::UInt32 g_createSwapchainCalls = 0u;
gp::UInt32 g_destroySwapchainCalls = 0u;
gp::UInt32 g_destroySurfaceCalls = 0u;

VkResult VKAPI_PTR mockQueuePresentKHR(VkQueue, const VkPresentInfoKHR*) noexcept
{
    ++g_presentCalls;
    return VK_SUBOPTIMAL_KHR;
}

VkResult VKAPI_PTR mockCreateSwapchainKHR(
    VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR* const outSwapchain
) noexcept
{
    ++g_createSwapchainCalls;
    if (outSwapchain != nullptr)
    {
        *outSwapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<gp::UIntPtr>(0x1234u));
    }
    return VK_SUCCESS;
}

void VKAPI_PTR mockDestroySwapchainKHR(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks*) noexcept
{
    ++g_destroySwapchainCalls;
}

void VKAPI_PTR mockDestroySurfaceKHR(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*) noexcept
{
    ++g_destroySurfaceCalls;
}

}   // namespace

TEST(VulkanLoaderTest, EntryPointMacroTablesAndCommandWrapperAliases)
{
    gp::UInt32 baseCount = 0u;
    gp::UInt32 optionalBaseCount = 0u;
    gp::UInt32 instanceCount = 0u;
    gp::UInt32 surfaceCount = 0u;
    gp::UInt32 rayTracingCount = 0u;
    gp::UInt32 optionalInstanceCount = 0u;

#define GP_COUNT_ENTRY(Type, Func) ++counter;
    {
        gp::UInt32 counter = 0u;
        GP_ENUM_VK_ENTRYPOINTS_BASE(GP_COUNT_ENTRY)
        baseCount = counter;
    }
    {
        gp::UInt32 counter = 0u;
        GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_BASE(GP_COUNT_ENTRY)
        optionalBaseCount = counter;
    }
    {
        gp::UInt32 counter = 0u;
        GP_ENUM_VK_ENTRYPOINTS_INSTANCE(GP_COUNT_ENTRY)
        instanceCount = counter;
    }
    {
        gp::UInt32 counter = 0u;
        GP_ENUM_VK_ENTRYPOINTS_SURFACE_INSTANCE(GP_COUNT_ENTRY)
        surfaceCount = counter;
    }
    {
        gp::UInt32 counter = 0u;
        GP_ENUM_VK_ENTRYPOINTS_RAYTRACING(GP_COUNT_ENTRY)
        rayTracingCount = counter;
    }
    {
        gp::UInt32 counter = 0u;
        GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(GP_COUNT_ENTRY)
        optionalInstanceCount = counter;
    }
#undef GP_COUNT_ENTRY

    EXPECT_EQ(baseCount, 5u);
    EXPECT_EQ(optionalBaseCount, 7u);
    EXPECT_EQ(instanceCount, 151u);
    EXPECT_EQ(surfaceCount, 5u);
    EXPECT_EQ(rayTracingCount, 12u);
    EXPECT_EQ(optionalInstanceCount, 120u);

    // Command wrapper aliases in gp::rhi::vulkan must bind to the exact dynamic_api function pointer variables.
    EXPECT_EQ(&gp::rhi::vulkan::vkCreateInstance, &gp::rhi::vulkan::dynamic_api::vkCreateInstance);
    EXPECT_EQ(&gp::rhi::vulkan::vkCmdDraw, &gp::rhi::vulkan::dynamic_api::vkCmdDraw);
    EXPECT_EQ(&gp::rhi::vulkan::vkQueuePresentKHR, &gp::rhi::vulkan::dynamic_api::vkQueuePresentKHR);
    EXPECT_EQ(&gp::rhi::vulkan::vkCmdTraceRaysKHR, &gp::rhi::vulkan::dynamic_api::vkCmdTraceRaysKHR);
    EXPECT_EQ(
        &gp::rhi::vulkan::vkGetSemaphoreCounterValueKHR, &gp::rhi::vulkan::dynamic_api::vkGetSemaphoreCounterValueKHR
    );
    EXPECT_EQ(&gp::rhi::vulkan::vkWaitSemaphoresKHR, &gp::rhi::vulkan::dynamic_api::vkWaitSemaphoresKHR);
    EXPECT_EQ(&gp::rhi::vulkan::vkSignalSemaphoreKHR, &gp::rhi::vulkan::dynamic_api::vkSignalSemaphoreKHR);
}

TEST(VulkanLoaderTest, LoadInvalidAndNonVulkanLibrariesAndDefaultProbe)
{
    const ScopedLoaderReset guard;

    EXPECT_FALSE(VulkanLoader::isLibraryLoaded());
    EXPECT_EQ(VulkanLoader::getLibraryHandle(), nullptr);

    // Explicit nonexistent library path fails without crashing or leaving handles behind.
    EXPECT_FALSE(VulkanLoader::loadLibrary("definitely_nonexistent_vulkan_loader_12345.dll"));
    EXPECT_FALSE(VulkanLoader::isLibraryLoaded());
    EXPECT_EQ(VulkanLoader::getLibraryHandle(), nullptr);
    EXPECT_EQ(dynamic_api::vkCreateInstance, nullptr);
    EXPECT_EQ(dynamic_api::vkGetInstanceProcAddr, nullptr);

#if GP_PLATFORM_WINDOWS
    // Valid OS DLL that lacks Vulkan exports triggers !foundAllRequired cleanup and unloads cleanly.
    EXPECT_FALSE(VulkanLoader::loadLibrary("kernel32.dll"));
    EXPECT_FALSE(VulkanLoader::isLibraryLoaded());
    EXPECT_EQ(VulkanLoader::getLibraryHandle(), nullptr);
    EXPECT_EQ(dynamic_api::vkCreateInstance, nullptr);
#endif

    // Probe the host OS Vulkan loader if present on the machine.
    const bool hostHasVulkan = VulkanLoader::loadLibrary();
    if (hostHasVulkan)
    {
        EXPECT_TRUE(VulkanLoader::isLibraryLoaded());
        EXPECT_NE(VulkanLoader::getLibraryHandle(), nullptr);
        EXPECT_NE(dynamic_api::vkCreateInstance, nullptr);
        EXPECT_NE(dynamic_api::vkGetInstanceProcAddr, nullptr);
        EXPECT_NE(dynamic_api::vkGetDeviceProcAddr, nullptr);
        EXPECT_NE(dynamic_api::vkEnumerateInstanceExtensionProperties, nullptr);
        EXPECT_NE(dynamic_api::vkEnumerateInstanceLayerProperties, nullptr);

        // Repeat default load hits cached result without reloading.
        void* const firstHandle = VulkanLoader::getLibraryHandle();
        EXPECT_TRUE(VulkanLoader::loadLibrary());
        EXPECT_EQ(VulkanLoader::getLibraryHandle(), firstHandle);

        // Global proc lookup resolves vkCreateInstance cleanly.
        EXPECT_NE(VulkanLoader::getInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"), nullptr);
        EXPECT_EQ(VulkanLoader::getInstanceProcAddr(VK_NULL_HANDLE, nullptr), nullptr);

        // Reloading with an invalid explicit path unloads the previous library cleanly.
        EXPECT_FALSE(VulkanLoader::loadLibrary("definitely_nonexistent_vulkan_loader_67890.dll"));
        EXPECT_FALSE(VulkanLoader::isLibraryLoaded());
        EXPECT_EQ(VulkanLoader::getLibraryHandle(), nullptr);
        EXPECT_EQ(dynamic_api::vkCreateInstance, nullptr);
    }
}

TEST(VulkanLoaderTest, MockInstanceDeviceRollbackAndClearResolution)
{
    const ScopedLoaderReset guard;

    const VkInstance fakeInstance = reinterpret_cast<VkInstance>(static_cast<gp::UIntPtr>(0x1000u));
    const VkDevice fakeDevice = reinterpret_cast<VkDevice>(static_cast<gp::UIntPtr>(0x2000u));

    // Fails cleanly when vkGetInstanceProcAddr or handle is null.
    EXPECT_FALSE(VulkanLoader::loadInstanceFunctions(VK_NULL_HANDLE));
    EXPECT_FALSE(VulkanLoader::loadInstanceFunctions(fakeInstance));
    EXPECT_FALSE(VulkanLoader::loadDeviceFunctions(VK_NULL_HANDLE));
    EXPECT_FALSE(VulkanLoader::loadDeviceFunctions(fakeDevice));
    EXPECT_EQ(VulkanLoader::getInstanceProcAddr(fakeInstance, "vkCmdDraw"), nullptr);
    EXPECT_EQ(VulkanLoader::getDeviceProcAddr(fakeDevice, "vkCmdDraw"), nullptr);

    // Bind mock base and optional-base proc address resolvers.
    dynamic_api::vkGetInstanceProcAddr = &mockGetInstanceProcAddr;
    dynamic_api::vkGetDeviceProcAddr = &mockGetDeviceProcAddr;
    dynamic_api::vkCreateInstance =
        reinterpret_cast<PFN_vkCreateInstance>(reinterpret_cast<gp::UIntPtr>(&mockInstanceStub));
    dynamic_api::vkEnumerateInstanceVersion =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(reinterpret_cast<gp::UIntPtr>(&mockInstanceStub));

    EXPECT_EQ(VulkanLoader::getInstanceProcAddr(fakeInstance, "vkCmdDraw"), &mockInstanceStub);
    EXPECT_EQ(VulkanLoader::getInstanceProcAddr(fakeInstance, nullptr), nullptr);
    EXPECT_EQ(VulkanLoader::getDeviceProcAddr(VK_NULL_HANDLE, "vkCmdDraw"), nullptr);

    // 1. Full resolution succeeds and populates core, surface, optional, ray tracing, and platform pointers.
    g_missingProcName = nullptr;
    g_omitSurfaceProcs = false;
    g_omitPlatformInstanceProcs = false;
    g_omitRayTracingProcs = false;

    EXPECT_TRUE(VulkanLoader::loadInstanceFunctions(fakeInstance));
    EXPECT_EQ(reinterpret_cast<gp::UIntPtr>(dynamic_api::vkCmdDraw), reinterpret_cast<gp::UIntPtr>(&mockInstanceStub));
    EXPECT_EQ(
        reinterpret_cast<gp::UIntPtr>(dynamic_api::vkDestroyInstance), reinterpret_cast<gp::UIntPtr>(&mockInstanceStub)
    );
    EXPECT_EQ(
        reinterpret_cast<gp::UIntPtr>(dynamic_api::vkDestroySurfaceKHR),
        reinterpret_cast<gp::UIntPtr>(&mockInstanceStub)
    );
    EXPECT_EQ(
        reinterpret_cast<gp::UIntPtr>(dynamic_api::vkCmdBeginRenderingKHR),
        reinterpret_cast<gp::UIntPtr>(&mockInstanceStub)
    );
    EXPECT_TRUE(VulkanLoader::checkRayTracingInstanceFunctions());

    // 2. loadDeviceFunctions upgrades device-dispatchable pointers while keeping instance pointers intact.
    EXPECT_TRUE(VulkanLoader::loadDeviceFunctions(fakeDevice));
    EXPECT_EQ(reinterpret_cast<gp::UIntPtr>(dynamic_api::vkCmdDraw), reinterpret_cast<gp::UIntPtr>(&mockDeviceStub));
    EXPECT_EQ(
        reinterpret_cast<gp::UIntPtr>(dynamic_api::vkCreateBuffer), reinterpret_cast<gp::UIntPtr>(&mockDeviceStub)
    );
    EXPECT_EQ(
        reinterpret_cast<gp::UIntPtr>(dynamic_api::vkQueueSubmit), reinterpret_cast<gp::UIntPtr>(&mockDeviceStub)
    );
    EXPECT_EQ(
        reinterpret_cast<gp::UIntPtr>(dynamic_api::vkCmdBeginRenderingKHR),
        reinterpret_cast<gp::UIntPtr>(&mockDeviceStub)
    );
    EXPECT_EQ(
        reinterpret_cast<gp::UIntPtr>(dynamic_api::vkDestroyInstance), reinterpret_cast<gp::UIntPtr>(&mockInstanceStub)
    );
    EXPECT_EQ(VulkanLoader::getDeviceProcAddr(fakeDevice, "vkCmdDraw"), &mockDeviceStub);
    EXPECT_EQ(VulkanLoader::getDeviceProcAddr(fakeDevice, nullptr), nullptr);

    // 3. clearInstanceFunctions zeroes instance/surface/optional/platform pointers while keeping base and
    //    optional-base pointers intact.
    VulkanLoader::clearInstanceFunctions();
    EXPECT_EQ(dynamic_api::vkCmdDraw, nullptr);
    EXPECT_EQ(dynamic_api::vkDestroyInstance, nullptr);
    EXPECT_EQ(dynamic_api::vkDestroySurfaceKHR, nullptr);
    EXPECT_EQ(dynamic_api::vkCmdBeginRenderingKHR, nullptr);
    EXPECT_FALSE(VulkanLoader::checkRayTracingInstanceFunctions());
    EXPECT_NE(dynamic_api::vkGetInstanceProcAddr, nullptr);
    EXPECT_NE(dynamic_api::vkGetDeviceProcAddr, nullptr);
    EXPECT_NE(dynamic_api::vkCreateInstance, nullptr);
    EXPECT_NE(dynamic_api::vkEnumerateInstanceVersion, nullptr);

    // 4. Missing required core instance entry point fails and rolls back partial pointers even in offscreen mode.
    g_missingProcName = "vkCreateDevice";
    EXPECT_FALSE(VulkanLoader::loadInstanceFunctions(fakeInstance, true));
    EXPECT_EQ(dynamic_api::vkCmdDraw, nullptr);
    EXPECT_EQ(dynamic_api::vkDestroyInstance, nullptr);
    g_missingProcName = nullptr;

    // 5. Missing surface entry points fail and roll back normally, but succeed offscreen or via -RenderOffScreen.
    g_omitSurfaceProcs = true;
    {
        const ScopedCommandLine cleanCmd("gp_test.exe");
        EXPECT_FALSE(VulkanLoader::loadInstanceFunctions(fakeInstance, false));
        EXPECT_EQ(dynamic_api::vkCmdDraw, nullptr);
        EXPECT_TRUE(VulkanLoader::loadInstanceFunctions(fakeInstance, true));
    }
    {
        const ScopedCommandLine offscreenCmd("gp_test.exe -RenderOffScreen");
        EXPECT_TRUE(VulkanLoader::loadInstanceFunctions(fakeInstance, false));
    }
    g_omitSurfaceProcs = false;

    // 6. Missing platform-instance entry point (while core & surface succeed) fails and rolls back unless offscreen.
    gp::UInt32 platformInstanceCount = 0u;
#define GP_COUNT_PLATFORM_PROC(Type, Func) ++platformInstanceCount;
    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GP_COUNT_PLATFORM_PROC)
#undef GP_COUNT_PLATFORM_PROC
    if (platformInstanceCount > 0u)
    {
        g_omitPlatformInstanceProcs = true;
        {
            const ScopedCommandLine cleanCmd("gp_test.exe");
            EXPECT_FALSE(VulkanLoader::loadInstanceFunctions(fakeInstance, false));
            EXPECT_EQ(dynamic_api::vkCmdDraw, nullptr);
            EXPECT_TRUE(VulkanLoader::loadInstanceFunctions(fakeInstance, true));
        }
        g_omitPlatformInstanceProcs = false;
    }

    // 7. Missing optional ray tracing entry point still succeeds loadInstanceFunctions but fails RT check.
    g_omitRayTracingProcs = true;
    EXPECT_TRUE(VulkanLoader::loadInstanceFunctions(fakeInstance));
    EXPECT_FALSE(VulkanLoader::checkRayTracingInstanceFunctions());
    g_omitRayTracingProcs = false;

    // 8. VulkanGenericPlatform forwarding helpers delegate cleanly to VulkanLoader.
    EXPECT_TRUE(VulkanGenericPlatform::loadVulkanInstanceFunctions(fakeInstance));
    EXPECT_NE(dynamic_api::vkCmdDraw, nullptr);
    VulkanGenericPlatform::clearVulkanInstanceFunctions();
    EXPECT_EQ(dynamic_api::vkCmdDraw, nullptr);
    EXPECT_NE(dynamic_api::vkGetInstanceProcAddr, nullptr);

    // 9. clearAllFunctions resets base and optional-base pointers as well.
    VulkanLoader::clearAllFunctions();
    EXPECT_EQ(dynamic_api::vkGetInstanceProcAddr, nullptr);
    EXPECT_EQ(dynamic_api::vkGetDeviceProcAddr, nullptr);
    EXPECT_EQ(dynamic_api::vkCreateInstance, nullptr);
    EXPECT_EQ(dynamic_api::vkEnumerateInstanceVersion, nullptr);
}

TEST(VulkanLoaderTest, GenericPlatformWSIWrappersForwardOrFailSafely)
{
    const ScopedLoaderReset guard;

    const VkInstance fakeInstance = reinterpret_cast<VkInstance>(static_cast<gp::UIntPtr>(0x10u));
    const VkPhysicalDevice fakeGpu = reinterpret_cast<VkPhysicalDevice>(static_cast<gp::UIntPtr>(0x20u));
    const VkDevice fakeDevice = reinterpret_cast<VkDevice>(static_cast<gp::UIntPtr>(0x30u));
    const VkQueue fakeQueue = reinterpret_cast<VkQueue>(static_cast<gp::UIntPtr>(0x40u));
    const VkSurfaceKHR fakeSurface = reinterpret_cast<VkSurfaceKHR>(static_cast<gp::UIntPtr>(0x50u));

    VulkanGenericPlatformWindowContext windowCtx;
    VkPresentInfoKHR presentInfo{};
    VkSwapchainCreateInfoKHR swapchainInfo{};
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    // With null function pointers, wrappers fail safely without dereferencing null.
    EXPECT_EQ(VulkanGenericPlatform::present(fakeQueue, presentInfo), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT_EQ(
        VulkanGenericPlatform::createSwapchainKHR(windowCtx, fakeGpu, fakeDevice, &swapchainInfo, nullptr, &swapchain),
        VK_ERROR_INITIALIZATION_FAILED
    );
    VulkanGenericPlatform::destroySwapchainKHR(fakeDevice, swapchain, nullptr);
    VulkanGenericPlatform::destroySurface(0u, fakeInstance, fakeSurface);

    // With bound function pointers, wrappers delegate directly.
    g_presentCalls = 0u;
    g_createSwapchainCalls = 0u;
    g_destroySwapchainCalls = 0u;
    g_destroySurfaceCalls = 0u;

    dynamic_api::vkQueuePresentKHR = &mockQueuePresentKHR;
    dynamic_api::vkCreateSwapchainKHR = &mockCreateSwapchainKHR;
    dynamic_api::vkDestroySwapchainKHR = &mockDestroySwapchainKHR;
    dynamic_api::vkDestroySurfaceKHR = &mockDestroySurfaceKHR;

    EXPECT_EQ(VulkanGenericPlatform::present(fakeQueue, presentInfo), VK_SUBOPTIMAL_KHR);
    EXPECT_EQ(g_presentCalls, 1u);

    EXPECT_EQ(
        VulkanGenericPlatform::createSwapchainKHR(windowCtx, fakeGpu, fakeDevice, &swapchainInfo, nullptr, &swapchain),
        VK_SUCCESS
    );
    EXPECT_EQ(g_createSwapchainCalls, 1u);
    EXPECT_NE(swapchain, VK_NULL_HANDLE);

    VulkanGenericPlatform::destroySwapchainKHR(fakeDevice, swapchain, nullptr);
    EXPECT_EQ(g_destroySwapchainCalls, 1u);

    VulkanGenericPlatform::destroySurface(0u, fakeInstance, fakeSurface);
    EXPECT_EQ(g_destroySurfaceCalls, 1u);
}

}   // namespace gp::rhi::vulkan::tests
