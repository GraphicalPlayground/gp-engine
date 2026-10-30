// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::memory
{

/// @section Malloc primitives forward declarations

class Malloc;
class MallocAnsi;

/// @section Allocator primitives & types forward declarations

enum class AllocatorFlags : gp::UInt8;
enum class AllocatorGrowthPolicy : gp::UInt8;

struct MemoryMark;
struct AllocatorStats;

template <typename InArenaAllocator>
class ScopedMemoryMark;

class IAllocator;
class HeapAllocator;
using DefaultAllocator = HeapAllocator;

template <gp::UInt32 InAlignment>
class AlignedHeapAllocator;

class LinearAllocator;
class StackAllocator;
class PoolAllocator;

template <gp::USize InCapacityBytes, gp::UInt32 InAlignment>
class FixedBufferAllocator;

template <gp::USize InCapacityBytes, gp::UInt32 InAlignment, typename InFallbackAllocator>
class InlineAllocator;

class ThreadScratchArena;
class ThreadScratchAllocator;
class ScopedScratchMark;

template <typename InInnerAllocator>
class TaggedAllocator;

class PolymorphicAllocator;

}   // namespace gp::memory

namespace gp
{

/// @section Pointers forward declarations

template <typename T, typename Deleter>
class UniquePtr;

template <typename T>
class SharedPtr;

template <typename T>
class WeakPtr;

template <typename T>
class RefCountedPtr;

}   // namespace gp
