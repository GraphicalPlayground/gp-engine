// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "commands/CommandForward.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"

namespace gp::rhi::allocator
{

/// @brief Fixed-size array of backend command contexts indexed by `sync::PipelineStage`.
using ContextArray = sync::PipelineArray<command::IComputeContext*>;

/// @brief Parameters passed to relocate a pooled allocation during defragmentation.
struct PoolMoveRequest
{
    /// @brief Contexts for both execution pipes; the copy must be ordered against both before unlocking old memory.
    const ContextArray& contexts;

    /// @brief Destination allocation block, already locked; backend frame fences unlock it after GPU completion.
    PoolAllocationData* newData{ nullptr };
};

}   // namespace gp::rhi::allocator
