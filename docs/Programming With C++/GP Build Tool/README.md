---
sidebar_position: 0
title: GP Build Tool
description: A deep-dive into the custom CMake orchestration system that powers every module, plugin, and executable in the Graphical Playground Engine.
tags:
  - c++
  - build system
  - cmake
  - gpbt
  - architecture
---

# Graphical Playground Build Tool (GPBT)

<p style={{ color: '#ffffffa6' }}>
  A custom CMake orchestration layer that manages target scopes, dependency visibility, and
  topological build ordering across the entire engine, so you never have to think about it.
</p>

The Graphical Playground Build Tool (GPBT) is the internal CMake layer that sits above raw CMake
and gives every engineer a clean, declarative API for defining engine targets. Instead of writing
hundreds of lines of `target_link_libraries`, `target_include_directories`, and `add_library` calls
by hand, you describe *what* your module needs and GPBT figures out *how* to build it correctly.

This document covers everything: the internal phases, all target types, the full dependency model,
platform-conditional features, and every macro in the public API.

---

## How It Works: The Three Build Phases

Understanding the build phases is the key to understanding *why* GPBT works the way it does. A
naive CMake setup processes `CMakeLists.txt` files in discovery order, which means a module that
depends on another module discovered later would fail to configure. GPBT solves this with a
two-pass system.

### Phase 1 - REGISTRATION

GPBT recursively scans the source tree for every `CMakeLists.txt` file, executing each one in a
lightweight "registration" mode. During this pass, each target definition records only its name,
type, and dependency list into global CMake properties. No actual CMake targets are created yet.

This gives GPBT a complete map of the entire dependency graph before any target is built.

### Phase 2 - CONFIGURATION

With the full graph known, GPBT performs a **topological sort** using a depth-first algorithm. This
determines the correct order in which targets must be configured, dependencies always come before
the targets that depend on them.

Once sorted, GPBT re-processes each `CMakeLists.txt` in the correct order. This time, each target
definition creates the real CMake target (`add_library`, `add_executable`), sets all properties,
and links all dependencies.

:::tip Why does this matter for students?
If module B depends on module A, but the file scanner discovers B before A, a naive system would
fail because A's CMake target doesn't exist yet when B tries to link against it. The two-phase
system avoids this entirely: by the time B is configured, A is guaranteed to already exist.
:::

### Phase 3 - GENERATION

This is standard CMake. After GPBT finishes configuration, CMake generates the final build system
files (Ninja, Makefile, Visual Studio `.sln`, etc.) from the configured targets.

### Circular Dependency Detection

During the topological sort, if two targets depend on each other (directly or transitively), the
sort algorithm will stall. GPBT detects this and emits a targeted error:

```text
FATAL ERROR: Circular dependency detected! The following targets form an infinite dependency
loop and cannot be ordered: audio;engine
```

If a dependency name does not match any registered target at all (a typo or a missing module),
GPBT emits a warning identifying the exact missing name before halting.

---

## Target Types

GPBT v0.3.0 supports three target types, each with a distinct purpose.

| Type | CMake Kind | Shared? | Use Case |
| --- | --- | --- | --- |
| `module` | Library | Follows `BUILD_SHARED_LIBS` | Core engine systems (Core, RHI, Audio, etc.) |
| `executable` | Executable | - | Final programs (Editor, Standalone) |
| `plugin` | Shared Library | Always shared | Optional runtime-loaded extensions |

### Modules

A module is the standard building block of the engine. It compiles to either a static or shared
library depending on the `GP_BUILD_SHARED` CMake option. Most of the engine, Core, Engine,
RHI, HAL, Renderer, are modules.

### Executables

An executable is a final program that links against one or more modules. The Editor and the
Standalone player launcher are executables.

### Plugins

A plugin is always built as a shared library, regardless of the global `GP_BUILD_SHARED` setting.
Plugins are designed to be discovered and loaded at runtime by the engine's plugin system. They
appear in the `plugins/` IDE folder and their output goes to the plugins output directory.

:::note
Plugins are a v0.3.0 addition. Use them for optional features that the engine can load and unload
at runtime, such as scripting backends, renderer extensions, or platform-specific services.
:::

---

## Creating Targets

Every target definition follows the same bracket pattern: open with a `gpStart*` macro, configure
the target with property macros, then close with a `gpEnd*` macro. The scoping system ensures that
variables set inside a target definition cannot accidentally bleed into adjacent definitions.

### Modules

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(my_module)
  # ... configuration macros go here ...
gpEndModule()
```

### Executables

```cmake showLineNumbers
include(gp-build-tool)

gpStartExecutable(my_program)
  # ... configuration macros go here ...
gpEndExecutable()
```

### Plugins

```cmake showLineNumbers
include(gp-build-tool)

gpStartPlugin(my_plugin)
  # ... configuration macros go here ...
gpEndPlugin()
```

---

## Dependency Management

Dependencies are the heart of GPBT. Every `gpAdd*Dependency` call maps to a `target_link_libraries`
call under the hood, but GPBT adds strict duplicate prevention, automatic name resolution (from
short names like `core` to fully-qualified CMake targets like `gp::core`), and correct
topological ordering.

### The Four Visibility Levels

#### `gpAddPublicDependency(dependency)`

The dependency is linked **PUBLIC**. This means:
- Your target links against it.
- Any target that links against *your* target will also automatically link against it.

Use this when the dependency appears in your public headers, i.e., consumers of your module need
it to compile code that uses your module.

```cmake
gpAddPublicDependency(core)      # gp::core is exposed to all consumers
```

#### `gpAddPrivateDependency(dependency)`

The dependency is linked **PRIVATE**. This means:
- Your target links against it.
- Consumers of your target do **not** inherit it.

Use this for implementation-only dependencies, libraries used only in your `.cpp` files, not in
any header that other modules include.

```cmake
gpAddPrivateDependency(SDL3::SDL3-static)   # used only internally
```

#### `gpAddInternalDependency(dependency)`

The dependency is linked **PRIVATE** and is treated as an implementation detail. Use this for
dependencies that are needed to build the module's internal units but are not part of the module's
public contract. Semantically, this is the same as private at the CMake level, but it signals
intent to other engineers reading the `CMakeLists.txt`.

```cmake
gpAddInternalDependency(spirv_cross_core)
```

#### `gpAddDynamicDependency(dependency)`

A dynamic dependency creates a **build-order edge** without any link-time coupling. This means:
- The dependency target is guaranteed to be built before your target.
- Your target does **not** link against it at compile time.
- The dependency is expected to be loaded at runtime.

This is the correct relationship for RHI backends and plugins: the base `rhi` module does not
link against `rhi/vulkan` or `rhi/d3d12`, it loads them at runtime. But the backends must
exist in the output directory before the engine runs, so they must be built first.

```cmake
gpAddDynamicDependency(rhi/null)
gpAddDynamicDependency(rhi/vulkan)
```

:::note For students
Think of dynamic dependencies like a game level that needs certain DLLs in the same folder to run.
The level file does not reference them at compile time, but at runtime the engine needs to find and
load them. `add_dependencies` in CMake ensures they are built, but they are not linked.
:::

### Platform-Conditional Dependencies

Often, a dependency is only valid on certain platforms, a Windows-only graphics driver, a
Linux-only threading library, a macOS-only display framework. Rather than wrapping every dependency
in `if(WIN32)` blocks, GPBT provides platform-aware wrappers for all four visibility levels.

**Supported platform tokens:** `WINDOWS`, `LINUX`, `MAC`, `UNIX`, `ALL`

```cmake
gpAddPublicDependencyOnPlatform(dependency platform)
gpAddPrivateDependencyOnPlatform(dependency platform)
gpAddInternalDependencyOnPlatform(dependency platform)
gpAddDynamicDependencyOnPlatform(dependency platform)
```

#### Example: Platform-Specific RHI Dependencies

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(rhi)
  gpAddPublicDependency(core)

  # The null RHI is always present as a safe fallback
  gpAddDynamicDependency(rhi/null)

  # Instead of if(WIN32) blocks, use platform-conditional macros
  gpAddDynamicDependencyOnPlatform(rhi/d3d11 WINDOWS)
  gpAddDynamicDependencyOnPlatform(rhi/d3d12 WINDOWS)
  gpAddDynamicDependencyOnPlatform(rhi/vulkan UNIX)
  gpAddDynamicDependencyOnPlatform(rhi/opengl UNIX)
  gpAddDynamicDependencyOnPlatform(rhi/metal MAC)
gpEndModule()
```

:::tip
Platform-conditional macros are evaluated at configure time. On Linux, `WINDOWS` deps are silently
skipped. On macOS, `LINUX` deps are skipped. This keeps module definitions clean and readable
without cluttering them with `if`/`endif` blocks.
:::

---

## Source File Management

### Automatic Discovery

When a target is initialized, GPBT automatically globs all `*.cpp`, `*.cc`, and `*.cxx` files
from the `private/` and `internal/` directories relative to the target's `CMakeLists.txt` location.
Header files (`*.h`, `*.hpp`, `*.hh`, `*.hxx`) from `public/`, `internal/`, and `private/` are
collected for IDE integration.

You do not need to list source files manually for the standard directory layout.

### Expected Directory Structure

```
source/runtime/my_module/
├── CMakeLists.txt
├── public/                   # Public headers, exposed to consumers
│   └── my_module/
│       └── MyClass.hpp
├── private/                  # Implementation files, not exposed
│   └── MyClass.cpp
├── internal/                 # Internal headers, used within the module
│   └── MyClassInternal.hpp
├── tests/                    # Test sources (see Testing page)
│   └── MyClassTests.cpp
└── benchmarks/               # Benchmark sources (see Testing page)
    └── MyClassBenchmarks.cpp
```

### Manual Source Management

When you have sources outside the standard layout or need fine-grained control:

```cmake
# Add specific files or files from a custom directory
gpTargetAddSources(${__targetLocation}/generated/MyGeneratedClass.cpp)
gpTargetAddSourcesFromDirectory(${__targetLocation}/platform/linux)

# Remove files matching a directory name
gpTargetExcludeDirectory(windows)

# Remove files matching a specific filename
gpTargetExcludeFile(LegacyHelper.cpp)

# Remove files matching a regex pattern
gpTargetExcludeSourcesRegex(".*\\.generated\\.cpp$")
```

### Header-Only Targets

If your target contains only headers and no compiled sources, mark it explicitly. Without this,
GPBT would add a dummy `.cpp` file to satisfy CMake's requirement that every library have at
least one source file.

```cmake
gpStartModule(my_header_only_lib)
  gpTargetSetIsHeaderOnly(TRUE)
gpEndModule()
```

---

## Compile Definitions

### Built-In Engine Definitions

GPBT automatically stamps a standard set of compile definitions onto every target, tests
executable, and benchmark executable. These definitions are always consistent across all targets:

| Definition | When Active |
| --- | --- |
| `GP_BUILD_DEBUG=1` | Debug configuration |
| `GP_BUILD_RELEASE=1` | Release configuration |
| `GP_BUILD_RELWITHDEBINFO=1` | RelWithDebInfo configuration |
| `GP_BUILD_MINSIZEREL=1` | MinSizeRel configuration |
| `GP_ENABLE_PROFILING=1` | When `GP_ENABLE_PROFILING` CMake option is ON |
| `GP_BUILD_EDITOR=1` | When `GP_BUILD_EDITOR` CMake option is ON |
| `GP_USE_VULKAN=1` | When `GP_USE_VULKAN` is ON |
| `GP_USE_D3D12=1` | When `GP_USE_D3D12` is ON |
| `GP_USE_D3D11=1` | When `GP_USE_D3D11` is ON |
| `GP_USE_METAL=1` | When `GP_USE_METAL` is ON |
| `GP_USE_OPENGL=1` | When `GP_USE_OPENGL` is ON |

Additionally, every target automatically receives a unique export macro:

```
GP_<UPPER_SNAKE_CASE_TARGET_NAME>_API_EXPORTS
```

For example, the `rhi/vulkan` module gets `GP_RHI_VULKAN_API_EXPORTS`. This definition is used
to tag your DLL-exported symbols correctly on Windows.

### Custom Compile Definitions

You can add your own compile definitions with the standard visibility rules:

```cmake
gpAddPublicDefinitions(MY_FEATURE_ENABLED=1)     # visible to consumers
gpAddPrivateDefinitions(INTERNAL_DEBUG_MODE=1)   # implementation only
gpAddInternalDefinitions(ENABLE_EXTRA_LOGGING=1) # internal implementation only
```

Definitions must follow the format `KEY` or `KEY=VALUE`. A bare `KEY` is automatically expanded
to `KEY=1`.

---

## Compile Flags

### Adding Flags

```cmake
gpAddPrivateCompileFlag(-ffast-math)    # implementation only
gpAddPublicCompileFlag(-fopenmp)        # propagated to consumers
```

:::warning
GPBT validates flag format and will emit a fatal error if a MSVC-style flag (starting with `/`)
is used with a non-MSVC compiler. Use compiler-conditional macros (`if(MSVC)`) for flags that
differ between compilers.
:::

### Replacing Flags

To replace an existing flag (for example, to switch from `-O2` to `-O3`):

```cmake
# Remove flags matching the regex and add the new flag in their place
gpTargetReplaceCompileFlag(-O3 "-O[0-9]+" private)
```

### Strict Warnings

By default, GPBT enables strict warnings for all targets. The exact flags depend on the detected
compiler:

- **MSVC**: `/W4 /WX /permissive-`
- **Clang / AppleClang**: `-Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers`
- **GCC**: `-Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers`

You can disable strict warnings for a target if needed:

```cmake
gpStartModule(legacy_module)
  gpTargetSetStrictWarningEnabled(FALSE)
gpEndModule()
```

---

## Target Properties

### Output Name

By default, a target's output name is `gp_<lower_snake_case_target_name>`. For example, the
`rhi/vulkan` module compiles to `gp_rhi_vulkan.so` on Linux. You can override this:

```cmake
gpSetTargetOutputName(gpeditor)   # output file will be named gpeditor
```

### Precompiled Headers

Add one or more precompiled headers to speed up compilation of large modules:

```cmake
gpTargetAddPCHHeaders(${__targetLocation}/private/CorePCH.hpp)
```

### Unity Build

Unity builds batch multiple `.cpp` files into a single translation unit for faster cold-build
throughput (at the cost of hiding certain `#include` hygiene issues). GPBT uses a batch size of
16 source files per unity file.

```cmake
gpStartModule(renderer)
  gpTargetSetUnityBuildEnabled(TRUE)
gpEndModule()
```

:::tip When to use Unity Build
Unity builds are most beneficial on large modules with many small `.cpp` files. They are
not recommended during active development because they can slow down incremental builds and make
linker errors harder to attribute to specific files. Enable them for CI release builds.
:::

### IDE Folder Override

By default, GPBT places targets in the following IDE folders:

| Target Type | Default Folder |
| --- | --- |
| `module` | `modules` |
| `executable` | `executables` |
| `plugin` | `plugins` |
| `*_tests` | `tests` |
| `*_benchmarks` | `benchmarks` |

You can override the folder for any target:

```cmake
gpStartModule(rhi/vulkan)
  gpTargetSetFolder("modules/rhi")
gpEndModule()
```

---

## Executable-Specific Features

### GUI vs Console Window

On Windows, executables can be either console applications or GUI applications (which suppresses
the console window at launch). The default is console (`FALSE`).

```cmake
gpStartExecutable(editor)
  gpExecutableSetIsGUI(FALSE)   # FALSE = console, TRUE = GUI (no console window on Windows)
gpEndExecutable()
```

The macro also stamps `GP_EXECUTABLE_IS_GUI=<0|1>` as a compile definition so your C++ code can
branch on it at compile time.

### Windows Resource Files

On Windows, executables can embed `.rc` resource files (icons, version info, manifests):

```cmake
gpStartExecutable(editor)
  gpExecutableAddResource(${__targetLocation}/resources/AppIcon.rc)
gpEndExecutable()
```

### Custom Entry Point

If your executable uses a non-standard entry point:

```cmake
gpExecutableSetEntryPoint(${__targetLocation}/private/WinMain.cpp)
```

---

## Install Export

Once you have defined all your targets, you can generate a proper CMake package configuration
file that allows other CMake projects to `find_package(GPEngine)` and use the engine's targets.

Call `gpGenerateInstallExport()` once from your root `CMakeLists.txt`, after all
`add_subdirectory()` calls:

```cmake showLineNumbers
# CMakeLists.txt (root)

add_subdirectory(thirdparty)
add_subdirectory(source)

# Generate the install export, call this exactly once at the root level
gpGenerateInstallExport()
```

This generates:
- `GPEngineTargets.cmake`, the exported target definitions
- `GPEngineConfig.cmake`, the `find_package` entry point
- `GPEngineConfigVersion.cmake`, the version compatibility file

:::note
This feature requires a `cmake/GPEngineConfig.cmake.in` template file in your source root.
:::

---

## Complete API Reference

### Target Lifecycle

| Macro | Description |
| --- | --- |
| `gpStartModule(name)` | Open a module (library) target scope |
| `gpStartExecutable(name)` | Open an executable target scope |
| `gpStartPlugin(name)` | Open a plugin (always-shared library) target scope |
| `gpEndModule()` | Close a module scope |
| `gpEndExecutable()` | Close an executable scope |
| `gpEndPlugin()` | Close a plugin scope |
| `gpScanForTargets()` | Trigger REGISTRATION + CONFIGURATION for a directory tree |

### Source Management

| Macro | Description |
| --- | --- |
| `gpTargetAddSources(files...)` | Append explicit source files |
| `gpTargetAddSourcesFromDirectory(dir)` | Glob sources from an arbitrary directory |
| `gpTargetExcludeDirectory(name)` | Remove all sources under a named directory |
| `gpTargetExcludeFile(name)` | Remove a specific source file by name |
| `gpTargetExcludeSourcesRegex(regex)` | Remove all sources matching a regex |
| `gpTargetAddPCHHeaders(header)` | Add a precompiled header |
| `gpTargetSetIsHeaderOnly(value)` | Mark the target as header-only (no compiled sources) |

### Dependencies

| Macro | Description |
| --- | --- |
| `gpAddPublicDependency(dep)` | Link PUBLIC, propagated to consumers |
| `gpAddPrivateDependency(dep)` | Link PRIVATE, implementation only |
| `gpAddInternalDependency(dep)` | Link PRIVATE, internal implementation detail |
| `gpAddDynamicDependency(dep)` | Build-order edge only, no link-time coupling |
| `gpAddPublicDependencyOnPlatform(dep platform)` | PUBLIC dep, active only on `platform` |
| `gpAddPrivateDependencyOnPlatform(dep platform)` | PRIVATE dep, active only on `platform` |
| `gpAddInternalDependencyOnPlatform(dep platform)` | Internal dep, active only on `platform` |
| `gpAddDynamicDependencyOnPlatform(dep platform)` | Dynamic dep, active only on `platform` |

### Compile Definitions

| Macro | Description |
| --- | --- |
| `gpAddPublicDefinitions(def)` | Add a PUBLIC compile definition |
| `gpAddPrivateDefinitions(def)` | Add a PRIVATE compile definition |
| `gpAddInternalDefinitions(def)` | Add a PRIVATE internal compile definition |

### Compile Flags

| Macro | Description |
| --- | --- |
| `gpAddPublicCompileFlag(flag)` | Add a PUBLIC compile flag |
| `gpAddPrivateCompileFlag(flag)` | Add a PRIVATE compile flag |
| `gpTargetReplaceCompileFlag(new regex visibility)` | Replace existing flags matching a regex |

### Include Paths

| Macro | Description |
| --- | --- |
| `gpAddPublicIncludesPath(path)` | Add a PUBLIC include directory |
| `gpAddPrivateIncludesPath(path)` | Add a PRIVATE include directory |
| `gpAddInternalIncludesPath(path)` | Add a PRIVATE internal include directory |

### Target Flags

| Macro | Description |
| --- | --- |
| `gpTargetSetTestsEnabled(value)` | Enable test executable generation |
| `gpTargetSetBenchmarksEnabled(value)` | Enable benchmark executable generation |
| `gpTargetSetExamplesEnabled(value)` | Enable example generation |
| `gpTargetSetISPCEnabled(value)` | Enable ISPC compilation |
| `gpTargetSetStrictWarningEnabled(value)` | Enable/disable strict warnings |
| `gpTargetSetUnityBuildEnabled(value)` | Enable unity build (batch 16 files) |
| `gpTargetSetIsHeaderOnly(value)` | Mark target as header-only |

### Target Properties

| Macro | Description |
| --- | --- |
| `gpSetTargetOutputName(name)` | Override the output binary name |
| `gpTargetSetFolder(path)` | Override the IDE folder |

### Executable-Only

| Macro | Description |
| --- | --- |
| `gpExecutableSetIsGUI(value)` | TRUE = GUI (no console), FALSE = console |
| `gpExecutableAddResource(path)` | Add a Windows `.rc` resource file |
| `gpExecutableSetEntryPoint(file)` | Set a custom entry point source file |

### Install

| Function | Description |
| --- | --- |
| `gpGenerateInstallExport()` | Generate CMake package config for downstream consumers |

---

## Usage Examples

### A Simple Module

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(audio)
  gpAddPublicDependency(core)
  gpAddPrivateDependency(SDL3::SDL3-static)
  gpTargetSetTestsEnabled(TRUE)
gpEndModule()
```

### A Platform-Aware Module (HAL)

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(hal)
  gpAddPublicDependency(core)

  # Exclude platform-specific subdirectories on irrelevant platforms
  if(NOT WIN32)
    gpTargetExcludeDirectory(windows)
  endif()

  if(NOT (UNIX AND NOT APPLE))
    gpTargetExcludeDirectory(linux)
  endif()

  if(NOT APPLE)
    gpTargetExcludeDirectory(macos)
  endif()

  gpAddPrivateDependency(SDL3::SDL3-static)
gpEndModule()
```

### A Plugin

```cmake showLineNumbers
include(gp-build-tool)

gpStartPlugin(python_scripting)
  # Plugins are always shared, they are loaded by the engine at runtime
  gpAddPrivateDependency(core)
  gpAddPrivateDependency(Python3::Python3)
  gpTargetSetFolder("plugins/scripting")
gpEndPlugin()
```

### A Feature-Rich Executable (Editor)

```cmake showLineNumbers
include(gp-build-tool)

gpStartExecutable(editor)
  gpSetTargetOutputName(gpeditor)
  gpExecutableSetIsGUI(FALSE)
  gpExecutableAddResource(${__targetLocation}/resources/AppIcon.rc)

  gpAddPublicDependency(core)
  gpAddPublicDependency(hal)
  gpAddPublicDependency(engine)
  gpAddPrivateDependency(rhi)

  gpTargetSetUnityBuildEnabled(TRUE)
gpEndExecutable()
```

### The RHI Module (Dynamic Backends)

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(rhi)
  gpAddPublicDependency(core)

  # Null RHI is always present as a safe fallback
  gpAddDynamicDependency(rhi/null)

  # Platform-conditional dynamic dependencies, clean, no if/endif clutter
  gpAddDynamicDependencyOnPlatform(rhi/d3d11 WINDOWS)
  gpAddDynamicDependencyOnPlatform(rhi/d3d12 WINDOWS)
  gpAddDynamicDependencyOnPlatform(rhi/vulkan UNIX)
  gpAddDynamicDependencyOnPlatform(rhi/opengl UNIX)
  gpAddDynamicDependencyOnPlatform(rhi/metal MAC)
gpEndModule()
```
