---
sidebar_position: 1
title: Third Party Management
tags:
  - c++
  - build system
  - cmake
  - thirdparty
  - gpbt
  - architecture
  - fetchcontent
---

# Third-Party Management (`gp-thirdparty`)

Managing external dependencies in C++ can quickly clutter build scripts. The Graphical Playground
Build Tool (GPBT) simplifies this through the `gp-thirdparty` module. It acts as an intelligent
wrapper around CMake's standard `find_package` and `FetchContent` modules.

The core philosophy of GPBT's third-party management is **Local First, Fetch Second**. It will attempt
to find the dependency already installed on your system; if it fails, it will automatically clone a
specified Git repository, inject custom build options, and build the dependency from source.

## Directory Organization and Licensing

To maintain a healthy codebase and respect legal constraints, the engine strictly enforces how
third-party dependencies are organized on disk.

:::warning
### Licensing and Directory Structure

When integrating a new third-party dependency, you must isolate it within its own dedicated
subfolder under the root `/thirdparty` directory.

Within this subfolder, you are strictly required to include:

1. The `CMakeLists.txt` file containing the `gpFetchContent` configuration for that specific
   dependency.
2. A copy of the library's `LICENSE.md`. This ensures full legal compliance and makes it easy to
   audit the licenses of all shipped dependencies.
:::

Your local directory structure should look exactly like this:

```
/thirdparty
  /mydependency
    CMakeLists.txt
    LICENSE.md
```

## Core Functions

`gpFetchContent`

This is the primary function for resolving external dependencies. It takes a list of named arguments
to configure the fetch behavior.

**Arguments:**

- `NAME` _(Required)_: The identifier for the dependency.
- `GIT_REPOSITORY` _(Required)_: The URL of the Git repository to fallback to.
- `GIT_TAG` _(Required)_: The specific Git tag, branch, or commit hash to checkout (shallow clones
  are used for speed).
- `PACKAGE_NAME` _(Optional)_: The name passed to `find_package`. If omitted, it defaults to the
  `NAME` argument.
- `REQUIRED_VERSION` _(Optional)_: The version requirement to pass to `find_package`.
- `COMPONENTS` _(Optional)_: Specific components to request from `find_package`.
- `OPTIONS` _(Optional)_: A list of custom CMake build options to inject into the cache before
  the dependency is configured, formatted as `"KEY=VALUE"`.

`gpSetTargetFolder`

When integrating third-party code, your IDE's solution explorer can quickly become overwhelmed
with external targets. This helper function sets the `FOLDER` property of a target, organizing it
into a specific IDE folder (like in Visual Studio or Xcode) without affecting the actual build
process.

:::note
GPBT automatically ignores `INTERFACE_LIBRARY` and `ALIAS_LIBRARY` target types, as CMake does not
allow folders to be set on them.
:::

**Arguments:**

- `TARGET_NAME`: The name of the target to organize.
- `FOLDER_NAME`: The IDE folder path (e.g., `"thirdparty/graphics"`).

## Usage Examples

Below are examples demonstrating how to properly integrate third-party libraries using GPBT.

### Example 1: Integrating SDL3 (Static Build)

This example attempts to find `SDL3` locally. If it fails, it fetches release `3.4.0` from GitHub
and forces it to build statically while disabling its internal test suite. Finally, it organizes
the resulting targets into an IDE folder.

```cmake showLineNumbers
include(gp-build-tool)

gpFetchContent(
  NAME SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.0
  PACKAGE_NAME SDL3
  REQUIRED_VERSION 3.4.0
  OPTIONS
    "SDL_SHARED=OFF"
    "SDL_STATIC=ON"
    "SDL_TEST=OFF"
)

if(TARGET SDL3)
  gpSetTargetFolder(SDL_uclibc "thirdparty/sdl3")
  gpSetTargetFolder(SDL3 "thirdparty/sdl3")
  gpSetTargetFolder(SDL3-static "thirdparty/sdl3")
endif()
```

### Example 2: Platform-Specific Header Integration (DirectX 12)

You can easily wrap `gpFetchContent` in standard CMake conditionals to only pull dependencies on
specific platforms or when specific engine features are enabled.

```cmake showLineNumbers
include(gp-build-tool)

option(GP_USE_D3D12 "Enable D3D12 support" ON)

if (NOT GP_USE_D3D12 OR NOT WIN32)
  return()
endif()

gpFetchContent(
  NAME DirectXHeaders
  GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
  GIT_TAG v1.619.1
  REQUIRED_VERSION 1.619.1
  PACKAGE_NAME DirectX-Headers
  OPTIONS
    "BUILD_TESTING=OFF"
)

if(TARGET DirectX-Headers)
  gpSetTargetFolder(DirectX-Headers "thirdparty/graphics")
  if(TARGET DirectX-Guids)
    gpSetTargetFolder(DirectX-Guids "thirdparty/graphics")
  endif()
endif()
```

### Example 3: Tooling Integration (Catch2)

When fetching tools like Catch2, you often want to disable their internal examples, tests, and
documentation to keep your build times fast.

```cmake showLineNumbers
include(gp-build-tool)

gpFetchContent(
  NAME Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.14.0
  REQUIRED_VERSION 3.14.0
  PACKAGE_NAME Catch2
  OPTIONS
    "CATCH_BUILD_TESTING=OFF"
    "CATCH_BUILD_EXAMPLES=OFF"
    "CATCH_BUILD_EXTRA_TESTS=OFF"
    "CATCH_ENABLE_COVERAGE=OFF"
    "CATCH_ENABLE_WERROR=OFF"
    "CATCH_INSTALL_DOCS=OFF"
    "CATCH_INSTALL_EXTRAS=ON"
)

if(TARGET Catch2)
  gpSetTargetFolder(Catch2 "thirdparty/testing")
  gpSetTargetFolder(Catch2WithMain "thirdparty/testing")

  # Expose Catch2's extra CMake modules to the parent scope
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
endif()
```
