// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Relationship.hpp"
#include "console/ConsoleForward.hpp"           // IWYU pragma: keep
#include "containers/strings/FixedString.hpp"   // IWYU pragma: keep
#include "containers/strings/String.hpp"        // IWYU pragma: keep
#include "containers/views/StringView.hpp"      // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "templates/Delegate.hpp"
#include "templates/Optional.hpp"
#include <type_traits>

namespace gp::console
{

/// @section Console Capacity Constants

/// @brief Maximum character length (excluding null terminator) of a registered console object name.
inline constexpr gp::USize kMaxConsoleNameLength = 127u;

/// @brief Maximum character length (excluding null terminator) of a console object help string.
inline constexpr gp::USize kMaxConsoleHelpLength = 255u;

/// @brief Maximum positional arguments accepted by a single console command invocation (excluding command name).
inline constexpr gp::USize kMaxConsoleCommandArgs = 32u;

/// @brief Maximum byte length of a raw command input line processed on the stack.
inline constexpr gp::USize kMaxConsoleCommandLineLength = 511u;

/// @brief Default open-addressed hash table slot capacity of the global `IConsoleManager::get()` registry.
inline constexpr gp::USize kDefaultConsoleHashCapacity = 2048u;

/// @brief Default dynamic variable pool capacity of the global `IConsoleManager::get()` registry.
inline constexpr gp::USize kMaxDynamicConsoleVariables = 256u;

/// @brief Default dynamic command pool capacity of the global `IConsoleManager::get()` registry.
inline constexpr gp::USize kMaxDynamicConsoleCommands = 128u;

/// @brief Default sink registration capacity of the global `IConsoleManager::get()` registry.
inline constexpr gp::USize kMaxConsoleSinks = 128u;

/// @brief Maximum multicast change listeners attached directly to a single `IConsoleVariable`.
inline constexpr gp::USize kMaxConsoleCallbacksPerVariable = 8u;

/// @section Console Delegate Types

/// @brief Delegate invoked when an `IConsoleVariable` value commits on the Game Thread or at a latch flush.
using ConsoleVarChangedDelegate = gp::Delegate<void(IConsoleVariable*)>;

/// @brief Zero-argument console command callback.
using ConsoleCommandDelegate = gp::Delegate<void()>;

/// @brief Console command callback receiving tokenized arguments and optional output routing.
using ConsoleCommandWithArgsDelegate = gp::Delegate<void(const ConsoleCommandArgs&)>;

/// @brief Deferred batch callback invoked once per frame via `IConsoleManager::callAllConsoleVariableSinks()`.
using ConsoleSinkDelegate = gp::Delegate<void()>;

/// @brief Visitor callback invoked while iterating console objects (`return true` continues, `false` stops early).
using ConsoleObjectVisitor = gp::Delegate<bool(IConsoleObject*)>;

/// @brief Output sink delegate receiving formatted console response lines (`help`, queries, error messages).
using ConsoleOutputDelegate = gp::Delegate<void(gp::StringView)>;

/// @section Value Concepts

/// @brief Matches string types accepted by `AutoConsoleVariable<T>` and `AutoConsoleVariableRef`.
template <typename InT>
concept IsConsoleStringType = gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::StringView> ||
                              gp::concepts::IsSameAs<std::remove_cvref_t<InT>, ConsoleString> ||
                              gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::String> ||
                              gp::concepts::IsSameAs<std::remove_cvref_t<InT>, const char*>;

/// @brief Matches value types supported by `AutoConsoleVariable<T>`.
template <typename InT>
concept IsConsoleValueType = gp::concepts::IsSameAs<std::remove_cvref_t<InT>, bool> ||
                             gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::Int32> ||
                             gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::UInt32> ||
                             gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::Float32> ||
                             gp::concepts::IsSameAs<std::remove_cvref_t<InT>, gp::Float64> || IsConsoleStringType<InT>;

/// @brief Zero-allocation tokenized representation of an interactive console command invocation.
/// @details Splits whitespace-delimited tokens while honoring double quotes (`"..."`), single quotes (`'...'`),
///          and backslash escapes into an inline 512-byte scratch buffer so `StringView` arguments remain valid for
///          the lifetime of the `ConsoleCommandArgs` object.
class GP_CORE_API ConsoleCommandArgs final
{
public:
    /// @brief Constructs an empty argument list.
    [[nodiscard]] ConsoleCommandArgs() noexcept = default;

    /// @brief Parses a raw command line (e.g., `"r.Preset \"Ultra High\" 2"`) into command name and arguments.
    /// @param[in] rawCommandLine Input UTF-8 command line view.
    /// @param[in] output Optional output sink passed through to command execution.
    [[nodiscard]] explicit ConsoleCommandArgs(
        gp::StringView rawCommandLine, ConsoleOutputDelegate* output = nullptr
    ) noexcept;

    /// @brief Tokenizes @p rawCommandLine into @p outArgs without allocating heap memory.
    /// @param[in] rawCommandLine Input command line view.
    /// @param[out] outArgs Destination `ConsoleCommandArgs` instance.
    /// @param[in] output Optional output sink pointer to attach.
    /// @return True if a non-empty command name was parsed, false if @p rawCommandLine was empty or exceeded buffers.
    static bool parse(
        gp::StringView rawCommandLine, ConsoleCommandArgs& outArgs, ConsoleOutputDelegate* output = nullptr
    ) noexcept;

public:
    /// @brief Returns the parsed command name (token `0`), or an empty view if none was parsed.
    /// @return Command name `gp::StringView`.
    [[nodiscard]] gp::StringView getCommandName() const noexcept
    {
        return m_commandName;
    }

    /// @brief Returns the unparsed trailing argument substring following the command name.
    /// @return Raw argument tail `gp::StringView`.
    [[nodiscard]] gp::StringView getRawArgs() const noexcept
    {
        return m_rawArgs;
    }

    /// @brief Returns the number of parsed positional arguments (excluding the command name).
    /// @return Argument count in `[0, kMaxConsoleCommandArgs]`.
    [[nodiscard]] gp::USize getArgCount() const noexcept
    {
        return m_argCount;
    }

    /// @brief Reports whether zero positional arguments were provided.
    /// @return True if `getArgCount() == 0`, false otherwise.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_argCount == 0u;
    }

    /// @brief Retrieves the positional argument at zero-based @p index (excluding command name).
    /// @param[in] index Zero-based argument index.
    /// @return Argument `gp::StringView`, or an empty view if out of bounds.
    [[nodiscard]] gp::StringView getArg(gp::USize index) const noexcept;

    /// @brief Attempts to parse argument @p index as a boolean (`true`/`false`/`1`/`0`/`on`/`off`/`yes`/`no`).
    /// @param[in] index Zero-based argument index.
    /// @return Parsed boolean or empty `gp::Optional` if out of bounds or invalid.
    [[nodiscard]] gp::Optional<bool> tryGetBool(gp::USize index) const noexcept;

    /// @brief Attempts to parse argument @p index as a signed 32-bit integer (decimal or `0x` hex).
    /// @param[in] index Zero-based argument index.
    /// @return Parsed `gp::Int32` or empty `gp::Optional`.
    [[nodiscard]] gp::Optional<gp::Int32> tryGetInt32(gp::USize index) const noexcept;

    /// @brief Attempts to parse argument @p index as an unsigned 32-bit integer (decimal or `0x` hex).
    /// @param[in] index Zero-based argument index.
    /// @return Parsed `gp::UInt32` or empty `gp::Optional`.
    [[nodiscard]] gp::Optional<gp::UInt32> tryGetUInt32(gp::USize index) const noexcept;

    /// @brief Attempts to parse argument @p index as a 32-bit floating-point value.
    /// @param[in] index Zero-based argument index.
    /// @return Parsed `gp::Float32` or empty `gp::Optional`.
    [[nodiscard]] gp::Optional<gp::Float32> tryGetFloat32(gp::USize index) const noexcept;

    /// @brief Attempts to parse argument @p index as a 64-bit floating-point value.
    /// @param[in] index Zero-based argument index.
    /// @return Parsed `gp::Float64` or empty `gp::Optional`.
    [[nodiscard]] gp::Optional<gp::Float64> tryGetFloat64(gp::USize index) const noexcept;

    /// @brief Emits a diagnostic or response line to the attached output sink, or skips if none was provided.
    /// @param[in] line Line text to write.
    void writeLine(gp::StringView line) const noexcept;

    /// @brief Returns the optional output sink attached to this command invocation.
    /// @return Output sink pointer, or `nullptr` if none was attached.
    [[nodiscard]] ConsoleOutputDelegate* getOutputDelegate() const noexcept
    {
        return m_output;
    }

private:
    char m_storage[kMaxConsoleCommandLineLength + 1u]{};
    char m_rawTail[kMaxConsoleCommandLineLength + 1u]{};
    gp::StringView m_commandName{};
    gp::StringView m_rawArgs{};
    gp::StringView m_args[kMaxConsoleCommandArgs]{};
    gp::USize m_argCount{ 0u };
    ConsoleOutputDelegate* m_output{ nullptr };
};

/// @section Zero-Allocation Console Parsing & Formatting Utilities

/// @brief Trims leading and trailing ASCII whitespace from @p view.
/// @param[in] view Source string view.
/// @return Trimmed view.
[[nodiscard]] GP_CORE_API gp::StringView trimConsoleWhitespace(gp::StringView view) noexcept;

/// @brief Computes the canonical case-insensitive 64-bit FNV-1a lookup hash for a console name.
/// @param[in] name Console variable or command name.
/// @return 64-bit case-insensitive FNV-1a digest.
[[nodiscard]] GP_CORE_API gp::UInt64 hashConsoleName(gp::StringView name) noexcept;

/// @brief Parses a console string representation into a `bool` (`true`/`false`, `1`/`0`, `on`/`off`, `yes`/`no`).
/// @param[in] text Input string view.
/// @return Parsed boolean or empty `gp::Optional`.
[[nodiscard]] GP_CORE_API gp::Optional<bool> parseConsoleBool(gp::StringView text) noexcept;

/// @brief Parses a console string representation into `gp::Int32` (decimal, `0x` hex, or `true`/`false` fallback).
/// @param[in] text Input string view.
/// @return Parsed integer or empty `gp::Optional`.
[[nodiscard]] GP_CORE_API gp::Optional<gp::Int32> parseConsoleInt32(gp::StringView text) noexcept;

/// @brief Parses a console string representation into `gp::UInt32` (decimal or `0x` hex).
/// @param[in] text Input string view.
/// @return Parsed unsigned integer or empty `gp::Optional`.
[[nodiscard]] GP_CORE_API gp::Optional<gp::UInt32> parseConsoleUInt32(gp::StringView text) noexcept;

/// @brief Parses a console string representation into `gp::Float32` via `<charconv>`.
/// @param[in] text Input string view.
/// @return Parsed float or empty `gp::Optional`.
[[nodiscard]] GP_CORE_API gp::Optional<gp::Float32> parseConsoleFloat32(gp::StringView text) noexcept;

/// @brief Parses a console string representation into `gp::Float64` via `<charconv>`.
/// @param[in] text Input string view.
/// @return Parsed double or empty `gp::Optional`.
[[nodiscard]] GP_CORE_API gp::Optional<gp::Float64> parseConsoleFloat64(gp::StringView text) noexcept;

/// @brief Formats a boolean value into a stack `ConsoleString` (`"true"` or `"false"`).
/// @param[in] value Boolean value to format.
/// @return Formatted `ConsoleString`.
[[nodiscard]] GP_CORE_API ConsoleString formatConsoleBool(bool value) noexcept;

/// @brief Formats a `gp::Int32` into a stack `ConsoleString` via `<charconv>`.
/// @param[in] value Integer value to format.
/// @return Formatted `ConsoleString`.
[[nodiscard]] GP_CORE_API ConsoleString formatConsoleInt32(gp::Int32 value) noexcept;

/// @brief Formats a `gp::UInt32` into a stack `ConsoleString` via `<charconv>`.
/// @param[in] value Unsigned integer value to format.
/// @return Formatted `ConsoleString`.
[[nodiscard]] GP_CORE_API ConsoleString formatConsoleUInt32(gp::UInt32 value) noexcept;

/// @brief Formats a `gp::Float32` into a stack `ConsoleString` via `<charconv>`.
/// @param[in] value Float value to format.
/// @return Formatted `ConsoleString`.
[[nodiscard]] GP_CORE_API ConsoleString formatConsoleFloat32(gp::Float32 value) noexcept;

/// @brief Formats a `gp::Float64` into a stack `ConsoleString` via `<charconv>`.
/// @param[in] value Double value to format.
/// @return Formatted `ConsoleString`.
[[nodiscard]] GP_CORE_API ConsoleString formatConsoleFloat64(gp::Float64 value) noexcept;

}   // namespace gp::console
