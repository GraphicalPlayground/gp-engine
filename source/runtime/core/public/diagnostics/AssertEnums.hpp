// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/base/Platform.hpp"

namespace gp::diagnostics
{

/// @brief Classifies which assertion macro triggered a failure report.
enum class AssertKind : UInt8
{
    /// @brief Fatal contract check active when `GP_DO_CHECK` is enabled (`GP_CHECK`, `GP_CHECKF`, `GP_ASSERT`).
    Check = 0u,

    /// @brief Fatal slow contract check active only when `GP_DO_GUARD_SLOW` is enabled (`GP_CHECK_SLOW`).
    CheckSlow,

    /// @brief Fatal check whose condition always evaluates even when `GP_DO_CHECK` is disabled (`GP_VERIFY`).
    Verify,

    /// @brief Non-fatal check that reports once per call site (`GP_ENSURE`, `GP_ENSURE_MSG`).
    Ensure,

    /// @brief Reached a branch marked unreachable (`GP_CHECK_NO_ENTRY`).
    NoEntry,

    /// @brief Reached an unimplemented stub (`GP_UNIMPLEMENTED`).
    Unimplemented,
};

/// @brief Selects how the engine responds after an assertion handler processes a failure.
enum class AssertAction : UInt8
{
    /// @brief Break into an attached debugger if present, then terminate the process cleanly via `std::abort()`.
    Abort = 0u,

    /// @brief Trigger a debugger breakpoint and then abort execution.
    Break,

    /// @brief Suppress termination and resume execution at the call site.
    Continue,
};

}   // namespace gp::diagnostics
