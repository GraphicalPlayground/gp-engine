// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "debug/VulkanDebug.hpp"
#include "device/DeviceEnums.hpp"
#include "device/RHIGlobals.hpp"
#include "device/VulkanDeviceCapabilities.hpp"
#include "device/VulkanPhysicalDeviceFeatures.hpp"
#include "device/VulkanPhysicalDeviceProperties.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanBarrierEventPool.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Owning logical Vulkan GPU device managing feature/extension negotiation, queue families, format support
///        tables, debug markers, and `RHIGlobals` capability population.
class GP_RHI_VULKAN_API VulkanDevice final
{
public:
    /// @brief Constructs a `VulkanDevice` bound to `instance` and `physicalDevice` and queries base physical-device
    ///        features and properties.
    /// @param[in] instance Active `VkInstance`.
    /// @param[in] physicalDevice Selected `VkPhysicalDevice`.
    /// @param[in] apiVersion Negotiated Vulkan instance API version.
    /// @param[in] supportsDebugUtilsExt True when `VK_EXT_debug_utils` is enabled on `instance`.
    VulkanDevice(
        VkInstance instance, VkPhysicalDevice physicalDevice, gp::UInt32 apiVersion, bool supportsDebugUtilsExt = false
    ) noexcept;

    /// @brief Destructor. Automatically destroys the logical `VkDevice` if still valid.
    ~VulkanDevice() noexcept;

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    /// @brief Runs extension/layer negotiation, creates the `VkDevice`, resolves device-direct entry points and queue
    ///        handles, initializes debug markers, probes pixel/vertex formats, and populates `RHIGlobals`.
    /// @param[in] applyToRHIGlobals When true, writes resolved limits, features, and format capabilities into
    ///                              `gp::rhi::getRHIGlobals()`.
    /// @return True when the logical `VkDevice` was created successfully; false otherwise.
    [[nodiscard]] bool initialize(bool applyToRHIGlobals = true) noexcept;

    /// @brief Blocks the calling thread until all submitted GPU work on `m_device` has completed.
    void waitUntilIdle() const noexcept;

    /// @brief Waits for device idle (if valid), destroys the `VkDevice`, and resets all internal handles.
    void destroy() noexcept;

    /// @brief Returns the underlying logical `VkDevice` handle.
    /// @return Active `VkDevice`, or `VK_NULL_HANDLE` if uninitialized.
    [[nodiscard]] VkDevice getHandle() const noexcept
    {
        return m_device;
    }

    /// @brief Reports whether a live logical `VkDevice` is currently owned.
    /// @return True when `m_device != VK_NULL_HANDLE`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_device != VK_NULL_HANDLE;
    }

    /// @brief Returns the bound `VkPhysicalDevice` handle.
    /// @return `VkPhysicalDevice` handle.
    [[nodiscard]] VkPhysicalDevice getPhysicalHandle() const noexcept
    {
        return m_physicalDevice;
    }

    /// @brief Returns the owning `VkInstance` handle.
    /// @return `VkInstance` handle.
    [[nodiscard]] VkInstance getInstanceHandle() const noexcept
    {
        return m_instance;
    }

    /// @brief Returns the negotiated Vulkan API version.
    /// @return Packed Vulkan API version.
    [[nodiscard]] gp::UInt32 getApiVersion() const noexcept
    {
        return m_apiVersion;
    }

    /// @brief Returns the normalized GPU vendor identifier.
    /// @return `gp::rhi::GpuVendorId`.
    [[nodiscard]] gp::rhi::GpuVendorId getVendorId() const noexcept
    {
        return m_vendorId;
    }

    /// @brief Returns the negotiated Vulkan 1.0..1.4 physical device features.
    /// @return Const reference to `VulkanPhysicalDeviceFeatures`.
    [[nodiscard]] const VulkanPhysicalDeviceFeatures& getFeatures() const noexcept
    {
        return m_features;
    }

    /// @brief Returns the queried Vulkan 1.0..1.4 physical device properties.
    /// @return Const reference to `VulkanPhysicalDeviceProperties`.
    [[nodiscard]] const VulkanPhysicalDeviceProperties& getProperties() const noexcept
    {
        return m_properties;
    }

    /// @brief Returns the physical device hardware limits (`VkPhysicalDeviceLimits`).
    /// @return Const reference to `VkPhysicalDeviceLimits`.
    [[nodiscard]] const VkPhysicalDeviceLimits& getLimits() const noexcept
    {
        return m_properties.getLimits();
    }

    /// @brief Returns the bitfield of enabled optional device extensions.
    /// @return Const reference to `OptionalVulkanDeviceExtensions`.
    [[nodiscard]] const OptionalVulkanDeviceExtensions& getOptionalExtensions() const noexcept
    {
        return m_optionalExtensions;
    }

    /// @brief Returns retained physical device extension properties.
    /// @return Const reference to `OptionalVulkanDeviceExtensionProperties`.
    [[nodiscard]] const OptionalVulkanDeviceExtensionProperties& getOptionalExtensionProperties() const noexcept
    {
        return m_optionalExtensionProperties;
    }

    /// @brief Returns mutable access to retained extension properties (used during extension discovery callbacks).
    /// @return Mutable reference to `OptionalVulkanDeviceExtensionProperties`.
    [[nodiscard]] OptionalVulkanDeviceExtensionProperties& getOptionalExtensionPropertiesMutable() noexcept
    {
        return m_optionalExtensionProperties;
    }

    /// @brief Returns all queue family properties enumerated from `m_physicalDevice`.
    /// @return Read-only view of `VkQueueFamilyProperties`.
    [[nodiscard]] gp::ConstVectorView<VkQueueFamilyProperties> getQueueFamilyProperties() const noexcept
    {
        return gp::ConstVectorView<VkQueueFamilyProperties>(m_queueFamilyProps.data(), m_queueFamilyProps.size());
    }

    /// @brief Returns the selected queue family indices and active queue family list.
    /// @return Const reference to `VulkanQueueFamilyIndices`.
    [[nodiscard]] const VulkanQueueFamilyIndices& getQueueFamilyIndices() const noexcept
    {
        return m_queueIndices;
    }

    /// @brief Returns the `VkQueue` handle for `type`, or `VK_NULL_HANDLE` if that queue role was not created.
    /// @param[in] type Queue role (`Graphics`, `AsyncCompute`, or `Transfer`).
    /// @return `VkQueue` handle or `VK_NULL_HANDLE`.
    [[nodiscard]] VkQueue getQueueHandle(const VulkanQueueType type) const noexcept
    {
        const gp::USize index = static_cast<gp::USize>(type);
        return (index < m_queueHandles.size()) ? m_queueHandles[index] : VK_NULL_HANDLE;
    }

    /// @brief Reports whether a dedicated async compute queue was created.
    /// @return True when `m_queueHandles[AsyncCompute] != VK_NULL_HANDLE`.
    [[nodiscard]] bool hasAsyncComputeQueue() const noexcept
    {
        return getQueueHandle(VulkanQueueType::AsyncCompute) != VK_NULL_HANDLE;
    }

    /// @brief Reports whether a dedicated transfer queue was created.
    /// @return True when `m_queueHandles[Transfer] != VK_NULL_HANDLE`.
    [[nodiscard]] bool hasTransferQueue() const noexcept
    {
        return getQueueHandle(VulkanQueueType::Transfer) != VK_NULL_HANDLE;
    }

    /// @brief Reports whether separate depth-stencil layouts, Synchronization2, and RenderPass2 are all active.
    /// @return True when parallel rendering submission is supported.
    [[nodiscard]] bool supportsParallelRendering() const noexcept
    {
        return (m_optionalExtensions.hasSeparateDepthStencilLayouts != 0u) &&
               (m_optionalExtensions.hasKHRSynchronization2 != 0u) && (m_optionalExtensions.hasKHRRenderPass2 != 0u);
    }

    /// @brief Reports whether images moved between the graphics and compute queue families need explicit queue
    ///        family ownership transfers.
    /// @return True unless `VK_KHR_maintenance9` reports optimal transfers between every active queue family.
    [[nodiscard]] bool needsQueueFamilyOwnershipTransfers() const noexcept
    {
        return m_queueIndices.needsQueueFamilyOwnershipTransfers;
    }

    /// @brief Takes a device-only `VkEvent` for a split barrier from the device pool, creating one if needed.
    /// @note Requires `supportsParallelRendering()`; split barriers are recorded with `synchronization2`.
    /// @return Unsignaled event, or `VK_NULL_HANDLE` if creation failed.
    [[nodiscard]] VkEvent getBarrierEvent() noexcept;

    /// @brief Returns a split-barrier event to the device pool once the GPU no longer uses it.
    /// @param[in] event Event previously returned by `getBarrierEvent()`.
    void releaseBarrierEvent(VkEvent event) noexcept;

    /// @brief Returns the split-barrier event pool owned by this device.
    /// @return Const reference to the pool.
    [[nodiscard]] const VulkanBarrierEventPool& getBarrierEventPool() const noexcept
    {
        return m_barrierEventPool;
    }

    /// @copydoc getBarrierEventPool() const
    [[nodiscard]] VulkanBarrierEventPool& getBarrierEventPool() noexcept
    {
        return m_barrierEventPool;
    }

    /// @brief Returns the populated Vulkan pixel format support and capability table.
    /// @return Const reference to `VulkanFormatSupportTable`.
    [[nodiscard]] const VulkanFormatSupportTable& getFormatSupportTable() const noexcept
    {
        return m_formatSupportTable;
    }

    /// @brief Returns the populated vertex attribute format support table.
    /// @return Const reference to `gp::rhi::VertexElementTypeSupportInfo`.
    [[nodiscard]] const gp::rhi::VertexElementTypeSupportInfo& getVertexElementTypeSupport() const noexcept
    {
        return m_vertexElementSupport;
    }

    /// @brief Returns cached `VkFormatProperties` for `format` (querying the physical device on demand for extension
    ///        formats).
    /// @param[in] format Native `VkFormat`.
    /// @return `VkFormatProperties` for `format`.
    [[nodiscard]] VkFormatProperties getFormatProperties(VkFormat format) const noexcept;

    /// @brief Returns the default `VkComponentMapping` swizzle for `format`.
    /// @param[in] format Engine `PixelFormat`.
    /// @return Const reference to `VkComponentMapping`.
    [[nodiscard]] const VkComponentMapping& getFormatComponentMapping(PixelFormat format) const noexcept;

    /// @brief Returns the device's `VulkanDebugMarkers` dispatch table.
    /// @return Const reference to `VulkanDebugMarkers`.
    [[nodiscard]] const VulkanDebugMarkers& getDebugMarkers() const noexcept
    {
        return m_debugMarkers;
    }

    /// @brief Assigns a UTF-8 debug name to a Vulkan object owned by this device.
    /// @param[in] objectType `VkObjectType` identifying the handle kind.
    /// @param[in] objectHandle 64-bit numeric handle value.
    /// @param[in] name Null-terminated UTF-8 debug name.
    void setObjectName(VkObjectType objectType, gp::UInt64 objectHandle, const char* name) const noexcept
    {
        m_debugMarkers.setObjectName(m_device, objectType, objectHandle, name);
    }

    /// @brief Returns the enabled device extension names passed to `vkCreateDevice`.
    /// @return Read-only view of null-terminated extension name strings.
    [[nodiscard]] gp::ConstVectorView<const char*> getEnabledExtensions() const noexcept
    {
        return gp::ConstVectorView<const char*>(m_enabledExtensions.data(), m_enabledExtensions.size());
    }

    /// @brief Returns the enabled device layer names passed to `vkCreateDevice`.
    /// @return Read-only view of null-terminated layer name strings.
    [[nodiscard]] gp::ConstVectorView<const char*> getEnabledLayers() const noexcept
    {
        return gp::ConstVectorView<const char*>(m_enabledLayers.data(), m_enabledLayers.size());
    }

    /// @brief Reports whether RenderDoc capture layer was detected during device setup.
    /// @return True if RenderDoc was detected.
    [[nodiscard]] bool isRenderDocFound() const noexcept
    {
        return m_renderDocFound;
    }

    /// @brief Returns the `VkResult` returned by `vkCreateDevice`.
    /// @return Last `VkResult`.
    [[nodiscard]] VkResult getLastCreateResult() const noexcept
    {
        return m_lastCreateResult;
    }

private:
    /// @brief Caches core `VkFormatProperties` and probes all engine pixel and vertex formats.
    void setupFormats() noexcept;

    /// @brief Queries `VK_KHR_maintenance9` queue ownership transfer properties when async compute is active.
    void queryQueueOwnershipTransferSupport() noexcept;

    VkInstance m_instance{ VK_NULL_HANDLE };
    VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
    VkDevice m_device{ VK_NULL_HANDLE };
    gp::UInt32 m_apiVersion{ 0u };
    bool m_supportsDebugUtilsExt{ false };
    bool m_renderDocFound{ false };
    bool m_useLegacyDebugMarkerExt{ false };
    VkResult m_lastCreateResult{ VK_SUCCESS };

    gp::rhi::GpuVendorId m_vendorId{ gp::rhi::GpuVendorId::NotQueried };
    VulkanPhysicalDeviceFeatures m_features{};
    VulkanPhysicalDeviceProperties m_properties{};
    OptionalVulkanDeviceExtensions m_optionalExtensions{};
    OptionalVulkanDeviceExtensionProperties m_optionalExtensionProperties{};
    gp::rhi::VariableRateShadingCapabilities m_vrsCaps{};

    gp::Vector<const char*> m_enabledExtensions{};
    gp::Vector<const char*> m_enabledLayers{};
    gp::Vector<VkQueueFamilyProperties> m_queueFamilyProps{};
    VulkanQueueFamilyIndices m_queueIndices{};
    gp::Array<VkQueue, kVulkanQueueTypeCount> m_queueHandles{};

    static constexpr gp::USize kCoreFormatRangeSize = 256u;
    gp::Array<VkFormatProperties, kCoreFormatRangeSize> m_cachedFormatProperties{};
    bool m_formatPropertiesCached{ false };
    VulkanFormatSupportTable m_formatSupportTable{};
    gp::rhi::VertexElementTypeSupportInfo m_vertexElementSupport{};

    VulkanDebugMarkers m_debugMarkers{};
    VulkanBarrierEventPool m_barrierEventPool{};
};

}   // namespace gp::rhi::vulkan
