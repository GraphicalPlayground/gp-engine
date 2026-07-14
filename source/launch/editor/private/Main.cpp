// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"
#include "Launch.hpp"
#include "memory/GlobalMemory.hpp"
#include "platforms/base/PlatformMemory.hpp"
#include <iostream>

namespace gp
{

void dumpPlatformConstants()
{
    auto constants = gp::platform::Memory::getPlatformConstants();

    std::cout << "Platform Memory Constants:" << std::endl;
    std::cout << "\tPhysical Memory (bytes): " << constants.physicalMemoryBytes << std::endl;
    std::cout << "\tVirtual Memory (bytes): " << constants.virtualMemoryBytes << std::endl;
    std::cout << "\tStandard Page Size (bytes): " << constants.standardPageSize << std::endl;
    std::cout << "\tBinned Page Size (bytes): " << constants.binnedPageSize << std::endl;
    std::cout << "\tStandard Allocation Granularity (bytes): " << constants.standardAllocationGranularity << std::endl;
    std::cout << "\tBinned Allocation Granularity (bytes): " << constants.binnedAllocationGranularity << std::endl;
    std::cout << "\tAddress Space Start: " << constants.addressSpaceStart << std::endl;
    std::cout << "\tAddress Space Size (bytes): " << constants.addressSpaceSizeBytes << std::endl;
    std::cout << "\tTotal Physical Memory (GB): " << constants.totalPhysicalMemoryGB << std::endl;
}

int launch([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    std::cout << "Launching editor..." << std::endl;
    dumpPlatformConstants();

    // Try allocating something with getGlobalAllocator
    auto allocator = gp::memory::getGlobalMalloc();

    std::cout << "Allocating 1024 bytes..." << std::endl;
    void* ptr = allocator->allocate(1024);
    if (ptr == nullptr)
    {
        std::cerr << "Allocation failed!" << std::endl;
        return 1;
    }

    std::cout << "Allocation successful: " << ptr << std::endl;
    std::memset(ptr, 0xAB, 1024);

    auto* bytes = static_cast<unsigned char*>(ptr);
    bool valid = true;
    for (size_t i = 0; i < 1024; ++i)
    {
        if (bytes[i] != 0xAB)
        {
            std::cerr << "Memory corruption at byte " << i << std::endl;
            valid = false;
            break;
        }
    }

    if (valid)
    {
        std::cout << "Memory write/read test passed." << std::endl;
    }

    allocator->deallocate(ptr);

    std::cout << "Memory released." << std::endl;

    return valid ? 0 : 1;
}

}   // namespace gp
