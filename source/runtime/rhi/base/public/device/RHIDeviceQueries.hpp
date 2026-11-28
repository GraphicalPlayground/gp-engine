// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "RHIBase.hpp"

namespace gp::rhi
{

/// @brief Returns the canonical `GpuVendorId` reported by the active RHI adapter (`NotQueried` if uninitialized).
/// @return Resolved `GpuVendorId` (normalizes alternate Qualcomm vendor IDs to `GpuVendorId::Qualcomm`).
[[nodiscard]] GP_RHI_API GpuVendorId getRHIDeviceVendorId() noexcept;

/// @brief Returns the PCI device ID reported by the active RHI adapter (`0` if uninitialized).
/// @return 32-bit PCI device ID.
[[nodiscard]] GP_RHI_API gp::UInt32 getRHIDeviceId() noexcept;

/// @brief Reports whether the active RHI adapter is manufactured by AMD (`0x1002`).
/// @return True when the active vendor ID is `GpuVendorId::Amd`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceAMD() noexcept;

/// @brief Reports whether the active RHI adapter is manufactured by Intel (`0x8086`).
/// @return True when the active vendor ID is `GpuVendorId::Intel`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceIntel() noexcept;

/// @brief Reports whether the active RHI adapter is manufactured by NVIDIA (`0x10DE`).
/// @return True when the active vendor ID is `GpuVendorId::Nvidia`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceNVIDIA() noexcept;

/// @brief Reports whether the active RHI adapter is manufactured by Qualcomm (`0x5143` or `0x4D4F4351`).
/// @return True when the active vendor ID resolves to `GpuVendorId::Qualcomm`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceQualcomm() noexcept;

/// @brief Reports whether the active RHI adapter is manufactured by Apple (`0x106B`).
/// @return True when the active vendor ID is `GpuVendorId::Apple`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceApple() noexcept;

/// @brief Reports whether the active RHI adapter is manufactured by ARM (`0x13B5`).
/// @return True when the active vendor ID is `GpuVendorId::Arm`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceARM() noexcept;

/// @brief Reports whether the active RHI adapter is a Samsung-AMD Xclipse GPU (`0x144D`).
/// @return True when the active vendor ID is `GpuVendorId::SamsungAMD`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceSamsungAMD() noexcept;

/// @brief Reports whether the active RHI adapter is a Microsoft software/WARP adapter (`0x1414`).
/// @return True when the active vendor ID is `GpuVendorId::Microsoft`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceMicrosoft() noexcept;

/// @brief Reports whether the active RHI adapter is a Mesa software/driver implementation (`0x10005`).
/// @return True when the active vendor ID is `GpuVendorId::Mesa`.
[[nodiscard]] GP_RHI_API bool isRHIDeviceMesa() noexcept;

/// @brief Reports whether vendor-specific GPU crash debugging tools are enabled via command line, CVar, or override.
/// @return True when GPU crash debugging is enabled.
[[nodiscard]] GP_RHI_API bool useGPUCrashDebugging() noexcept;

/// @brief Overrides whether GPU crash debugging is enabled (`setUseGPUCrashDebugging` takes precedence over CVars).
/// @param[in] enabled True to enable GPU crash debugging; false to disable it.
GP_RHI_API void setUseGPUCrashDebugging(bool enabled) noexcept;

/// @brief Clears any programmatic GPU crash debugging override so command-line and CVar settings take effect.
GP_RHI_API void clearUseGPUCrashDebuggingOverride() noexcept;

/// @brief Reports whether engine-managed GPU crash breadcrumb tracking is enabled.
/// @return True when GPU crash breadcrumbs are enabled (default `true` unless `-nogpucrashdebugging` is set).
[[nodiscard]] GP_RHI_API bool useGPUCrashBreadcrumbs() noexcept;

/// @brief Overrides whether engine-managed GPU crash breadcrumb tracking is enabled.
/// @param[in] enabled True to enable GPU crash breadcrumbs; false to disable them.
GP_RHI_API void setUseGPUCrashBreadcrumbs(bool enabled) noexcept;

/// @brief Clears any programmatic GPU crash breadcrumb override so command-line and CVar settings take effect.
GP_RHI_API void clearUseGPUCrashBreadcrumbsOverride() noexcept;

}   // namespace gp::rhi
