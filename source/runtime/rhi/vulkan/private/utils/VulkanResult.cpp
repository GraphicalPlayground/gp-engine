// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "utils/VulkanResult.hpp"
#include "logging/LogMacros.hpp"
#include "memory/MemoryOrder.hpp"
#include "platform/VulkanPlatform.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::thread::Atomic<VulkanResultErrorHandler> g_errorHandler{ nullptr };
gp::thread::Mutex g_crashMutex;

void appendSignedInt32(gp::String& outStr, const gp::Int32 value) noexcept
{
    gp::UInt32 magnitude = 0u;
    if (value < 0)
    {
        outStr.pushBack('-');
        magnitude = static_cast<gp::UInt32>(-(static_cast<gp::Int64>(value)));
    }
    else
    {
        magnitude = static_cast<gp::UInt32>(value);
    }

    const gp::String digits = formatVulkanValue(magnitude);
    outStr.append(digits.view());
}

[[nodiscard]] gp::String formatVulkanResultMessageWithError(
    const VkResult result,
    const char* const vkFunction,
    const char* const filename,
    const gp::UInt32 line,
    const gp::StringView errorString
) noexcept
{
    gp::String message;
    message.append((vkFunction != nullptr) ? vkFunction : "<unknown>");
    message.append(" failed, VkResult=");
    appendSignedInt32(message, static_cast<gp::Int32>(result));
    message.append("\n at ");
    message.append((filename != nullptr) ? filename : "<unknown>");
    message.pushBack(':');
    message.append(formatVulkanValue(line).view());
    message.append(" \n with error ");
    message.append(errorString);
    return message;
}

}   // namespace

gp::String vulkanResultToString(const VkResult result) noexcept
{
    switch (result)
    {
#define GP_VK_RESULT_CASE(EnumName) \
    case EnumName:                  \
        return gp::String(#EnumName)

        GP_VK_RESULT_CASE(VK_SUCCESS);
        GP_VK_RESULT_CASE(VK_NOT_READY);
        GP_VK_RESULT_CASE(VK_TIMEOUT);
        GP_VK_RESULT_CASE(VK_EVENT_SET);
        GP_VK_RESULT_CASE(VK_EVENT_RESET);
        GP_VK_RESULT_CASE(VK_INCOMPLETE);
        GP_VK_RESULT_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
        GP_VK_RESULT_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        GP_VK_RESULT_CASE(VK_ERROR_INITIALIZATION_FAILED);
        GP_VK_RESULT_CASE(VK_ERROR_DEVICE_LOST);
        GP_VK_RESULT_CASE(VK_ERROR_MEMORY_MAP_FAILED);
        GP_VK_RESULT_CASE(VK_ERROR_LAYER_NOT_PRESENT);
        GP_VK_RESULT_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
        GP_VK_RESULT_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
        GP_VK_RESULT_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
        GP_VK_RESULT_CASE(VK_ERROR_TOO_MANY_OBJECTS);
        GP_VK_RESULT_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
        GP_VK_RESULT_CASE(VK_ERROR_FRAGMENTED_POOL);
        GP_VK_RESULT_CASE(VK_ERROR_UNKNOWN);
        GP_VK_RESULT_CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
        GP_VK_RESULT_CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        GP_VK_RESULT_CASE(VK_ERROR_FRAGMENTATION);
        GP_VK_RESULT_CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
        GP_VK_RESULT_CASE(VK_PIPELINE_COMPILE_REQUIRED);
        GP_VK_RESULT_CASE(VK_ERROR_SURFACE_LOST_KHR);
        GP_VK_RESULT_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        GP_VK_RESULT_CASE(VK_SUBOPTIMAL_KHR);
        GP_VK_RESULT_CASE(VK_ERROR_OUT_OF_DATE_KHR);
        GP_VK_RESULT_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        GP_VK_RESULT_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
        GP_VK_RESULT_CASE(VK_ERROR_INVALID_SHADER_NV);
        GP_VK_RESULT_CASE(VK_ERROR_NOT_PERMITTED_EXT);

#undef GP_VK_RESULT_CASE
    default:
        return GP_VK_TYPE_TO_STRING(VkResult, result);
    }
}

gp::String formatVulkanResultMessage(
    const VkResult result, const char* const vkFunction, const char* const filename, const gp::UInt32 line
) noexcept
{
    const gp::String errorString = vulkanResultToString(result);
    return formatVulkanResultMessageWithError(result, vkFunction, filename, line, errorString.view());
}

void setVulkanResultErrorHandler(const VulkanResultErrorHandler handler) noexcept
{
    g_errorHandler.store(handler, gp::MemoryOrder::Release);
}

VulkanResultErrorHandler getVulkanResultErrorHandler() noexcept
{
    return g_errorHandler.load(gp::MemoryOrder::Acquire);
}

void verifyVulkanResult(
    const VkResult result, const char* const vkFunction, const char* const filename, const gp::UInt32 line
) noexcept
{
    VulkanResultErrorContext context{};
    context.result = result;
    context.vkFunction = (vkFunction != nullptr) ? vkFunction : "<unknown>";
    context.filename = (filename != nullptr) ? filename : "<unknown>";
    context.line = line;
    context.errorString = vulkanResultToString(result);
    context.formattedMessage = formatVulkanResultMessageWithError(
        result, context.vkFunction, context.filename, line, context.errorString.view()
    );
    context.dumpMemory = isVulkanOutOfMemoryResult(result);
    context.isGPUCrashed = isVulkanDeviceLostResult(result);

    if (const VulkanResultErrorHandler customHandler = g_errorHandler.load(gp::MemoryOrder::Acquire);
        customHandler != nullptr)
    {
        customHandler(context);
        return;
    }

    // Multiple threads may hit a GPU crash or Vulkan error concurrently. Serialize logging so only the first failing
    // thread reports the failure before fatal termination.
    g_crashMutex.lock();

    GP_LOG_ERROR(gp::LogVulkanRHI, "{}", context.formattedMessage.view());

    if (context.isGPUCrashed)
    {
        VulkanPlatform::onGPUCrash(context.formattedMessage.view());
    }

    GP_LOG_FATAL(gp::LogVulkanRHI, "{}", context.formattedMessage.view());
}

}   // namespace gp::rhi::vulkan
