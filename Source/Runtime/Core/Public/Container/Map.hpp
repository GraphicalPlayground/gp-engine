// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Memory/MemoryForward.hpp"
#include "Templates/Concepts.hpp"

namespace GP::Container
{

/// \brief
/// \tparam Key
/// \tparam Value
/// \tparam Alloc
template <typename Key, typename Value, GP::Concepts::IsAllocator Alloc>
class TMap
{
private:
};

}   // namespace GP::Container
