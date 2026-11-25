// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "debug/VulkanDebug.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/Memory.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <cstdio>
#include <cstring>

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanUniqueValidationMessages(
    "r.Vulkan.UniqueValidationMessages",
    1,
    "Filter out repeated validation errors with the same message ID when r.Vulkan.EnableValidation is non-zero.",
    gp::console::ConsoleVarFlags::None
);

#if GP_VULKAN_ENABLE_DRAW_MARKERS
gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanDebugMarkers(
    "r.Vulkan.DebugMarkers",
    4,
    "Controls Vulkan debug markers: 0=disable all, 1=object names, 2=command labels, 3=both, 4=auto (default).",
    gp::console::ConsoleVarFlags::ReadOnly | gp::console::ConsoleVarFlags::RenderThreadSafe
);
#endif

VulkanDebugReportCallback g_debugReportCallback{ nullptr };
gp::thread::Mutex g_seenCodesMutex{};
gp::Array<gp::Vector<gp::UInt32>, static_cast<gp::USize>(VulkanDebugMessageBucket::Count)> g_seenMessageCrcs{};

// Legacy VK_EXT_debug_marker trampoline targets (used only when VK_EXT_debug_utils is unavailable).
PFN_vkCmdDebugMarkerBeginEXT g_legacyCmdBeginDebugLabel{ nullptr };
PFN_vkCmdDebugMarkerEndEXT g_legacyCmdEndDebugLabel{ nullptr };
PFN_vkCmdDebugMarkerInsertEXT g_legacyCmdInsertDebugLabel{ nullptr };
PFN_vkDebugMarkerSetObjectNameEXT g_legacySetObjectName{ nullptr };

VKAPI_ATTR void VKAPI_CALL
    legacyCmdBeginDebugLabelWrapper(const VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* const labelInfo)
{
    if (g_legacyCmdBeginDebugLabel == nullptr || labelInfo == nullptr)
    {
        return;
    }

    VkDebugMarkerMarkerInfoEXT markerInfo{};
    zeroVulkanStruct(markerInfo, VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
    markerInfo.pMarkerName = labelInfo->pLabelName;
    gp::memory::copyMemory(markerInfo.color, labelInfo->color, sizeof(labelInfo->color));
    g_legacyCmdBeginDebugLabel(commandBuffer, &markerInfo);
}

VKAPI_ATTR void VKAPI_CALL legacyCmdEndDebugLabelWrapper(const VkCommandBuffer commandBuffer)
{
    if (g_legacyCmdEndDebugLabel != nullptr)
    {
        g_legacyCmdEndDebugLabel(commandBuffer);
    }
}

VKAPI_ATTR void VKAPI_CALL
    legacyCmdInsertDebugLabelWrapper(const VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* const labelInfo)
{
    if (g_legacyCmdInsertDebugLabel == nullptr || labelInfo == nullptr)
    {
        return;
    }

    VkDebugMarkerMarkerInfoEXT markerInfo{};
    zeroVulkanStruct(markerInfo, VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
    markerInfo.pMarkerName = labelInfo->pLabelName;
    gp::memory::copyMemory(markerInfo.color, labelInfo->color, sizeof(labelInfo->color));
    g_legacyCmdInsertDebugLabel(commandBuffer, &markerInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL
    legacySetObjectNameWrapper(const VkDevice device, const VkDebugUtilsObjectNameInfoEXT* const nameInfo)
{
    if (g_legacySetObjectName == nullptr || nameInfo == nullptr)
    {
        return VK_SUCCESS;
    }

    // Legacy VK_EXT_debug_marker only maps core VkObjectType values up to 25.
    if (static_cast<gp::UInt32>(nameInfo->objectType) <= 25u)
    {
        VkDebugMarkerObjectNameInfoEXT markerInfo{};
        zeroVulkanStruct(markerInfo, VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT);
        markerInfo.pObjectName = nameInfo->pObjectName;
        markerInfo.object = nameInfo->objectHandle;
        markerInfo.objectType = static_cast<VkDebugReportObjectTypeEXT>(nameInfo->objectType);
        return g_legacySetObjectName(device, &markerInfo);
    }

    return VK_SUCCESS;
}

[[nodiscard]] gp::UInt32 computeStringCrc32(const char* const text) noexcept
{
    if (text == nullptr)
    {
        return 0u;
    }

    gp::hashing::Crc32 hasher;
    hasher.update(reinterpret_cast<const gp::UInt8*>(text), std::strlen(text));
    return hasher.finalize();
}

void emitDebugReport(
    const VkDebugUtilsMessageSeverityFlagBitsEXT severity, const gp::String& message, void* const userData
) noexcept
{
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u)
    {
        GP_LOG_ERROR(LogVulkanRHI, "{}", message.data());
    }
    else
    {
        GP_LOG_WARNING(LogVulkanRHI, "{}", message.data());
    }

    if (g_debugReportCallback != nullptr)
    {
        g_debugReportCallback(severity, message.data(), userData);
    }
}

}   // namespace

void setVulkanDebugReportCallback(const VulkanDebugReportCallback callback) noexcept
{
    g_debugReportCallback = callback;
}

VulkanDebugReportCallback getVulkanDebugReportCallback() noexcept
{
    return g_debugReportCallback;
}

const char* classifyDebugSeverity(const VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept
{
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u)
    {
        return "Error";
    }
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0u)
    {
        return "Warning";
    }
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0u)
    {
        return "Info";
    }
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0u)
    {
        return "Verbose";
    }
    return "";
}

const char* classifyDebugMessageType(
    const VkDebugUtilsMessageTypeFlagsEXT messageType, VulkanDebugMessageBucket& outBucket
) noexcept
{
    outBucket = VulkanDebugMessageBucket::Count;

    if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0u)
    {
        if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0u)
        {
            outBucket = VulkanDebugMessageBucket::GeneralValidation;
            return "General/Validation";
        }

        outBucket = VulkanDebugMessageBucket::General;
        return "General";
    }

    if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0u)
    {
        if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u)
        {
            outBucket = VulkanDebugMessageBucket::PerfValidation;
            return "Perf/Validation";
        }

        outBucket = VulkanDebugMessageBucket::Validation;
        return "Validation";
    }

    if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u)
    {
        outBucket = VulkanDebugMessageBucket::Perf;
        return "Perf";
    }

    return "";
}

bool isFilteredValidationMessage(const VkDebugUtilsMessengerCallbackDataEXT* const callbackData) noexcept
{
    if (callbackData == nullptr)
    {
        return true;
    }

    if (callbackData->pMessageIdName != nullptr)
    {
        const char* const idName = callbackData->pMessageIdName;
        if (std::strcmp(idName, "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed") == 0 ||
            std::strcmp(idName, "Undefined-Value-ShaderOutputNotConsumed") == 0 ||
            std::strcmp(idName, "Undefined-Value-ShaderOutputNotConsumed-DynamicRendering") == 0)
        {
            return true;
        }

        if (std::strcmp(idName, "VUID-VkSwapchainCreateInfoKHR-imageExtent-01274") == 0 ||
            std::strcmp(idName, "VUID-VkSwapchainCreateInfoKHR-pNext-07781") == 0)
        {
            return true;
        }

        if (std::strcmp(idName, "UNASSIGNED-GPU-Assisted Validation Setup Error.") == 0)
        {
            return true;
        }

        if (std::strcmp(idName, "VUID-RuntimeSpirv-Fragment-06427") == 0)
        {
            return true;
        }

        if (std::strcmp(idName, "UNASSIGNED-BestPractices-vkCreateDevice-deprecated-extension") == 0)
        {
            return true;
        }
    }

    if (callbackData->pMessage != nullptr)
    {
        if (std::strstr(callbackData->pMessage, "SPV_GOOGLE_hlsl_functionality1") != nullptr ||
            std::strstr(callbackData->pMessage, "SPV_GOOGLE_user_type") != nullptr)
        {
            return true;
        }
    }

    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    const VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* const callbackData,
    void* const userData
) noexcept
{
    if (callbackData == nullptr)
    {
        return VK_FALSE;
    }

    const char* const severityStr = classifyDebugSeverity(messageSeverity);
    VulkanDebugMessageBucket bucket = VulkanDebugMessageBucket::Count;
    const char* const typeStr = classifyDebugMessageType(messageType, bucket);
    const char* const messageText = (callbackData->pMessage != nullptr) ? callbackData->pMessage : "";

    if (callbackData->pMessageIdName == nullptr)
    {
        char idBuffer[32]{};
        std::snprintf(idBuffer, sizeof(idBuffer), "%d", callbackData->messageIdNumber);

        gp::String formatted("*** [");
        formatted.append(severityStr);
        formatted.append(":");
        formatted.append(typeStr);
        formatted.append("(NULL)");
        formatted.append(idBuffer);
        formatted.append("] ");
        formatted.append(messageText);

        emitDebugReport(messageSeverity, formatted, userData);
        return VK_FALSE;
    }

    if (isFilteredValidationMessage(callbackData))
    {
        return VK_FALSE;
    }

    const bool uniqueMessages = (g_cvarVulkanUniqueValidationMessages.getValueOnAnyThread(true) != 0);
    bool shouldPrint = true;
    const gp::UInt32 messageCrc = uniqueMessages ? computeStringCrc32(callbackData->pMessageIdName) : 0u;

    if (uniqueMessages && bucket < VulkanDebugMessageBucket::Count)
    {
        const gp::thread::UniqueLock<gp::thread::Mutex> lock(g_seenCodesMutex);
        gp::Vector<gp::UInt32>& bucketCodes = g_seenMessageCrcs[static_cast<gp::USize>(bucket)];
        for (const gp::UInt32 seen: bucketCodes)
        {
            if (seen == messageCrc)
            {
                shouldPrint = false;
                break;
            }
        }
        if (shouldPrint)
        {
            bucketCodes.pushBack(messageCrc);
        }
    }

    if (shouldPrint)
    {
        gp::String formatted("*** [");
        formatted.append(severityStr);
        formatted.append(":");
        formatted.append(typeStr);
        formatted.append("(");
        formatted.append(callbackData->pMessageIdName);
        formatted.append(")] ");
        formatted.append(messageText);

        emitDebugReport(messageSeverity, formatted, userData);
    }

    return VK_FALSE;
}

void resetUniqueValidationMessageCache() noexcept
{
    const gp::thread::UniqueLock<gp::thread::Mutex> lock(g_seenCodesMutex);
    for (gp::USize i = 0u; i < g_seenMessageCrcs.size(); ++i)
    {
        g_seenMessageCrcs[i].clear();
    }
}

VkDebugUtilsMessengerCreateInfoEXT
    buildDebugMessengerCreateInfo(const gp::Int32 validationLevel, void* const userData) noexcept
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    zeroVulkanStruct(createInfo, VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);

    if (validationLevel >= 1)
    {
        createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType |=
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    }
    if (validationLevel >= 2)
    {
        createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    }
    if (validationLevel >= 3)
    {
        createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    }
    if (validationLevel >= 5)
    {
        createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    }

    createInfo.pfnUserCallback = &debugUtilsCallback;
    createInfo.pUserData = userData;
    return createInfo;
}

bool VulkanDebugMessenger::create(
    const VkInstance instance,
    const ActiveDebugLayerExtension activeDebugExtension,
    const gp::Int32 validationLevel,
    void* const userData
) noexcept
{
    if (m_messenger != VK_NULL_HANDLE || instance == VK_NULL_HANDLE ||
        activeDebugExtension != ActiveDebugLayerExtension::DebugUtilsExtension || validationLevel <= 0 ||
        dynamic_api::vkGetInstanceProcAddr == nullptr)
    {
        return false;
    }

    const PFN_vkCreateDebugUtilsMessengerEXT createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );
    if (createFn == nullptr)
    {
        return false;
    }

    const VkDebugUtilsMessengerCreateInfoEXT createInfo = buildDebugMessengerCreateInfo(validationLevel, userData);
    const VkResult result = createFn(instance, &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_messenger);
    if (result != VK_SUCCESS)
    {
        m_messenger = VK_NULL_HANDLE;
        GP_LOG_WARNING(
            LogVulkanRHI, "vkCreateDebugUtilsMessengerEXT failed with result {}.", static_cast<gp::Int32>(result)
        );
        return false;
    }

    return true;
}

void VulkanDebugMessenger::destroy(const VkInstance instance) noexcept
{
    if (m_messenger == VK_NULL_HANDLE)
    {
        return;
    }

    if (instance != VK_NULL_HANDLE && dynamic_api::vkGetInstanceProcAddr != nullptr)
    {
        const PFN_vkDestroyDebugUtilsMessengerEXT destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
        );
        if (destroyFn != nullptr)
        {
            destroyFn(instance, m_messenger, GP_VULKAN_CPU_ALLOCATOR);
        }
    }

    m_messenger = VK_NULL_HANDLE;
}

void VulkanDebugMarkers::initialize(
    const VkInstance instance,
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const bool supportsDebugUtilsExt,
    const bool useLegacyDebugMarkerExt,
    const bool hasExtToolingInfo,
    const bool gpuCrashDebugging,
    const bool renderDocFound
) noexcept
{
    reset();

#if GP_VULKAN_ENABLE_DRAW_MARKERS
    bool traceToolFound = gpuCrashDebugging || renderDocFound;

    if (supportsDebugUtilsExt || useLegacyDebugMarkerExt)
    {
        bool useLabel = gpuCrashDebugging;
        bool useName = false;

        if (hasExtToolingInfo && physicalDevice != VK_NULL_HANDLE &&
            dynamic_api::vkGetPhysicalDeviceToolPropertiesEXT != nullptr)
        {
            gp::UInt32 toolCount = 0u;
            vkGetPhysicalDeviceToolPropertiesEXT(physicalDevice, &toolCount, nullptr);
            if (toolCount > 0u)
            {
                gp::Vector<VkPhysicalDeviceToolPropertiesEXT> tools;
                tools.resize(toolCount);
                for (VkPhysicalDeviceToolPropertiesEXT& tool: tools)
                {
                    zeroVulkanStruct(tool, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES);
                }

                vkGetPhysicalDeviceToolPropertiesEXT(physicalDevice, &toolCount, tools.data());
                for (const VkPhysicalDeviceToolPropertiesEXT& tool: tools)
                {
                    if ((tool.purposes & VK_TOOL_PURPOSE_DEBUG_MARKERS_BIT_EXT) != 0u)
                    {
                        useName = true;
                        if ((tool.purposes & VK_TOOL_PURPOSE_TRACING_BIT_EXT) != 0u)
                        {
                            useLabel = true;
                        }
                    }
                    if ((tool.purposes & VK_TOOL_PURPOSE_TRACING_BIT_EXT) != 0u)
                    {
                        traceToolFound = true;
                    }
                    GP_LOG_INFO(
                        LogVulkanRHI,
                        "Tool '{}' version {} purposeFlags=0x{:x}",
                        tool.name,
                        tool.version,
                        static_cast<gp::UInt32>(tool.purposes)
                    );
                }
            }
        }

        const gp::Int32 markerSetting = gp::CommandLine::hasArgument("forcevulkandrawmarkers")
                                            ? 3
                                            : g_cvarVulkanDebugMarkers.getValueOnGameThread();
        if (markerSetting == 0)
        {
            useLabel = false;
            useName = false;
        }
        else if (markerSetting != 4)
        {
            useLabel = useLabel || ((markerSetting & 2) != 0);
            useName = useName || ((markerSetting & 1) != 0);
        }

        if (useLegacyDebugMarkerExt && device != VK_NULL_HANDLE && dynamic_api::vkGetDeviceProcAddr != nullptr)
        {
            if (useLabel)
            {
                g_legacyCmdBeginDebugLabel = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(
                    vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT")
                );
                g_legacyCmdEndDebugLabel =
                    reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
                g_legacyCmdInsertDebugLabel = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(
                    vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT")
                );
                if (g_legacyCmdBeginDebugLabel != nullptr && g_legacyCmdEndDebugLabel != nullptr)
                {
                    m_cmdBeginDebugLabel = &legacyCmdBeginDebugLabelWrapper;
                    m_cmdEndDebugLabel = &legacyCmdEndDebugLabelWrapper;
                }
                if (g_legacyCmdInsertDebugLabel != nullptr)
                {
                    m_cmdInsertLabel = &legacyCmdInsertDebugLabelWrapper;
                }
            }
            if (useName)
            {
                g_legacySetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(
                    vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT")
                );
                if (g_legacySetObjectName != nullptr)
                {
                    m_setDebugName = &legacySetObjectNameWrapper;
                }
            }
        }
        else if (
            !useLegacyDebugMarkerExt && supportsDebugUtilsExt && instance != VK_NULL_HANDLE &&
            dynamic_api::vkGetInstanceProcAddr != nullptr
        )
        {
            if (useLabel)
            {
                m_cmdBeginDebugLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                    vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT")
                );
                m_cmdEndDebugLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                    vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT")
                );
                m_cmdInsertLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
                    vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT")
                );
            }
            if (useName)
            {
                m_setDebugName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                    vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT")
                );
            }
        }

        GP_LOG_INFO(
            LogVulkanRHI,
            "Vulkan debug markers: object names {}, command labels {}.",
            supportsDebugNames() ? "enabled" : "disabled",
            supportsDebugLabels() ? "enabled" : "disabled"
        );
    }

    m_traceToolDetected = traceToolFound;
#else
    static_cast<void>(instance);
    static_cast<void>(physicalDevice);
    static_cast<void>(device);
    static_cast<void>(supportsDebugUtilsExt);
    static_cast<void>(useLegacyDebugMarkerExt);
    static_cast<void>(hasExtToolingInfo);
    static_cast<void>(gpuCrashDebugging);
    static_cast<void>(renderDocFound);
#endif
}

void VulkanDebugMarkers::initializeWithFunctions(
    const PFN_vkCmdBeginDebugUtilsLabelEXT beginLabelFn,
    const PFN_vkCmdEndDebugUtilsLabelEXT endLabelFn,
    const PFN_vkCmdInsertDebugUtilsLabelEXT insertLabelFn,
    const PFN_vkSetDebugUtilsObjectNameEXT setObjectNameFn
) noexcept
{
    m_cmdBeginDebugLabel = beginLabelFn;
    m_cmdEndDebugLabel = endLabelFn;
    m_cmdInsertLabel = insertLabelFn;
    m_setDebugName = setObjectNameFn;
}

void VulkanDebugMarkers::reset() noexcept
{
    m_cmdBeginDebugLabel = nullptr;
    m_cmdEndDebugLabel = nullptr;
    m_cmdInsertLabel = nullptr;
    m_setDebugName = nullptr;
    m_traceToolDetected = false;
}

void VulkanDebugMarkers::setObjectName(
    const VkDevice device, const VkObjectType objectType, const gp::UInt64 objectHandle, const char* const name
) const noexcept
{
    if (m_setDebugName == nullptr || device == VK_NULL_HANDLE || objectHandle == 0ull || name == nullptr)
    {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT info{};
    zeroVulkanStruct(info, VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
    info.objectType = objectType;
    info.objectHandle = objectHandle;
    info.pObjectName = name;
    m_setDebugName(device, &info);
}

void VulkanDebugMarkers::cmdBeginDebugLabel(
    const VkCommandBuffer commandBuffer,
    const char* const labelName,
    const gp::Float32 red,
    const gp::Float32 green,
    const gp::Float32 blue,
    const gp::Float32 alpha
) const noexcept
{
    if (m_cmdBeginDebugLabel == nullptr || commandBuffer == VK_NULL_HANDLE || labelName == nullptr)
    {
        return;
    }

    VkDebugUtilsLabelEXT labelInfo{};
    zeroVulkanStruct(labelInfo, VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
    labelInfo.pLabelName = labelName;
    labelInfo.color[0] = red;
    labelInfo.color[1] = green;
    labelInfo.color[2] = blue;
    labelInfo.color[3] = alpha;
    m_cmdBeginDebugLabel(commandBuffer, &labelInfo);
}

void VulkanDebugMarkers::cmdEndDebugLabel(const VkCommandBuffer commandBuffer) const noexcept
{
    if (m_cmdEndDebugLabel == nullptr || commandBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    m_cmdEndDebugLabel(commandBuffer);
}

void VulkanDebugMarkers::cmdInsertDebugLabel(
    const VkCommandBuffer commandBuffer,
    const char* const labelName,
    const gp::Float32 red,
    const gp::Float32 green,
    const gp::Float32 blue,
    const gp::Float32 alpha
) const noexcept
{
    if (m_cmdInsertLabel == nullptr || commandBuffer == VK_NULL_HANDLE || labelName == nullptr)
    {
        return;
    }

    VkDebugUtilsLabelEXT labelInfo{};
    zeroVulkanStruct(labelInfo, VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
    labelInfo.pLabelName = labelName;
    labelInfo.color[0] = red;
    labelInfo.color[1] = green;
    labelInfo.color[2] = blue;
    labelInfo.color[3] = alpha;
    m_cmdInsertLabel(commandBuffer, &labelInfo);
}

}   // namespace gp::rhi::vulkan
