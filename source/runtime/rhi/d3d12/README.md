# RHI D3D12 Module

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
| `gp::rhi::base`        | Public         | Provides base RHI interfaces and abstractions that the D3D12 implementation builds upon. |

### CMake Targets

| Target Name           | Type           | Description                                             |
|-----------------------|----------------|---------------------------------------------------------|
| `gp-rhi-d3d12`        | Library        | The main RHI D3D12 module library target.               |
| `gp::rhi::d3d12`      | Alias          | CMake alias for `gp-rhi-d3d12` target.                  |

## Documentation

- [RHI D3D12 Module Reference](https://docs.graphical-playground.com/docs/gp-engine/source/runtime/rhi/d3d12)

## License

`gp-engine` is open-source software. Please see the [LICENSE.md](/LICENSE.md) file in the root
directory for full terms regarding modification, distribution, and use in your own projects.
