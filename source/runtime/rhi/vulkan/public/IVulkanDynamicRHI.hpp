// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DynamicRHI.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Interop surface of the Vulkan dynamic RHI, for code that must talk to Vulkan directly.
/// @details Including `rhi/vulkan` only exposes this class: the Vulkan headers and every backend type stay private to
///          the module. Native handles therefore travel as opaque `void*` and are cast back to `VkInstance`,
///          `VkPhysicalDevice`, `VkDevice` or `VkQueue` by the caller, which links Vulkan itself (an external SDK, a
///          capture layer, a video decoder sharing the device).
/// @note Obtain it through `getVulkanDynamicRHI()`; never `static_cast` an unknown `DynamicRHI*` to this type.
class GP_RHI_VULKAN_API IVulkanDynamicRHI : public DynamicRHI
{
public:
    /// @copydoc DynamicRHI::getInterfaceType()
    /// @note Sealed: every Vulkan RHI reports `InterfaceType::Vulkan`, which is what `getVulkanDynamicRHI()` checks.
    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept override final
    {
        return InterfaceType::Vulkan;
    }

public:
    /// @brief Returns the Vulkan API version the logical device was created against.
    /// @return Packed `VK_MAKE_API_VERSION` value, or `0` before initialization.
    [[nodiscard]] virtual gp::UInt32 getVulkanApiVersion() const noexcept = 0;

    /// @brief Returns the `VkInstance` owned by the RHI.
    /// @return Opaque instance handle, or `nullptr` before initialization.
    [[nodiscard]] virtual void* getVulkanInstanceHandle() const noexcept = 0;

    /// @brief Returns the `VkPhysicalDevice` the RHI selected.
    /// @return Opaque physical device handle, or `nullptr` before initialization.
    [[nodiscard]] virtual void* getVulkanPhysicalDeviceHandle() const noexcept = 0;

    /// @brief Returns the logical `VkDevice` owned by the RHI.
    /// @return Opaque device handle, or `nullptr` before initialization.
    [[nodiscard]] virtual void* getVulkanDeviceHandle() const noexcept = 0;

    /// @brief Returns the `VkQueue` graphics work is submitted to.
    /// @note Submitting to this queue from outside the RHI requires external synchronization with the RHI thread.
    /// @return Opaque queue handle, or `nullptr` before initialization.
    [[nodiscard]] virtual void* getVulkanGraphicsQueueHandle() const noexcept = 0;

    /// @brief Returns the index of the graphics queue inside its family.
    /// @return Queue index.
    [[nodiscard]] virtual gp::UInt32 getVulkanGraphicsQueueIndex() const noexcept = 0;

    /// @brief Returns the queue family index of the graphics queue.
    /// @return Family index, or `~0u` before initialization.
    [[nodiscard]] virtual gp::UInt32 getVulkanGraphicsQueueFamilyIndex() const noexcept = 0;

    /// @brief Returns the 16-byte UUID identifying the physical device (`VkPhysicalDeviceIDProperties::deviceUUID`).
    /// @return Pointer to `VK_UUID_SIZE` bytes owned by the RHI, valid until shutdown.
    [[nodiscard]] virtual const gp::UInt8* getVulkanDeviceUUID() const noexcept = 0;

    /// @brief Resolves an instance-level entry point through the RHI's instance.
    /// @param[in] functionName Null-terminated Vulkan function name.
    /// @return Function pointer as `void*`, or `nullptr` when unknown or before initialization.
    [[nodiscard]] virtual void* getVulkanInstanceProcAddress(const char* functionName) const noexcept = 0;

    /// @brief Resolves a device-level entry point through the RHI's logical device.
    /// @param[in] functionName Null-terminated Vulkan function name.
    /// @return Function pointer as `void*`, or `nullptr` when unknown or before initialization.
    [[nodiscard]] virtual void* getVulkanDeviceProcAddress(const char* functionName) const noexcept = 0;
};

/// @brief Returns the active RHI as a Vulkan RHI, looking through validation wrappers.
/// @return Vulkan interop interface, or `nullptr` when no RHI is installed or the active one is not Vulkan.
[[nodiscard]] GP_RHI_VULKAN_API IVulkanDynamicRHI* getVulkanDynamicRHI() noexcept;

}   // namespace gp::rhi::vulkan
