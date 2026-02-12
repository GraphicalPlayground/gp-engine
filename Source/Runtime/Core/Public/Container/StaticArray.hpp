// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Memory/MemoryForward.hpp"

namespace GP::Container
{

/// \brief
/// \tparam T
/// \tparam N
/// \tparam Alloc
template <typename T, SizeT N, typename Alloc = Memory::TAllocator<T>>
class TStaticArray
{
private:
};

}   // namespace GP::Container
