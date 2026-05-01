---
sidebar_position: 4
title: Third Parties
description: Information about third-party libraries and tools used in the project.
tags:
  - third-party
  - libraries
  - tools
---

# Third Parties

<p style={{ color: '#ffffffa6' }}> Information about third-party libraries and tools used in the project. </p>

`gp-engine` integrates a curated set of third-party libraries to support windowing, graphics API
abstraction, shader compilation, and performance profiling. This document lists every external
dependency, its version, and its role within the engine. We do not bundle dependencies beyond what
is listed here; all third-party code is tracked as submodules or vendored under the `third-party/`
directory.

## Windowing & Input

### SDL3

- **Version**: `3.4.0`
- **Role**: Cross-platform windowing, OpenGL/Vulkan surface creation, input event handling, and
  gamepad support.
- SDL3 is the primary abstraction layer between `gp-engine` and the host operating system's
  windowing system. All platform window handles passed to the rendering backends are obtained
  through SDL3.
- **License**: [Zlib](https://www.libsdl.org/license.php)
- **Repository**: [libsdl-org/SDL](https://github.com/libsdl-org/SDL)

## Testing

### Catch2

- **Version**: `3.14.0`
- **Role**: Unit and integration testing framework for all engine modules.
- We use Catch2 as the sole testing framework across the codebase. All tests are located in the
  `tests/` directory and are organized to mirror the module structure of the engine.
  Do not introduce additional testing frameworks without prior discussion.
- **License**: [BSL-1.0](https://github.com/catchorg/Catch2/blob/devel/LICENSE.txt)
- **Repository**: [catchorg/Catch2](https://github.com/catchorg/Catch2)

## Graphics APIs

The engine supports multiple rendering backends. Each backend maps to one or more of the following
graphics API integrations.

:::tip

The active backend is selected at CMake configuration time using the `-DGP_USE_VULKAN=ON` or
`-DGP_USE_OPENGL=ON` flags. Integrated APIs such as `OpenGL`, `Metal`, and `DirectX 11` do not
require a separate installation step, as they are provided by the host platform.

:::

### DirectX 11 *(integrated)*

- **Version**: Integrated, provided by the Windows SDK.
- **Role**: Legacy Direct3D 11 rendering backend targeting Windows platforms.
- No vendored headers are required. The engine links against `d3d11.lib` and `dxgi.lib` supplied
  by the Windows SDK present in the build environment.

### DirectXHeaders (D3D12)

- **Version**: `1.619.1`
- **Role**: Provides the official D3D12 and DXGI headers for the Direct3D 12 rendering backend.
- These headers are sourced from Microsoft's official distribution and are preferred over those
  bundled with older Windows SDKs to ensure access to the latest API surface.
- **License**: [MIT](https://github.com/microsoft/DirectX-Headers/blob/main/LICENSE)
- **Repository**: [microsoft/DirectX-Headers](https://github.com/microsoft/DirectX-Headers)

### D3D12 Memory Allocator

- **Version**: `v3.1.0`
- **Role**: GPU memory suballocation library for the Direct3D 12 rendering backend.
- We use D3D12MA to manage all D3D12 heap allocations. Do not call `ID3D12Device::CreateCommittedResource`
  directly in rendering code; route all resource creation through the allocator to ensure correct
  pooling and budget tracking.
- **License**: [MIT](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator/blob/master/LICENSE.txt)
- **Repository**: [GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)

### Metal *(integrated)*

- **Version**: Integrated, provided by the macOS/iOS SDK.
- **Role**: Metal rendering backend targeting Apple platforms.
- No vendored headers are required. The engine links against the `Metal.framework` and
  `QuartzCore.framework` supplied by Xcode.

### OpenGL *(integrated)*

- **Version**: Integrated, provided by the host platform's GPU drivers.
- **Role**: Cross-platform OpenGL rendering backend.
- OpenGL function pointers are loaded at runtime. We do not vendor a separate loader library;
  loading is handled by SDL3's built-in OpenGL function pointer resolution.

### Vulkan-Headers

- **Version**: `1.4.341`
- **Role**: Provides the Vulkan C API headers (`vulkan/vulkan.h`) and the Vulkan XML registry.
- These headers are used by the Vulkan rendering backend and by all shader toolchain libraries that
  consume Vulkan-specific SPIR-V metadata.
- **License**: [Apache-2.0](https://github.com/KhronosGroup/Vulkan-Headers/blob/main/LICENSE.md)
- **Repository**: [KhronosGroup/Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)

### Vulkan-Loader

- **Version**: `1.4.341`
- **Role**: Runtime loader that resolves Vulkan instance and device function pointers from the
  installed ICD (Installable Client Driver).
- On platforms where a system Vulkan loader is available, linking against the vendored loader is
  optional and controlled by the `GP_USE_SYSTEM_VULKAN_LOADER` CMake flag.
- **License**: [Apache-2.0](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/LICENSE.txt)
- **Repository**: [KhronosGroup/Vulkan-Loader](https://github.com/KhronosGroup/Vulkan-Loader)

### Vulkan Memory Allocator

- **Version**: `v3.3.0`
- **Role**: GPU memory suballocation library for the Vulkan rendering backend.
- We use VMA to manage all `VkDeviceMemory` allocations. Do not call `vkAllocateMemory` directly
  in rendering code; route all resource creation through the allocator to ensure correct pooling,
  defragmentation support, and budget tracking.
- **License**: [MIT](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/LICENSE.txt)
- **Repository**: [GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)

## Shader Toolchain

The engine uses a SPIR-V–centric shader pipeline. GLSL source shaders are compiled to SPIR-V at
build time, reflected for resource binding metadata, and cross-compiled to backend-native shading
languages where required.

### Glslang

- **Role**: Reference GLSL/HLSL front-end compiler that produces SPIR-V bytecode.
- All GLSL shader sources in `shaders/` are compiled via `glslangValidator` as a build step.
  Do not check in precompiled SPIR-V binaries; they are generated as part of the standard build.
- **License**: [Multiple, see repository](https://github.com/KhronosGroup/glslang/blob/main/LICENSE.txt)
- **Repository**: [KhronosGroup/glslang](https://github.com/KhronosGroup/glslang)

### DXC

- **Version**: `v1.9.2602`
- **Role**: Microsoft's DirectX Shader Compiler, used to compile HLSL shader sources to DXIL and
  SPIR-V.
- DXC is the preferred compiler for all HLSL sources targeting the D3D12 backend. It is also used
  as an alternative SPIR-V front-end for HLSL shaders on the Vulkan path. Do not use the legacy
  `fxc` compiler for any new shader work.
- **License**: [University of Illinois Open Source License](https://github.com/microsoft/DirectXShaderCompiler/blob/main/LICENSE.TXT)
- **Repository**: [microsoft/DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler)

### SPIRV-Cross

- **Role**: Transpiles SPIR-V bytecode to GLSL, HLSL, and Metal Shading Language (MSL) for
  backends that do not natively consume SPIR-V.
- SPIRV-Cross is used at runtime on Metal and legacy OpenGL paths to produce backend-appropriate
  shader source from the canonical SPIR-V representation.
- **License**: [Apache-2.0](https://github.com/KhronosGroup/SPIRV-Cross/blob/main/LICENSE)
- **Repository**: [KhronosGroup/SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)

### SPIRV-Tools

- **Role**: Provides SPIR-V validation, optimization, and binary manipulation utilities.
- The optimizer pass is run as a post-processing step on all compiled SPIR-V modules in `Release`
  builds. Always validate SPIR-V output during development using the `spirv-val` utility.
- **License**: [Apache-2.0](https://github.com/KhronosGroup/SPIRV-Tools/blob/main/LICENSE)
- **Repository**: [KhronosGroup/SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools)

### SPIRV-Reflect

- **Role**: Lightweight library for extracting shader resource binding metadata directly from
  SPIR-V bytecode.
- We use SPIRV-Reflect at runtime to automatically build pipeline layouts and descriptor set
  layouts from compiled shader modules, without requiring manually authored binding tables.
- **License**: [Apache-2.0](https://github.com/KhronosGroup/SPIRV-Reflect/blob/main/LICENSE)
- **Repository**: [KhronosGroup/SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)

### SPIRV-Headers

- **Role**: Provides the canonical SPIR-V header files (`spirv.h`, `GLSL.std.450.h`, etc.) and
  machine-readable grammar JSON files consumed by other SPIR-V toolchain libraries.
- SPIRV-Headers is a transitive dependency of Glslang, SPIRV-Tools, and SPIRV-Cross.
  Do not depend on it directly from engine code; use the higher-level toolchain libraries instead.
- **License**: [MIT](https://github.com/KhronosGroup/SPIRV-Headers/blob/main/LICENSE)
- **Repository**: [KhronosGroup/SPIRV-Headers](https://github.com/KhronosGroup/SPIRV-Headers)

## Profiling

### Tracy

- **Version**: `0.13.1`
- **Role**: Frame profiler providing CPU and GPU timing, memory allocation tracking, and flame
  graph visualization.
- Tracy instrumentation is compiled in under the `GP_ENABLE_PROFILING=ON` CMake flag and stripped
  entirely from `Shipping` builds. Prefer Tracy zones over ad-hoc timing code for any performance
  investigation. Do not leave profiling markers in code paths that are not intentionally
  instrumented.
- **License**: [BSD-3-Clause](https://github.com/wolfpld/tracy/blob/master/LICENSE)
- **Repository**: [wolfpld/tracy](https://github.com/wolfpld/tracy)
