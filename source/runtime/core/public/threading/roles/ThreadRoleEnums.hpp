// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::thread
{

/// @brief Dense index of the long-lived engine threads that own a dispatch queue.
/// @details The values are array indices into `NamedThreadDispatcher` queues and into the process-wide ownership table
///          of `ThreadRoleRegistry`. Each named thread can be claimed by at most one OS thread at a time.
enum class NamedThread : gp::UInt8
{
    /// @brief Simulation thread: gameplay, world ticking, and frame orchestration.
    Game = 0,

    /// @brief Render thread: turns the game's frame snapshot into render commands.
    Render = 1,

    /// @brief RHI thread: translates render commands into graphics API calls and submits them to the GPU.
    RHI = 2,

    /// @brief Number of named threads. Also used as the invalid sentinel.
    Count = 3
};

/// @brief Bitmask of the roles the calling OS thread currently plays.
/// @details Every OS thread keeps one `ThreadRole` mask in thread-local storage. The mask has two kinds of bits:
///          - Exclusive bits (`Game`, `Render`, `RHI`) mirror a `NamedThread` claim. Only
///            `ThreadRoleRegistry::attachCurrentThread()` sets them, so at most one OS thread holds each bit.
///          - Scoped bits (`ParallelRender`) can be held by any number of threads at once. Worker threads push them
///            for the duration of a job through `ScopedThreadRole`.
///          A single thread may hold several exclusive bits. A headless or single-threaded build can attach the main
///          thread as `Game`, `Render`, and `RHI` together and pump all three queues itself.
enum class ThreadRole : gp::UInt8
{
    /// @brief The thread plays no engine role (plain OS thread or idle job worker).
    None = 0x00u,

    /// @brief The thread owns `NamedThread::Game`.
    Game = 0x01u,

    /// @brief The thread owns `NamedThread::Render`.
    Render = 0x02u,

    /// @brief The thread owns `NamedThread::RHI`.
    RHI = 0x04u,

    /// @brief The thread is currently recording render work on behalf of the render thread (parallel command list
    ///        recording, visibility jobs, and similar).
    ParallelRender = 0x08u
};

/// @brief Status codes returned by thread-role registration and named-thread dispatch.
enum class ThreadRoleResult : gp::UInt8
{
    /// @brief The operation completed.
    Success = 0,

    /// @brief Out-of-range `NamedThread`, null task entry, zero queue capacity, or a role mask that the call cannot
    ///        accept (for example an exclusive bit passed to a scoped push).
    InvalidArguments = 1,

    /// @brief `NamedThreadDispatcher::initialize()` was called on an initialized dispatcher.
    AlreadyInitialized = 2,

    /// @brief The dispatcher has not been initialized, or has been shut down.
    NotInitialized = 3,

    /// @brief The allocator could not provide the queue storage.
    OutOfMemory = 4,

    /// @brief Another OS thread already owns the requested `NamedThread`.
    RoleClaimedByOtherThread = 5,

    /// @brief The calling thread already owns the requested `NamedThread`.
    AlreadyAttached = 6,

    /// @brief The calling thread does not own the `NamedThread` it tried to release.
    NotAttached = 7,

    /// @brief The calling thread does not hold the role required to drain the target queue.
    WrongThread = 8,

    /// @brief The target dispatch queue has no free slot.
    QueueFull = 9
};

}   // namespace gp::thread

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::thread::ThreadRole);
