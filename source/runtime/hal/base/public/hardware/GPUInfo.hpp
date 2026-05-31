// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Enumeration of common GPU vendors.
enum class GPUVendor : UInt8
{
    Unknown = 0,
    NVIDIA,
    AMD,
    Intel,
    Apple,
    Qualcomm,
    ARM,
    Imagination,
    Microsoft,   // For software rasterizers like WARP
    Other
};

/// @brief Contains information about the GPU.
struct GPUInfo
{
    /// @brief The GPU vendor.
    GPUVendor Vendor{ GPUVendor::Unknown };

    /// @brief The device name or model of the GPU.
    char deviceName[128]{ 0 };

    /// @brief The device id.
    UInt32 deviceID{ 0u };

    /// @brief The vendor id.
    UInt32 vendorID{ 0u };

    /// @brief The amount of dedicated video memory in bytes.
    UInt64 dedicatedVRAM{ 0u };

    /// @brief The amount of shared system memory in bytes.
    UInt64 sharedSystemMemory{ 0u };

    /// @brief Driver version major.
    UInt32 driverVersionMajor{ 0u };

    /// @brief Driver version minor.
    UInt32 driverVersionMinor{ 0u };
};

}   // namespace gp::hal
