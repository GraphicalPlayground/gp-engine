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

namespace gp
{

/// @section Concepts

/// @brief Concept describing types manageable by gp::RefCountPtr.
template <typename T>
concept IsRefCounted = requires(T* object) {
    { object->addRef() };
    { object->release() };
};

/// @section Smart Pointers

template <typename T>
struct DefaultDelete;
template <typename T, typename Deleter>
class UniquePtr;
template <typename T, typename Policy>
class SharedPtr;
template <typename T, typename Policy>
class WeakPtr;
template <gp::IsRefCounted T>
class RefCountPtr;

}   // namespace gp
