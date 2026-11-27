// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"            // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DeviceForward.hpp"   // IWYU pragma: keep
#include "device/IDynamicRHIModule.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Dynamic RHI module exposing the Vulkan backend.
/// @details Support checks never create a logical device: they rely on a cached adapter probe (loader present, an
///          adapter passes the selection policy, the requested tier's features are present), so the engine can ask
///          every backend module cheaply before picking one. `createRHI()` only allocates the RHI; the device is
///          created later by `rhiInit()`.
class GP_RHI_VULKAN_API VulkanDynamicRHIModule final : public IDynamicRHIModule
{
public:
    /// @brief Constructs the Vulkan RHI module.
    constexpr VulkanDynamicRHIModule() noexcept = default;

    /// @copydoc IDynamicRHIModule::~IDynamicRHIModule()
    virtual ~VulkanDynamicRHIModule() noexcept override = default;

public:
    /// @copydoc gp::modules::IModule::supportsDynamicReloading()
    /// @note The RHI owns process-wide loader state, so the module cannot be swapped at runtime.
    [[nodiscard]] virtual bool supportsDynamicReloading() const noexcept override
    {
        return false;
    }

    /// @copydoc IDynamicRHIModule::isSupported()
    /// @note Probes the loader and the adapters once per process.
    [[nodiscard]] virtual bool isSupported() const noexcept override;

    /// @copydoc IDynamicRHIModule::isSupported(FeatureLevel)
    /// @note Checks the selected adapter against the tier when the platform enables profile checks; `ES3_1` is refused
    ///       in editor builds.
    [[nodiscard]] virtual bool isSupported(FeatureLevel requestedFeatureLevel) const noexcept override;

    /// @copydoc IDynamicRHIModule::createRHI()
    /// @note Applies the platform feature-level overrides, publishes the result in
    ///       `RHIGlobals::maxSupportedFeatureLevel`, and returns an uninitialized `VulkanDynamicRHI`.
    [[nodiscard]] virtual DynamicRHI*
        createRHI(FeatureLevel requestedFeatureLevel = FeatureLevel::Count) noexcept override;
};

}   // namespace gp::rhi::vulkan
