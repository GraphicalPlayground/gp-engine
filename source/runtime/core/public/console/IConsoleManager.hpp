// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "console/ConsoleDesc.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleForward.hpp"   // IWYU pragma: keep
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleCommand.hpp"
#include "console/IConsoleObject.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "templates/Delegate.hpp"

namespace gp::console
{

/// @brief Central console variable and command registry contract ([ADR-0057]).
/// @details Provides case-insensitive $O(1)$ lookup, imperative and RAII `AutoConsole*` registration,
///          `gp::CommandLine` startup overrides, Game-to-Render value synchronization, `Latched` flushing,
///          batched `AutoConsoleVariableSink` execution, and console input parsing.
class GP_CORE_API IConsoleManager
{
public:
    /// @brief Virtual destructor.
    virtual ~IConsoleManager() noexcept = default;

    /// @brief Returns the process-wide singleton `IConsoleManager` instance, initializing it lazily if needed.
    /// @return Reference to the global `IConsoleManager`.
    [[nodiscard]] static IConsoleManager& get() noexcept;

    /// @brief Reports whether the global `IConsoleManager::get()` singleton has completed `initialize()`.
    /// @return True if initialized and active, false before initialization or after shutdown.
    [[nodiscard]] static bool isGlobalInitialized() noexcept;

public:
    /// @section Lifecycle & Policy Control

    /// @brief Initializes the manager, registers built-in commands if requested, drains static objects, and applies
    ///        command-line overrides according to @p desc.
    /// @param[in] desc Initialization configuration options.
    /// @return `ConsoleResult::Success` on initialization, or an error code.
    virtual ConsoleResult initialize(const ConsoleManagerDesc& desc = {}) noexcept = 0;

    /// @brief Clears registered objects, dynamic pools, and sinks and resets manager state.
    virtual void shutdown() noexcept = 0;

    /// @brief Reports whether `initialize()` is currently active on this manager.
    /// @return True if initialized, false otherwise.
    [[nodiscard]] virtual bool isInitialized() const noexcept = 0;

    /// @brief Enables or disables interactive console command execution (`processUserConsoleInput`).
    /// @param[in] enabled True to allow interactive input, false to reject with `ConsoleResult::ConsoleDisabled`.
    virtual void setConsoleEnabled(bool enabled) noexcept = 0;

    /// @brief Reports whether interactive console processing is currently enabled.
    /// @return True if enabled, false otherwise.
    [[nodiscard]] virtual bool isConsoleEnabled() const noexcept = 0;

    /// @brief Enables or disables modifying `Cheat` variables and invoking `Cheat` commands.
    /// @param[in] enabled True to permit cheat CVars and commands, false to block them.
    virtual void setCheatsEnabled(bool enabled) noexcept = 0;

    /// @brief Reports whether `Cheat` variables and commands are currently permitted.
    /// @return True if cheats are enabled, false otherwise.
    [[nodiscard]] virtual bool areCheatsEnabled() const noexcept = 0;

public:
    /// @section Imperative & External Registration

    /// @brief Registers a self-contained boolean console variable from the manager's internal dynamic pool.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in] defaultValue Initial default boolean value.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` if duplicate, invalid, or pool full.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name, bool defaultValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a self-contained `gp::Int32` console variable from the manager's internal dynamic pool.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in] defaultValue Initial default integer value.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name, gp::Int32 defaultValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a self-contained `gp::UInt32` console variable from the manager's internal dynamic pool.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in] defaultValue Initial default unsigned integer value.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name, gp::UInt32 defaultValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a self-contained `gp::Float32` console variable from the manager's internal dynamic pool.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in] defaultValue Initial default float value.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name,
        gp::Float32 defaultValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a self-contained `gp::Float64` console variable from the manager's internal dynamic pool.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in] defaultValue Initial default double value.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name,
        gp::Float64 defaultValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a self-contained string console variable from the manager's internal dynamic pool.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in] defaultValue Initial default UTF-8 string view.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariable(
        gp::StringView name,
        gp::StringView defaultValue,
        gp::StringView help,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a console variable backed by an external `bool&` variable.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in,out] refValue Caller-owned boolean reference (must outlive registration).
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, bool& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a console variable backed by an external `gp::Int32&` variable.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in,out] refValue Caller-owned integer reference.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::Int32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a console variable backed by an external `gp::UInt32&` variable.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in,out] refValue Caller-owned unsigned integer reference.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::UInt32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a console variable backed by an external `gp::Float32&` variable.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in,out] refValue Caller-owned float reference.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::Float32& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a console variable backed by an external `gp::Float64&` variable.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in,out] refValue Caller-owned double reference.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::Float64& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a console variable backed by an external `ConsoleString&` buffer.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in,out] refValue Caller-owned `ConsoleString` reference.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, ConsoleString& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a console variable backed by an external `gp::String&` instance.
    /// @param[in] name Unique case-insensitive lookup name.
    /// @param[in,out] refValue Caller-owned `gp::String` reference.
    /// @param[in] help Help text description.
    /// @param[in] flags Optional behavioral flags.
    /// @return Pointer to the registered `IConsoleVariable`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleVariable* registerConsoleVariableRef(
        gp::StringView name, gp::String& refValue, gp::StringView help, ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a zero-argument console command from the manager's internal dynamic pool.
    /// @param[in] name Unique case-insensitive command name.
    /// @param[in] help Help text description.
    /// @param[in] command Bound callback delegate.
    /// @param[in] flags Optional behavioral flags (`Cheat`, `Invisible`).
    /// @return Pointer to the registered `IConsoleCommand`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleCommand* registerConsoleCommand(
        gp::StringView name,
        gp::StringView help,
        ConsoleCommandDelegate command,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers a console command accepting parsed arguments from the manager's internal dynamic pool.
    /// @param[in] name Unique case-insensitive command name.
    /// @param[in] help Help text description.
    /// @param[in] command Bound callback delegate receiving `const ConsoleCommandArgs&`.
    /// @param[in] flags Optional behavioral flags (`Cheat`, `Invisible`).
    /// @return Pointer to the registered `IConsoleCommand`, or `nullptr` on failure.
    [[nodiscard]] virtual IConsoleCommand* registerConsoleCommand(
        gp::StringView name,
        gp::StringView help,
        ConsoleCommandWithArgsDelegate command,
        ConsoleVarFlags flags = ConsoleVarFlags::None
    ) noexcept = 0;

    /// @brief Registers an external caller-owned `IConsoleObject` (used by `AutoConsole*` wrappers).
    /// @param[in] object Non-owning pointer to a console variable or command.
    /// @return `ConsoleResult::Success`, `AlreadyExists`, `InvalidArguments`, or `CapacityExceeded`.
    virtual ConsoleResult registerExternalObject(IConsoleObject* object) noexcept = 0;

    /// @brief Unregisters a console object by name (reclaiming dynamic pool slots when applicable).
    /// @param[in] name Name of the variable or command to remove.
    /// @return True if found and unregistered, false otherwise.
    virtual bool unregisterConsoleObject(gp::StringView name) noexcept = 0;

    /// @brief Unregisters a specific console object pointer.
    /// @param[in] object Pointer to remove.
    /// @return True if found and unregistered, false otherwise.
    virtual bool unregisterConsoleObject(IConsoleObject* object) noexcept = 0;

    /// @brief Registers a deferred variable sink invoked by `callAllConsoleVariableSinks()`.
    /// @param[in] sink Bound sink delegate.
    /// @return Valid `gp::DelegateHandle`, or invalid handle if unbound or sink capacity is full.
    [[nodiscard]] virtual gp::DelegateHandle registerConsoleVariableSink(ConsoleSinkDelegate sink) noexcept = 0;

    /// @brief Unregisters a variable sink previously added via `registerConsoleVariableSink()`.
    /// @param[in] handle Handle returned by `registerConsoleVariableSink()`.
    /// @return True if removed, false otherwise.
    virtual bool unregisterConsoleVariableSink(gp::DelegateHandle handle) noexcept = 0;

    /// @brief Marks a variable dirty for Game-to-Render sync and flags registered variable sinks to run.
    /// @param[in] variable Variable whose committed value changed.
    virtual void notifyVariableChanged(IConsoleVariable* variable) noexcept = 0;

    /// @brief Marks a `Latched` variable as holding a pending unapplied value.
    /// @param[in] variable Variable holding a pending latched change.
    virtual void notifyVariableLatched(IConsoleVariable* variable) noexcept = 0;

public:
    /// @section Lookup, Iteration & Autocomplete

    /// @brief Finds any registered variable or command by case-insensitive name.
    /// @param[in] name Object name to search for.
    /// @return Matching `IConsoleObject*`, or `nullptr` if not found.
    [[nodiscard]] virtual IConsoleObject* findConsoleObject(gp::StringView name) const noexcept = 0;

    /// @brief Finds a registered console variable by case-insensitive name.
    /// @param[in] name Variable name to search for.
    /// @return Matching `IConsoleVariable*`, or `nullptr` if absent or registered as a command.
    [[nodiscard]] virtual IConsoleVariable* findConsoleVariable(gp::StringView name) const noexcept = 0;

    /// @brief Finds a registered console command by case-insensitive name.
    /// @param[in] name Command name to search for.
    /// @return Matching `IConsoleCommand*`, or `nullptr` if absent or registered as a variable.
    [[nodiscard]] virtual IConsoleCommand* findConsoleCommand(gp::StringView name) const noexcept = 0;

    /// @brief Visits all registered objects whose name starts with @p prefix (case-insensitive).
    /// @param[in] visitor Callback receiving each matching object; returning `false` stops iteration early.
    /// @param[in] prefix Optional case-insensitive prefix filter (empty visits all).
    /// @param[in] includeInvisible True to include objects flagged `ConsoleVarFlags::Invisible`.
    virtual void forEachConsoleObjectStartingWith(
        const ConsoleObjectVisitor& visitor, gp::StringView prefix = {}, bool includeInvisible = false
    ) const noexcept = 0;

    /// @brief Collects up to @p maxResults object names matching case-insensitive @p prefix, sorted alphabetically.
    /// @param[in] prefix Prefix filter (empty matches all visible objects).
    /// @param[out] outNames Caller-owned array receiving matching non-owning `gp::StringView` names.
    /// @param[in] maxResults Capacity of @p outNames.
    /// @param[in] includeInvisible True to include objects flagged `ConsoleVarFlags::Invisible`.
    /// @return Number of matching names written to @p outNames.
    [[nodiscard]] virtual gp::USize autoComplete(
        gp::StringView prefix, gp::StringView* outNames, gp::USize maxResults, bool includeInvisible = false
    ) const noexcept = 0;

    /// @brief Returns the total number of registered console variables (`Auto*` plus dynamic).
    /// @return Active variable count.
    [[nodiscard]] virtual gp::USize getVariableCount() const noexcept = 0;

    /// @brief Returns the total number of registered console commands (`Auto*` plus dynamic).
    /// @return Active command count.
    [[nodiscard]] virtual gp::USize getCommandCount() const noexcept = 0;

    /// @brief Returns the total number of registered console variable sinks.
    /// @return Active sink count.
    [[nodiscard]] virtual gp::USize getSinkCount() const noexcept = 0;

public:
    /// @section Execution, CLI & Frame Synchronization

    /// @brief Parses and executes an interactive console command or CVar assignment/query string.
    /// @details Supports:
    ///          - `"cmd arg1 arg2"` -> invokes `IConsoleCommand::execute()`
    ///          - `"cvar"` or `"cvar ?"` -> formats current value, default value, and help text to @p output
    ///          - `"cvar value"`, `"cvar = value"`, or `"cvar=value"` -> sets CVar with `ConsoleVarSetBy::Console`
    /// @param[in] input Raw console command or assignment string.
    /// @param[in] output Optional output sink receiving formatted response lines.
    /// @return `ConsoleResult` describing execution or validation status.
    virtual ConsoleResult
        processUserConsoleInput(gp::StringView input, ConsoleOutputDelegate* output = nullptr) noexcept = 0;

    /// @brief Scans `gp::CommandLine` tokens (`-cvar=val`, `-cvar:val`, `+cvar val`, `-ExecCmds="..."`) and applies
    ///        matching overrides to registered CVars and commands.
    /// @param[in] setBy Priority tier assigned to applied overrides (defaults to `ConsoleVarSetBy::CommandLine`).
    /// @return Number of variables or commands successfully updated from the command line.
    virtual gp::USize applyCommandLineOverrides(ConsoleVarSetBy setBy = ConsoleVarSetBy::CommandLine) noexcept = 0;

    /// @brief Synchronizes all dirty Game Thread values and `AutoConsoleVariableRef` mirrors into their Render Thread
    ///        slots for lock-free frame-coherent reads.
    /// @return Number of variables whose Render Thread slots were synchronized.
    virtual gp::USize syncRenderThreadValues() noexcept = 0;

    /// @brief Commits all pending `Latched` CVar changes, fires their change delegates, and schedules render sync.
    /// @return Number of latched variables committed.
    virtual gp::USize flushLatchedVariables() noexcept = 0;

    /// @brief Invokes all registered `AutoConsoleVariableSink` callbacks if any variable changed since the last sweep.
    /// @param[in] force If true, invokes sinks unconditionally even when no CVar changed.
    /// @return Number of sink callbacks invoked.
    virtual gp::USize callAllConsoleVariableSinks(bool force = false) noexcept = 0;

    /// @brief Resets all non-`ReadOnly` registered variables whose priority is `<= maxPriority` back to defaults.
    /// @param[in] maxPriority Maximum priority tier allowed to be reset.
    /// @return Number of variables successfully reset.
    virtual gp::USize resetAllVariables(ConsoleVarSetBy maxPriority = ConsoleVarSetBy::Console) noexcept = 0;
};

}   // namespace gp::console
