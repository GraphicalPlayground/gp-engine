// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "miscellaneous/BuildDefines.hpp"

namespace gp::console
{

/// @brief Configuration descriptor used to initialize `ConsoleManagerBase` / `IConsoleManager`.
struct ConsoleManagerDesc final
{
    /// @brief Enables interactive console parsing and command execution (`GP_ALLOW_CONSOLE` by default).
    bool enableConsole{ GP_ALLOW_CONSOLE != 0 };

    /// @brief Enables variables and commands flagged `ConsoleVarFlags::Cheat`.
    bool enableCheats{ GP_BUILD_SHIPPING == 0 };

    /// @brief Scans `gp::CommandLine` overrides (`-cvar=val`, `+cvar val`, `-ExecCmds=...`) on initialization.
    bool applyCommandLineOverrides{ true };

    /// @brief Registers built-in diagnostic commands (`help`, `cvar.list`, `cvar.dump`, `cvar.reset`).
    bool registerBuiltInCommands{ true };

    /// @brief Drains static-init `AutoConsole*` objects from the global deferred queue on initialization.
    /// @note Standalone unit-test managers typically set this to `false` to stay isolated from global static CVars.
    bool registerDeferredStaticObjects{ true };
};

}   // namespace gp::console
