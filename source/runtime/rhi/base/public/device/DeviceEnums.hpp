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
    /// @brief Platform-specific private graphics backend.
    Hidden,
    /// @brief Headless null RHI backend for servers and offline tools.
    Null,
    /// @brief Direct3D 11 graphics backend.
    D3D11,
    /// @brief Direct3D 12 graphics backend.
    D3D12,
    /// @brief Vulkan graphics backend.
    Vulkan,
    /// @brief Apple Metal graphics backend.
    Metal,
    /// @brief Platform AGX graphics backend.
    Agx,
    /// @brief OpenGL graphics backend.
    OpenGL
};

/// @brief Shader model and hardware capability tier targeted by the renderer.
enum class FeatureLevel : gp::Int32
{
    /// @brief Legacy OpenGL ES 2.0 tier (removed; retained for enum index stability).
    ES2Removed = 0,
    /// @brief Mobile OpenGL ES 3.1 / Vulkan mobile feature tier.
    ES3_1,
    /// @brief Legacy Shader Model 4 tier (removed; retained for enum index stability).
    SM4Removed,
    /// @brief Desktop Shader Model 5 feature tier.
    SM5,
    /// @brief Modern Desktop Shader Model 6 feature tier.
    SM6
};

/// @brief Total number of enumerators defined in `FeatureLevel`.
inline constexpr gp::UInt32 kFeatureLevelCount = static_cast<gp::UInt32>(FeatureLevel::SM6) + 1u;

/// @brief High-level rendering pipeline architecture selected for a target feature level.
enum class ShadingPath : gp::UInt8
{
    /// @brief Deferred shading pipeline with G-buffer passes.
    Deferred = 0,
    /// @brief Forward shading pipeline.
    Forward,
    /// @brief Mobile-optimized shading pipeline.
    Mobile
};

/// @brief Total number of enumerators defined in `ShadingPath`.
inline constexpr gp::UInt32 kShadingPathCount = static_cast<gp::UInt32>(ShadingPath::Mobile) + 1u;

/// @brief Runtime availability of an RHI hardware or driver feature.
enum class FeatureSupport : gp::UInt8
{
    /// @brief Feature is never available on the current backend or platform.
    Unsupported = 0,
    /// @brief Feature availability depends on runtime adapter and driver queries.
    RuntimeDependent,
    /// @brief Feature is guaranteed to be available at runtime.
    RuntimeGuaranteed
};

/// @brief Total number of enumerators in `FeatureSupport` and minimum bit width to pack it.
inline constexpr gp::UInt32 kFeatureSupportCount = static_cast<gp::UInt32>(FeatureSupport::RuntimeGuaranteed) + 1u;
inline constexpr gp::UInt32 kFeatureSupportNumBits = 2u;
static_assert((1u << kFeatureSupportNumBits) >= kFeatureSupportCount);

/// @brief Hardware support level for static shader binding layouts.
enum class StaticShaderBindingLayoutSupport : gp::UInt8
{
    /// @brief Static shader binding layouts are not supported.
    Unsupported = 0,
    /// @brief Static shader binding layouts are supported for ray tracing shaders only.
    RayTracingOnly,
    /// @brief Static shader binding layouts are supported across all shader stages.
    AllShaderTypes
};

/// @brief Minimum bit width required to store any `StaticShaderBindingLayoutSupport` enumerator.
inline constexpr gp::UInt32 kStaticShaderBindingLayoutSupportNumBits = 2u;
static_assert(
    (1u << kStaticShaderBindingLayoutSupportNumBits) >
    static_cast<gp::UInt32>(StaticShaderBindingLayoutSupport::AllShaderTypes)
);

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
