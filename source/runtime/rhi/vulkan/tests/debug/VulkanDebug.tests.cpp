// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "debug/VulkanDebug.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstring>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

struct CapturedReport final
{
    VkDebugUtilsMessageSeverityFlagBitsEXT severity{ VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT };
    gp::String message{};
};

struct ReportCaptureContext final
{
    gp::Vector<CapturedReport> reports{};
};

void captureReportCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT severity, const char* const formattedMessage, void* const userData
) noexcept
{
    if (userData == nullptr)
    {
        return;
    }
    ReportCaptureContext* const ctx = static_cast<ReportCaptureContext*>(userData);
    CapturedReport entry;
    entry.severity = severity;
    entry.message = (formattedMessage != nullptr) ? formattedMessage : "";
    ctx->reports.pushBack(entry);
}

struct MockMarkerState final
{
    gp::Int32 beginCount{ 0 };
    gp::Int32 endCount{ 0 };
    gp::Int32 insertCount{ 0 };
    gp::Int32 setNameCount{ 0 };
    gp::String lastLabel{};
    gp::String lastObjectName{};
    VkObjectType lastObjectType{ VK_OBJECT_TYPE_UNKNOWN };
    gp::UInt64 lastObjectHandle{ 0u };
    gp::Float32 lastColor[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
};

MockMarkerState g_mockMarkers{};

VKAPI_ATTR void VKAPI_CALL
    mockCmdBeginDebugLabel(const VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* const labelInfo)
{
    static_cast<void>(commandBuffer);
    ++g_mockMarkers.beginCount;
    if (labelInfo != nullptr)
    {
        g_mockMarkers.lastLabel = (labelInfo->pLabelName != nullptr) ? labelInfo->pLabelName : "";
        for (gp::USize i = 0u; i < 4u; ++i)
        {
            g_mockMarkers.lastColor[i] = labelInfo->color[i];
        }
    }
}

VKAPI_ATTR void VKAPI_CALL mockCmdEndDebugLabel(const VkCommandBuffer commandBuffer)
{
    static_cast<void>(commandBuffer);
    ++g_mockMarkers.endCount;
}

VKAPI_ATTR void VKAPI_CALL
    mockCmdInsertDebugLabel(const VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* const labelInfo)
{
    static_cast<void>(commandBuffer);
    ++g_mockMarkers.insertCount;
    if (labelInfo != nullptr)
    {
        g_mockMarkers.lastLabel = (labelInfo->pLabelName != nullptr) ? labelInfo->pLabelName : "";
    }
}

VKAPI_ATTR VkResult VKAPI_CALL
    mockSetObjectName(const VkDevice device, const VkDebugUtilsObjectNameInfoEXT* const nameInfo)
{
    static_cast<void>(device);
    ++g_mockMarkers.setNameCount;
    if (nameInfo != nullptr)
    {
        g_mockMarkers.lastObjectType = nameInfo->objectType;
        g_mockMarkers.lastObjectHandle = nameInfo->objectHandle;
        g_mockMarkers.lastObjectName = (nameInfo->pObjectName != nullptr) ? nameInfo->pObjectName : "";
    }
    return VK_SUCCESS;
}

}   // namespace

TEST(VulkanDebugTest, ClassifySeverityAndMessageType)
{
    EXPECT_STREQ(classifyDebugSeverity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT), "Error");
    EXPECT_STREQ(classifyDebugSeverity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT), "Warning");
    EXPECT_STREQ(classifyDebugSeverity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT), "Info");
    EXPECT_STREQ(classifyDebugSeverity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT), "Verbose");
    EXPECT_STREQ(classifyDebugSeverity(static_cast<VkDebugUtilsMessageSeverityFlagBitsEXT>(0)), "");

    VulkanDebugMessageBucket bucket = VulkanDebugMessageBucket::Count;

    EXPECT_STREQ(classifyDebugMessageType(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, bucket), "General");
    EXPECT_EQ(bucket, VulkanDebugMessageBucket::General);

    EXPECT_STREQ(
        classifyDebugMessageType(
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, bucket
        ),
        "General/Validation"
    );
    EXPECT_EQ(bucket, VulkanDebugMessageBucket::GeneralValidation);

    EXPECT_STREQ(classifyDebugMessageType(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, bucket), "Validation");
    EXPECT_EQ(bucket, VulkanDebugMessageBucket::Validation);

    EXPECT_STREQ(
        classifyDebugMessageType(
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, bucket
        ),
        "Perf/Validation"
    );
    EXPECT_EQ(bucket, VulkanDebugMessageBucket::PerfValidation);

    EXPECT_STREQ(classifyDebugMessageType(VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, bucket), "Perf");
    EXPECT_EQ(bucket, VulkanDebugMessageBucket::Perf);

    // Unclassified message type leaves bucket at Count and returns empty string.
    EXPECT_STREQ(classifyDebugMessageType(0u, bucket), "");
    EXPECT_EQ(bucket, VulkanDebugMessageBucket::Count);
}

TEST(VulkanDebugTest, FilteredValidationMessagesRecognizesKnownFalsePositives)
{
    EXPECT_TRUE(isFilteredValidationMessage(nullptr));

    const char* const knownFilteredIds[] = { "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed",
                                             "Undefined-Value-ShaderOutputNotConsumed",
                                             "Undefined-Value-ShaderOutputNotConsumed-DynamicRendering",
                                             "VUID-VkSwapchainCreateInfoKHR-imageExtent-01274",
                                             "VUID-VkSwapchainCreateInfoKHR-pNext-07781",
                                             "UNASSIGNED-GPU-Assisted Validation Setup Error.",
                                             "VUID-RuntimeSpirv-Fragment-06427",
                                             "UNASSIGNED-BestPractices-vkCreateDevice-deprecated-extension" };

    for (const char* const id: knownFilteredIds)
    {
        VkDebugUtilsMessengerCallbackDataEXT data = zeroVulkanStruct<VkDebugUtilsMessengerCallbackDataEXT>(
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT
        );
        data.pMessageIdName = id;
        EXPECT_TRUE(isFilteredValidationMessage(&data)) << "Expected filter match for: " << id;
    }

    VkDebugUtilsMessengerCallbackDataEXT hlslFuncMsg = zeroVulkanStruct<VkDebugUtilsMessengerCallbackDataEXT>(
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT
    );
    hlslFuncMsg.pMessageIdName = "VUID-SomeOtherId";
    hlslFuncMsg.pMessage = "Shader uses SPV_GOOGLE_hlsl_functionality1 extension";
    EXPECT_TRUE(isFilteredValidationMessage(&hlslFuncMsg));

    VkDebugUtilsMessengerCallbackDataEXT userTypeMsg = zeroVulkanStruct<VkDebugUtilsMessengerCallbackDataEXT>(
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT
    );
    userTypeMsg.pMessageIdName = "VUID-SomeOtherId";
    userTypeMsg.pMessage = "Shader uses SPV_GOOGLE_user_type extension";
    EXPECT_TRUE(isFilteredValidationMessage(&userTypeMsg));

    VkDebugUtilsMessengerCallbackDataEXT unfiltered = zeroVulkanStruct<VkDebugUtilsMessengerCallbackDataEXT>(
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT
    );
    unfiltered.pMessageIdName = "VUID-vkCmdDraw-None-02699";
    unfiltered.pMessage = "Descriptor set not bound";
    EXPECT_FALSE(isFilteredValidationMessage(&unfiltered));
}

TEST(VulkanDebugTest, DebugUtilsCallbackFormatsDeduplicatesAndRespectsFilters)
{
    resetUniqueValidationMessageCache();

    ReportCaptureContext captureCtx;
    setVulkanDebugReportCallback(&captureReportCallback);
    EXPECT_EQ(getVulkanDebugReportCallback(), &captureReportCallback);

    // Null callback data returns VK_FALSE without invoking the sink.
    EXPECT_EQ(
        debugUtilsCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            nullptr,
            &captureCtx
        ),
        VK_FALSE
    );
    EXPECT_EQ(captureCtx.reports.size(), 0u);

    // Filtered VUID is suppressed completely.
    VkDebugUtilsMessengerCallbackDataEXT filtered = zeroVulkanStruct<VkDebugUtilsMessengerCallbackDataEXT>(
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT
    );
    filtered.pMessageIdName = "VUID-VkSwapchainCreateInfoKHR-imageExtent-01274";
    filtered.pMessage = "Filtered benign warning";
    EXPECT_EQ(
        debugUtilsCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            &filtered,
            &captureCtx
        ),
        VK_FALSE
    );
    EXPECT_EQ(captureCtx.reports.size(), 0u);

    // Unfiltered message is emitted once; duplicate in same bucket is suppressed when unique filter is on.
    VkDebugUtilsMessengerCallbackDataEXT validMsg = zeroVulkanStruct<VkDebugUtilsMessengerCallbackDataEXT>(
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT
    );
    validMsg.pMessageIdName = "VUID-Test-Custom-00001";
    validMsg.messageIdNumber = 42;
    validMsg.pMessage = "Sample validation diagnostic";

    EXPECT_EQ(
        debugUtilsCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            &validMsg,
            &captureCtx
        ),
        VK_FALSE
    );
    ASSERT_EQ(captureCtx.reports.size(), 1u);
    EXPECT_EQ(captureCtx.reports[0].severity, VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT);
    EXPECT_NE(std::strstr(captureCtx.reports[0].message.data(), "VUID-Test-Custom-00001"), nullptr);

    // Second identical message in the same bucket is deduplicated.
    EXPECT_EQ(
        debugUtilsCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            &validMsg,
            &captureCtx
        ),
        VK_FALSE
    );
    EXPECT_EQ(captureCtx.reports.size(), 1u);

    // Same message ID in a different bucket (Perf) is emitted.
    EXPECT_EQ(
        debugUtilsCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            &validMsg,
            &captureCtx
        ),
        VK_FALSE
    );
    EXPECT_EQ(captureCtx.reports.size(), 2u);

    // Disabling r.Vulkan.UniqueValidationMessages allows repeated messages through.
    gp::console::IConsoleVariable* const uniqueCvar =
        gp::console::IConsoleManager::get().findConsoleVariable("r.Vulkan.UniqueValidationMessages");
    if (uniqueCvar != nullptr)
    {
        const gp::Int32 previousValue = uniqueCvar->getInt();
        static_cast<void>(uniqueCvar->set(0));

        EXPECT_EQ(
            debugUtilsCallback(
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                &validMsg,
                &captureCtx
            ),
            VK_FALSE
        );
        EXPECT_EQ(captureCtx.reports.size(), 3u);

        static_cast<void>(uniqueCvar->set(previousValue));
    }

    // Null pMessageIdName path formats cleanly without crashing.
    VkDebugUtilsMessengerCallbackDataEXT nullIdMsg = zeroVulkanStruct<VkDebugUtilsMessengerCallbackDataEXT>(
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT
    );
    nullIdMsg.pMessageIdName = nullptr;
    nullIdMsg.messageIdNumber = 99;
    nullIdMsg.pMessage = "Diagnostic without VUID string";
    EXPECT_EQ(
        debugUtilsCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
            &nullIdMsg,
            &captureCtx
        ),
        VK_FALSE
    );

    setVulkanDebugReportCallback(nullptr);
    EXPECT_EQ(getVulkanDebugReportCallback(), nullptr);
    resetUniqueValidationMessageCache();
}

TEST(VulkanDebugTest, BuildDebugMessengerCreateInfoScalesWithValidationLevel)
{
    const VkDebugUtilsMessengerCreateInfoEXT level0 = buildDebugMessengerCreateInfo(0);
    EXPECT_EQ(level0.sType, VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
    EXPECT_EQ(level0.messageSeverity, 0u);
    EXPECT_EQ(level0.messageType, 0u);
    EXPECT_EQ(level0.pfnUserCallback, &debugUtilsCallback);

    const VkDebugUtilsMessengerCreateInfoEXT level1 = buildDebugMessengerCreateInfo(1);
    EXPECT_EQ(level1.messageSeverity, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
    EXPECT_EQ(
        level1.messageType,
        static_cast<VkDebugUtilsMessageTypeFlagsEXT>(
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        )
    );

    const VkDebugUtilsMessengerCreateInfoEXT level2 = buildDebugMessengerCreateInfo(2);
    EXPECT_NE(level2.messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, 0u);
    EXPECT_EQ(level2.messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, 0u);

    const VkDebugUtilsMessengerCreateInfoEXT level3 = buildDebugMessengerCreateInfo(3);
    EXPECT_NE(level3.messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, 0u);
    EXPECT_NE(level3.messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, 0u);
    EXPECT_EQ(level3.messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, 0u);

    const VkDebugUtilsMessengerCreateInfoEXT level5 = buildDebugMessengerCreateInfo(5);
    EXPECT_NE(level5.messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, 0u);
}

TEST(VulkanDebugTest, DebugMessengerSafelyNoOpsWithoutValidInstanceOrExtension)
{
    VulkanDebugMessenger messenger;
    EXPECT_FALSE(messenger.isValid());
    EXPECT_EQ(messenger.getHandle(), VK_NULL_HANDLE);

    EXPECT_FALSE(messenger.create(VK_NULL_HANDLE, ActiveDebugLayerExtension::DebugUtilsExtension, 2));
    EXPECT_FALSE(messenger.create(VK_NULL_HANDLE, ActiveDebugLayerExtension::None, 2));
    EXPECT_FALSE(messenger.create(VK_NULL_HANDLE, ActiveDebugLayerExtension::DebugUtilsExtension, 0));
    EXPECT_FALSE(messenger.isValid());

    messenger.destroy(VK_NULL_HANDLE);
    EXPECT_FALSE(messenger.isValid());
}

TEST(VulkanDebugTest, DebugMarkersAndScopedLabelDispatch)
{
    g_mockMarkers = MockMarkerState{};

    VulkanDebugMarkers markers;
    EXPECT_FALSE(markers.supportsDebugNames());
    EXPECT_FALSE(markers.supportsDebugLabels());
    EXPECT_FALSE(markers.isTraceToolDetected());

    // Calling helpers before initialization must safely no-op.
    const VkDevice fakeDevice = reinterpret_cast<VkDevice>(static_cast<gp::UIntPtr>(0x1000u));
    const VkCommandBuffer fakeCmdBuf = reinterpret_cast<VkCommandBuffer>(static_cast<gp::UIntPtr>(0x2000u));
    const VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<gp::UIntPtr>(0x3000u));

    markers.setObjectName(fakeDevice, VK_OBJECT_TYPE_IMAGE, fakeImage, "IgnoredBeforeInit");
    markers.cmdBeginDebugLabel(fakeCmdBuf, "Ignored");
    markers.cmdInsertDebugLabel(fakeCmdBuf, "Ignored");
    markers.cmdEndDebugLabel(fakeCmdBuf);
    EXPECT_EQ(g_mockMarkers.setNameCount, 0);
    EXPECT_EQ(g_mockMarkers.beginCount, 0);

    markers.initializeWithFunctions(
        &mockCmdBeginDebugLabel, &mockCmdEndDebugLabel, &mockCmdInsertDebugLabel, &mockSetObjectName
    );
    EXPECT_TRUE(markers.supportsDebugNames());
    EXPECT_TRUE(markers.supportsDebugLabels());

    // Typed setObjectName overload.
    markers.setObjectName(fakeDevice, VK_OBJECT_TYPE_IMAGE, fakeImage, "SceneDepthTexture");
    EXPECT_EQ(g_mockMarkers.setNameCount, 1);
    EXPECT_EQ(g_mockMarkers.lastObjectType, VK_OBJECT_TYPE_IMAGE);
    EXPECT_EQ(g_mockMarkers.lastObjectHandle, static_cast<gp::UInt64>(0x3000u));
    EXPECT_STREQ(g_mockMarkers.lastObjectName.data(), "SceneDepthTexture");

    // Null device or null name safely ignored.
    markers.setObjectName(VK_NULL_HANDLE, VK_OBJECT_TYPE_IMAGE, fakeImage, "NullDevice");
    markers.setObjectName(fakeDevice, VK_OBJECT_TYPE_IMAGE, fakeImage, nullptr);
    EXPECT_EQ(g_mockMarkers.setNameCount, 1);

    // Scoped RAII label opens on construction and closes on destruction.
    {
        VulkanScopedDebugLabel scope(markers, fakeCmdBuf, "ShadowPass", 0.25f, 0.5f, 0.75f, 1.0f);
        EXPECT_EQ(g_mockMarkers.beginCount, 1);
        EXPECT_EQ(g_mockMarkers.endCount, 0);
        EXPECT_STREQ(g_mockMarkers.lastLabel.data(), "ShadowPass");
        EXPECT_FLOAT_EQ(g_mockMarkers.lastColor[0], 0.25f);
        EXPECT_FLOAT_EQ(g_mockMarkers.lastColor[1], 0.5f);
        EXPECT_FLOAT_EQ(g_mockMarkers.lastColor[2], 0.75f);
        EXPECT_FLOAT_EQ(g_mockMarkers.lastColor[3], 1.0f);

        markers.cmdInsertDebugLabel(fakeCmdBuf, "Cascade0");
        EXPECT_EQ(g_mockMarkers.insertCount, 1);
        EXPECT_STREQ(g_mockMarkers.lastLabel.data(), "Cascade0");
    }
    EXPECT_EQ(g_mockMarkers.endCount, 1);

    markers.reset();
    EXPECT_FALSE(markers.supportsDebugNames());
    EXPECT_FALSE(markers.supportsDebugLabels());

    // Verify initialize() records trace tool detection cleanly even without a live Vulkan device.
    markers.initialize(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, true, false, false, false, true);
    EXPECT_TRUE(markers.isTraceToolDetected());
    EXPECT_FALSE(markers.supportsDebugNames());
    EXPECT_FALSE(markers.supportsDebugLabels());

    markers.initialize(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, true, false, false, true, false);
    EXPECT_TRUE(markers.isTraceToolDetected());

    markers.initialize(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, false, false, false, false, false);
    EXPECT_FALSE(markers.isTraceToolDetected());
}

}   // namespace gp::rhi::vulkan::tests
