// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi
{

/// @brief Graphics backend API implemented by the active RHI.
enum class InterfaceType : gp::UInt8
{
    /// @brief No RHI backend is initialized yet.
    NotSet = 0,
    Hidden,
    Null,
    D3D11,
    D3D12,
    Vulkan,
    Metal,
    OpenGL
};

/// @brief Runtime availability of an RHI hardware or driver feature.
enum class FeatureSupport : gp::UInt8
{
    Unsupported = 0,
    RuntimeDependent,
    RuntimeGuaranteed
};

/// @brief Hardware support level for static shader binding layouts.
enum class StaticShaderBindingLayoutSupport : gp::UInt8
{
    Unsupported = 0,
    RayTracingOnly,
    AllShaderTypes
};

/// @brief PCI vendor IDs for known GPU manufacturers and software rasterizers.
enum class GpuVendorId : gp::UInt32
{
    Unknown = 0xFFFFFFFFu,
    NotQueried = 0x00000000u,
    Amd = 0x1002u,
    ImgTec = 0x1010u,
    Nvidia = 0x10DEu,
    Arm = 0x13B5u,
    Broadcom = 0x14E4u,
    Qualcomm = 0x5143u,
    Intel = 0x8086u,
    Apple = 0x106Bu,
    Vivante = 0x7A05u,
    VeriSilicon = 0x1EB1u,
    SamsungAMD = 0x144Du,
    Microsoft = 0x1414u,
    Kazan = 0x10003u,
    Codeplay = 0x10004u,
    Mesa = 0x10005u
};

/// @brief Converts a raw PCI vendor ID into a recognized `GpuVendorId`.
/// @param[in] vendorId Raw 32-bit PCI vendor identifier reported by the driver or OS.
/// @return Matching `GpuVendorId`, `GpuVendorId::NotQueried` if zero, or `GpuVendorId::Unknown` if unrecognized.
[[nodiscard]] inline constexpr GpuVendorId convertToGpuVendorId(const gp::UInt32 vendorId) noexcept
{
    switch (static_cast<GpuVendorId>(vendorId))
    {
    case GpuVendorId::NotQueried:
        return GpuVendorId::NotQueried;

    case GpuVendorId::Amd:
    case GpuVendorId::Kazan:
    case GpuVendorId::Codeplay:
    case GpuVendorId::Mesa:
    case GpuVendorId::ImgTec:
    case GpuVendorId::Nvidia:
    case GpuVendorId::Arm:
    case GpuVendorId::Broadcom:
    case GpuVendorId::Qualcomm:
    case GpuVendorId::Intel:
    case GpuVendorId::Apple:
    case GpuVendorId::Vivante:
    case GpuVendorId::VeriSilicon:
    case GpuVendorId::SamsungAMD:
    case GpuVendorId::Microsoft:
        return static_cast<GpuVendorId>(vendorId);

    case GpuVendorId::Unknown:
    default:
        return GpuVendorId::Unknown;
    }
}

}   // namespace gp::rhi
