// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::imgui
{

/// @brief Creation parameters for `ImGuiConsole`.
struct ImGuiConsoleDesc
{
    /// @brief Output log capacity; the oldest lines are overwritten once it is full (ring buffer).
    gp::USize maxOutputLines{ 1024u };

    /// @brief Number of submitted commands remembered for Up/Down recall.
    gp::USize maxHistoryEntries{ 64u };

    /// @brief Maximum number of ranked suggestions kept per query.
    gp::USize maxSuggestions{ 32u };

    /// @brief Fraction of the main viewport height covered by the console panel, in `(0, 1]`.
    gp::Float32 heightRatio{ 0.4f };

    /// @brief Also suggest objects flagged `ConsoleVarFlags::Invisible`.
    bool includeInvisible{ false };

    /// @brief Let the grave/tilde key (`²` on AZERTY) toggle the console from `draw()`.
    bool handleToggleKey{ true };
};

}   // namespace gp::imgui
