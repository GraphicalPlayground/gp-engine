// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::allocator
{

/// @brief Bitmask of resource categories supported by a memory pool.
enum class PoolResourceTypes : gp::UInt8
{
    None = 0x0u,
    Buffers = 0x1u,
    RTDSTextures = 0x2u,
    NonRTDSTextures = 0x4u,
    All = Buffers | RTDSTextures | NonRTDSTextures
};

/// @brief Describes how a memory pool provides backing storage for individual allocations.
enum class ResourceAllocationStrategy : gp::UInt8
{
    /// @brief The pool owns a heap and each allocation is a placed native resource at an offset.
    /// @details Each allocation keeps its own resource state, while placement alignment can introduce internal
    ///          fragmentation.
    PlacedResource = 0u,

    /// @brief The pool owns one large resource and allocations are byte ranges inside it.
    /// @details Supports finer granularity, but every allocation must share the backing resource's state.
    ManualSubAllocation
};

/// @brief Policy controlling whether new backing pools are created synchronously or pre-allocated in the background.
enum class PoolAllocationStrategy : gp::UInt8
{
    Immediate = 0u,
    Async,
    Count
};

/// @brief State machine tracking an asynchronous background pool allocation request.
enum class PoolAllocationStatus : gp::UInt8
{
    Idle = 0u,
    Allocating,
    ShutDown,
    Count
};

/// @brief Ordering rule maintained for a memory pool's free-block array.
enum class PoolFreeListOrder : gp::UInt8
{
    SortBySize = 0u,
    SortByOffset
};

}   // namespace gp::rhi::allocator

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::allocator::PoolResourceTypes);
