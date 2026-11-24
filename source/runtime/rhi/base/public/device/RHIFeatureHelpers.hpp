// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "shaders/ShaderForward.hpp"
#include "views/ViewEnums.hpp"

namespace gp::rhi
{

/// @brief Reports whether SM6 feature level selection requires hardware 64-bit typed atomics.
/// @return True when 64-bit atomics are required for SM6 (default `true`).
[[nodiscard]] GP_RHI_API bool isAtomic64RequiredForSM6() noexcept;

/// @brief Overrides whether SM6 selection requires 64-bit typed atomics.
/// @param[in] required True to require 64-bit atomics for SM6; false to allow SM6 without them.
GP_RHI_API void setAtomic64RequiredForSM6(bool required) noexcept;

/// @brief Reports whether vendor-specific device extensions and driver APIs are permitted.
/// @details Returns `false` if the `-novendordevice` command-line flag is present or if disabled via console variable
///          or `setAllowVendorDevice(false)`.
/// @return True when vendor device extensions are allowed (default `true`).
[[nodiscard]] GP_RHI_API bool allowVendorDevice() noexcept;

/// @brief Enables or disables vendor-specific device extensions and driver APIs.
/// @param[in] allow True to allow vendor device extensions; false to disable them.
GP_RHI_API void setAllowVendorDevice(bool allow) noexcept;

/// @brief Reports whether full ray tracing shader pipelines (beyond inline ray queries) are enabled.
/// @return True when ray tracing shaders are enabled (default `true`).
[[nodiscard]] GP_RHI_API bool areRayTracingShadersEnabled() noexcept;

/// @brief Enables or disables ray tracing shader pipelines.
/// @param[in] enabled True to enable ray tracing shaders; false to disable them.
GP_RHI_API void setRayTracingShadersEnabled(bool enabled) noexcept;

/// @brief Returns the startup bindless resource configuration resolved for `platform`.
/// @param[in] platform Target shader compilation platform.
/// @return Active `view::BindlessConfiguration` tier (default `view::BindlessConfiguration::Disabled`).
[[nodiscard]] GP_RHI_API view::BindlessConfiguration
    getBindlessConfigurationOnStartup(shader::ShaderPlatform platform) noexcept;

/// @brief Overrides the startup bindless resource configuration returned by `getBindlessConfigurationOnStartup()`.
/// @param[in] configuration Target `view::BindlessConfiguration` tier.
GP_RHI_API void setBindlessConfigurationOnStartup(view::BindlessConfiguration configuration) noexcept;

}   // namespace gp::rhi
