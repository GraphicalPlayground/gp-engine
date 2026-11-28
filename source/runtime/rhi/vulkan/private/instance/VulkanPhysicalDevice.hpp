// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/RHIGlobals.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Maps a raw Vulkan `VkPhysicalDeviceProperties::vendorID` (PCI or Khronos `VkVendorId`) to `GpuVendorId`.
/// @param[in] rawVendorId Raw 32-bit vendor identifier reported by the Vulkan driver.
/// @return Normalized `gp::rhi::GpuVendorId`.
[[nodiscard]] GP_RHI_VULKAN_API gp::rhi::GpuVendorId resolveVulkanVendorId(gp::UInt32 rawVendorId) noexcept;

/// @brief Parses preferred GPU vendor command-line flags (`-prefernvidia`, `-preferamd`, `-preferintel`, etc.).
/// @return Requested `GpuVendorId`, or `GpuVendorId::Unknown` when no vendor flag is present.
[[nodiscard]] GP_RHI_VULKAN_API gp::rhi::GpuVendorId resolvePreferredAdapterVendor() noexcept;

/// @brief Builds a `VulkanPhysicalDeviceSelectionDesc` from `r.GraphicsAdapter`, `-graphicsadapter=N`,
///        `-AllowCPUDevices`, `-AllowSoftwareRendering`, and vendor preference flags.
/// @return Resolved selection policy descriptor.
[[nodiscard]] GP_RHI_VULKAN_API VulkanPhysicalDeviceSelectionDesc resolvePhysicalDeviceSelectionDesc() noexcept;

/// @brief Computes total device-local video memory in bytes by summing `VK_MEMORY_HEAP_DEVICE_LOCAL_BIT` heaps.
/// @param[in] memoryProperties Physical device memory heap properties.
/// @return Total device-local heap size in bytes.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64
    computeDedicatedVideoMemory(const VkPhysicalDeviceMemoryProperties& memoryProperties) noexcept;

/// @brief Formats a Vulkan `driverVersion` integer into a human-readable version string.
/// @details Decodes NVIDIA's 10/8/8/6-bit packed format as `"Major.Minor"` (two-digit zero-padded minor) and all other
///          vendors as `"Major.Minor.Patch"`.
/// @param[in] vendorId Normalized GPU vendor identifier.
/// @param[in] rawDriverVersion Raw `VkPhysicalDeviceProperties::driverVersion` field.
/// @return Formatted driver version string.
[[nodiscard]] GP_RHI_VULKAN_API gp::String
    formatVulkanDriverVersion(gp::rhi::GpuVendorId vendorId, gp::UInt32 rawDriverVersion) noexcept;

/// @brief Formats a packed Vulkan API version (`VK_API_VERSION_*`) as `"Major.Minor.Patch"`.
/// @param[in] apiVersion Packed Vulkan version integer.
/// @return Formatted API version string.
[[nodiscard]] GP_RHI_VULKAN_API gp::String formatVulkanApiVersion(gp::UInt32 apiVersion) noexcept;

/// @brief Computes derived identity, memory, vendor, and version fields on `info` from its populated Vulkan structs.
/// @details Separated from live Vulkan queries so unit tests can verify every derivation deterministically.
/// @param[in,out] info Physical device info whose `properties2`, `idProperties`, `driverProperties`, and
///                     `memoryProperties` have already been populated.
GP_RHI_VULKAN_API void populatePhysicalDeviceInfo(VulkanPhysicalDeviceInfo& info) noexcept;

/// @brief Queries a live `VkPhysicalDevice` for properties, UUID/LUID, driver info, and memory heaps.
/// @param[in] physicalDevice Target `VkPhysicalDevice`.
/// @param[in] originalIndex Zero-based enumeration index from `vkEnumeratePhysicalDevices`.
/// @param[in] instanceApiVersion Active Vulkan instance API version.
/// @return Populated `VulkanPhysicalDeviceInfo`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanPhysicalDeviceInfo inspectPhysicalDevice(
    VkPhysicalDevice physicalDevice, gp::UInt32 originalIndex, gp::UInt32 instanceApiVersion
) noexcept;

/// @brief Enumerates and inspects all `VkPhysicalDevice` adapters exposed by `instance`.
/// @param[in] instance Active `VkInstance`.
/// @param[in] instanceApiVersion Active Vulkan instance API version.
/// @return Vector of inspected `VulkanPhysicalDeviceInfo` descriptors (empty if none found or enumeration failed).
[[nodiscard]] GP_RHI_VULKAN_API gp::Vector<VulkanPhysicalDeviceInfo>
    enumeratePhysicalDevices(VkInstance instance, gp::UInt32 instanceApiVersion) noexcept;

/// @brief Selects the best physical device index from `candidates` according to `desc`.
/// @details Selection priority matches standard desktop/mobile Vulkan RHI ordering:
///          1. Preferred LUID match (`desc.hasPreferredLuid`)
///          2. Explicit adapter index (`desc.explicitAdapterIndex >= 0`, clamped to `0` if out of range)
///          3. First unsorted adapter (`desc.explicitAdapterIndex == -2`)
///          4. Discrete-first stable sort (`desc.explicitAdapterIndex == -1`: discrete first, CPU last)
///          5. Preferred vendor match (`desc.preferredVendor != GpuVendorId::Unknown`)
///          6. First non-CPU adapter (or CPU adapter when `desc.allowCpuDevices` is true)
/// @param[in] candidates Inspected candidate physical devices.
/// @param[in] desc Selection policy parameters.
/// @return Zero-based index into `candidates`, or `-1` when no candidate satisfies the policy.
[[nodiscard]] GP_RHI_VULKAN_API gp::Int32 selectPhysicalDeviceIndex(
    gp::ConstVectorView<VulkanPhysicalDeviceInfo> candidates, const VulkanPhysicalDeviceSelectionDesc& desc
) noexcept;

/// @brief Enumerates physical devices on `instance` and selects the best adapter.
/// @param[in] instance Active `VkInstance`.
/// @param[in] instanceApiVersion Active Vulkan instance API version.
/// @param[out] outSelectedInfo Receives the inspected metadata of the selected adapter on success.
/// @param[in] overrideDesc Optional explicit selection policy; when `nullptr`, resolves policy from CVars and CLI.
/// @return Selected `VkPhysicalDevice` handle, or `VK_NULL_HANDLE` if no compatible device was found.
[[nodiscard]] GP_RHI_VULKAN_API VkPhysicalDevice selectPhysicalDevice(
    VkInstance instance,
    gp::UInt32 instanceApiVersion,
    VulkanPhysicalDeviceInfo& outSelectedInfo,
    const VulkanPhysicalDeviceSelectionDesc* overrideDesc = nullptr
) noexcept;

/// @brief Queries the 64-bit LUID of a live `VkPhysicalDevice` via `VkPhysicalDeviceIDProperties`.
/// @param[in] physicalDevice Target `VkPhysicalDevice`.
/// @return 64-bit adapter LUID, or `0` if unavailable or `deviceLUIDValid == VK_FALSE`.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64 getGraphicsAdapterLuid(VkPhysicalDevice physicalDevice) noexcept;

/// @brief Checks whether a caller-supplied 8-byte OS adapter LUID matches `info`.
/// @details When `info.hasValidLuid` is false or `adapterId` is `nullptr`, returns true (insufficient data to reject).
/// @param[in] info Inspected physical device metadata.
/// @param[in] adapterId Pointer to an 8-byte platform LUID (`VK_LUID_SIZE` bytes).
/// @return True when the adapter matches or when LUID comparison is unavailable.
[[nodiscard]] GP_RHI_VULKAN_API bool
    doesAdapterMatchDevice(const VulkanPhysicalDeviceInfo& info, const void* adapterId) noexcept;

/// @brief Populates `outGpuInfo` from inspected physical device metadata.
/// @param[in] info Inspected physical device metadata.
/// @param[out] outGpuInfo Target `gp::rhi::GPUInfo` struct to populate.
GP_RHI_VULKAN_API void
    populateRHIGlobalsGpuInfo(const VulkanPhysicalDeviceInfo& info, gp::rhi::GPUInfo& outGpuInfo) noexcept;

/// @brief Populates `gp::rhi::getRHIGlobals().gpuInfo` and timestamp query capability from `info`.
/// @param[in] info Inspected physical device metadata.
GP_RHI_VULKAN_API void applySelectedPhysicalDeviceToRHIGlobals(const VulkanPhysicalDeviceInfo& info) noexcept;

}   // namespace gp::rhi::vulkan
