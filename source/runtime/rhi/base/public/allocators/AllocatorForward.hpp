// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::allocator
{

/// @brief Bitmask of resource categories that a memory pool can back.
enum class PoolResourceTypes : gp::UInt8;

/// @brief Mechanism a memory pool uses to provide backing memory for allocations.
enum class ResourceAllocationStrategy : gp::UInt8;

/// @brief Strategy governing how a pool allocator provisions new backing pools.
enum class PoolAllocationStrategy : gp::UInt8;

/// @brief Lifecycle status of background asynchronous pool creation.
enum class PoolAllocationStatus : gp::UInt8;

/// @brief Ordering policy applied to a memory pool's free block list.
enum class PoolFreeListOrder : gp::UInt8;

/// @brief Base class for resources owning a pooled memory allocation that can be relocated.
class PoolResource;

/// @brief Intrusive linked-list node describing a single free, allocated, or sentinel block in a memory pool.
struct PoolAllocationData;

/// @brief Parameters required to relocate a live pooled allocation during defragmentation.
struct PoolMoveRequest;

/// @brief Single GPU memory pool managing sub-allocations through a doubly-linked block list.
class MemoryPool;

/// @brief Multi-pool allocator supporting sorted pool selection, async pre-allocation, and defragmentation.
class PoolAllocator;

/// @brief Inclusive index range `[first, last]` inside a descriptor heap.
struct DescriptorAllocatorRange;

/// @brief Contiguous descriptor allocation defined by a start index and descriptor count.
struct DescriptorAllocation;

/// @brief Thread-safe free-list allocator for contiguous ranges of descriptor indices.
class DescriptorAllocator;

/// @brief Descriptor allocator bound to a specific mask of supported descriptor types.
class HeapDescriptorAllocator;

/// @brief Heap descriptor allocator whose returned indices are shifted by a constant base offset.
class OffsetHeapDescriptorAllocator;

}   // namespace gp::rhi::allocator
