// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "utils/VulkanResult.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

gp::UInt32 g_capturedErrorCount = 0u;
VulkanResultErrorContext g_lastCapturedError{};

void testErrorHandler(const VulkanResultErrorContext& context) noexcept
{
    ++g_capturedErrorCount;
    g_lastCapturedError = context;
}

struct ScopedErrorHandler final
{
    VulkanResultErrorHandler m_previous{ nullptr };

    explicit ScopedErrorHandler(const VulkanResultErrorHandler handler) noexcept
        : m_previous(getVulkanResultErrorHandler())
    {
        g_capturedErrorCount = 0u;
        g_lastCapturedError = VulkanResultErrorContext{};
        setVulkanResultErrorHandler(handler);
    }

    ~ScopedErrorHandler() noexcept
    {
        setVulkanResultErrorHandler(m_previous);
    }
};

VkResult returnVkResultAndCount(const VkResult value, gp::UInt32& outCallCount) noexcept
{
    ++outCallCount;
    return value;
}

}   // namespace

TEST(VulkanResultTest, ResultToStringMapsAllKnownCodesAndFallbacks)
{
    EXPECT_EQ(vulkanResultToString(VK_SUCCESS).view(), "VK_SUCCESS");
    EXPECT_EQ(vulkanResultToString(VK_NOT_READY).view(), "VK_NOT_READY");
    EXPECT_EQ(vulkanResultToString(VK_TIMEOUT).view(), "VK_TIMEOUT");
    EXPECT_EQ(vulkanResultToString(VK_EVENT_SET).view(), "VK_EVENT_SET");
    EXPECT_EQ(vulkanResultToString(VK_EVENT_RESET).view(), "VK_EVENT_RESET");
    EXPECT_EQ(vulkanResultToString(VK_INCOMPLETE).view(), "VK_INCOMPLETE");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_OUT_OF_HOST_MEMORY).view(), "VK_ERROR_OUT_OF_HOST_MEMORY");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_OUT_OF_DEVICE_MEMORY).view(), "VK_ERROR_OUT_OF_DEVICE_MEMORY");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_INITIALIZATION_FAILED).view(), "VK_ERROR_INITIALIZATION_FAILED");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_DEVICE_LOST).view(), "VK_ERROR_DEVICE_LOST");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_MEMORY_MAP_FAILED).view(), "VK_ERROR_MEMORY_MAP_FAILED");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_LAYER_NOT_PRESENT).view(), "VK_ERROR_LAYER_NOT_PRESENT");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_EXTENSION_NOT_PRESENT).view(), "VK_ERROR_EXTENSION_NOT_PRESENT");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_FEATURE_NOT_PRESENT).view(), "VK_ERROR_FEATURE_NOT_PRESENT");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_INCOMPATIBLE_DRIVER).view(), "VK_ERROR_INCOMPATIBLE_DRIVER");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_TOO_MANY_OBJECTS).view(), "VK_ERROR_TOO_MANY_OBJECTS");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_FORMAT_NOT_SUPPORTED).view(), "VK_ERROR_FORMAT_NOT_SUPPORTED");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_FRAGMENTED_POOL).view(), "VK_ERROR_FRAGMENTED_POOL");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_UNKNOWN).view(), "VK_ERROR_UNKNOWN");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_OUT_OF_POOL_MEMORY).view(), "VK_ERROR_OUT_OF_POOL_MEMORY");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_INVALID_EXTERNAL_HANDLE).view(), "VK_ERROR_INVALID_EXTERNAL_HANDLE");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_FRAGMENTATION).view(), "VK_ERROR_FRAGMENTATION");
    EXPECT_EQ(
        vulkanResultToString(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS).view(), "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"
    );
    EXPECT_EQ(vulkanResultToString(VK_PIPELINE_COMPILE_REQUIRED).view(), "VK_PIPELINE_COMPILE_REQUIRED");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_SURFACE_LOST_KHR).view(), "VK_ERROR_SURFACE_LOST_KHR");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR).view(), "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR");
    EXPECT_EQ(vulkanResultToString(VK_SUBOPTIMAL_KHR).view(), "VK_SUBOPTIMAL_KHR");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_OUT_OF_DATE_KHR).view(), "VK_ERROR_OUT_OF_DATE_KHR");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR).view(), "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_VALIDATION_FAILED_EXT).view(), "VK_ERROR_VALIDATION_FAILED_EXT");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_INVALID_SHADER_NV).view(), "VK_ERROR_INVALID_SHADER_NV");
    EXPECT_EQ(vulkanResultToString(VK_ERROR_NOT_PERMITTED_EXT).view(), "VK_ERROR_NOT_PERMITTED_EXT");

    // Unrecognized positive and negative codes fall back to unsigned decimal GP_VK_TYPE_TO_STRING.
    EXPECT_EQ(vulkanResultToString(static_cast<VkResult>(999999)).view(), "999999");
    EXPECT_EQ(vulkanResultToString(static_cast<VkResult>(-999)).view(), "4294966297");
}

TEST(VulkanResultTest, ClassificationHelpersAndMessageFormatting)
{
    static_assert(isVulkanOutOfMemoryResult(VK_ERROR_OUT_OF_HOST_MEMORY));
    static_assert(isVulkanOutOfMemoryResult(VK_ERROR_OUT_OF_DEVICE_MEMORY));
    static_assert(!isVulkanOutOfMemoryResult(VK_ERROR_DEVICE_LOST));
    static_assert(!isVulkanOutOfMemoryResult(VK_SUCCESS));

    static_assert(isVulkanDeviceLostResult(VK_ERROR_DEVICE_LOST));
    static_assert(!isVulkanDeviceLostResult(VK_ERROR_OUT_OF_DEVICE_MEMORY));
    static_assert(!isVulkanDeviceLostResult(VK_SUCCESS));

    EXPECT_TRUE(isVulkanOutOfMemoryResult(VK_ERROR_OUT_OF_HOST_MEMORY));
    EXPECT_TRUE(isVulkanOutOfMemoryResult(VK_ERROR_OUT_OF_DEVICE_MEMORY));
    EXPECT_FALSE(isVulkanOutOfMemoryResult(VK_ERROR_DEVICE_LOST));
    EXPECT_TRUE(isVulkanDeviceLostResult(VK_ERROR_DEVICE_LOST));

    const gp::String msg = formatVulkanResultMessage(VK_ERROR_DEVICE_LOST, "vkQueueSubmit", "VulkanQueue.cpp", 42u);
    EXPECT_EQ(
        msg.view(), "vkQueueSubmit failed, VkResult=-4\n at VulkanQueue.cpp:42 \n with error VK_ERROR_DEVICE_LOST"
    );

    const gp::String positiveMsg =
        formatVulkanResultMessage(VK_SUBOPTIMAL_KHR, "vkAcquireNextImageKHR", "SwapChain.cpp", 7u);
    EXPECT_EQ(
        positiveMsg.view(),
        "vkAcquireNextImageKHR failed, VkResult=1000001003\n at SwapChain.cpp:7 \n with error VK_SUBOPTIMAL_KHR"
    );

    const gp::String zeroMsg = formatVulkanResultMessage(VK_SUCCESS, "vkNoOp", "Test.cpp", 0xFFFFFFFFu);
    EXPECT_EQ(zeroMsg.view(), "vkNoOp failed, VkResult=0\n at Test.cpp:4294967295 \n with error VK_SUCCESS");

    const gp::String minIntMsg =
        formatVulkanResultMessage(static_cast<VkResult>(static_cast<gp::Int32>(-2147483647 - 1)), "vkMin", "F.cpp", 1u);
    EXPECT_EQ(minIntMsg.view(), "vkMin failed, VkResult=-2147483648\n at F.cpp:1 \n with error 2147483648");

    const gp::String nullSafeMsg = formatVulkanResultMessage(VK_TIMEOUT, nullptr, nullptr, 0u);
    EXPECT_EQ(nullSafeMsg.view(), "<unknown> failed, VkResult=2\n at <unknown>:0 \n with error VK_TIMEOUT");
}

TEST(VulkanResultTest, VerifyMacrosDirectCallsAndCustomErrorHandler)
{
    {
        const ScopedErrorHandler handlerGuard(&testErrorHandler);
        EXPECT_EQ(getVulkanResultErrorHandler(), &testErrorHandler);

        // Single-evaluation check and if/else macro hygiene on pass and fail paths.
        gp::UInt32 evalCount = 0u;
        if (true)
        {
            GP_VERIFY_VULKAN_RESULT(returnVkResultAndCount(VK_SUCCESS, evalCount));
        }
        else
        {
            FAIL();
        }
        EXPECT_EQ(evalCount, 1u);
        EXPECT_EQ(g_capturedErrorCount, 0u);

        if (true)
        {
            GP_VERIFY_VULKAN_RESULT_EXPANDED(returnVkResultAndCount(VK_SUBOPTIMAL_KHR, evalCount));
        }
        else
        {
            FAIL();
        }
        EXPECT_EQ(evalCount, 2u);
        EXPECT_EQ(g_capturedErrorCount, 0u);

        GP_VERIFY_VULKAN_RESULT_EXPANDED(returnVkResultAndCount(VK_INCOMPLETE, evalCount));
        EXPECT_EQ(evalCount, 3u);
        EXPECT_EQ(g_capturedErrorCount, 0u);

        // Strict macro triggers on positive non-success, evaluating expression exactly once.
        GP_VERIFY_VULKAN_RESULT(returnVkResultAndCount(VK_SUBOPTIMAL_KHR, evalCount));
        EXPECT_EQ(evalCount, 4u);
        EXPECT_EQ(g_capturedErrorCount, 1u);
        EXPECT_EQ(g_lastCapturedError.result, VK_SUBOPTIMAL_KHR);
        EXPECT_STREQ(g_lastCapturedError.vkFunction, "returnVkResultAndCount(VK_SUBOPTIMAL_KHR, evalCount)");
        EXPECT_NE(g_lastCapturedError.filename, nullptr);
        EXPECT_EQ(g_lastCapturedError.errorString.view(), "VK_SUBOPTIMAL_KHR");
        EXPECT_FALSE(g_lastCapturedError.formattedMessage.isEmpty());
        EXPECT_FALSE(g_lastCapturedError.dumpMemory);
        EXPECT_FALSE(g_lastCapturedError.isGPUCrashed);
        EXPECT_GT(g_lastCapturedError.line, 0u);

        // Negative out-of-device-memory triggers EXPANDED with dumpMemory flag set.
        GP_VERIFY_VULKAN_RESULT_EXPANDED(returnVkResultAndCount(VK_ERROR_OUT_OF_DEVICE_MEMORY, evalCount));
        EXPECT_EQ(evalCount, 5u);
        EXPECT_EQ(g_capturedErrorCount, 2u);
        EXPECT_EQ(g_lastCapturedError.result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
        EXPECT_TRUE(g_lastCapturedError.dumpMemory);
        EXPECT_FALSE(g_lastCapturedError.isGPUCrashed);

        // Direct verifyVulkanResult call with VK_ERROR_OUT_OF_HOST_MEMORY and nullptr function/file.
        verifyVulkanResult(VK_ERROR_OUT_OF_HOST_MEMORY, nullptr, nullptr, 123u);
        EXPECT_EQ(g_capturedErrorCount, 3u);
        EXPECT_EQ(g_lastCapturedError.result, VK_ERROR_OUT_OF_HOST_MEMORY);
        EXPECT_STREQ(g_lastCapturedError.vkFunction, "<unknown>");
        EXPECT_STREQ(g_lastCapturedError.filename, "<unknown>");
        EXPECT_EQ(g_lastCapturedError.line, 123u);
        EXPECT_TRUE(g_lastCapturedError.dumpMemory);
        EXPECT_FALSE(g_lastCapturedError.isGPUCrashed);
        EXPECT_EQ(
            g_lastCapturedError.formattedMessage.view(),
            "<unknown> failed, VkResult=-1\n at <unknown>:123 \n with error VK_ERROR_OUT_OF_HOST_MEMORY"
        );

        // Device lost triggers isGPUCrashed flag.
        GP_VERIFY_VULKAN_RESULT(returnVkResultAndCount(VK_ERROR_DEVICE_LOST, evalCount));
        EXPECT_EQ(evalCount, 6u);
        EXPECT_EQ(g_capturedErrorCount, 4u);
        EXPECT_EQ(g_lastCapturedError.result, VK_ERROR_DEVICE_LOST);
        EXPECT_FALSE(g_lastCapturedError.dumpMemory);
        EXPECT_TRUE(g_lastCapturedError.isGPUCrashed);
    }

    // Exiting ScopedErrorHandler restores default nullptr handler.
    setVulkanResultErrorHandler(nullptr);
    EXPECT_EQ(getVulkanResultErrorHandler(), nullptr);
}

}   // namespace gp::rhi::vulkan::tests
