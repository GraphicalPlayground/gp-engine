# Core Module

**Table of content**  
[Overview](#overview)  
┕ [Responsibilities](#responsibilities)  
┕ [Dependencies](#dependencies)  
┕ [CMake Targets](#cmake-targets)  
[Documentation](#documentation)  
[License](#license)  

## Overview

The **Core** module is the foundational layer of `gp-engine`. It provides the essential
building blocks that every other module depends on: memory management, logging, assertion
utilities, type aliases, and base container wrappers.

### Responsibilities

- Centralized memory allocation strategies (arenas, pools, stack allocators)
- Platform-abstracted type definitions and compile-time introspection utilities
- Engine-wide logging and structured diagnostics
- Core math primitives (vectors, matrices, quaternions)
- Base container implementations (dynamic arrays, hash maps, string utilities)
- Assertion and error handling macros

### Dependencies

| Dependency             | Type           | Purpose                                                |
|------------------------|----------------|--------------------------------------------------------|
| _None_                 |                |                                                        |

### CMake Targets

| Target Name           | Type           | Description                                             |
|-----------------------|----------------|---------------------------------------------------------|
| `gp-core`             | Library        | The main Core module library target.                    |
| `gp::core`            | Alias          | CMake alias for `gp-core` target.                       |

## Documentation

- [Core Module Reference](https://docs.graphical-playground.com/docs/gp-engine/source/runtime/core)

## License

`gp-engine` is open-source software. Please see the [LICENSE.md](/LICENSE.md) file in the root
directory for full terms regarding modification, distribution, and use in your own projects.
