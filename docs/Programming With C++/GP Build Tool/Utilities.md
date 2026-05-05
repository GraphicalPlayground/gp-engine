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

<p style={{ color: '#ffffffa6' }}>
  Logging, string manipulation, scoping, and phase management utilities that power the build tool
  internals and are available to any custom CMake code in the project.
</p>

GPBT exposes a set of utility modules that are used internally throughout the build tool and are
available to any CMake code that includes `gp-build-tool`. These utilities solve real pain points
in CMake development: messy output, inconsistent naming, variable leakage between scopes, and
accidental re-execution of initialization code.

---

## Logging (`gp-logger`)

CMake's default output is verbose, hard to scan, and impossible to color. When a warning fires,
CMake dumps a full stack trace that buries the actual message. When a fatal error occurs, the
user sees a wall of text. GPBT replaces all of this with a clean, color-coded logging layer.

All log functions accept one or more arguments, which are automatically concatenated. By default,
every message is prefixed with `[GPBT]` to visually separate build-tool output from raw CMake
or third-party output.

### Log Functions

#### `gpLog(...)`

Standard informational message. Use this for configuration progress and status updates.

```cmake
gpLog("Configuring the Audio Engine...")
gpLog("Audio Engine path resolved to: " ${CMAKE_CURRENT_SOURCE_DIR})
```

#### `gpSuccess(...)`

Bold green message prefixed with `SUCCESS:`. Use this to confirm completion of significant
configuration steps.

```cmake
gpSuccess("Vulkan backend configured and linked.")
```

#### `gpWarning(...)`

Bold yellow message prefixed with `WARNING:`. GPBT deliberately uses CMake's `STATUS` level
internally, which means no stack trace is dumped. The warning is highly visible without the noise.

```cmake
gpWarning("No test sources found in '${testDir}'. Skipping test generation.")
```

#### `gpFatal(...)`

Bold red message prefixed with `FATAL ERROR:`. GPBT prints the human-readable error cleanly,
then calls `FATAL_ERROR` with a short generic message to halt CMake execution. This ensures the
user sees the colored error before CMake's own error output.

```cmake
gpFatal("Target '${targetName}' was registered twice. Target names must be unique.")
```

#### `gpVerbose(...)`

Cyan message prefixed with `VERBOSE:`. Only printed when the `GP_BUILD_TOOL_VERBOSE` CMake option
is `ON`. Essential for debugging dependency resolution, scope state, and target configuration
order.

```cmake
# Activate with:  cmake -DGP_BUILD_TOOL_VERBOSE=ON ...
gpVerbose("Public includes resolved to: ${__targetPublicIncludes}")
```

#### `gpNewLine()`

Prints a blank line. Use this to separate logical groups of output for readability.

```cmake
gpLog("Configuring shaders...")
# ... shader configuration ...
gpSuccess("Shader pipeline ready.")
gpNewLine()
```

### Color Constants

The logger module exposes ANSI escape sequences as CMake variables so you can use colors in
your own formatted messages. These are automatically initialized when `gp-build-tool` is included.

| Variable | Effect |
| --- | --- |
| `${GP_RESET}` | Reset all formatting |
| `${GP_BOLD}` | Bold text |
| `${GP_RED}` | Red foreground |
| `${GP_GREEN}` | Green foreground |
| `${GP_YELLOW}` | Yellow foreground |
| `${GP_BLUE}` | Blue foreground |
| `${GP_MAGENTA}` | Magenta foreground |
| `${GP_CYAN}` | Cyan foreground |
| `${GP_WHITE}` | White foreground |
| `${GP_BOLD_RED}` | Bold red |
| `${GP_BOLD_GREEN}` | Bold green |
| `${GP_BOLD_YELLOW}` | Bold yellow |

```cmake
gpLog("${GP_BOLD}Initializing RHI pipeline...${GP_RESET}")
gpLog("${GP_CYAN}Selected backend: ${GP_BOLD}Vulkan${GP_RESET}")
```

### Prefix Management

In some contexts you may want to suppress the `[GPBT]` prefix, for example when printing a
formatted banner or table of configuration values where the prefix would break alignment.

```cmake
gpSetLogPrefixEnabled(FALSE)
gpLog("  Compiler:  ${CMAKE_CXX_COMPILER_ID}")
gpLog("  Standard:  C++23")
gpLog("  Platform:  ${CMAKE_SYSTEM_NAME}")
gpRestorePreviousLogPrefixState()
# [GPBT] prefix is restored here
```

- `gpSetLogPrefixEnabled(value)` — Enables (`TRUE`) or disables (`FALSE`) the prefix. Saves
  the previous state automatically.
- `gpRestorePreviousLogPrefixState()` — Restores the prefix to its state before the last
  `gpSetLogPrefixEnabled` call.

### Full Usage Example

```cmake showLineNumbers
include(gp-build-tool)

gpLog("Configuring Audio Engine...")

gpVerbose("Audio Engine source path: ${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT GP_ENABLE_SPATIAL_AUDIO)
  gpWarning("Spatial audio is disabled. Falling back to stereo-only output.")
  gpNewLine()
endif()

find_package(FMOD QUIET)
if(NOT FMOD_FOUND)
  gpFatal("FMOD library not found. The Audio Engine requires FMOD to compile.")
endif()

gpSuccess("Audio Engine configured successfully.")
```

---

## Stringification (`gp-stringify`)

GPBT maintains consistent naming across all its generated targets, properties, and macros by
using a set of string manipulation functions. These convert between naming conventions — from a
module path like `rhi/vulkan` to a CMake target export name like `gp_rhi_vulkan`, an alias like
`gp::rhi_vulkan`, or an uppercase property key like `GPBT_TARGET_RHI_VULKAN_LOCATION`.

All functions take an `input` string and write the result into the CMake variable named by
`outVar`.

### Case Checking

These functions return `TRUE` or `FALSE` in the output variable.

| Function | Convention Checked |
| --- | --- |
| `gpIsLowerSnakeCase(input outVar)` | `lower_snake_case` |
| `gpIsUpperSnakeCase(input outVar)` | `UPPER_SNAKE_CASE` |
| `gpIsCamelCase(input outVar)` | `camelCase` |
| `gpIsPascalCase(input outVar)` | `PascalCase` |

```cmake showLineNumbers
set(proposedName "my_render_pass")

gpIsLowerSnakeCase("${proposedName}" isValid)
if(NOT isValid)
  gpFatal("Module name '${proposedName}' must use lower_snake_case.")
endif()
```

### Case Conversion

These functions transform the input into a new naming convention. Internally they tokenize the
string on case transitions (e.g., `myHTTPResponse` becomes tokens `my`, `HTTP`, `Response`) and
on separator characters (`-` and `_`).

| Function | Output Convention | Example |
| --- | --- | --- |
| `gpToLowerSnakeCase(input outVar)` | `lower_snake_case` | `rhi/vulkan` -> `rhi_vulkan` |
| `gpToUpperSnakeCase(input outVar)` | `UPPER_SNAKE_CASE` | `rhi/vulkan` -> `RHI_VULKAN` |
| `gpToCamelCase(input outVar)` | `camelCase` | `rhi/vulkan` -> `rhiVulkan` |
| `gpToPascalCase(input outVar)` | `PascalCase` | `rhi/vulkan` -> `RhiVulkan` |

```cmake showLineNumbers
set(moduleName "NetworkManager")

# GPBT uses this internally to generate export macro names
gpToUpperSnakeCase("${moduleName}" macroPrefix)
set(exportMacro "GP_${macroPrefix}_API_EXPORTS")
# exportMacro = "GP_NETWORK_MANAGER_API_EXPORTS"
gpLog("Generated export macro: ${exportMacro}")

# And lower snake case for output binary names
gpToLowerSnakeCase("${moduleName}" binaryName)
set(binaryName "gp_${binaryName}")
# binaryName = "gp_network_manager"
```

---

## Scope Management (`gp-scope`)

CMake has no real notion of block scope. Variables set inside an `if`, `foreach`, or `function`
can bleed into the outer scope in unexpected ways. GPBT's scope system adds an explicit, opt-in
scope stack that tracks variable mutations and automatically rolls them back when the scope is
closed.

This is the mechanism that makes the `gpStart*` / `gpEnd*` pattern safe: every variable set
inside a target definition is rolled back to its previous value when `gpEnd*` is called.

:::note For students
Think of this like RAII in C++. When you push a scope, all subsequent variable assignments are
"registered". When you pop the scope, every registered variable is restored to the value it had
before the scope was opened (or unset, if it did not exist at all). No variable can accidentally
leak from one target definition into the next.
:::

### Manual Scope Management

Most code uses the named-scope variants (see below), but you can also push and pop raw scopes:

```cmake
gpPushScope()
  set(MY_VAR "hello")   # MY_VAR is now tracked
  # ... do work ...
gpPopScope()
# MY_VAR is now gone (or restored to its pre-push value)
```

### Scoped Variable Assignment

Inside a scope, use `gpSetScoped` instead of `set`. This registers the variable for automatic
cleanup and backs up any previous value.

```cmake showLineNumbers
gpPushScope()
  gpSetScoped(CMAKE_BUILD_TYPE "Release")
  # CMAKE_BUILD_TYPE is Release here
gpPopScope()
# CMAKE_BUILD_TYPE is restored to whatever it was before
```

For bulk initialization, use `gpSetScopedMultiple` with alternating `KEY VALUE` pairs:

```cmake showLineNumbers
gpPushScope()
  gpSetScopedMultiple(
    __targetName   "my_module"
    __targetType   "module"
    __targetSources ""
  )
gpPopScope()
```

To register variables that already exist (so they are restored without modifying their current
value), use `gpTrackScopedMultiple`:

```cmake
gpPushScope()
  gpTrackScopedMultiple(CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS)
  # Modify them freely; they will be restored on gpPopScope()
gpPopScope()
```

### Named Scopes

Named scopes add an additional safety layer: only one scope with a given name can be active at
a time, and scopes must be popped with the same name they were pushed with. This prevents
accidentally nesting two `target` scopes inside each other.

```cmake
gpPushNamedScope("target")
  # ... target configuration ...
gpPopNamedScope("target")
```

Attempting to push the same name twice, or popping with the wrong name, emits a fatal error.

### Scope Guards

These macros enforce invariants at the call site:

```cmake
# Check if a named scope is currently active
gpIsInNamedScope("target" isInsideTarget)
if(isInsideTarget)
  # ...
endif()

# Fatal error if NOT inside the named scope (used internally by all gpAdd* macros)
gpFatalIfNotInNamedScope("target" "gpAddPublicDependency can only be called within a target scope.")
```

### One-Time Execution Guard

Some initialization code should run exactly once per CMake invocation, regardless of how many
times the file is included. `gpExecuteOnce` provides a global guard backed by a CMake property.

```cmake showLineNumbers
gpExecuteOnce("MY_INIT_BANNER" _shouldRun)
if(_shouldRun)
  gpLog("${GP_MAGENTA}=== My Build Tool v1.0 ===${GP_RESET}")
endif()
# On all subsequent includes of this file, _shouldRun is FALSE
```

GPBT uses this for the startup banner so it prints exactly once at the beginning of the
configuration run, not once per `include(gp-build-tool)` call.

---

## Build Phase Management (`gp-utils`)

GPBT's two-phase system (REGISTRATION then CONFIGURATION) is enforced through a global phase
property. The phase system prevents target-creation code from running during registration, and
prevents registration-only code from running during configuration.

Most code never needs to interact with the phase system directly. It is documented here for
contributors working on the build tool internals.

### Phases

| Phase | Set By | What Happens |
| --- | --- | --- |
| `REGISTRATION` | `_implGpScanForTargets` at scan start | Targets register names and deps; no CMake targets are created |
| `CONFIGURATION` | `_implGpScanForTargets` after sorting | Targets create real CMake targets and link dependencies |
| `GENERATION` | CMake itself | CMake generates Ninja/Makefile/etc. build files |

### Phase Query

```cmake
_gpGetCurrentPhase(currentPhase)
if(currentPhase STREQUAL "REGISTRATION")
  # Safe to register global properties
elseif(currentPhase STREQUAL "CONFIGURATION")
  # Safe to call add_library / target_link_libraries / etc.
endif()
```

### Phase Transition

```cmake
# Called only by the scan system -- never call these manually
_gpSetCurrentPhase("REGISTRATION")
_gpSetCurrentPhase("CONFIGURATION")
```

Attempting to transition to the current phase (e.g., calling REGISTRATION when already
REGISTRATION) emits a fatal error. Attempting to set an unknown phase emits a fatal error.
Phase transitions are logged so they are visible in the CMake output.

:::warning
The phase functions are prefixed with `_gp` (single underscore) to signal they are internal
implementation details. Do not call them from module `CMakeLists.txt` files.
All public-facing GPBT API is prefixed with `gp` (no underscore).
:::
