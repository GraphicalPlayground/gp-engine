// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "console/ConsoleForward.hpp"
#include "console/ImGuiConsoleEnums.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep

namespace gp::imgui
{

/// @brief One ranked auto-complete candidate produced from the console registry.
/// @note `name` and `object` are non-owning views into the console manager; they stay valid until the object is
///       unregistered, which is why suggestions are rebuilt on every edit instead of being cached across frames.
struct ImGuiConsoleSuggestion
{
    /// @brief Registered console object (variable or command).
    gp::console::IConsoleObject* object{ nullptr };

    /// @brief Registered name, as returned by `IConsoleObject::getName()`.
    gp::StringView name{};

    /// @brief Variable or command.
    ImGuiConsoleSuggestionKind kind{ ImGuiConsoleSuggestionKind::Variable };

    /// @brief How the name matched the query (primary sort key).
    ImGuiConsoleMatchKind match{ ImGuiConsoleMatchKind::Substring };
};

/// @brief One line of the console output log.
struct ImGuiConsoleLine
{
    /// @brief UTF-8 text without a trailing newline.
    gp::String text{};

    /// @brief Semantic category (drives the color).
    ImGuiConsoleLineKind kind{ ImGuiConsoleLineKind::Output };
};

}   // namespace gp::imgui
