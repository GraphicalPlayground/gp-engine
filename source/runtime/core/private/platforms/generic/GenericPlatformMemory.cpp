// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/GenericPlatformMemory.hpp"
#include "memory/backends/MallocAnsi.hpp"
#include "memory/MemoryConstants.hpp"

namespace gp::platform::generic
{

memory::Malloc* Memory::getDefaultAllocator()
{
    static memory::Malloc* instance = nullptr;
    if (instance != nullptr) [[likely]]
    {
        return instance;
    }
    instance = new memory::MallocAnsi();
    return instance;
}

memory::PlatformConstants Memory::getPlatformConstants()
{
    static memory::PlatformConstants constants{};
    return constants;
}

}   // namespace gp::platform::generic
