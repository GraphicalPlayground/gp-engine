// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"

namespace gp::rhi::vulkan
{

/// @brief Deduplication category bucket used to suppress repeated identical validation messages.
enum class VulkanDebugMessageBucket : gp::UInt32
{
    General = 0u,
    GeneralValidation,
    PerfValidation,
    Validation,
    Perf,
    Count
};

/// @brief Optional callback sink invoked when `debugUtilsCallback` emits a non-filtered diagnostic message.
/// @param[in] severity Vulkan severity flag bit (`ERROR`, `WARNING`, `INFO`, or `VERBOSE`).
/// @param[in] formattedMessage Null-terminated formatted diagnostic text.
/// @param[in] userData Caller-provided context pointer from `VkDebugUtilsMessengerCreateInfoEXT::pUserData`.
using VulkanDebugReportCallback =
    void (*)(VkDebugUtilsMessageSeverityFlagBitsEXT severity, const char* formattedMessage, void* userData) noexcept;

/// @brief Installs an optional report callback sink used by `debugUtilsCallback()` in addition to `LogVulkanRHI`.
/// @param[in] callback Sink function pointer, or `nullptr` to restore default logging only.
GP_RHI_VULKAN_API void setVulkanDebugReportCallback(VulkanDebugReportCallback callback) noexcept;

/// @brief Returns the currently installed `VulkanDebugReportCallback`, or `nullptr` if none is set.
/// @return Active report callback pointer.
[[nodiscard]] GP_RHI_VULKAN_API VulkanDebugReportCallback getVulkanDebugReportCallback() noexcept;

/// @brief Converts a `VkDebugUtilsMessageSeverityFlagBitsEXT` bit into its human-readable label.
/// @param[in] severity Severity bit reported by the validation layer.
/// @return `"Error"`, `"Warning"`, `"Info"`, `"Verbose"`, or `""`.
[[nodiscard]] GP_RHI_VULKAN_API const char*
    classifyDebugSeverity(VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept;

/// @brief Classifies a `VkDebugUtilsMessageTypeFlagsEXT` mask into its label and deduplication bucket.
/// @param[in] messageType Type bitmask reported by the validation layer.
/// @param[out] outBucket Receives the corresponding `VulkanDebugMessageBucket`.
/// @return Human-readable type label (e.g., `"Validation"`, `"General/Validation"`, `"Perf"`).
[[nodiscard]] GP_RHI_VULKAN_API const char*
    classifyDebugMessageType(VkDebugUtilsMessageTypeFlagsEXT messageType, VulkanDebugMessageBucket& outBucket) noexcept;

/// @brief Reports whether a validation callback payload matches a known benign or false-positive VUID filter.
/// @param[in] callbackData Callback payload supplied by `VK_EXT_debug_utils`.
/// @return True when the message should be suppressed.
[[nodiscard]] GP_RHI_VULKAN_API bool
    isFilteredValidationMessage(const VkDebugUtilsMessengerCallbackDataEXT* callbackData) noexcept;

/// @brief Standard `PFN_vkDebugUtilsMessengerCallbackEXT` implementation routing Vulkan diagnostics into
///        `LogVulkanRHI`.
/// @details Suppresses known false-positive VUIDs, optionally deduplicates repeated message IDs per bucket when
///          `r.Vulkan.UniqueValidationMessages` is non-zero, and always returns `VK_FALSE`.
/// @param[in] messageSeverity Severity bit of the validation message.
/// @param[in] messageType Type bitmask of the validation message.
/// @param[in] callbackData Detailed validation payload.
/// @param[in] userData Optional user pointer passed through `VkDebugUtilsMessengerCreateInfoEXT::pUserData`.
/// @return Always `VK_FALSE` (never aborts the triggering Vulkan API call).
GP_RHI_VULKAN_API VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData
) noexcept;

/// @brief Clears all cached CRC-32 hashes used for unique validation message deduplication.
GP_RHI_VULKAN_API void resetUniqueValidationMessageCache() noexcept;

/// @brief Builds a `VkDebugUtilsMessengerCreateInfoEXT` struct configured for `validationLevel`.
/// @details Severity and type masks scale progressively with `validationLevel`:
///          - `>= 1`: `ERROR`, `GENERAL | VALIDATION`
///          - `>= 2`: `+ WARNING`
///          - `>= 3`: `+ INFO`, `+ PERFORMANCE`
///          - `>= 5`: `+ VERBOSE`
/// @param[in] validationLevel Validation severity level (`0`..`5`).
/// @param[in] userData Optional user data pointer stored in `pUserData`.
/// @return Zero-initialized and populated `VkDebugUtilsMessengerCreateInfoEXT`.
[[nodiscard]] GP_RHI_VULKAN_API VkDebugUtilsMessengerCreateInfoEXT
    buildDebugMessengerCreateInfo(gp::Int32 validationLevel, void* userData = nullptr) noexcept;

/// @brief Manages creation and destruction of a `VkDebugUtilsMessengerEXT` handle for an active `VkInstance`.
class GP_RHI_VULKAN_API VulkanDebugMessenger final
{
public:
    VulkanDebugMessenger() noexcept = default;
    ~VulkanDebugMessenger() noexcept = default;

    VulkanDebugMessenger(const VulkanDebugMessenger&) = delete;
    VulkanDebugMessenger& operator=(const VulkanDebugMessenger&) = delete;

    /// @brief Creates and registers the `VkDebugUtilsMessengerEXT` callback on `instance`.
    /// @param[in] instance Active `VkInstance`.
    /// @param[in] activeDebugExtension Active debug extension selected during instance setup.
    /// @param[in] validationLevel Validation severity level (`0`..`5`).
    /// @param[in] userData Optional user data pointer forwarded to `debugUtilsCallback`.
    /// @return True when a debug messenger was successfully created; false if disabled or unavailable.
    [[nodiscard]] bool create(
        VkInstance instance,
        ActiveDebugLayerExtension activeDebugExtension,
        gp::Int32 validationLevel,
        void* userData = nullptr
    ) noexcept;

    /// @brief Destroys the active `VkDebugUtilsMessengerEXT` handle if valid and resets it to `VK_NULL_HANDLE`.
    /// @param[in] instance Owning `VkInstance`.
    void destroy(VkInstance instance) noexcept;

    /// @brief Returns the underlying `VkDebugUtilsMessengerEXT` handle.
    /// @return Active messenger handle, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkDebugUtilsMessengerEXT getHandle() const noexcept
    {
        return m_messenger;
    }

    /// @brief Reports whether a live debug messenger handle is currently registered.
    /// @return True when `m_messenger != VK_NULL_HANDLE`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_messenger != VK_NULL_HANDLE;
    }

private:
    VkDebugUtilsMessengerEXT m_messenger{ VK_NULL_HANDLE };
};

/// @brief Dispatch table and helper wrappers for Vulkan object debug naming and command-buffer debug labels.
/// @details Supports both modern `VK_EXT_debug_utils` entry points and legacy `VK_EXT_debug_marker` shims.
class GP_RHI_VULKAN_API VulkanDebugMarkers final
{
public:
    VulkanDebugMarkers() noexcept = default;

    /// @brief Resolves debug marker and object naming entry points for a logical device.
    /// @param[in] instance Active `VkInstance`.
    /// @param[in] physicalDevice Active `VkPhysicalDevice`.
    /// @param[in] device Active `VkDevice`.
    /// @param[in] supportsDebugUtilsExt True when `VK_EXT_debug_utils` is enabled on the instance.
    /// @param[in] useLegacyDebugMarkerExt True when legacy `VK_EXT_debug_marker` should be used instead.
    /// @param[in] hasExtToolingInfo True when `VK_EXT_tooling_info` is available to probe attached capture tools.
    /// @param[in] gpuCrashDebugging True when GPU crash debugging is active (forces command-buffer labels on).
    /// @param[in] renderDocFound True when RenderDoc capture layer was detected.
    void initialize(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        bool supportsDebugUtilsExt,
        bool useLegacyDebugMarkerExt,
        bool hasExtToolingInfo,
        bool gpuCrashDebugging,
        bool renderDocFound
    ) noexcept;

    /// @brief Populates debug marker function pointers directly (useful for custom dispatch or unit tests).
    /// @param[in] beginLabelFn Function pointer for `vkCmdBeginDebugUtilsLabelEXT`.
    /// @param[in] endLabelFn Function pointer for `vkCmdEndDebugUtilsLabelEXT`.
    /// @param[in] insertLabelFn Function pointer for `vkCmdInsertDebugUtilsLabelEXT`.
    /// @param[in] setObjectNameFn Function pointer for `vkSetDebugUtilsObjectNameEXT`.
    void initializeWithFunctions(
        PFN_vkCmdBeginDebugUtilsLabelEXT beginLabelFn,
        PFN_vkCmdEndDebugUtilsLabelEXT endLabelFn,
        PFN_vkCmdInsertDebugUtilsLabelEXT insertLabelFn,
        PFN_vkSetDebugUtilsObjectNameEXT setObjectNameFn
    ) noexcept;

    /// @brief Clears all resolved debug marker function pointers.
    void reset() noexcept;

    /// @brief Assigns a UTF-8 debug name to a Vulkan object handle (`VkImage`, `VkBuffer`, `VkPipeline`, etc.).
    /// @param[in] device Owning logical `VkDevice`.
    /// @param[in] objectType `VkObjectType` identifying the handle kind.
    /// @param[in] objectHandle 64-bit numeric handle value.
    /// @param[in] name Null-terminated UTF-8 debug name.
    void setObjectName(
        VkDevice device, VkObjectType objectType, gp::UInt64 objectHandle, const char* name
    ) const noexcept;

    /// @brief Typed convenience overload casting a Vulkan handle directly to `gp::UInt64`.
    /// @tparam InHandleType Vulkan handle type (`VkImage`, `VkBuffer`, `VkQueue`, etc.).
    /// @param[in] device Owning logical `VkDevice`.
    /// @param[in] objectType `VkObjectType` identifying the handle kind.
    /// @param[in] handle Vulkan handle to name.
    /// @param[in] name Null-terminated UTF-8 debug name.
    template <typename InHandleType>
    void setObjectName(
        const VkDevice device, const VkObjectType objectType, const InHandleType handle, const char* const name
    ) const noexcept
    {
        static_assert(sizeof(InHandleType) <= sizeof(gp::UInt64), "Vulkan handle exceeds 64 bits.");
        setObjectName(device, objectType, reinterpret_cast<gp::UInt64>(handle), name);
    }

    /// @brief Opens a nested debug label region on `commandBuffer`.
    /// @param[in] commandBuffer Recording `VkCommandBuffer`.
    /// @param[in] labelName Null-terminated UTF-8 label text.
    /// @param[in] red Red color channel in `[0, 1]`.
    /// @param[in] green Green color channel in `[0, 1]`.
    /// @param[in] blue Blue color channel in `[0, 1]`.
    /// @param[in] alpha Alpha color channel in `[0, 1]`.
    void cmdBeginDebugLabel(
        VkCommandBuffer commandBuffer,
        const char* labelName,
        gp::Float32 red = 1.0f,
        gp::Float32 green = 1.0f,
        gp::Float32 blue = 1.0f,
        gp::Float32 alpha = 1.0f
    ) const noexcept;

    /// @brief Closes the innermost open debug label region on `commandBuffer`.
    /// @param[in] commandBuffer Recording `VkCommandBuffer`.
    void cmdEndDebugLabel(VkCommandBuffer commandBuffer) const noexcept;

    /// @brief Inserts a single-point debug marker label into `commandBuffer`.
    /// @param[in] commandBuffer Recording `VkCommandBuffer`.
    /// @param[in] labelName Null-terminated UTF-8 label text.
    /// @param[in] red Red color channel in `[0, 1]`.
    /// @param[in] green Green color channel in `[0, 1]`.
    /// @param[in] blue Blue color channel in `[0, 1]`.
    /// @param[in] alpha Alpha color channel in `[0, 1]`.
    void cmdInsertDebugLabel(
        VkCommandBuffer commandBuffer,
        const char* labelName,
        gp::Float32 red = 1.0f,
        gp::Float32 green = 1.0f,
        gp::Float32 blue = 1.0f,
        gp::Float32 alpha = 1.0f
    ) const noexcept;

    /// @brief Reports whether object debug naming is active.
    /// @return True when `m_setDebugName != nullptr`.
    [[nodiscard]] bool supportsDebugNames() const noexcept
    {
        return m_setDebugName != nullptr;
    }

    /// @brief Reports whether command-buffer begin/end debug labels are active.
    /// @return True when both begin and end label entry points are available.
    [[nodiscard]] bool supportsDebugLabels() const noexcept
    {
        return (m_cmdBeginDebugLabel != nullptr) && (m_cmdEndDebugLabel != nullptr);
    }

    /// @brief Reports whether a GPU tracing or capture tool was detected during initialization.
    /// @return True when RenderDoc, GPU crash debugging, or a `VK_TOOL_PURPOSE_TRACING_BIT_EXT` tool was found.
    [[nodiscard]] bool isTraceToolDetected() const noexcept
    {
        return m_traceToolDetected;
    }

private:
    PFN_vkCmdBeginDebugUtilsLabelEXT m_cmdBeginDebugLabel{ nullptr };
    PFN_vkCmdEndDebugUtilsLabelEXT m_cmdEndDebugLabel{ nullptr };
    PFN_vkCmdInsertDebugUtilsLabelEXT m_cmdInsertLabel{ nullptr };
    PFN_vkSetDebugUtilsObjectNameEXT m_setDebugName{ nullptr };
    bool m_traceToolDetected{ false };
};

/// @brief RAII scope guard opening a command-buffer debug label on construction and closing it on destruction.
class VulkanScopedDebugLabel final
{
public:
    /// @brief Begins a debug label region on `commandBuffer`.
    /// @param[in] markers Active `VulkanDebugMarkers` dispatch table.
    /// @param[in] commandBuffer Recording `VkCommandBuffer`.
    /// @param[in] labelName Null-terminated UTF-8 label text.
    /// @param[in] red Red color channel in `[0, 1]`.
    /// @param[in] green Green color channel in `[0, 1]`.
    /// @param[in] blue Blue color channel in `[0, 1]`.
    /// @param[in] alpha Alpha color channel in `[0, 1]`.
    VulkanScopedDebugLabel(
        const VulkanDebugMarkers& markers,
        const VkCommandBuffer commandBuffer,
        const char* const labelName,
        const gp::Float32 red = 1.0f,
        const gp::Float32 green = 1.0f,
        const gp::Float32 blue = 1.0f,
        const gp::Float32 alpha = 1.0f
    ) noexcept
        : m_markers(markers)
        , m_commandBuffer(commandBuffer)
    {
        m_markers.cmdBeginDebugLabel(m_commandBuffer, labelName, red, green, blue, alpha);
    }

    /// @brief Ends the debug label region on destruction.
    ~VulkanScopedDebugLabel() noexcept
    {
        m_markers.cmdEndDebugLabel(m_commandBuffer);
    }

    VulkanScopedDebugLabel(const VulkanScopedDebugLabel&) = delete;
    VulkanScopedDebugLabel& operator=(const VulkanScopedDebugLabel&) = delete;

private:
    const VulkanDebugMarkers& m_markers;
    VkCommandBuffer m_commandBuffer{ VK_NULL_HANDLE };
};

}   // namespace gp::rhi::vulkan
