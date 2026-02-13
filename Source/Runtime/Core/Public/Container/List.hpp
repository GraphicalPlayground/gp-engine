// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Memory/MemoryForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Container
{

/// \brief
/// \tparam T
/// \tparam Alloc
template <typename T, Concepts::IsAllocator Alloc = Memory::TAllocator<T>>
class TList
{
private:
};

}   // namespace GP::Container
