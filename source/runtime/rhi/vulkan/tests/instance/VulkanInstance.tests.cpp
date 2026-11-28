// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "instance/VulkanInstance.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "memory/Memory.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstring>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

VkExtensionProperties makeInstExtProps(const char* const name) noexcept
{
    VkExtensionProperties props{};
    gp::memory::zeroMemory(&props, sizeof(props));
    const gp::USize length = std::strlen(name);
    const gp::USize copyLen =
        (length < VK_MAX_EXTENSION_NAME_SIZE - 1u) ? length : static_cast<gp::USize>(VK_MAX_EXTENSION_NAME_SIZE - 1u);
    gp::memory::copyMemory(props.extensionName, name, copyLen);
    props.specVersion = 1u;
    return props;
}

}   // namespace

TEST(VulkanInstanceTest, BuildApplicationInfoPopulatesAndRespectsDisableRegistration)
{
    VulkanInstanceCreateDesc desc{};
    desc.applicationName = "TestApp";
    desc.applicationVersion = VK_MAKE_API_VERSION(0u, 2u, 3u, 4u);
    desc.engineName = "TestEngine";
    desc.engineVersion = VK_MAKE_API_VERSION(0u, 5u, 6u, 7u);
    desc.apiVersion = VK_API_VERSION_1_3;

    const VkApplicationInfo normalInfo = VulkanInstance::buildApplicationInfo(desc, false);
    EXPECT_EQ(normalInfo.sType, VK_STRUCTURE_TYPE_APPLICATION_INFO);
    EXPECT_EQ(normalInfo.pNext, nullptr);
    EXPECT_STREQ(normalInfo.pApplicationName, "TestApp");
    EXPECT_EQ(normalInfo.applicationVersion, desc.applicationVersion);
    EXPECT_STREQ(normalInfo.pEngineName, "TestEngine");
    EXPECT_EQ(normalInfo.engineVersion, desc.engineVersion);
    EXPECT_EQ(normalInfo.apiVersion, VK_API_VERSION_1_3);

    const VkApplicationInfo disabledInfo = VulkanInstance::buildApplicationInfo(desc, true);
    EXPECT_EQ(disabledInfo.pApplicationName, nullptr);
    EXPECT_EQ(disabledInfo.pEngineName, nullptr);
    EXPECT_EQ(disabledInfo.apiVersion, VK_API_VERSION_1_3);

    // Zero apiVersion falls back to kVulkanApiVersion.
    desc.apiVersion = 0u;
    const VkApplicationInfo fallbackInfo = VulkanInstance::buildApplicationInfo(desc, false);
    EXPECT_EQ(fallbackInfo.apiVersion, kVulkanApiVersion);
}

TEST(VulkanInstanceTest, FindMissingInstanceExtensionsDetectsAbsences)
{
    gp::Vector<VkExtensionProperties> available;
    available.pushBack(makeInstExtProps(VK_KHR_SURFACE_EXTENSION_NAME));
    available.pushBack(makeInstExtProps(VK_EXT_DEBUG_UTILS_EXTENSION_NAME));

    const char* const requested[] = { VK_KHR_SURFACE_EXTENSION_NAME,
                                      nullptr,
                                      "VK_EXT_missing_extension_a",
                                      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                      "VK_EXT_missing_extension_b" };

    const gp::Vector<const char*> missing = VulkanInstance::findMissingInstanceExtensions(
        gp::ConstVectorView<const char*>(requested, 5u),
        gp::ConstVectorView<VkExtensionProperties>(available.data(), available.size())
    );

    ASSERT_EQ(missing.size(), 2u);
    EXPECT_STREQ(missing[0], "VK_EXT_missing_extension_a");
    EXPECT_STREQ(missing[1], "VK_EXT_missing_extension_b");

    // Empty requested list returns empty missing list.
    const gp::Vector<const char*> noneMissing = VulkanInstance::findMissingInstanceExtensions(
        {}, gp::ConstVectorView<VkExtensionProperties>(available.data(), available.size())
    );
    EXPECT_EQ(noneMissing.size(), 0u);

    // Empty available list marks all non-null requested extensions as missing.
    const gp::Vector<const char*> allMissing =
        VulkanInstance::findMissingInstanceExtensions(gp::ConstVectorView<const char*>(requested, 5u), {});
    EXPECT_EQ(allMissing.size(), 4u);
}

TEST(VulkanInstanceTest, DefaultConstructedStateAndSafeShutdown)
{
    VulkanInstance instance;
    EXPECT_FALSE(instance.isValid());
    EXPECT_EQ(instance.getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(instance.getApiVersion(), 0u);
    EXPECT_EQ(instance.getOptionalExtensions().hasKHRGetSurfaceCapabilities2, 0u);
    EXPECT_EQ(instance.getEnabledExtensions().size(), 0u);
    EXPECT_EQ(instance.getEnabledLayers().size(), 0u);
    EXPECT_EQ(instance.getActiveDebugLayerExtension(), ActiveDebugLayerExtension::None);
    EXPECT_FALSE(instance.getDebugMessenger().isValid());
    EXPECT_EQ(instance.getLastCreateResult(), VK_SUCCESS);

    // Shutting down an uninitialized instance must not clobber global dynamic_api instance pointers.
    const PFN_vkDestroyInstance savedDestroy = dynamic_api::vkDestroyInstance;
    const PFN_vkDestroyInstance sentinelDestroy =
        reinterpret_cast<PFN_vkDestroyInstance>(static_cast<gp::UIntPtr>(0x1234u));
    dynamic_api::vkDestroyInstance = sentinelDestroy;

    instance.shutdown();
    EXPECT_FALSE(instance.isValid());
    EXPECT_EQ(dynamic_api::vkDestroyInstance, sentinelDestroy);

    dynamic_api::vkDestroyInstance = savedDestroy;
}

TEST(VulkanInstanceTest, LiveInstanceLifecycleWhenDriverAvailable)
{
    if (!VulkanLoader::loadLibrary())
    {
        GTEST_SKIP() << "Vulkan loader library not available on this host.";
    }

    VulkanInstance instance;
    VulkanInstanceCreateDesc desc{};
    desc.applicationName = "VulkanInstanceUnitTest";
    desc.allowRetryWithoutLayers = true;
    desc.allowMissingSurfaceFunctions = true;
    desc.enableDebugCallbacks = false;

    if (!instance.initialize(desc))
    {
        VulkanLoader::freeLibrary();
        GTEST_SKIP() << "No Vulkan ICD available to create a live VkInstance on this host.";
    }

    EXPECT_TRUE(instance.isValid());
    EXPECT_NE(instance.getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(instance.getApiVersion(), kVulkanApiVersion);
    EXPECT_EQ(instance.getLastCreateResult(), VK_SUCCESS);

    // Calling initialize() again on an already-valid instance is an idempotent no-op.
    EXPECT_TRUE(instance.initialize(desc));

    // Probe physical device selection on the live instance (may be empty on software-less CI).
    VulkanPhysicalDeviceInfo selectedInfo{};
    const VkPhysicalDevice selectedGpu =
        selectPhysicalDevice(instance.getHandle(), instance.getApiVersion(), selectedInfo);
    if (selectedGpu != VK_NULL_HANDLE)
    {
        EXPECT_EQ(selectedInfo.physicalDevice, selectedGpu);
        EXPECT_FALSE(selectedInfo.formattedApiVersion.isEmpty());
        EXPECT_FALSE(selectedInfo.formattedDriverVersion.isEmpty());
    }

    instance.shutdown();
    EXPECT_FALSE(instance.isValid());
    EXPECT_EQ(instance.getHandle(), VK_NULL_HANDLE);

    VulkanLoader::freeLibrary();
}

}   // namespace gp::rhi::vulkan::tests
