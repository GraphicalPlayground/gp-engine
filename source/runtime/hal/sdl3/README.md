# HAL SDL3 Module

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
| `gp::hal::base`        | Public         | Provides base HAL interfaces and abstractions that the SDL3 implementation builds upon. |

### CMake Targets

| Target Name           | Type           | Description                                             |
|-----------------------|----------------|---------------------------------------------------------|
| `gp-hal-sdl3`         | Library        | The main HAL SDL3 module library target.                |
| `gp::hal::sdl3`       | Alias          | CMake alias for `gp-hal-sdl3` target.                   |

## Documentation

- [HAL SDL3 Module Reference](https://docs.graphical-playground.com/docs/gp-engine/source/runtime/hal/sdl3)

## License

`gp-engine` is open-source software. Please see the [LICENSE.md](/LICENSE.md) file in the root
directory for full terms regarding modification, distribution, and use in your own projects.
