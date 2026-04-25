---
sidebar_position: 0
title: GP Build Tool
description: GP Build Tool is a custom build system designed to streamline the compilation and linking process for C++ projects, providing efficient dependency management and build automation.
tags:
  - c++
  - build system
  - cmake
  - gpbt
  - architecture
---

# Graphical Playground Build Tool (GPBT)

The Graphical Playground Build Tool (GPBT) is our internal CMake-based orchestration system.
It is designed to cleanly manage target scopes, visibility, dependencies, and complex topological
build orders across the engine.
The tool supports two primary target types: `module` and `executable`.

## Build Phases

To ensure dependencies are correctly resolved before target generation, GPBT operates in three
distinct phases:

- **REGISTRATION**: The source tree is recursively scanned for `CMakeLists.txt` files.
  Targets and their dependencies are registered globally, ensuring target names are unique.
- **CONFIGURATION**: Using depth-first search, the tool performs a topological sort to resolve
  circular dependencies and determine the exact build order.
  Targets are then translated into actual CMake targets (e.g., `add_library`, `add_executable`).
- **GENERATION**: CMake handles the final generation of the build system files.

## Target Definition

A target definition must always begin with a start macro and conclude with an end macro to maintain
scope integrity. Under the hood, this utilizes `gpPushNamedScope` and `gpPopNamedScope` to prevent
variable bleeding across CMakeLists.txt files.

### Core Macros

- `gpStartModule(targetName)`: Initializes a library target scope.
- `gpStartExecutable(targetName)`: Initializes an executable target scope.
- `gpEndModule()` / `gpEndExecutable()`: Finalizes the definition and cleans up the target scope.

### Source Management & Directory Structure

When a target is initialized, GPBT automatically assumes a specific directory structure for source
and header files:

- **Public Includes**: The `<target_location>/public` directory is mapped to the target's public
  build interface.
- **Private/Internal Includes**: The `<target_location>/private` and `<target_location>/internal`
  directories remain local to the target.
- **Automatic Source Globbing**: `*.cpp`, `*.cc`, and `*.cxx` files within the private and internal
  directories are automatically appended to the target's source list.

You can explicitly manage sources and exclusions:

- `gpTargetExcludeDirectory(directoryName)`: Excludes all files matching the directory pattern from
  the target.
- `gpTargetAddSources(...)`: Explicitly appends specific sources to the target.

## Dependency Management

Dependencies are strictly categorized by their visibility levels: `public`, `private`, `internal`,
and `dynamic`.

- `gpAddPublicDependency(dependency)`: The dependency is linked publicly, meaning consumers of your
  target will also inherit this dependency.
- `gpAddPrivateDependency(dependency)`: The dependency is linked privately and is used only in the
  implementation of your target.
- `gpAddInternalDependency(dependency)`: The dependency is linked as an interface dependency.
- `gpAddDynamicDependency(dependency)`: Registers a dynamic or conditionally linked dependency.

## Testing Integration

If tests are enabled for a target, GPBT will attempt to define a dedicated test executable.

- `gpTargetSetTestsEnabled(TRUE)`: Flags the target to generate tests.
- GPBT will automatically scan the `<target_location>/tests` directory for source files.
- The test executable automatically links privately against the `Catch2WithMain` target and your
  module.

## Usage Examples

Below are practical examples of how to define modules and executables using the GPBT standard.

### Defining an Executable (Editor)

```cmake showLineNumbers
include(gp-build-tool)

gpStartExecutable(editor)
  gpSetTargetOutputName(gpeditor)
  gpExecutableSetIsGUI(FALSE)
  gpExecutableAddResource(${__targetLocation}/resources/AppIcon.rc)

  gpAddPublicDependency(core)
  gpAddPublicDependency(hal)
gpEndExecutable()
```

### Defining a Complex Module (RHI)

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(rhi)
  gpAddPublicDependency(core)

  gpAddDynamicDependency(rhi/null)

  if (WIN32 AND GP_USE_D3D11)
    gpAddDynamicDependency(rhi/d3d11)
  endif()

  if (WIN32 AND GP_USE_D3D12)
    gpAddDynamicDependency(rhi/d3d12)
  endif()

  if ((UNIX OR WIN32) AND GP_USE_VULKAN)
    gpAddDynamicDependency(rhi/vulkan)
  endif()

  if ((WIN32 OR UNIX) AND GP_USE_OPENGL)
    gpAddDynamicDependency(rhi/opengl)
  endif()

  if (APPLE AND GP_USE_METAL)
    gpAddDynamicDependency(rhi/metal)
  endif()
gpEndModule()
```

### Filtering Platform-Specific Code (HAL)

```cmake showLineNumbers
include(gp-build-tool)

gpStartModule(hal)
  gpAddPublicDependency(core)

  if (NOT WIN32)
    gpTargetExcludeDirectory("windows")
  endif()

  if (NOT UNIX AND NOT APPLE)
    gpTargetExcludeDirectory("linux")
  endif()

  if (NOT APPLE)
    gpTargetExcludeDirectory("macos")
  endif()

  gpAddPrivateDependency(SDL3::SDL3-static)
gpEndModule()
```
