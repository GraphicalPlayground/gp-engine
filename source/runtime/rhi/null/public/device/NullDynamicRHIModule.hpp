// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DeviceForward.hpp"
#include "device/IDynamicRHIModule.hpp"
#include "RHINull.hpp"

namespace gp::rhi::null
{

/// @brief Dynamic RHI module exposing the headless `NullDynamicRHI` backend.
/// @details Always reports `isSupported() == true` on every host platform so dedicated servers, commandlets, and
///          automated tests can exercise the full RHI contract without GPU drivers or a windowing system.
class GP_RHI_NULL_API NullDynamicRHIModule final : public IDynamicRHIModule
{
public:
    /// @brief Constructs the null RHI module.
    constexpr NullDynamicRHIModule() noexcept = default;

    /// @copydoc IDynamicRHIModule::~IDynamicRHIModule()
    virtual ~NullDynamicRHIModule() noexcept override = default;

public:
    /// @copydoc gp::modules::IModule::supportsDynamicReloading()
    [[nodiscard]] virtual bool supportsDynamicReloading() const noexcept override
    {
        return false;
    }

    using IDynamicRHIModule::isSupported;

    /// @copydoc IDynamicRHIModule::isSupported()
    [[nodiscard]] virtual bool isSupported() const noexcept override
    {
        return true;
    }

    /// @copydoc IDynamicRHIModule::createRHI()
    [[nodiscard]] virtual DynamicRHI*
        createRHI(FeatureLevel requestedFeatureLevel = FeatureLevel::Count) noexcept override;
};

}   // namespace gp::rhi::null
