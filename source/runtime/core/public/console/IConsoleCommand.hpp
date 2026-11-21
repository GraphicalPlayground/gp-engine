// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "console/ConsoleEnums.hpp"
#include "console/ConsoleForward.hpp"   // IWYU pragma: keep
#include "console/IConsoleObject.hpp"
#include "CoreMinimal.hpp"

namespace gp::console
{

/// @brief Pure abstract contract for executable runtime console commands.
class GP_CORE_API IConsoleCommand : public IConsoleObject
{
public:
    /// @brief Executes the command with tokenized arguments and optional output routing.
    /// @param[in] args Tokenized command arguments and optional output sink.
    /// @return `ConsoleResult::Success`, `ConsoleResult::CheatBlocked`, or `ConsoleResult::ConsoleDisabled`.
    virtual ConsoleResult execute(const ConsoleCommandArgs& args) noexcept = 0;

    /// @brief Reports whether the command is currently permitted to execute under active build and cheat flags.
    /// @return True if enabled, false if blocked by `Cheat` or `ConsoleDisabled` policy.
    [[nodiscard]] virtual bool isEnabled() const noexcept = 0;
};

}   // namespace gp::console
