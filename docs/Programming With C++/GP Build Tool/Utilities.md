---
sidebar_position: 2
title: Utilities
tags:
  - c++
  - build system
  - cmake
  - utilities
  - gpbt
  - architecture
---

# Utilities

## Logging (`gp-logger`)

Debugging CMake configuration steps can be notoriously difficult due to its verbose and often
cluttered output. To solve this, the Graphical Playground Build Tool (GPBT) provides a suite of
custom logging utilities. These functions utilize ASCII color codes to categorize information,
suppress unnecessary CMake stack traces on warnings, and gracefully halt execution on fatal errors.

### Standard Logging Functions

All logging functions accept one or more arguments, which are automatically concatenated into a
single message string. By default, all messages are prefixed with `[GPBT] `.

- `gpLog(...)`: Prints a standard informational message to the console.
- `gpSuccess(...)`: Prints a bold green message prefixed with `SUCCESS:`. Ideal for confirming the
  completion of major configuration steps.
- `gpWarning(...)`: Prints a bold yellow message prefixed with `WARNING:`.
  - _Note: GPBT explicitly uses CMake's `STATUS` level internally for warnings. This ensures the
    warning is highly visible without triggering CMake's default behavior of dumping an unreadable
    stack trace_.
- `gpFatal(...)`: Prints a bold red message prefixed with `FATAL ERROR:`, and immediately halts
  CMake execution.
  - _Note: GPBT prints your custom colored error clearly, and then calls `FATAL_ERROR` with a
    generic message to cleanly stop the build process_.
- `gpVerbose(...)`: Prints a cyan message prefixed with `VERBOSE:`. These messages are only printed
  if the `GP_BUILD_TOOL_VERBOSE` option is set to ON during CMake configuration. This is highly
  recommended for debugging complex dependency graphs or target properties.

### Formatting & Readability

- `gpNewLine()`: Prints a blank line to the console. Use this to separate logical blocks of
  configuration output for better readability.

### Prefix Management

By default, GPBT prepends `[GPBT] ` to all log outputs to distinguish engine build steps from
standard CMake or third-party outputs. You can control this behavior within your scopes:

- `gpSetLogPrefixEnabled(value)`: Enables (`TRUE`) or disables (`FALSE`) the `[GPBT]` prefix on
  subsequent log calls. This also stores the previous state in memory.
- `gpRestorePreviousLogPrefixState()`: Restores the log prefix to whatever state it was in prior to
  the last gpSetLogPrefixEnabled call. Useful for ensuring temporary logging changes don't bleed out
  of your current function or scope.

### Usage Example

Here is how you might use the logging utilities within a custom target configuration or module
registration:

```cmake showLineNumbers
include(gp-build-tool)

# Standard informational logging
gpLog("Configuring the Audio Engine...")

# Verbose logging (only shows if GP_BUILD_TOOL_VERBOSE=ON)
gpVerbose("Audio Engine path resolved to: ${CMAKE_CURRENT_SOURCE_DIR}")

# Check a condition and warn the user
if(NOT GP_ENABLE_SPATIAL_AUDIO)
  gpWarning("Spatial audio is disabled. Falling back to stereo output.")
  gpNewLine() # Add some breathing room in the console
endif()

# Check a critical dependency
find_package(FMOD)
if(NOT FMOD_FOUND)
  # This will print in bold red and completely stop the CMake configuration
  gpFatal("FMOD library not found! The Audio Engine requires FMOD to compile.")
endif()

# Success state
gpSuccess("Audio Engine successfully configured.")
```

## Stringification (`gp-stringify`)

The Graphical Playground Build Tool (GPBT) includes a suite of robust string manipulation functions
designed to help maintain clean, standard-compliant naming conventions during the configuration and
generation phases. These utilities are divided into two main categories: Case Checking and
Case Conversion.

All stringification utilities take an `input` string and an `outVar` (the name of the CMake variable
where the result will be stored).

### Case Checking

Case checking functions evaluate an input string against standard naming conventions using
regular expressions. They return a boolean `TRUE` or `FALSE` in the provided output variable.

- `gpIsLowerSnakeCase(input outVar)`: Checks if the provided string strictly follows
  `lower_snake_case` formatting.
- `gpIsUpperSnakeCase(input outVar)`: Checks if the provided string strictly follows
  `UPPER_SNAKE_CASE` formatting.
- `gpIsCamelCase(input outVar)`: Checks if the provided string strictly follows `camelCase`
  formatting, where the first letter is lowercase and subsequent words are capitalized.
- `gpIsPascalCase(input outVar)`: Checks if the provided string strictly follows `PascalCase`
  formatting, where every word begins with a capital letter.

#### Example: Validating a Target Name

```cmake showLineNumbers
include(gp-build-tool)

set(myTargetName "my_awesome_module")

gpIsLowerSnakeCase("${myTargetName}" isStandardCompliant)

if(NOT isStandardCompliant)
    gpFatal("Target name '${myTargetName}' must be in lower_snake_case!")
endif()
```

### Case Conversion

Case conversion functions allow you to easily transform strings from one naming convention to
another. Internally, these functions tokenize the input string based on case transitions and
separators (such as `_` or `-`) before safely reassembling them into the desired format.

- `gpToLowerSnakeCase(input outVar)`: Converts the provided string into `lower_snake_case`. This is
  extensively used internally by GPBT to standardize target output names (e.g., `gp_my_module`).
- `gpToUpperSnakeCase(input outVar)`: Converts the provided string into `UPPER_SNAKE_CASE`. This is
  particularly useful for generating C++ preprocessor definitions or global property keys.
- `gpToCamelCase(input outVar)`: Converts the provided string into `camelCase`.
- `gpToPascalCase(input outVar)`: Converts the provided string into `PascalCase`.

#### Example: Generating Export Definitions

```cmake showLineNumbers
include(gp-build-tool)

set(moduleName "NetworkManager")

# Convert to upper snake case to generate a standard API export macro
gpToUpperSnakeCase("${moduleName}" macroPrefix)
set(exportMacro "GP_${macroPrefix}_API_EXPORTS")

# exportMacro now holds: "GP_NETWORK_MANAGER_API_EXPORTS"
gpLog("Generated macro: ${exportMacro}")
```
