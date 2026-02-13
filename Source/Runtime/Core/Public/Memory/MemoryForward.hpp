// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

namespace GP::Memory
{

template <typename T>
class TAllocator;

template <SizeT BufferSize, SizeT Alignment>
class TLinearAllocator;

template <SizeT BufferSize, SizeT Alignment>
class TStackAllocator;

template <SizeT BlockSize, SizeT BlockCount, SizeT Alignment>
class TPoolAllocator;

template <typename T, SizeT InlineCount, typename FallbackAllocator>
class TInlineAllocator;

}   // namespace GP::Memory
