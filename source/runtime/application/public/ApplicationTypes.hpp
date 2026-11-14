// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "ApplicationForward.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryForward.hpp"
#include "threading/jobs/JobSystemForward.hpp"
#include "time/EngineClock.hpp"   // IWYU pragma: keep

namespace gp
{

/// @brief Number of per-frame linear arenas maintained by `gp::Application` (`0` and `1` for Frame `N` and `N-1`).
inline constexpr gp::UInt32 kFrameArenaBufferCount = 2u;

/// @brief Default byte capacity (`4 MiB`) for each of the double-buffered per-frame `LinearAllocator` arenas
///        ([ADR-0053]).
inline constexpr gp::USize kDefaultFrameArenaCapacityInBytes = 4ULL * 1024ULL * 1024ULL;

/// @brief Default maximum number of `JobGraphNode` passes supported per frame in `Application::getFrameGraph()`.
inline constexpr gp::UInt32 kDefaultFrameGraphMaxNodes = 256u;

/// @brief Default maximum number of outgoing dependent edges per node in `Application::getFrameGraph()`.
inline constexpr gp::UInt16 kDefaultFrameGraphMaxDependentsPerNode = 16u;

/// @brief Per-frame execution context passed to `Application` task-graph and frame-boundary hooks ([ADR-0053]).
/// @details Bundles the current frame's timing snapshot, the active `JobSystem`, the arena-backed `JobGraph`, and
///          references to both the current frame's `LinearAllocator` (`Frame N`) and the previous frame's intact
///          `LinearAllocator` (`Frame N-1`).
struct ApplicationFrameContext
{
    /// @brief Timing snapshot for the active frame (`frameIndex`, `deltaTime`, `fixedStep.interpolationAlpha`, etc.).
    const gp::time::FrameTimeStats& timeStats;

    /// @brief Reference to the application's `gp::thread::JobSystem` scheduler.
    gp::thread::JobSystem& jobSystem;

    /// @brief Reference to the per-frame `gp::thread::JobGraph` backed by `currentFrameArena`.
    gp::thread::JobGraph& frameGraph;

    /// @brief Active frame's `O(1)` linear bump arena (`m_frameArenas[frameIndex & 1u]`), reset at the start of Frame
    ///        `N` and valid through the end of Frame `N + 1`.
    gp::memory::LinearAllocator& currentFrameArena;

    /// @brief Previous frame's `O(1)` linear bump arena (`m_frameArenas[(frameIndex - 1u) & 1u]`), containing intact
    ///        allocations from Frame `N - 1` throughout Frame `N`.
    gp::memory::LinearAllocator& previousFrameArena;

    /// @brief Queries the 1-based monotonic frame index from `timeStats`.
    /// @return Current frame index.
    [[nodiscard]] constexpr gp::UInt64 getFrameIndex() const noexcept
    {
        return timeStats.frameIndex;
    }
};

}   // namespace gp
