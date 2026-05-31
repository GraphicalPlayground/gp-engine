// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief The type of storage device.
enum class StorageType : UInt8
{
    Unknown = 0,
    HDD,   // Mechanical platter (Slow seek times)
    SSD,   // Solid State Drive (SATA)
    NVMe   // Ultra-fast PCIe SSD
};

/// @brief Information about a storage device, such as a hard drive or SSD.
struct DiskInfo
{
    /// @brief The type of storage device.
    StorageType type{ StorageType::Unknown };

    /// @brief The amount of available space in bytes.
    UInt64 availableSpace{ 0u };

    /// @brief The total amount of space in bytes.
    UInt64 totalSpace{ 0u };

    /// @brief The size of each sector in bytes.
    /// @note Crucial for direct unbuffered asynchronous I/O alignment.
    UInt32 sectorSize{ 4096u };
};

}   // namespace gp::hal
