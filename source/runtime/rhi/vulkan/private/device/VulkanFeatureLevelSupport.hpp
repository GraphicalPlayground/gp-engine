// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief What the feature-level checks need to know about one physical device.
struct VulkanAdapterCapabilities final
{
    /// @brief Usable API version: the lower of the device version and the instance version.
    gp::UInt32 apiVersion{ 0u };

    /// @brief Core Vulkan 1.0 features reported by the device.
    VkPhysicalDeviceFeatures features{};

    /// @brief True for software rasterizers (`VK_PHYSICAL_DEVICE_TYPE_CPU`).
    bool isCpu{ false };
};

/// @brief Snapshot of the host's Vulkan support, gathered without creating a logical device.
/// @details The probe creates a throwaway instance, enumerates every adapter, records its capabilities and runs the
///          same adapter selection policy the RHI will use, so `isSupported()` agrees with `createRHI()`.
struct VulkanAdapterProbe final
{
    /// @brief True when the Vulkan loader library could be opened.
    bool hasLoader{ false };

    /// @brief True when adapters were actually enumerated.
    /// @details False when enumeration was skipped because a live instance already owns the process-wide instance
    ///          entry points (creating and destroying a second instance would clear them). Such a probe only proves
    ///          the loader exists and is never cached.
    bool isComplete{ false };

    /// @brief Enumerated adapters, in driver order.
    gp::Vector<VulkanAdapterCapabilities> adapters{};

    /// @brief Index in `adapters` the selection policy picks; `-1` when no adapter qualifies.
    gp::Int32 selectedAdapter{ -1 };

    /// @brief Returns the adapter the RHI would run on.
    /// @return Selected adapter, or `nullptr` when none qualifies.
    [[nodiscard]] const VulkanAdapterCapabilities* getSelectedAdapter() const noexcept
    {
        if ((selectedAdapter < 0) || (static_cast<gp::USize>(selectedAdapter) >= adapters.size()))
        {
            return nullptr;
        }
        return &adapters[static_cast<gp::USize>(selectedAdapter)];
    }
};

/// @brief Reports whether a device meets the engine's Vulkan requirements for a feature level.
/// @details The tiers are an engine approximation of what each shading model relies on, checked against core
///          Vulkan 1.0 features and the API version only:
///          - `ES3_1`: Vulkan 1.1.
///          - `SM5`: Vulkan 1.1 plus independent blending, cube map arrays, anisotropic filtering, stores and atomics
///            in fragment shaders, extended image gathers and 32-bit draw indices.
///          - `SM6`: Vulkan 1.3 plus everything `SM5` needs, 64-bit and 16-bit shader integers, multi-draw indirect
///            and a first instance in indirect draws.
///          The removed tiers and the `Count` sentinel are never met.
/// @param[in] featureLevel Feature level to check.
/// @param[in] deviceApiVersion Usable API version of the device.
/// @param[in] features Core features reported by the device.
/// @return True when every requirement of `featureLevel` holds.
[[nodiscard]] GP_RHI_VULKAN_API bool meetsVulkanFeatureLevelRequirements(
    FeatureLevel featureLevel, gp::UInt32 deviceApiVersion, const VkPhysicalDeviceFeatures& features
) noexcept;

/// @brief Returns the highest feature level a device meets.
/// @param[in] deviceApiVersion Usable API version of the device.
/// @param[in] features Core features reported by the device.
/// @return `SM6`, `SM5` or `ES3_1`; `Count` when the device meets none.
[[nodiscard]] GP_RHI_VULKAN_API FeatureLevel
    getMaxVulkanFeatureLevel(gp::UInt32 deviceApiVersion, const VkPhysicalDeviceFeatures& features) noexcept;

/// @brief Decides whether the Vulkan backend can be created on the probed host.
/// @param[in] probe Host snapshot.
/// @return True when the loader exists and either an adapter qualifies or enumeration was skipped.
[[nodiscard]] GP_RHI_VULKAN_API bool isVulkanBackendAvailable(const VulkanAdapterProbe& probe) noexcept;

/// @brief Decides whether the Vulkan backend can run at a feature level on the probed host.
/// @details `ES3_1` is refused in editor builds (the editor needs a desktop tier). When `enforceRequirements` is false
///          (profile checks disabled) or the probe is incomplete, availability alone decides. `Count` means "the best
///          tier available" and only needs availability.
/// @param[in] featureLevel Requested feature level.
/// @param[in] probe Host snapshot.
/// @param[in] enforceRequirements True to check the selected adapter against `meetsVulkanFeatureLevelRequirements()`.
/// @param[in] isEditor True in editor builds.
/// @return True when the backend can be created for `featureLevel`.
[[nodiscard]] GP_RHI_VULKAN_API bool isVulkanFeatureLevelAvailable(
    FeatureLevel featureLevel, const VulkanAdapterProbe& probe, bool enforceRequirements, bool isEditor
) noexcept;

/// @brief Turns a platform-resolved feature level into the one the RHI advertises.
/// @details A concrete request is kept as is. `Count` ("best available") becomes the highest tier the selected adapter
///          meets; when the probe is incomplete, has no selected adapter, or the adapter meets no tier, `SM5` is
///          assumed, matching the default of `RHIGlobals::maxSupportedFeatureLevel`.
/// @param[in] featureLevel Feature level after platform overrides.
/// @param[in] probe Host snapshot.
/// @return Feature level to advertise; never `Count`.
[[nodiscard]] GP_RHI_VULKAN_API FeatureLevel
    resolveVulkanFeatureLevel(FeatureLevel featureLevel, const VulkanAdapterProbe& probe) noexcept;

/// @brief Returns the host snapshot, probing it once per process.
/// @details A complete probe is cached; an incomplete one (another instance is alive) is recomputed on each call. The
///          loader library is only unloaded afterwards when the probe was the one that loaded it.
/// @note Thread-safe.
/// @return Host snapshot.
[[nodiscard]] GP_RHI_VULKAN_API VulkanAdapterProbe getVulkanAdapterProbe() noexcept;

/// @brief Forgets the cached host snapshot so the next `getVulkanAdapterProbe()` probes again.
/// @note Intended for tests and for hot-plugged adapters.
GP_RHI_VULKAN_API void resetVulkanAdapterProbeCache() noexcept;

}   // namespace gp::rhi::vulkan
