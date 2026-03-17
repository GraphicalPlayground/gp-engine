// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "RHIBuild.hpp"

namespace GP
{

/// @brief Memory heap information for a single hardware memory heap.
struct FRHIMemoryHeapInfo
{
    UInt64 totalSize{ 0ull };
    UInt64 availableSize{ 0ull };
    bool isDeviceLocal{ false };
};

}   // namespace GP
