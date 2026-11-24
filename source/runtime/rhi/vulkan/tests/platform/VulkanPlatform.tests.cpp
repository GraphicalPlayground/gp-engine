// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platform/VulkanPlatform.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::rhi::vulkan::tests
{

namespace
{

struct ScopedCommandLine final
{
    bool wasInitialized{ false };
    gp::String savedExecutable{};
    gp::String savedArgs{};

    explicit ScopedCommandLine(const gp::StringView commandLine) noexcept
        : wasInitialized(gp::CommandLine::isInitialized())
    {
        if (wasInitialized)
        {
            savedExecutable = gp::String(gp::CommandLine::getExecutablePath());
            savedArgs = gp::String(gp::CommandLine::getOriginalCommandLine());
        }
        gp::CommandLine::initialize(commandLine);
    }

    ~ScopedCommandLine() noexcept
    {
        if (wasInitialized)
        {
            gp::String combined = savedExecutable;
            if (!savedArgs.isEmpty())
            {
                combined.pushBack(' ');
                combined.append(savedArgs.view());
            }
            gp::CommandLine::initialize(combined.view());
        }
        else
        {
            gp::CommandLine::shutdown();
        }
    }
};

struct ScopedUninitializedCommandLine final
{
    bool wasInitialized{ false };
    gp::String savedExecutable{};
    gp::String savedArgs{};

    ScopedUninitializedCommandLine() noexcept
        : wasInitialized(gp::CommandLine::isInitialized())
    {
        if (wasInitialized)
        {
            savedExecutable = gp::String(gp::CommandLine::getExecutablePath());
            savedArgs = gp::String(gp::CommandLine::getOriginalCommandLine());
        }
        gp::CommandLine::shutdown();
    }

    ~ScopedUninitializedCommandLine() noexcept
    {
        if (wasInitialized)
        {
            gp::String combined = savedExecutable;
            if (!savedArgs.isEmpty())
            {
                combined.pushBack(' ');
                combined.append(savedArgs.view());
            }
            gp::CommandLine::initialize(combined.view());
        }
    }
};

VkPhysicalDeviceProperties makeTestDeviceProperties(const gp::UInt32 vendorId, const gp::UInt32 deviceId) noexcept
{
    VkPhysicalDeviceProperties props{};
    props.vendorID = vendorId;
    props.deviceID = deviceId;
    for (gp::UInt32 i = 0u; i < VK_UUID_SIZE; ++i)
    {
        props.pipelineCacheUUID[i] = static_cast<gp::UInt8>(0xA0u + i);
    }
    return props;
}

gp::Vector<gp::UInt8> makePipelineCacheHeader(
    const gp::UInt32 headerSize,
    const gp::UInt32 headerVersion,
    const gp::UInt32 vendorId,
    const gp::UInt32 deviceId,
    const gp::UInt8* const uuidBytes,
    const gp::USize trailingPayloadBytes = 8u
) noexcept
{
    gp::Vector<gp::UInt8> bytes;
    bytes.resize(16u + VK_UUID_SIZE + trailingPayloadBytes, 0u);
    gp::memory::copyMemory(bytes.data() + 0u, &headerSize, sizeof(gp::UInt32));
    gp::memory::copyMemory(bytes.data() + 4u, &headerVersion, sizeof(gp::UInt32));
    gp::memory::copyMemory(bytes.data() + 8u, &vendorId, sizeof(gp::UInt32));
    gp::memory::copyMemory(bytes.data() + 12u, &deviceId, sizeof(gp::UInt32));
    gp::memory::copyMemory(bytes.data() + 16u, uuidBytes, VK_UUID_SIZE);
    return bytes;
}

}   // namespace

TEST(VulkanPlatformTest, GenericWindowContextExtractsRawAndHALHandles)
{
    const VulkanGenericPlatformWindowContext defaultCtx;
    EXPECT_EQ(defaultCtx.getWindowHandle(), nullptr);
    EXPECT_EQ(defaultCtx.getWindowId(), 0u);
    EXPECT_TRUE(defaultCtx.isValid());
    EXPECT_TRUE(VulkanGenericPlatformWindowContext::canCreateSwapchainOnDemand());

    gp::Int32 dummy = 42;
    const VulkanGenericPlatformWindowContext rawCtx(&dummy);
    EXPECT_EQ(rawCtx.getWindowHandle(), &dummy);

    // Verify extraction for every NativeWindowBackend enumerator.
    {
        gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::Win32);
        handle.win32.hwnd = &dummy;
        handle.win32.hinstance = reinterpret_cast<void*>(static_cast<gp::UIntPtr>(0x1000u));
        const VulkanGenericPlatformWindowContext ctx(handle);
        EXPECT_EQ(ctx.getWindowHandle(), &dummy);
        EXPECT_EQ(ctx.getNativeWindowHandle().backend, gp::hal::NativeWindowBackend::Win32);
        EXPECT_EQ(ctx.getNativeWindowHandle().win32.hinstance, handle.win32.hinstance);
    }
    {
        gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::Cocoa);
        handle.cocoa.nsWindow = &dummy;
        EXPECT_EQ(VulkanGenericPlatformWindowContext(handle).getWindowHandle(), &dummy);
    }
    {
        gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::UIKit);
        handle.uikit.uiWindow = &dummy;
        EXPECT_EQ(VulkanGenericPlatformWindowContext(handle).getWindowHandle(), &dummy);
    }
    {
        gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::Wayland);
        handle.wayland.surface = &dummy;
        EXPECT_EQ(VulkanGenericPlatformWindowContext(handle).getWindowHandle(), &dummy);
    }
    {
        gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::X11);
        handle.x11.display = &dummy;
        EXPECT_EQ(VulkanGenericPlatformWindowContext(handle).getWindowHandle(), &dummy);
    }
    {
        gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::Android);
        handle.android.window = &dummy;
        EXPECT_EQ(VulkanGenericPlatformWindowContext(handle).getWindowHandle(), &dummy);
    }
    {
        gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::KMSDRM);
        handle.kmsdrm.gbmSurface = &dummy;
        EXPECT_EQ(VulkanGenericPlatformWindowContext(handle).getWindowHandle(), &dummy);
    }
    {
        gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::Vivante);
        handle.vivante.window = &dummy;
        EXPECT_EQ(VulkanGenericPlatformWindowContext(handle).getWindowHandle(), &dummy);
    }
    {
        const gp::hal::NativeWindowHandle handle(gp::hal::NativeWindowBackend::Unknown);
        EXPECT_EQ(VulkanGenericPlatformWindowContext(handle).getWindowHandle(), nullptr);
    }
}

TEST(VulkanPlatformTest, GenericPlatformCapabilitiesAndNoOpHooks)
{
    EXPECT_TRUE(VulkanGenericPlatform::isSupported());
    EXPECT_STREQ(VulkanGenericPlatform::getDefaultLibraryName(), "libvulkan.so.1");
    // Host machines without Vulkan drivers may return false from loadVulkanLibrary(); verify clean teardown either way.
    const bool loaded = VulkanGenericPlatform::loadVulkanLibrary();
    static_cast<void>(loaded);
    EXPECT_FALSE(VulkanGenericPlatform::loadVulkanInstanceFunctions(VK_NULL_HANDLE));
    VulkanGenericPlatform::clearVulkanInstanceFunctions();
    VulkanGenericPlatform::freeVulkanLibrary();

    VulkanGenericPlatform::initDevice(nullptr);
    VulkanGenericPlatform::notifyFoundInstanceLayersAndExtensions({}, {});
    VulkanGenericPlatform::notifyFoundDeviceLayersAndExtensions(VK_NULL_HANDLE, {}, {});

    gp::Vector<const char*> list;
    VulkanGenericPlatform::getInstanceLayers(list);
    VulkanGenericPlatform::getDeviceLayers(list);
    VulkanGenericPlatform::getPlatformInstanceExtensionNames(list);
    VulkanGenericPlatform::getPlatformDeviceExtensionNames(list);
    EXPECT_TRUE(list.isEmpty());

    VulkanGenericPlatform::onGPUCrash("simulated crash");
    VulkanGenericPlatform::overridePlatformHandlers(true);
    VulkanGenericPlatform::overridePlatformHandlers(false);
    VulkanGenericPlatform::notifySwapchainPresentTiming(VK_NULL_HANDLE, VK_NULL_HANDLE, true);

    EXPECT_TRUE(VulkanGenericPlatform::supportsBCTextureFormats());
    EXPECT_FALSE(VulkanGenericPlatform::supportsASTCTextureFormats());
    EXPECT_FALSE(VulkanGenericPlatform::supportsETC2TextureFormats());
    EXPECT_TRUE(VulkanGenericPlatform::supportsR16UnormTextureFormat());
    EXPECT_TRUE(VulkanGenericPlatform::supportsQuerySurfaceProperties());
    EXPECT_FALSE(VulkanGenericPlatform::requiresMobileRenderer());
    EXPECT_FALSE(VulkanGenericPlatform::requiresPresentLayoutFix());
    EXPECT_FALSE(VulkanGenericPlatform::hasUnifiedMemory());
    EXPECT_TRUE(VulkanGenericPlatform::supportParallelRenderingTasks());
    EXPECT_TRUE(VulkanGenericPlatform::supportsDynamicResolution());
    EXPECT_TRUE(VulkanGenericPlatform::supportsVolumeTextureRendering());
    EXPECT_FALSE(VulkanGenericPlatform::requiresSwapchainGeneralInitialLayout());
    EXPECT_TRUE(VulkanGenericPlatform::recreateSwapchainOnFail());

    gp::Int32 dummyWindow = 1;
    EXPECT_FALSE(VulkanGenericPlatform::isWindowValid(nullptr));
    EXPECT_TRUE(VulkanGenericPlatform::isWindowValid(&dummyWindow));
    EXPECT_FALSE(VulkanGenericPlatform::tryConsumeSwapchainRecreateRequest(&dummyWindow));

    EXPECT_TRUE(VulkanGenericPlatform::requiresWaitingForFrameCompletionEvent());
    EXPECT_TRUE(VulkanGenericPlatform::supportsNullPixelShader());
    EXPECT_FALSE(VulkanGenericPlatform::requiresDepthStencilFullWrite());
    EXPECT_FALSE(VulkanGenericPlatform::requiresMergedDepthStencilLayout());

    const VkPhysicalDeviceProperties props{};
    VkDeviceCreateInfo devInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    VulkanGenericPlatform::serializeDeviceCreateInfo(props, devInfo);

    VkImageCreateInfo imgInfo = zeroVulkanStruct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    VulkanGenericPlatform::setImageMemoryRequirementWorkaround(imgInfo);

    EXPECT_EQ(
        VulkanGenericPlatform::debugReportFunction(
            0u, VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT, 0u, 0u, 0, "layer", "msg", nullptr
        ),
        VK_TRUE
    );

    const VkShaderStageFlags stages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    EXPECT_EQ(VulkanGenericPlatform::requiredWaveOpsShaderStageFlags(stages), stages);
    EXPECT_EQ(VulkanGenericPlatform::getTimeDomain(), VK_TIME_DOMAIN_DEVICE_KHR);
    EXPECT_GT(VulkanGenericPlatform::calibratedTimestampToCycles64(0u), 0u);
    EXPECT_EQ(VulkanGenericPlatform::getCompiledPSOCacheTopFolderPath().view(), "Saved/VulkanProgramBinaryCache");
    EXPECT_TRUE(VulkanGenericPlatform::getPSOCacheFilenames().isEmpty());
}

TEST(VulkanPlatformTest, FeatureLevelResolutionAndCommandLineOverrides)
{
    // Uninitialized command line safely preserves requested desktop feature level.
    {
        const ScopedUninitializedCommandLine uninitGuard;
        EXPECT_EQ(VulkanGenericPlatform::getFeatureLevel(FeatureLevel::SM6), FeatureLevel::SM6);
        EXPECT_EQ(VulkanGenericPlatform::getFeatureLevel(FeatureLevel::ES3_1), FeatureLevel::ES3_1);
    }

    {
        const ScopedCommandLine cleanCmd("gp_test.exe");
        EXPECT_EQ(VulkanGenericPlatform::getFeatureLevel(FeatureLevel::SM6), FeatureLevel::SM6);
        EXPECT_EQ(VulkanGenericPlatform::getFeatureLevel(FeatureLevel::SM5), FeatureLevel::SM5);
        EXPECT_EQ(VulkanGenericPlatform::getFeatureLevel(FeatureLevel::ES3_1), FeatureLevel::ES3_1);
    }

#if !GP_WITH_EDITOR
    {
        const ScopedCommandLine es31Cmd("gp_test.exe -FeatureLevelES31");
        EXPECT_EQ(VulkanGenericPlatform::getFeatureLevel(FeatureLevel::SM6), FeatureLevel::ES3_1);
    }
    {
        const ScopedCommandLine es31UnderscoreCmd("gp_test.exe -FeatureLevelES3_1");
        EXPECT_EQ(VulkanGenericPlatform::getFeatureLevel(FeatureLevel::SM5), FeatureLevel::ES3_1);
    }
#endif
}

TEST(VulkanPlatformTest, PSOBinaryCacheMatchingAndFilenameFormatting)
{
    const VkPhysicalDeviceProperties props = makeTestDeviceProperties(0x10DEu, 0x2684u);

    // Exact header match succeeds both with trailing payload and at exact 32-byte header boundary.
    {
        const gp::Vector<gp::UInt8> validCache = makePipelineCacheHeader(
            16u + VK_UUID_SIZE,
            VK_PIPELINE_CACHE_HEADER_VERSION_ONE,
            props.vendorID,
            props.deviceID,
            props.pipelineCacheUUID,
            8u
        );
        EXPECT_TRUE(VulkanGenericPlatform::psoBinaryCacheMatches(props, validCache.view()));

        const gp::Vector<gp::UInt8> exactHeaderOnly = makePipelineCacheHeader(
            16u + VK_UUID_SIZE,
            VK_PIPELINE_CACHE_HEADER_VERSION_ONE,
            props.vendorID,
            props.deviceID,
            props.pipelineCacheUUID,
            0u
        );
        EXPECT_EQ(exactHeaderOnly.size(), 16u + static_cast<gp::USize>(VK_UUID_SIZE));
        EXPECT_TRUE(VulkanGenericPlatform::psoBinaryCacheMatches(props, exactHeaderOnly.view()));
    }

    // Truncated cache fails safely without OOB read (0, 8, and exact headerSize - 1 = 31 bytes).
    {
        const gp::Array<gp::UInt8, 8u> shortCache{};
        EXPECT_FALSE(
            VulkanGenericPlatform::psoBinaryCacheMatches(
                props, gp::ConstVectorView<gp::UInt8>(shortCache.data(), shortCache.size())
            )
        );
        EXPECT_FALSE(VulkanGenericPlatform::psoBinaryCacheMatches(props, gp::ConstVectorView<gp::UInt8>{}));

        gp::Vector<gp::UInt8> oneByteShort = makePipelineCacheHeader(
            16u + VK_UUID_SIZE,
            VK_PIPELINE_CACHE_HEADER_VERSION_ONE,
            props.vendorID,
            props.deviceID,
            props.pipelineCacheUUID,
            0u
        );
        oneByteShort.popBack();
        EXPECT_EQ(oneByteShort.size(), 16u + static_cast<gp::USize>(VK_UUID_SIZE) - 1u);
        EXPECT_FALSE(VulkanGenericPlatform::psoBinaryCacheMatches(props, oneByteShort.view()));
    }

    // Mismatched header size, header version, vendor ID, device ID, or first/last UUID byte all return false.
    {
        const gp::Vector<gp::UInt8> badSize = makePipelineCacheHeader(
            12u, VK_PIPELINE_CACHE_HEADER_VERSION_ONE, props.vendorID, props.deviceID, props.pipelineCacheUUID
        );
        EXPECT_FALSE(VulkanGenericPlatform::psoBinaryCacheMatches(props, badSize.view()));

        const gp::Vector<gp::UInt8> badVersion =
            makePipelineCacheHeader(16u + VK_UUID_SIZE, 99u, props.vendorID, props.deviceID, props.pipelineCacheUUID);
        EXPECT_FALSE(VulkanGenericPlatform::psoBinaryCacheMatches(props, badVersion.view()));

        const gp::Vector<gp::UInt8> badVendor = makePipelineCacheHeader(
            16u + VK_UUID_SIZE, VK_PIPELINE_CACHE_HEADER_VERSION_ONE, 0x1002u, props.deviceID, props.pipelineCacheUUID
        );
        EXPECT_FALSE(VulkanGenericPlatform::psoBinaryCacheMatches(props, badVendor.view()));

        const gp::Vector<gp::UInt8> badDevice = makePipelineCacheHeader(
            16u + VK_UUID_SIZE, VK_PIPELINE_CACHE_HEADER_VERSION_ONE, props.vendorID, 0x9999u, props.pipelineCacheUUID
        );
        EXPECT_FALSE(VulkanGenericPlatform::psoBinaryCacheMatches(props, badDevice.view()));

        gp::Vector<gp::UInt8> badUuidFirst = makePipelineCacheHeader(
            16u + VK_UUID_SIZE,
            VK_PIPELINE_CACHE_HEADER_VERSION_ONE,
            props.vendorID,
            props.deviceID,
            props.pipelineCacheUUID
        );
        badUuidFirst[16u] ^= 0xFFu;
        EXPECT_FALSE(VulkanGenericPlatform::psoBinaryCacheMatches(props, badUuidFirst.view()));

        gp::Vector<gp::UInt8> badUuidLast = makePipelineCacheHeader(
            16u + VK_UUID_SIZE,
            VK_PIPELINE_CACHE_HEADER_VERSION_ONE,
            props.vendorID,
            props.deviceID,
            props.pipelineCacheUUID
        );
        badUuidLast[16u + VK_UUID_SIZE - 1u] ^= 0xFFu;
        EXPECT_FALSE(VulkanGenericPlatform::psoBinaryCacheMatches(props, badUuidLast.view()));
    }

    // PSO binary cache filename formatting appends lowercase hex vendor/device IDs once (case-insensitively).
    {
        const gp::String qualified = VulkanGenericPlatform::createPSOBinaryCacheFilename(props, "pipeline.cache");
        EXPECT_EQ(qualified.view(), "pipeline.cache.10de.2684");

        // Idempotent for both lowercase and uppercase existing suffixes.
        EXPECT_EQ(
            VulkanGenericPlatform::createPSOBinaryCacheFilename(props, qualified.view()).view(),
            "pipeline.cache.10de.2684"
        );
        EXPECT_EQ(
            VulkanGenericPlatform::createPSOBinaryCacheFilename(props, "pipeline.cache.10DE.2684").view(),
            "pipeline.cache.10DE.2684"
        );

        // Empty filename, zero IDs, and max 32-bit IDs.
        EXPECT_EQ(VulkanGenericPlatform::createPSOBinaryCacheFilename(props, "").view(), ".10de.2684");

        const VkPhysicalDeviceProperties zeroProps = makeTestDeviceProperties(0u, 0u);
        EXPECT_EQ(VulkanGenericPlatform::createPSOBinaryCacheFilename(zeroProps, "pso").view(), "pso.0.0");

        const VkPhysicalDeviceProperties maxProps = makeTestDeviceProperties(0xFFFFFFFFu, 0xFFFFFFFFu);
        EXPECT_EQ(VulkanGenericPlatform::createPSOBinaryCacheFilename(maxProps, "pso").view(), "pso.ffffffff.ffffffff");
    }
}

TEST(VulkanPlatformTest, ProfileChecksCVarCLIAndProfileNames)
{
    gp::console::IConsoleManager& consoleManager = gp::console::IConsoleManager::get();
    if (!consoleManager.isInitialized())
    {
        static_cast<void>(consoleManager.initialize());
    }

    gp::console::IConsoleVariable* const cvarProfile = consoleManager.findConsoleVariable("r.Vulkan.UseProfileCheck");
    ASSERT_NE(cvarProfile, nullptr);
    EXPECT_EQ(cvarProfile->getInt(), 1);
    EXPECT_NE(static_cast<gp::UInt32>(cvarProfile->getFlags() & gp::console::ConsoleVarFlags::ReadOnly), 0u);

    {
        const ScopedUninitializedCommandLine uninitGuard;
        EXPECT_TRUE(VulkanGenericPlatform::supportsProfileChecks());
    }
    {
        const ScopedCommandLine cleanCmd("gp_test.exe");
        EXPECT_TRUE(VulkanGenericPlatform::supportsProfileChecks());
    }
    {
        const ScopedCommandLine skipCmd("gp_test.exe -SkipVulkanProfileCheck");
        EXPECT_FALSE(VulkanGenericPlatform::supportsProfileChecks());
    }

    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::ES2Removed, false).view(),
        "VP_GP_Vulkan_ES2"
    );
    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::ES3_1, false).view(),
        "VP_GP_Vulkan_ES3_1"
    );
    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::ES3_1, true).view(),
        "VP_GP_Vulkan_ES3_1_RT"
    );
    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::SM4Removed, false).view(),
        "VP_GP_Vulkan_SM4"
    );
    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::SM5, false).view(), "VP_GP_Vulkan_SM5"
    );
    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::SM5, true).view(),
        "VP_GP_Vulkan_SM5_RT"
    );
    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::SM6, false).view(), "VP_GP_Vulkan_SM6"
    );
    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::SM6, true).view(),
        "VP_GP_Vulkan_SM6_RT"
    );
    EXPECT_EQ(
        VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::Count, false).view(),
        "VP_GP_Vulkan_Unknown"
    );
}

TEST(VulkanPlatformTest, ActiveHostPlatformSpecialization)
{
#if GP_PLATFORM_WINDOWS
    static_assert(std::is_same_v<VulkanPlatform, VulkanWindowsPlatform>);
    static_assert(std::is_same_v<VulkanPlatformWindowContext, VulkanGenericPlatformWindowContext>);

    EXPECT_STREQ(VulkanPlatform::getDefaultLibraryName(), "vulkan-1.dll");
    EXPECT_EQ(VulkanPlatform::getTimeDomain(), VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR);
    EXPECT_EQ(VulkanPlatform::calibratedTimestampToCycles64(987654321ULL), 987654321ULL);
    EXPECT_EQ(GP_VULKAN_ENABLE_DESKTOP_HMD_SUPPORT, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE, 1);
    EXPECT_EQ(GP_VULKAN_SUPPORTS_RAY_TRACING_POSITION_FETCH, 1);

    gp::Vector<const char*> instanceExts;
    VulkanPlatform::getPlatformInstanceExtensionNames(instanceExts);
    ASSERT_EQ(instanceExts.size(), 1u);
    EXPECT_STREQ(instanceExts[0], VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

    gp::Vector<const char*> deviceExts;
    VulkanPlatform::getPlatformDeviceExtensionNames(deviceExts);
    ASSERT_EQ(deviceExts.size(), 3u);
    EXPECT_STREQ(deviceExts[0], VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
    EXPECT_STREQ(deviceExts[1], VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
    EXPECT_STREQ(deviceExts[2], VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);

    // Verify platform instance entrypoint macro expands vkCreateWin32SurfaceKHR.
    gp::UInt32 platformInstanceEntryCount = 0u;
    #define GP_TEST_COUNT_ENTRY(Type, Func) ++platformInstanceEntryCount;
    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GP_TEST_COUNT_ENTRY)
    #undef GP_TEST_COUNT_ENTRY
    EXPECT_EQ(platformInstanceEntryCount, 1u);
#elif GP_PLATFORM_LINUX
    static_assert(std::is_same_v<VulkanPlatform, VulkanLinuxPlatform>);
    static_assert(std::is_same_v<VulkanPlatformWindowContext, VulkanLinuxPlatformWindowContext>);

    EXPECT_STREQ(VulkanPlatform::getDefaultLibraryName(), "libvulkan.so.1");
    EXPECT_EQ(VulkanPlatform::getTimeDomain(), VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR);
    EXPECT_EQ(VulkanPlatform::calibratedTimestampToCycles64(5000ULL), 5000ULL);
    EXPECT_EQ(VulkanPlatform::supportsASTCTextureFormats(), static_cast<bool>(GP_ARCHITECTURE_ARM64));
    EXPECT_EQ(VulkanPlatform::supportsETC2TextureFormats(), static_cast<bool>(GP_ARCHITECTURE_ARM64));

    gp::Vector<const char*> deviceExts;
    VulkanPlatform::getPlatformDeviceExtensionNames(deviceExts);
    ASSERT_EQ(deviceExts.size(), 2u);
    EXPECT_STREQ(deviceExts[0], VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    EXPECT_STREQ(deviceExts[1], VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);

    gp::Int32 dummyDisplay = 1;
    gp::hal::NativeWindowHandle x11Invalid(gp::hal::NativeWindowBackend::X11);
    x11Invalid.x11.display = &dummyDisplay;
    x11Invalid.x11.window = 0u;
    EXPECT_FALSE(VulkanLinuxPlatformWindowContext(x11Invalid, 7u).isValid());
    EXPECT_EQ(VulkanLinuxPlatformWindowContext(x11Invalid, 7u).getWindowId(), 7u);

    x11Invalid.x11.window = 42u;
    EXPECT_TRUE(VulkanLinuxPlatformWindowContext(x11Invalid, 7u).isValid());

    gp::hal::NativeWindowHandle wlHandle(gp::hal::NativeWindowBackend::Wayland);
    wlHandle.wayland.surface = &dummyDisplay;
    wlHandle.wayland.display = nullptr;
    EXPECT_FALSE(VulkanLinuxPlatformWindowContext(wlHandle, 9u).isValid());
    wlHandle.wayland.display = &dummyDisplay;
    EXPECT_TRUE(VulkanLinuxPlatformWindowContext(wlHandle, 9u).isValid());
#elif GP_PLATFORM_MACOS
    static_assert(std::is_same_v<VulkanPlatform, VulkanMacPlatform>);
    EXPECT_STREQ(VulkanPlatform::getDefaultLibraryName(), "libvulkan.1.dylib");
    EXPECT_EQ(VulkanPlatform::hasUnifiedMemory(), static_cast<bool>(GP_ARCHITECTURE_ARM64));

    gp::Vector<const char*> instanceExts;
    VulkanPlatform::getPlatformInstanceExtensionNames(instanceExts);
    ASSERT_GE(instanceExts.size(), 1u);
    EXPECT_STREQ(instanceExts[0], VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#elif defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
    static_assert(std::is_same_v<VulkanPlatform, VulkanAndroidPlatform>);
    static_assert(std::is_same_v<VulkanPlatformWindowContext, VulkanAndroidPlatformWindowContext>);

    EXPECT_STREQ(VulkanPlatform::getDefaultLibraryName(), "libvulkan.so");
    EXPECT_FALSE(VulkanPlatform::supportsBCTextureFormats());
    EXPECT_TRUE(VulkanPlatform::supportsASTCTextureFormats());
    EXPECT_TRUE(VulkanPlatform::supportsETC2TextureFormats());
    EXPECT_TRUE(VulkanPlatform::requiresMobileRenderer());
    EXPECT_TRUE(VulkanPlatform::requiresSwapchainGeneralInitialLayout());
    EXPECT_FALSE(VulkanPlatform::requiresWaitingForFrameCompletionEvent());
    EXPECT_FALSE(VulkanPlatform::supportsNullPixelShader());
    EXPECT_FALSE(VulkanPlatform::recreateSwapchainOnFail());
    EXPECT_EQ(VulkanPlatform::getTimeDomain(), VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR);
    EXPECT_EQ(VulkanPlatform::calibratedTimestampToCycles64(5000ULL), 5000ULL);
    EXPECT_EQ(
        VulkanPlatform::getVulkanProfileNameForFeatureLevel(FeatureLevel::ES3_1, true).view(),
        "VP_GP_Vulkan_ES3_1_Android_RT"
    );
#endif
}

}   // namespace gp::rhi::vulkan::tests
