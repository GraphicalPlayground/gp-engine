// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "console/ConsoleEnums.hpp"
#include "console/ConsoleForward.hpp"        // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"

namespace gp::console
{

/// @brief Common pure abstract interface shared by registered console variables and console commands.
class GP_CORE_API IConsoleObject
{
public:
    /// @brief Virtual destructor.
    virtual ~IConsoleObject() noexcept = default;

    /// @brief Returns the registered lookup name of this console object.
    /// @return Non-owning name `gp::StringView`.
    [[nodiscard]] virtual gp::StringView getName() const noexcept = 0;

    /// @brief Returns the precomputed 64-bit case-insensitive FNV-1a hash of `getName()`.
    /// @return 64-bit name hash.
    [[nodiscard]] virtual gp::UInt64 getNameHash() const noexcept = 0;

    /// @brief Returns the human-readable description shown by `help` and inspector overlays.
    /// @return Help text `gp::StringView`.
    [[nodiscard]] virtual gp::StringView getHelp() const noexcept = 0;

    /// @brief Returns the active behavioral bitflags for this object.
    /// @return Current `ConsoleVarFlags` bitmask.
    [[nodiscard]] virtual ConsoleVarFlags getFlags() const noexcept = 0;

    /// @brief Replaces the behavioral bitflags on this object.
    /// @param[in] flags New bitmask to assign.
    virtual void setFlags(ConsoleVarFlags flags) noexcept = 0;

    /// @brief Sets the specified bitflags while preserving existing flags.
    /// @param[in] flags Bitflags to enable.
    virtual void addFlags(ConsoleVarFlags flags) noexcept = 0;

    /// @brief Clears the specified bitflags while preserving other flags.
    /// @param[in] flags Bitflags to clear.
    virtual void clearFlags(ConsoleVarFlags flags) noexcept = 0;

    /// @brief Reports whether all bits in @p flags are currently set on this object.
    /// @param[in] flags Bitmask to test.
    /// @return True if `(getFlags() & flags) == flags`, false otherwise.
    [[nodiscard]] virtual bool hasFlags(ConsoleVarFlags flags) const noexcept = 0;

    /// @brief Returns this object as an `IConsoleVariable*` if it is a variable, or `nullptr` if it is a command.
    /// @return Mutable variable interface pointer, or `nullptr`.
    [[nodiscard]] virtual IConsoleVariable* asVariable() noexcept = 0;

    /// @brief Returns this object as a const `IConsoleVariable*` if it is a variable, or `nullptr` if it is a
    ///        command.
    /// @return Const variable interface pointer, or `nullptr`.
    [[nodiscard]] virtual const IConsoleVariable* asVariable() const noexcept = 0;

    /// @brief Returns this object as an `IConsoleCommand*` if it is a command, or `nullptr` if it is a variable.
    /// @return Mutable command interface pointer, or `nullptr`.
    [[nodiscard]] virtual IConsoleCommand* asCommand() noexcept = 0;

    /// @brief Returns this object as a const `IConsoleCommand*` if it is a command, or `nullptr` if it is a variable.
    /// @return Const command interface pointer, or `nullptr`.
    [[nodiscard]] virtual const IConsoleCommand* asCommand() const noexcept = 0;

    /// @brief Notifies this object when it is attached to an active `IConsoleManager`.
    /// @param[in] manager Owning console manager pointer.
    virtual void onRegistered([[maybe_unused]] IConsoleManager* const manager) noexcept
    {}

    /// @brief Notifies this object when it is detached from its owning `IConsoleManager` or during manager shutdown.
    virtual void onUnregistered() noexcept
    {}
};

}   // namespace gp::console
