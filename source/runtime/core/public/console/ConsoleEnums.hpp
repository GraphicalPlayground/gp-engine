// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "console/ConsoleForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::console
{

/// @brief Underlying storage type of a registered console variable.
enum class ConsoleVarType : gp::UInt8
{
    Bool = 0u,
    Int32 = 1u,
    UInt32 = 2u,
    Float32 = 3u,
    Float64 = 4u,
    String = 5u
};

/// @brief Behavioral bitflags controlling visibility, mutability, and thread synchronization of console objects.
enum class ConsoleVarFlags : gp::UInt32
{
    /// @brief Default mutable Game-Thread variable or command.
    None = 0u,

    /// @brief Prevents runtime writes after construction (except during initialization defaults).
    ReadOnly = 1u << 0u,

    /// @brief Blocks execution or mutation when cheats are disabled (`IConsoleManager::areCheatsEnabled() == false`).
    Cheat = 1u << 1u,

    /// @brief Enables dual-slot atomic mirroring for lock-free `getValueOnRenderThread()` reads.
    RenderThreadSafe = 1u << 2u,

    /// @brief Stages writes into a pending slot until `IConsoleManager::flushLatchedVariables()` commits them.
    Latched = 1u << 3u,

    /// @brief Marks user-facing variables eligible for config persistence.
    Archive = 1u << 4u,

    /// @brief Hides the variable or command from `help`, `cvar.list`, and autocomplete queries.
    Invisible = 1u << 5u,

    /// @brief Set automatically while the active value differs from its constructor default.
    Modified = 1u << 6u,

    /// @brief Indicates the object has not yet been registered into an active console manager.
    Unregistered = 1u << 7u
};

/// @brief Priority tier of the source that wrote a console variable, ordered from lowest to highest precedence.
/// @details Higher tiers override lower tiers, while writes from a lower tier return `ConsoleResult::PriorityTooLow`
///          without overwriting higher-priority overrides such as command-line flags or developer console edits.
enum class ConsoleVarSetBy : gp::UInt8
{
    Constructor = 0u,
    Scalability = 1u,
    GameSetting = 2u,
    ProjectSetting = 3u,
    SystemConfig = 4u,
    DeviceProfile = 5u,
    ConsoleVariables = 6u,
    CommandLine = 7u,
    Code = 8u,
    Console = 9u
};

/// @brief Explicit outcome codes returned by console variable writes, registrations, and command invocations.
enum class [[nodiscard]] ConsoleResult : gp::UInt8
{
    Success = 0u,
    NotFound = 1u,
    AlreadyExists = 2u,
    InvalidArguments = 3u,
    TypeMismatch = 4u,
    ParseFailed = 5u,
    ReadOnly = 6u,
    CheatBlocked = 7u,
    PriorityTooLow = 8u,
    CapacityExceeded = 9u,
    ConsoleDisabled = 10u
};

/// @brief Returns a human-readable ASCII name for a `ConsoleVarType` value.
/// @param[in] type Variable type enumerator.
/// @return Null-terminated static string literal.
[[nodiscard]] constexpr const char* varTypeToString(const ConsoleVarType type) noexcept
{
    switch (type)
    {
    case ConsoleVarType::Bool:
        return "Bool";
    case ConsoleVarType::Int32:
        return "Int32";
    case ConsoleVarType::UInt32:
        return "UInt32";
    case ConsoleVarType::Float32:
        return "Float32";
    case ConsoleVarType::Float64:
        return "Float64";
    case ConsoleVarType::String:
        return "String";
    default:
        return "Unknown";
    }
}

/// @brief Returns a human-readable ASCII name for a `ConsoleVarSetBy` priority tier.
/// @param[in] setBy Priority tier enumerator.
/// @return Null-terminated static string literal.
[[nodiscard]] constexpr const char* setByToString(const ConsoleVarSetBy setBy) noexcept
{
    switch (setBy)
    {
    case ConsoleVarSetBy::Constructor:
        return "Constructor";
    case ConsoleVarSetBy::Scalability:
        return "Scalability";
    case ConsoleVarSetBy::GameSetting:
        return "GameSetting";
    case ConsoleVarSetBy::ProjectSetting:
        return "ProjectSetting";
    case ConsoleVarSetBy::SystemConfig:
        return "SystemConfig";
    case ConsoleVarSetBy::DeviceProfile:
        return "DeviceProfile";
    case ConsoleVarSetBy::ConsoleVariables:
        return "ConsoleVariables";
    case ConsoleVarSetBy::CommandLine:
        return "CommandLine";
    case ConsoleVarSetBy::Code:
        return "Code";
    case ConsoleVarSetBy::Console:
        return "Console";
    default:
        return "Unknown";
    }
}

/// @brief Returns a human-readable ASCII name for a `ConsoleResult` status code.
/// @param[in] result Status code enumerator.
/// @return Null-terminated static string literal.
[[nodiscard]] constexpr const char* consoleResultToString(const ConsoleResult result) noexcept
{
    switch (result)
    {
    case ConsoleResult::Success:
        return "Success";
    case ConsoleResult::NotFound:
        return "NotFound";
    case ConsoleResult::AlreadyExists:
        return "AlreadyExists";
    case ConsoleResult::InvalidArguments:
        return "InvalidArguments";
    case ConsoleResult::TypeMismatch:
        return "TypeMismatch";
    case ConsoleResult::ParseFailed:
        return "ParseFailed";
    case ConsoleResult::ReadOnly:
        return "ReadOnly";
    case ConsoleResult::CheatBlocked:
        return "CheatBlocked";
    case ConsoleResult::PriorityTooLow:
        return "PriorityTooLow";
    case ConsoleResult::CapacityExceeded:
        return "CapacityExceeded";
    case ConsoleResult::ConsoleDisabled:
        return "ConsoleDisabled";
    default:
        return "Unknown";
    }
}

}   // namespace gp::console

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::console::ConsoleVarFlags);
