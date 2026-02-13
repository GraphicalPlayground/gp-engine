// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

namespace GP::Memory
{

template <typename T>
class TAllocator
{
public:
    GP_NODISCARD void* Allocate(SizeT size, SizeT align = alignof(T));
    void Deallocate(void* ptr);
};

}   // namespace GP::Memory
