// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/base/Convertions.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/MemoryConstants.hpp"
#include "platforms/base/PlatformMemory.hpp"
#include "platforms/windows/WindowsPlatformMemory.hpp"
#include <Windows.h>

namespace gp::platform::windows
{

memory::PlatformConstants Memory::getPlatformConstants()
{
    static memory::PlatformConstants constants{};

    if (constants.physicalMemoryBytes == 0ull) [[unlikely]]
    {
        MEMORYSTATUSEX memoryStatusEx{};
        platform::Memory::zeroMemory(&memoryStatusEx, sizeof(memoryStatusEx));
        memoryStatusEx.dwLength = sizeof(memoryStatusEx);
        ::GlobalMemoryStatusEx(&memoryStatusEx);

        SYSTEM_INFO systemInfo{};
        platform::Memory::zeroMemory(&systemInfo, sizeof(systemInfo));
        ::GetSystemInfo(&systemInfo);

        constants.physicalMemoryBytes = memoryStatusEx.ullTotalPhys;
        constants.virtualMemoryBytes = memoryStatusEx.ullTotalPageFile;
        constants.binnedPageSize = systemInfo.dwAllocationGranularity;
        constants.binnedAllocationGranularity = systemInfo.dwPageSize;
        constants.standardAllocationGranularity = systemInfo.dwAllocationGranularity;
        constants.standardPageSize = systemInfo.dwPageSize;
        constants.addressSpaceStart = math::roundUpToPowerOfTwo<UInt64>(constants.physicalMemoryBytes);
        constants.totalPhysicalMemoryGB = math::convert::bytesToGigabytes<UInt32>(constants.physicalMemoryBytes);
    }

    return constants;
}

}   // namespace gp::platform::windows
