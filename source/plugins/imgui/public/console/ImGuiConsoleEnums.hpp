// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::imgui
{

/// @brief Semantic category of one line in the console output log, used to pick its color.
enum class ImGuiConsoleLineKind : gp::UInt8
{
    Input = 0u,    ///< Echo of a submitted command line (`> app.TargetFps 60`).
    Output = 1u,   ///< Text emitted by the console manager or a command through the output delegate.
    Info = 2u,     ///< Console-local status text (banner, `[ok]`, cleared log).
    Error = 3u     ///< Rejected input (`NotFound`, `PriorityTooLow`, parse failures, ...).
};

/// @brief What a suggestion row points at.
enum class ImGuiConsoleSuggestionKind : gp::UInt8
{
    Variable = 0u,   ///< An `IConsoleVariable` (shows current value and type).
    Command = 1u     ///< An `IConsoleCommand`.
};

/// @brief How a suggestion matched the typed query, in ranking order.
enum class ImGuiConsoleMatchKind : gp::UInt8
{
    Exact = 0u,      ///< The query is the full name (case-insensitive).
    Prefix = 1u,     ///< The name starts with the query.
    Substring = 2u   ///< The query appears elsewhere in the name.
};

}   // namespace gp::imgui
