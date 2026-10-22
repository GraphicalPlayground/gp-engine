// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "Launch.hpp"
#include "memory/GlobalMemory.hpp"
#include "platforms/base/PlatformMemory.hpp"
#include "platforms/base/system/Thread.hpp"
#include <cstdlib>
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

unsigned int otherThreadFunction(void* userData)
{
    std::cout << "Hello from another thread! User data: " << static_cast<const char*>(userData) << std::endl;
    auto currentThreadId = gp::platform::Thread::currentThreadId();
    std::cout << "Current thread ID (other): " << currentThreadId << std::endl;
    return 0;
}

int launch([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    // Dump the arguments for debugging purposes
    std::cout << "Launching with arguments:" << std::endl;
    for (int i = 0; i < argc; ++i)
    {
        std::cout << "\targv[" << i << "]: " << argv[i] << std::endl;
    }

    std::cout << "Launching editor..." << std::endl;
    std::cout << "Is Tracy enabled? " << (TRACY_ENABLE ? "Yes" : "No") << std::endl;
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

    auto currentThreadId = gp::platform::Thread::currentThreadId();
    std::cout << "Current thread ID (main): " << currentThreadId << std::endl;

    // Create a thread to test the Thread class
    const char* userData = "MY_USER_DATA";
    gp::platform::ThreadDesc desc{ .entry = otherThreadFunction,
                                   .userData = const_cast<void*>(static_cast<const void*>(userData)) };

    gp::platform::Thread thread;
    const bool started = (thread.start(desc) == gp::platform::ThreadResult::Success);
    if (!started)
    {
        std::cerr << "Failed to start thread!" << std::endl;
    }
    else
    {
        std::cout << "Thread started successfully." << std::endl;
    }

    // Wait for user input before exiting, allows us to actually attach the profiler if needed
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    if (started)
    {
        (void)thread.join();
        std::cout << "Thread joined successfully." << std::endl;
    }

    return valid ? 0 : 1;
}

}   // namespace gp
