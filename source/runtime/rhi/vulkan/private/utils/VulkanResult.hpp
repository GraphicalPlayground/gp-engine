// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Diagnostic payload describing a failed Vulkan API call captured by `verifyVulkanResult()`.
struct VulkanResultErrorContext
{
    /// @brief Failing `VkResult` code returned by the Vulkan driver or loader.
    VkResult result{ VK_SUCCESS };

    /// @brief Name or call expression string of the failing Vulkan function.
    const char* vkFunction{ nullptr };

    /// @brief Source filename where the failure occurred.
    const char* filename{ nullptr };

    /// @brief Source line number where the failure occurred.
    gp::UInt32 line{ 0u };

    /// @brief Human-readable `VkResult` enumerator name or numeric fallback.
    gp::String errorString{};

    /// @brief Full formatted diagnostic message including call site and error code.
    gp::String formattedMessage{};

    /// @brief True when `result` indicates a host or device out-of-memory error (`dumpMemory` candidate).
    bool dumpMemory{ false };

    /// @brief True when `result` is `VK_ERROR_DEVICE_LOST`.
    bool isGPUCrashed{ false };
};

/// @brief Callback signature for intercepting `verifyVulkanResult()` failures (used by backend crash handlers and
/// tests).
using VulkanResultErrorHandler = void (*)(const VulkanResultErrorContext& context) noexcept;

/// @brief Converts a `VkResult` code into its symbolic enumerator name (or decimal fallback string if unrecognized).
/// @param[in] result Vulkan API result code.
/// @return Symbolic name (e.g., `"VK_ERROR_DEVICE_LOST"`) or decimal string fallback.
[[nodiscard]] GP_RHI_VULKAN_API gp::String vulkanResultToString(VkResult result) noexcept;

/// @brief Formats the canonical diagnostic error string for a failed Vulkan call.
/// @param[in] result Failing `VkResult` value.
/// @param[in] vkFunction Failing Vulkan call expression or function name (nullable).
/// @param[in] filename Source filename where the failure occurred (nullable).
/// @param[in] line Source line number where the failure occurred.
/// @return Multi-line formatted diagnostic message.
[[nodiscard]] GP_RHI_VULKAN_API gp::String
    formatVulkanResultMessage(VkResult result, const char* vkFunction, const char* filename, gp::UInt32 line) noexcept;

/// @brief Reports whether `result` indicates a host or device out-of-memory condition.
/// @param[in] result Vulkan API result code.
/// @return True for `VK_ERROR_OUT_OF_HOST_MEMORY` or `VK_ERROR_OUT_OF_DEVICE_MEMORY`.
[[nodiscard]] inline constexpr bool isVulkanOutOfMemoryResult(const VkResult result) noexcept
{
    return result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

/// @brief Reports whether `result` indicates an unrecoverable GPU hang or device loss.
/// @param[in] result Vulkan API result code.
/// @return True for `VK_ERROR_DEVICE_LOST`.
[[nodiscard]] inline constexpr bool isVulkanDeviceLostResult(const VkResult result) noexcept
{
    return result == VK_ERROR_DEVICE_LOST;
}

/// @brief Installs a process-wide handler invoked by `verifyVulkanResult()` instead of terminating immediately.
/// @details Pass `nullptr` to restore the default fatal logging and termination path.
/// @param[in] handler Custom error handler function pointer, or `nullptr` for default fatal behavior.
GP_RHI_VULKAN_API void setVulkanResultErrorHandler(VulkanResultErrorHandler handler) noexcept;

/// @brief Returns the currently installed `verifyVulkanResult()` error handler, or `nullptr` if default behavior is
///        active.
/// @return Active `VulkanResultErrorHandler` pointer, or `nullptr`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanResultErrorHandler getVulkanResultErrorHandler() noexcept;

/// @brief Handles a failed Vulkan call by classifying `result`, formatting diagnostics, and escalating to fatal error.
/// @details Invokes the custom `VulkanResultErrorHandler` immediately if one is installed (allowing interrupt-thread
///          crash handlers or tests to manage locking directly). On the default fatal path, serializes concurrent
///          failures with an internal mutex, logs the diagnostic, notifies `VulkanPlatform::onGPUCrash()` on device
///          loss, and terminates via `GP_LOG_FATAL`.
/// @param[in] result Failing `VkResult` value.
/// @param[in] vkFunction Failing Vulkan function name or call expression.
/// @param[in] filename Source filename containing the call.
/// @param[in] line Source line number containing the call.
GP_RHI_VULKAN_API void
    verifyVulkanResult(VkResult result, const char* vkFunction, const char* filename, gp::UInt32 line) noexcept;

}   // namespace gp::rhi::vulkan

/// @brief Evaluates `VkCall` and invokes `verifyVulkanResult()` unless the return value is exactly `VK_SUCCESS`.
/// @param[in] VkCall Vulkan API call expression yielding a `VkResult`.
#define GP_VERIFY_VULKAN_RESULT(VkCall)                                                \
    do                                                                                 \
    {                                                                                  \
        const VkResult gpScopedVkResult = (VkCall);                                    \
        if (gpScopedVkResult != VK_SUCCESS)                                            \
        {                                                                              \
            ::gp::rhi::vulkan::verifyVulkanResult(                                     \
                gpScopedVkResult, #VkCall, __FILE__, static_cast<::gp::UInt32>(__LINE__) \
            );                                                                         \
        }                                                                              \
    } while (false)

/// @brief Evaluates `VkCall` and invokes `verifyVulkanResult()` only on negative error codes (`< VK_SUCCESS`), allowing
///        positive non-error statuses such as `VK_INCOMPLETE` or `VK_SUBOPTIMAL_KHR`.
/// @param[in] VkCall Vulkan API call expression yielding a `VkResult`.
#define GP_VERIFY_VULKAN_RESULT_EXPANDED(VkCall)                                       \
    do                                                                                 \
    {                                                                                  \
        const VkResult gpScopedVkResult = (VkCall);                                    \
        if (gpScopedVkResult < VK_SUCCESS)                                             \
        {                                                                              \
            ::gp::rhi::vulkan::verifyVulkanResult(                                     \
                gpScopedVkResult, #VkCall, __FILE__, static_cast<::gp::UInt32>(__LINE__) \
            );                                                                         \
        }                                                                              \
    } while (false)
