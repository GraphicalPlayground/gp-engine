// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "diagnostics/DiagnosticsForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::diagnostics
{

/// @brief Verbosity tier filtering whether a breadcrumb scope is emitted.
enum class BreadcrumbVerbosity : gp::UInt8
{
    /// @brief High-level frame, pass, and scene milestones always active when breadcrumbs are enabled.
    Minimal = 0u,

    /// @brief Standard render-pass and compute-pass scopes.
    Normal = 1u,

    /// @brief Fine-grained draw/dispatch batch scopes.
    Verbose = 2u,

    /// @brief Unfiltered diagnostic level emitting all breadcrumb scopes.
    All = 3u
};

/// @brief GPU execution status of a breadcrumb node reconstructed after a device fault or hang.
enum class BreadcrumbMarkerState : gp::UInt8
{
    /// @brief The GPU had not yet reached the entry marker for this breadcrumb.
    NotStarted = 0u,

    /// @brief The GPU wrote the entry marker for this breadcrumb but had not yet written its exit marker.
    Active = 1u,

    /// @brief The GPU completed both the entry and exit markers for this breadcrumb.
    Finished = 2u
};

}   // namespace gp::rhi::diagnostics
