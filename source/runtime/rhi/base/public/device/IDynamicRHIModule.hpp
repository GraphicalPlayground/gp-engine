// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"            // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DeviceForward.hpp"   // IWYU pragma: keep
#include "modules/IModule.hpp"
#include "RHIBase.hpp"

namespace gp::rhi
{

/// @brief Module interface implemented by every loadable RHI backend plugin (`rhi/null`, `rhi/vulkan`, `rhi/d3d12`).
/// @details Allows engine startup to probe candidate RHI backend modules via `isSupported()` and instantiate the
///          selected `DynamicRHI` device through `createRHI()` without hard-coding backend link dependencies.
class GP_RHI_API IDynamicRHIModule : public gp::modules::IModule
{
public:
    /// @brief 64-bit ASCII interface tag (`"GPRHIMOD"`) verifying that a loaded `IModule` implements
    ///        `IDynamicRHIModule` safely with RTTI disabled.
    static constexpr gp::UInt64 kInterfaceTag = 0x47505248494D4F44ULL;

public:
    /// @brief Constructs a dynamic RHI module.
    constexpr IDynamicRHIModule() noexcept = default;

    /// @copydoc gp::modules::IModule::~IModule()
    virtual ~IDynamicRHIModule() noexcept override = default;

    IDynamicRHIModule(const IDynamicRHIModule&) = delete;
    IDynamicRHIModule& operator=(const IDynamicRHIModule&) = delete;
    IDynamicRHIModule(IDynamicRHIModule&&) = delete;
    IDynamicRHIModule& operator=(IDynamicRHIModule&&) = delete;

public:
    /// @copydoc gp::modules::IModule::getInterfaceTag()
    [[nodiscard]] virtual gp::UInt64 getInterfaceTag() const noexcept override final
    {
        return kInterfaceTag;
    }

    /// @brief Checks whether this backend can run on the current operating system, driver, and hardware.
    /// @return True if `createRHI()` can initialize a usable device.
    [[nodiscard]] virtual bool isSupported() const noexcept = 0;

    /// @brief Checks whether this backend supports the requested `requestedFeatureLevel`.
    /// @param[in] requestedFeatureLevel Target shader model / feature tier.
    /// @return True when supported (defaults to `isSupported()`).
    [[nodiscard]] virtual bool isSupported(FeatureLevel requestedFeatureLevel) const noexcept;

    /// @brief Allocates and returns a new concrete `DynamicRHI` device instance for this backend.
    /// @param[in] requestedFeatureLevel Target feature level (`FeatureLevel::Count` selects the backend's highest
    /// tier).
    /// @return Newly allocated `DynamicRHI*` owned by the caller / `rhiExit()`.
    [[nodiscard]] virtual DynamicRHI* createRHI(FeatureLevel requestedFeatureLevel = FeatureLevel::Count) noexcept = 0;
};

}   // namespace gp::rhi
