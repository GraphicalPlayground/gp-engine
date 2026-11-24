// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platform/VulkanGenericPlatform.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "memory/Memory.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platform/VulkanPlatform.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanUseProfileCheck(
    "r.Vulkan.UseProfileCheck",
    1,
    "0 to assume all requested feature levels are supported. 1 to verify feature level support using a profile check.",
    gp::console::ConsoleVarFlags::ReadOnly
);

void appendLowerHex(gp::String& outStr, const gp::UInt32 value) noexcept
{
    static constexpr char kHexDigits[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                             '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

    char buffer[8]{};
    gp::UInt32 count = 0u;
    gp::UInt32 remaining = value;
    do
    {
        buffer[count++] = kHexDigits[remaining & 0xFu];
        remaining >>= 4u;
    }
    while (remaining != 0u);

    for (gp::UInt32 i = count; i > 0u; --i)
    {
        outStr.pushBack(buffer[i - 1u]);
    }
}

char toLowerAscii(const char ch) noexcept
{
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

bool endsWithIgnoreCase(const gp::StringView text, const gp::StringView suffix) noexcept
{
    if (suffix.length() > text.length())
    {
        return false;
    }

    const gp::USize offset = text.length() - suffix.length();
    for (gp::USize i = 0u; i < suffix.length(); ++i)
    {
        if (toLowerAscii(text[offset + i]) != toLowerAscii(suffix[i]))
        {
            return false;
        }
    }
    return true;
}

gp::StringView featureLevelToProfileSuffix(const FeatureLevel featureLevel) noexcept
{
    switch (featureLevel)
    {
    case FeatureLevel::ES2Removed:
        return "ES2";
    case FeatureLevel::ES3_1:
        return "ES3_1";
    case FeatureLevel::SM4Removed:
        return "SM4";
    case FeatureLevel::SM5:
        return "SM5";
    case FeatureLevel::SM6:
        return "SM6";
    case FeatureLevel::Count:
    default:
        return "Unknown";
    }
}

}   // namespace

bool VulkanGenericPlatform::loadVulkanLibrary() noexcept
{
    return VulkanLoader::loadLibrary();
}

bool VulkanGenericPlatform::loadVulkanInstanceFunctions(const VkInstance instance) noexcept
{
    return VulkanLoader::loadInstanceFunctions(instance);
}

void VulkanGenericPlatform::clearVulkanInstanceFunctions() noexcept
{
    VulkanLoader::clearInstanceFunctions();
}

void VulkanGenericPlatform::freeVulkanLibrary() noexcept
{
    VulkanLoader::freeLibrary();
}

void VulkanGenericPlatform::destroySurface(
    const gp::UInt32 windowId, const VkInstance instance, const VkSurfaceKHR surface
) noexcept
{
    static_cast<void>(windowId);
    if (vkDestroySurfaceKHR != nullptr)
    {
        vkDestroySurfaceKHR(instance, surface, GP_VULKAN_CPU_ALLOCATOR);
    }
}

VkResult VulkanGenericPlatform::present(const VkQueue queue, VkPresentInfoKHR& presentInfo) noexcept
{
    if (vkQueuePresentKHR == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkQueuePresentKHR(queue, &presentInfo);
}

VkResult VulkanGenericPlatform::createSwapchainKHR(
    VulkanGenericPlatformWindowContext& windowContext,
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const VkSwapchainCreateInfoKHR* const createInfo,
    const VkAllocationCallbacks* const allocator,
    VkSwapchainKHR* const outSwapchain
) noexcept
{
    static_cast<void>(windowContext);
    static_cast<void>(physicalDevice);
    if (vkCreateSwapchainKHR == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkCreateSwapchainKHR(device, createInfo, allocator, outSwapchain);
}

void VulkanGenericPlatform::destroySwapchainKHR(
    const VkDevice device, const VkSwapchainKHR swapchain, const VkAllocationCallbacks* const allocator
) noexcept
{
    if (vkDestroySwapchainKHR != nullptr)
    {
        vkDestroySwapchainKHR(device, swapchain, allocator);
    }
}

FeatureLevel VulkanGenericPlatform::getFeatureLevel(const FeatureLevel requestedFeatureLevel) noexcept
{
    const bool cliForcesES31 = gp::CommandLine::isInitialized() && (gp::CommandLine::hasArgument("FeatureLevelES31") ||
                                                                    gp::CommandLine::hasArgument("FeatureLevelES3_1"));

    const bool forceES31 =
        VulkanPlatform::requiresMobileRenderer() || (requestedFeatureLevel == FeatureLevel::ES3_1) || cliForcesES31;

#if GP_WITH_EDITOR
    static_cast<void>(forceES31);
    return requestedFeatureLevel;
#else
    return forceES31 ? FeatureLevel::ES3_1 : requestedFeatureLevel;
#endif
}

bool VulkanGenericPlatform::psoBinaryCacheMatches(
    const VkPhysicalDeviceProperties& deviceProperties, const gp::ConstVectorView<gp::UInt8> deviceCache
) noexcept
{
    constexpr gp::USize kExpectedHeaderSize = 16u + static_cast<gp::USize>(VK_UUID_SIZE);
    if (deviceCache.size() < kExpectedHeaderSize)
    {
        return false;
    }

    const gp::UInt8* const bytes = deviceCache.data();

    gp::UInt32 headerSize = 0u;
    gp::UInt32 headerVersion = 0u;
    gp::UInt32 vendorId = 0u;
    gp::UInt32 deviceId = 0u;

    gp::memory::copyMemory(&headerSize, bytes + 0u, sizeof(gp::UInt32));
    if (headerSize != static_cast<gp::UInt32>(kExpectedHeaderSize))
    {
        return false;
    }

    gp::memory::copyMemory(&headerVersion, bytes + 4u, sizeof(gp::UInt32));
    if (headerVersion != static_cast<gp::UInt32>(VK_PIPELINE_CACHE_HEADER_VERSION_ONE))
    {
        return false;
    }

    gp::memory::copyMemory(&vendorId, bytes + 8u, sizeof(gp::UInt32));
    if (vendorId != deviceProperties.vendorID)
    {
        return false;
    }

    gp::memory::copyMemory(&deviceId, bytes + 12u, sizeof(gp::UInt32));
    if (deviceId != deviceProperties.deviceID)
    {
        return false;
    }

    return gp::memory::compareMemory(
               deviceProperties.pipelineCacheUUID, bytes + 16u, static_cast<gp::USize>(VK_UUID_SIZE)
           ) == 0;
}

gp::String VulkanGenericPlatform::createPSOBinaryCacheFilename(
    const VkPhysicalDeviceProperties& deviceProperties, const gp::StringView cacheFilename
) noexcept
{
    gp::String appendage;
    appendage.pushBack('.');
    appendLowerHex(appendage, deviceProperties.vendorID);
    appendage.pushBack('.');
    appendLowerHex(appendage, deviceProperties.deviceID);

    gp::String result(cacheFilename);
    if (!endsWithIgnoreCase(result.view(), appendage.view()))
    {
        result.append(appendage.view());
    }

    return result;
}

gp::String VulkanGenericPlatform::getCompiledPSOCacheTopFolderPath() noexcept
{
    return gp::String("Saved/VulkanProgramBinaryCache");
}

gp::Vector<gp::String> VulkanGenericPlatform::getPSOCacheFilenames() noexcept
{
    return gp::Vector<gp::String>{};
}

bool VulkanGenericPlatform::supportsProfileChecks() noexcept
{
    const bool skipProfileCheck =
        gp::CommandLine::isInitialized() && gp::CommandLine::hasArgument("SkipVulkanProfileCheck");
    return (g_cvarVulkanUseProfileCheck.getValueOnAnyThread(true) != 0) && !skipProfileCheck;
}

gp::String VulkanGenericPlatform::getVulkanProfileNameForFeatureLevel(
    const FeatureLevel featureLevel, const bool raytracing
) noexcept
{
    gp::String profileName("VP_GP_Vulkan_");
    profileName.append(featureLevelToProfileSuffix(featureLevel));
    if (raytracing)
    {
        profileName.append("_RT");
    }
    return profileName;
}

}   // namespace gp::rhi::vulkan
