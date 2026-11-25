// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Configuration parameters passed to `VulkanInstance::initialize()`.
struct VulkanInstanceCreateDesc final
{
    /// @brief UTF-8 application name reported in `VkApplicationInfo::pApplicationName`.
    const char* applicationName{ "GraphicalPlayground" };

    /// @brief Packed application version reported in `VkApplicationInfo::applicationVersion`.
    gp::UInt32 applicationVersion{ VK_MAKE_API_VERSION(0u, 1u, 0u, 0u) };

    /// @brief UTF-8 engine name reported in `VkApplicationInfo::pEngineName`.
    const char* engineName{ "GraphicalPlaygroundEngine" };

    /// @brief Packed engine version reported in `VkApplicationInfo::engineVersion`.
    gp::UInt32 engineVersion{ VK_MAKE_API_VERSION(0u, 1u, 0u, 0u) };

    /// @brief Requested Vulkan API version (`0` uses `kVulkanApiVersion`).
    gp::UInt32 apiVersion{ kVulkanApiVersion };

    /// @brief When true, passes `nullptr` for `pApplicationName` and `pEngineName` (mirrors
    ///        `r.DisableEngineAndAppRegistration`).
    bool disableEngineAndAppRegistration{ false };

    /// @brief When true, retries `vkCreateInstance` with zero layers if `VK_ERROR_LAYER_NOT_PRESENT` is returned.
    bool allowRetryWithoutLayers{ true };

    /// @brief When true, allows instance entry-point loading to succeed without WSI surface functions (headless mode).
    bool allowMissingSurfaceFunctions{ false };

    /// @brief When true and debugging is compiled in, creates the `VkDebugUtilsMessengerEXT` callback after instance
    ///        creation.
    bool enableDebugCallbacks{ true };

    /// @brief Optional user data pointer forwarded to `VulkanDebugMessenger::create()`.
    void* debugUserData{ nullptr };
};

/// @brief Selection policy controlling how `selectPhysicalDevice()` chooses among enumerated GPUs.
struct VulkanPhysicalDeviceSelectionDesc final
{
    /// @brief Explicit adapter override:
    ///        - `>= 0`: select the adapter at this zero-based index (clamped to `0` if out of range).
    ///        - `-1`: prefer discrete GPUs over integrated GPUs, and place CPU software devices last (default).
    ///        - `-2`: select the first enumerated adapter without sorting.
    gp::Int32 explicitAdapterIndex{ -1 };

    /// @brief Human-readable source label for logging when `explicitAdapterIndex` is set (`"command line"` or
    ///        `"'r.GraphicsAdapter'"`).
    const char* adapterOriginLabel{ "'r.GraphicsAdapter'" };

    /// @brief Optional preferred GPU vendor (`GpuVendorId::Unknown` disables vendor filtering).
    gp::rhi::GpuVendorId preferredVendor{ gp::rhi::GpuVendorId::Unknown };

    /// @brief When true, `VK_PHYSICAL_DEVICE_TYPE_CPU` adapters are eligible for selection.
    bool allowCpuDevices{ false };

    /// @brief Optional 64-bit adapter LUID override (for example from an attached HMD runtime).
    gp::UInt64 preferredLuid{ 0u };

    /// @brief When true, matches `preferredLuid` against `VkPhysicalDeviceIDProperties::deviceLUID` before other rules.
    bool hasPreferredLuid{ false };
};

/// @brief Endian-independent decoder for NVIDIA's packed 10/8/8/6-bit Vulkan `driverVersion` field.
struct VulkanNvidiaDriverVersion final
{
    /// @brief 10-bit major driver version component.
    gp::UInt32 major{ 0u };

    /// @brief 8-bit minor driver version component.
    gp::UInt32 minor{ 0u };

    /// @brief 8-bit secondary driver version component.
    gp::UInt32 secondary{ 0u };

    /// @brief 6-bit tertiary driver version component.
    gp::UInt32 tertiary{ 0u };

    /// @brief Unpacks a raw NVIDIA `VkPhysicalDeviceProperties::driverVersion` integer.
    /// @param[in] packed Raw 32-bit `driverVersion` value.
    /// @return Decoded `VulkanNvidiaDriverVersion` components.
    [[nodiscard]] static constexpr VulkanNvidiaDriverVersion unpack(const gp::UInt32 packed) noexcept
    {
        VulkanNvidiaDriverVersion version;
        version.major = (packed >> 22u) & 0x3FFu;
        version.minor = (packed >> 14u) & 0xFFu;
        version.secondary = (packed >> 6u) & 0xFFu;
        version.tertiary = packed & 0x3Fu;
        return version;
    }

    /// @brief Packs major, minor, secondary, and tertiary fields into NVIDIA's 32-bit `driverVersion` encoding.
    /// @param[in] inMajor 10-bit major driver version.
    /// @param[in] inMinor 8-bit minor driver version.
    /// @param[in] inSecondary 8-bit secondary driver version.
    /// @param[in] inTertiary 6-bit tertiary driver version.
    /// @return Packed 32-bit driver version value.
    [[nodiscard]] static constexpr gp::UInt32 pack(
        const gp::UInt32 inMajor,
        const gp::UInt32 inMinor,
        const gp::UInt32 inSecondary = 0u,
        const gp::UInt32 inTertiary = 0u
    ) noexcept
    {
        return ((inMajor & 0x3FFu) << 22u) | ((inMinor & 0xFFu) << 14u) | ((inSecondary & 0xFFu) << 6u) |
               (inTertiary & 0x3Fu);
    }
};

/// @brief Inspected Vulkan physical device properties, identity UUID/LUID, memory heaps, and derived RHI telemetry.
struct VulkanPhysicalDeviceInfo final
{
    /// @brief Original zero-based enumeration index returned by `vkEnumeratePhysicalDevices`.
    gp::UInt32 originalIndex{ 0u };

    /// @brief Underlying `VkPhysicalDevice` handle.
    VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };

    /// @brief Core physical device properties populated by `vkGetPhysicalDeviceProperties2`.
    VkPhysicalDeviceProperties2 properties2{};

    /// @brief Physical device UUID and LUID identity properties.
    VkPhysicalDeviceIDProperties idProperties{};

    /// @brief Vulkan 1.2 driver properties (driver ID, name, info, conformance version).
    VkPhysicalDeviceDriverProperties driverProperties{};

    /// @brief Physical device memory heaps and memory types.
    VkPhysicalDeviceMemoryProperties memoryProperties{};

    /// @brief True when `driverProperties` was populated via Vulkan 1.2 `VkPhysicalDeviceDriverProperties`.
    bool hasValidDriverProperties{ false };

    /// @brief Normalized RHI vendor identifier mapped from `properties2.properties.vendorID`.
    gp::rhi::GpuVendorId vendorId{ gp::rhi::GpuVendorId::NotQueried };

    /// @brief Sum of all `VK_MEMORY_HEAP_DEVICE_LOCAL_BIT` heap sizes in bytes.
    gp::UInt64 dedicatedVideoMemoryBytes{ 0u };

    /// @brief True when `properties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU`.
    bool deviceIsIntegrated{ false };

    /// @brief 64-bit locally unique adapter identifier decoded from `idProperties.deviceLUID` when valid.
    gp::UInt64 luid{ 0u };

    /// @brief True when `idProperties.deviceLUIDValid == VK_TRUE`.
    bool hasValidLuid{ false };

    /// @brief 16-byte Vulkan device UUID copied from `idProperties.deviceUUID`.
    gp::Array<gp::UInt8, VK_UUID_SIZE> deviceUuid{};

    /// @brief 16-byte Vulkan driver UUID copied from `idProperties.driverUUID`.
    gp::Array<gp::UInt8, VK_UUID_SIZE> driverUuid{};

    /// @brief Human-readable driver version string (`"Major.Minor"` for NVIDIA, `"Major.Minor.Patch"` otherwise).
    gp::String formattedDriverVersion{};

    /// @brief Human-readable Vulkan API version string (`"Major.Minor.Patch"`).
    gp::String formattedApiVersion{};
};

}   // namespace gp::rhi::vulkan
