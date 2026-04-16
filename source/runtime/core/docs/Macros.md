---
title: Core Macros
description: Core macros for the C++ runtime.
tags:
    - c++
    - macros
    - runtime
    - build
---

# Core Macros

This document outlines the suite of preprocessor macros used across the engine to establish
compiler, platform, and architecture details, as well as essential utilities for memory alignment
and compiler hints.

:::tip
All boolean-style detection macros in the GP engine are strictly defined to either `1` or `0`. You
can safely use them in both `#if` directives and standard `if` statements without worrying about
undefined macro warnings.
:::

## Platform Detection

These macros define the operating system environment where the engine is being compiled. They are
implemented in `DetectPlatform.hpp`.

### Base Platforms

| Macro                       | Description                                                        |
|-----------------------------|--------------------------------------------------------------------|
| `GP_PLATFORM_WINDOWS`       | Defined to `1` when compiling for Windows.                         |
| `GP_PLATFORM_MACOS`         | Defined to `1` when compiling for macOS.                           |
| `GP_PLATFORM_IOS`           | Defined to `1` when compiling for iOS (device or simulator).       |
| `GP_PLATFORM_LINUX`         | Defined to `1` when compiling for Linux.                           |
| `GP_PLATFORM_ANDROID`       | Defined to `1` when compiling for Android.                         |
| `GP_PLATFORM_WEB`           | Defined to `1` when compiling for WebAssembly via Emscripten.      |

### Platform Families

To simplify checks across related operating systems, use the following family macros:

| Macro                     | Included Platforms                                                   |
|---------------------------|----------------------------------------------------------------------|
| `GP_PLATFORM_DESKTOP`     | Windows, macOS, Linux                                                |
| `GP_PLATFORM_MOBILE`      | iOS, Android                                                         |
| `GP_PLATFORM_APPLE`       | macOS, iOS                                                           |
| `GP_PLATFORM_UNIX`        | Linux, macOS, iOS, Android                                           |

:::note Windows Specifics
When `GP_PLATFORM_WINDOWS` is active, the engine automatically defines `WIN32_LEAN_AND_MEAN` and
`NOMINMAX` to prevent header bloat and naming collisions with the standard library
`std::min` and `std::max`.
:::

## Architecture Detection

Found in `DetectArchitecture.hpp`, these macros identify the target CPU architecture.

### Instruction Sets

| Macro                     | Description                                                          |
|---------------------------|----------------------------------------------------------------------|
| `GP_ARCHITECTURE_X64`     | 64-bit x86 architecture (AMD64/x86_64).                              |
| `GP_ARCHITECTURE_X86`     | 32-bit x86 architecture.                                             |
| `GP_ARCHITECTURE_ARM64`   | 64-bit ARM architecture (AArch64).                                   |
| `GP_ARCHITECTURE_ARM32`   | 32-bit ARM architecture.                                             |
| `GP_ARCHITECTURE_WASM`    | WebAssembly target.                                                  |

### Architecture Families & Bit-Width

| Macro                           | Description                                                    |
|---------------------------------|----------------------------------------------------------------|
| `GP_ARCHITECTURE_X86_FAMILY`    | Evaluates to `1` for either x86 or x64.                        |
| `GP_ARCHITECTURE_ARM_FAMILY`    | Evaluates to `1` for either ARM32 or ARM64.                    |
| `GP_ARCHITECTURE_64BIT`         | Evaluates to `1` for x64 or ARM64.                             |
| `GP_ARCHITECTURE_32BIT`         | Evaluates to `1` for x86 or ARM32.                             |

## SIMD Detection

The engine detects available Single Instruction, Multiple Data (SIMD) instruction sets to allow
vectorization optimizations (`DetectSimd.hpp`).

You can check `GP_SIMD_AVAILABLE` to quickly determine if any SIMD instruction set is supported by
the current target.
- x86/x64 Family: `GP_SIMD_SSE`, `GP_SIMD_SSE2`, `GP_SIMD_SSE3`, `GP_SIMD_SSSE3`, `GP_SIMD_SSE4_1`, `GP_SIMD_SSE4_2`, `GP_SIMD_AVX`, `GP_SIMD_AVX2`, `GP_SIMD_AVX512`, `GP_SIMD_FMA`
- ARM Family: `GP_SIMD_NEON`, `GP_SIMD_ARM_FMA`
- WebAssembly: `GP_SIMD_WASM128`

## Compiler Detection

Located in `DetectCompiler.hpp`, these macros identify the active C++ compiler and its version.

| Macro                         | Description                                                      |
|-------------------------------|------------------------------------------------------------------|
| `GP_COMPILER_MSVC`            | Microsoft Visual C++.                                            |
| `GP_COMPILER_CLANG`           | LLVM Clang.                                                      |
| `GP_COMPILER_GCC`             | GNU Compiler Collection.                                         |
| `GP_COMPILER_EMSCRIPTEN`      | Emscripten compiler (WebAssembly).                               |
| `GP_COMPILER_INTEL`           | Intel C++ Compiler (`__INTEL_COMPILER` or `__ICC`).              |

:::info
You can access the compiler's normalized version using `GP_COMPILER_VERSION`. The format varies
slightly depending on the compiler (e.g., Clang and GCC use `major * 10000 + minor * 100 + patch`).
:::

## Build Configuration Detection

Build configurations and language standards are managed in `DetectBuild.hpp`.

### C++ Standard

The engine enforces modern C++ practices. `GP_CXX_STANDARD` contains the numeric value of the active
standard (e.g., `202302L` for C++23).

Additionally, internal macros (`GP_INTERNAL_CXX11` through `GP_INTERNAL_CXX26`) evaluate to `1` if
the current compilation targets that standard or higher.

:::danger Minimum C++ Requirement
The Graphical Playground Engine **requires C++23 by default**. Compiling with an older standard
will trigger a hard compiler error. If you absolutely must build against an older standard,
define `GP_ALLOW_OLDER_STANDARDS` beforehand.
:::

### Build Types

| Macro                     | Description                                                          |
|---------------------------|----------------------------------------------------------------------|
| `GP_BUILD_DEBUG`          | `1` when `DEBUG` or `_DEBUG` is defined; `0` otherwise.              |
| `GP_BUILD_RELEASE`        | `1` when debug flags are absent; `0` otherwise.                      |

## Utility Macros

Defined in `MacroUtilities.hpp`, these provide cross-platform abstractions for common compiler
attributes and memory operations.

### String & Token Manipulation

- `GP_CONCAT(a, b)`: Safely concatenates two tokens.
- `GP_STRINGIFY(x)`: Converts a token into a string literal.
- `GP_FIRST(...)` / `GP_FIRST_OR_DEFAULT(first, ...)`: Variadic helpers to extract the first
  argument, primarily used for formatting fallbacks (GP_FORMAT_MSG).

### Versioning

- `GP_VERSION(major, minor, patch)`: Packs a semantic version into a single integer.
- `GP_VERSION_STRINGIFY(major, minor, patch)`: Creates a "major.minor.patch" string literal.

### Inlining & Optimization Constraints

| Macro            | Description                                                                   |
|------------------|-------------------------------------------------------------------------------|
| `GP_FORCEINLINE` | Forces the compiler to inline the function (e.g., `__forceinline` or `__attribute__((always_inline))`). |
| `GP_INLINE`      | Suggests inlining, but the compiler may ignore the hint.                      |
| `GP_NOINLINE`    | Explicitly prevents the compiler from inlining the function.                  |
| `GP_RESTRICT`    | Asserts that a pointer does not alias any other pointer in the current scope. |

### Code Flow & Hints

| Macro             | Description                                                                  |
|-------------------|------------------------------------------------------------------------------|
| `GP_LIKELY(x)`    | Hints to the branch predictor whether an expression evaluates to true or false. |
| `GP_UNLIKELY(x)`  | Hints to the branch predictor whether an expression evaluates to true or false. |
| `GP_UNREACHABLE()`| Indicates a code path will never be executed, allowing the optimizer to aggressively prune logic. Undefined behavior if reached. |
| `GP_ASSUME(x)`    | Provides a boolean hint to the optimizer without evaluating a branch.        |
| `GP_DEBUGBREAK()` | Triggers a programmatic breakpoint in an attached debugger (`__debugbreak()` / `SIGTRAP`). |
| `GP_FUNCSIG`      | Gets the full signature of the current function as a string literal.         |

### Attributes

| Macro                                    | Description                                           |
|------------------------------------------|-------------------------------------------------------|
| `GP_DEPRECATED(version, msg)`            | Marks a function/class as deprecated with a version reference and alternative suggestion. |
| `GP_NODISCARD` / `GP_NODISCARD_MSG(msg)` | Yields a compiler warning if the caller ignores the return value. |
| `GP_MAYBE_UNUSED` / `GP_UNUSED(x)`       | Suppresses warnings for variables or parameters that might not be used in all build configs. |
| `GP_NORETURN`                            | Marks a function as never returning to the caller (e.g., an exception thrower or fatal error handler). |
| `GP_NO_UNIQUE_ADDRESS`                   | Allows empty members to occupy zero space (Empty Base Optimization). |

### Memory & Alignment

- `GP_ALIGN(x)`: Specifies the minimum alignment of a variable/struct in bytes.
- `GP_ALIGNED_ALLOC(size, alignment)`: Allocates heap memory aligned to the specified boundary. (Ensure size is a multiple of the alignment where required by the platform).
- `GP_ALIGNED_FREE(ptr)`: Frees memory allocated with `GP_ALIGNED_ALLOC`.

### Assertions

- `GP_ASSERT(condition, msg)`: Evaluates the condition and triggers a breakpoint with a message if it fails. Only active in debug builds.
