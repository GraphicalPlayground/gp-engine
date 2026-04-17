// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

namespace gp::memory
{

/// @section Allocators Forward Declarations

class Allocator;
class DefaultAllocator;
class StackAllocator;
class PoolAllocator;
class ArenaAllocator;
class LinearAllocator;
class InlineAllocator;
class FreeListAllocator;
class ProxyAllocator;
class TLSFAllocator;
class BuddyAllocator;

template <typename T>
class PolymorphicAllocator;

/// @section Allocator Utilities

template <typename Alloc>
struct AllocatorTraits;

}   // namespace gp::memory
