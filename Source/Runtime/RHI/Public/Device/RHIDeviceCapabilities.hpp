// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Device/RHIDeviceFeatures.hpp"
#include "Device/RHIDeviceLimits.hpp"
#include "Memory/RHIMemoryHeapInfo.hpp"
#include "RHIBuild.hpp"
#include "RHIEnums.hpp"
#include "Templates/Utility/Enums.hpp"

namespace GP
{

/// @brief Comprehensive device capabilities structure returned by IRHIDevice::GetCaps().
struct FRHIDeviceCapabilities
{
    char deviceName[256]{};
    char driverVersion[64]{};
    UInt32 vendorID{ 0u };
    UInt32 deviceID{ 0u };

    FRHIDeviceLimits limits{};
    FRHIDeviceFeatures features{};

    /// @brief Populated memory heap descriptors (up to 8 heaps are common on desktop GPUs).
    FRHIMemoryHeapInfo memoryHeaps[16]{};
    UInt32 memoryHeapCount{ 0u };

    UInt64 deviceLocalMemoryBytes{ 0ull };   //<! Total VRAM available.
    UInt64 sharedMemoryBytes{ 0ull };        //<! Shared system RAM accessible by GPU.

    /// @brief Number of hardware queues per family.
    UInt32 graphicsQueueCount{ 0u };
    UInt32 computeQueueCount{ 0u };
    UInt32 transferQueueCount{ 0u };

    /// @brief Whether this device supports the given format for the specified use.
    EFormatSupport formatSupport[static_cast<UInt32>(EFormat::Count)]{};
};

}   // namespace GP
