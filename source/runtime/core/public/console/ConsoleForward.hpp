// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep

namespace gp::console
{

/// @section Enumerations

enum class ConsoleVarType : gp::UInt8;
enum class ConsoleVarFlags : gp::UInt32;
enum class ConsoleVarSetBy : gp::UInt8;
enum class [[nodiscard]] ConsoleResult : gp::UInt8;

/// @section Types & Descriptors

/// @brief Maximum UTF-8 byte length stored by string console variables without trailing null.
inline constexpr gp::USize kMaxConsoleStringLength = 255u;

/// @brief Fixed-capacity zero-allocation string buffer used by string CVars.
using ConsoleString = gp::container::BasicFixedString<char, kMaxConsoleStringLength>;

class ConsoleCommandArgs;
struct ConsoleManagerDesc;

/// @section Public Interfaces

class IConsoleObject;
class IConsoleVariable;
class IConsoleCommand;
class IConsoleManager;

/// @section Auto-Registration Wrappers

class AutoConsoleVariableRef;
class AutoConsoleVariableSink;
class AutoConsoleCommand;

/// @section Concrete Manager Types

class ConsoleManagerBase;

template <
    gp::USize InHashSlotCapacity,
    gp::USize InMaxDynamicVariables,
    gp::USize InMaxDynamicCommands,
    gp::USize InMaxSinks>
class BasicConsoleManager;

/// @brief Compact stack-friendly concrete console manager suitable for isolated subsystems and unit tests.
using ConsoleManager = BasicConsoleManager<256u, 32u, 32u, 32u>;

namespace internal
{

class ConsoleObjectBase;
class ConsoleVariableBase;
class ConsoleCommandBase;
class ConsoleDynamicVariable;
class ConsoleDynamicCommand;

}   // namespace internal

}   // namespace gp::console
