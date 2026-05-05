---
sidebar_position: 1
title: Third-Party Management
tags:
  - c++
  - build system
  - cmake
  - thirdparty
  - gpbt
  - architecture
  - fetchcontent
---

<p style={{ color: '#ffffffa6' }}>
  A Local-First, Fetch-Second strategy for integrating external dependencies without cluttering
  the codebase or violating licenses.
</p>

Managing external dependencies in C++ is notoriously brittle. Header-only libraries get copied
manually, Git submodules drift out of sync, and CMake's `find_package` fails silently on
developer machines that have the wrong version installed. GPBT solves this through the
`gp-thirdparty` module.

The core philosophy is **Local First, Fetch Second**: for every dependency, GPBT first checks
whether a compatible version is already installed on the host system. Only if that check fails
does it fall back to fetching the exact pinned version from Git, building it from source, and
making it available, transparently and reproducibly.

---

## Directory Organization and Licensing

To maintain a healthy codebase and respect legal constraints, the engine enforces a strict layout
for all third-party code.

:::warning Licensing is mandatory
When integrating any new third-party dependency, you must isolate it in its own dedicated
subfolder under the root `/thirdparty` directory. Within that subfolder you are required to
include:

1. A `CMakeLists.txt` file containing the `gpFetchContent` call for that dependency.
2. A copy of the library's license file, named `LICENSE.md` (or the upstream format, e.g.,
   `LICENSE`, `LICENSE.txt`).

Shipping a library without its license is a legal violation. GPBT's directory structure makes
auditing trivial, every dependency and its license live side by side.
:::

The expected layout:

```text
/thirdparty
  /sdl3
    CMakeLists.txt
    LICENSE.md
  /catch2
    CMakeLists.txt
    LICENSE.md
  /vulkan
    CMakeLists.txt
    LICENSE.md
```

Each subdirectory is added to the build via the root `thirdparty/CMakeLists.txt`, which calls
`add_subdirectory` for each one (conditionally, for platform-specific libraries).

---

## `gpFetchContent`

This is the primary function for resolving external dependencies. It wraps CMake's standard
`find_package` and `FetchContent` modules with additional validation, logging, and option
injection.

### Arguments

| Argument | Required | Description |
| --- | --- | --- |
| `NAME` | Yes | The identifier for this dependency. Used as the FetchContent name. |
| `GIT_REPOSITORY` | Yes | The Git repository URL to clone if no local installation is found. |
| `GIT_TAG` | Yes | The exact tag, branch, or commit hash to clone (shallow clone). |
| `PACKAGE_NAME` | No | The name passed to `find_package`. Defaults to `NAME`. |
| `REQUIRED_VERSION` | No | The version passed to `find_package`. |
| `COMPONENTS` | No | Specific components to request from `find_package`. |
| `OPTIONS` | No | Cache variables to inject before configuring the dependency, formatted as `"KEY=VALUE"`. |

### How It Works

1. **Local check**, `find_package(<PACKAGE_NAME> <REQUIRED_VERSION> QUIET)` is called first.
   GPBT uses `QUIET` to suppress the verbose "not found" output that CMake normally emits.
2. **Local success**, If the package is found, its path and version are logged and the function
   returns. The fetched sources are never touched.
3. **Fallback fetch**, If the package is not found locally, `FetchContent_Declare` registers the
   Git repository with `GIT_SHALLOW TRUE` for speed. Any `OPTIONS` entries are injected into the
   CMake cache as `CACHE STRING ... FORCE` before `FetchContent_MakeAvailable` is called.
4. **Available**, The dependency's targets are now visible to the rest of the build.

:::tip For students
`GIT_SHALLOW TRUE` means CMake only downloads the single commit pointed to by `GIT_TAG`, not the
entire Git history. For large repositories like SDL3 this can reduce download time from minutes
to seconds. Always pin to a specific release tag (e.g., `release-3.4.0`) rather than a branch
name, branches move, tags do not.
:::

---

## `gpSetTargetFolder`

When third-party libraries are integrated, their CMake targets appear alongside engine targets in
your IDE. This can make the solution explorer difficult to navigate. `gpSetTargetFolder` moves a
target into a named IDE folder without affecting the build at all.

```cmake
gpSetTargetFolder(TARGET_NAME FOLDER_NAME)
```

GPBT automatically skips `INTERFACE_LIBRARY` and `ALIAS_LIBRARY` targets, since CMake does not
permit setting the `FOLDER` property on them.

```cmake showLineNumbers
if(TARGET SDL3)
  gpSetTargetFolder(SDL3         "thirdparty/sdl3")
  gpSetTargetFolder(SDL3-static  "thirdparty/sdl3")
  gpSetTargetFolder(SDL_uclibc   "thirdparty/sdl3")
endif()
```

---

## Usage Examples

### Example 1, SDL3 (Static Build)

This example attempts to find SDL3 locally. If it is not found, it fetches release `3.4.0` from
GitHub, forces a static build, disables SDL's internal test suite (which would otherwise add
dozens of test executables to your IDE), and organizes the targets into a `thirdparty/sdl3`
folder.

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
  gpSetTargetFolder(SDL_uclibc  "thirdparty/sdl3")
  gpSetTargetFolder(SDL3        "thirdparty/sdl3")
  gpSetTargetFolder(SDL3-static "thirdparty/sdl3")
endif()
```

### Example 2, Platform-Specific Headers (DirectX 12)

Wrap `gpFetchContent` in a CMake `return()` guard to skip the entire file on platforms where
the dependency is irrelevant. This is cleaner than an `if/endif` around the entire block.

```cmake showLineNumbers
include(gp-build-tool)

if(NOT GP_USE_D3D12 OR NOT WIN32)
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

### Example 3, Testing Frameworks (Catch2)

Catch2 ships with its own set of examples, tests, and documentation targets. Disabling them
keeps build times fast and keeps the IDE clean.

After fetching Catch2, its extras directory must be appended to `CMAKE_MODULE_PATH` so that
the `Catch` CMake module (which provides `catch_discover_tests`) is available to the rest
of the build. The `PARENT_SCOPE` propagates this change up to the calling `CMakeLists.txt`.

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
  gpSetTargetFolder(Catch2WithMain "thirdparty/testing")
  gpSetTargetFolder(Catch2         "thirdparty/testing")

  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
endif()
```

### Example 4, Shader Toolchain (glslang)

Some dependencies need multiple targets organized. Using a helper variable keeps the
`gpSetTargetFolder` calls readable.

```cmake showLineNumbers
include(gp-build-tool)

gpFetchContent(
  NAME glslang
  GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG vulkan-sdk-1.3.296.0
  OPTIONS
    "GLSLANG_TESTS=OFF"
    "GLSLANG_ENABLE_INSTALL=OFF"
    "BUILD_EXTERNAL=OFF"
    "ENABLE_GLSLANG_BINARIES=OFF"
    "ENABLE_SPVREMAPPER=OFF"
)

foreach(_glslangTarget IN ITEMS
    glslang glslang-default-resource-limits
    MachineIndependent OSDependent GenericCodeGen
    SPIRV glslang-build-info)
  gpSetTargetFolder(${_glslangTarget} "thirdparty/shaders/glslang")
endforeach()
```

---

## Adding a New Dependency Checklist

When adding a third-party library to the engine, follow this checklist:

1. Create `/thirdparty/<library_name>/CMakeLists.txt`.
2. Copy the library's license into `/thirdparty/<library_name>/LICENSE.md`.
3. Write a `gpFetchContent(...)` call pinned to an exact release tag, never a branch.
4. Disable the library's own tests, examples, and documentation via `OPTIONS`.
5. Add `gpSetTargetFolder(...)` calls to keep the IDE organized.
6. Add `add_subdirectory(<library_name>)` in `/thirdparty/CMakeLists.txt`, guarded by
   platform conditions if the library is platform-specific.
7. Test the build on a clean machine (or inside the CI Docker image) to verify the fetch path
   works, not just the local-installation path.
