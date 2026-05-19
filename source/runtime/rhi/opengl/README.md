# RHI OpenGL Module

**Table of content**  
[Overview](#overview)  
┕ [Responsibilities](#responsibilities)  
┕ [Dependencies](#dependencies)  
┕ [CMake Targets](#cmake-targets)  
[Documentation](#documentation)  
[License](#license)  

## Overview

_wip_

### Responsibilities

_wip_

### Dependencies

| Dependency             | Type           | Purpose                                                |
|------------------------|----------------|--------------------------------------------------------|
| `gp::core`             | Public         | Provides core utilities and foundational types used across the Engine module. |
| `gp::rhi::base`        | Public         | Provides base RHI interfaces and abstractions that the OpenGL implementation builds upon. |

### CMake Targets

| Target Name           | Type           | Description                                             |
|-----------------------|----------------|---------------------------------------------------------|
| `gp-rhi-opengl`        | Library        | The main RHI OpenGL module library target.               |
| `gp::rhi::opengl`      | Alias          | CMake alias for `gp-rhi-opengl` target.                  |

## Documentation

- [RHI OpenGL Module Reference](https://docs.graphical-playground.com/docs/gp-engine/source/runtime/rhi/opengl)

## License

`gp-engine` is open-source software. Please see the [LICENSE.md](/LICENSE.md) file in the root
directory for full terms regarding modification, distribution, and use in your own projects.
