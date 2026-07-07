![Graphical Playground - Engine](https://github.com/GraphicalPlayground/.github/blob/main/assets/banners/gp-engine.svg)

[![Explore the Platform](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-explore-platform.svg)](https://graphical-playground.com)
[![Read the Handbook](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-read-handbook.svg)](https://handbook.graphical-playground.com)
[![Documentation](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-documentation.svg)](https://docs.graphical-playground.com)
[![Learn about Licensing](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-learn-licensing.svg)](https://graphical-playground.com/licensing)

# [@GraphicalPlayground](https://github.com/GraphicalPlayground)/gp-engine
<!-- gp:protected:start -->

🌎 Read this in: [English](README.md) | [Español](translations/es/README.md) | [Français](translations/fr/README.md) | [简体中文](translations/zh-cn/README.md)

**Table of content**  
[Overview](#overview)  
┕ [Getting Started](#getting-started)  
┕ [Prerequisites](#prerequisites)  
┕ [Building the Engine](#building-the-engine)  
┕ [Gallery](#gallery)  
[Architecture](#architecture)  
┕ [Core Systems](#core-systems)  
┕ [Progressive Abstractions](#progressive-abstractions)  
┕ [Rendering Pipelines (OpenGL / Vulkan / DirectX / ...)](#rendering-pipelines-opengl--vulkan--directx--)  
[Usage & Experimentation](#usage--experimentation)  
┕ [Sample Projects](#sample-projects)  
┕ [Shader Programming](#shader-programming)  
[Documentation](#documentation)  
┕ [API Reference](#api-reference)  
┕ [Learning Paths](#learning-paths)  
[Contributing](#contributing)  
┕ [Code of Conduct](#code-of-conduct)  
┕ [Security](#security)  
┕ [License](#license)  
┕ [Donations](#donations)  
[Contact](#contact)  

## Overview

**gp-engine** is a C++ graphics engine specifically engineered for learning, experimentation, and
teaching modern rendering architectures.

Built around a **Deconstructionist Pedagogy**, the engine bridges the gap between high-level creative
tools and low-level hardware programming. Rather than hiding complexity behind a black-box system,
`gp-engine` provides progressive abstractions over modern rendering APIs (OpenGL and Vulkan). It
empowers users to tear down, study, modify, and cleanly reimplement core graphics concepts, from
foundational rendering algorithms to advanced GPU architectures, without needing to master millions
of lines of legacy code.

### Getting Started

Welcome to the Graphical Playground ecosystem. To start experimenting with `gp-engine`, you will need
to clone the repository and set up the build environment. The engine is built using a modern C++23
toolchain and relies on a modular, production-grade architecture.

First, clone the repository along with its submodules:

```bash
git clone --recursive https://github.com/GraphicalPlayground/gp-engine.git
cd gp-engine
```

### Prerequisites

Because `gp-engine` takes advantage of modern graphics APIs and the latest C++ features, ensure
your development environment meets the following requirements:

- **Compiler**: A C++23 compatible compiler (GCC 13+, Clang 16+, or MSVC 19.38+)
- **Build System**: CMake (Version 3.20 or higher)
- **Graphics Drivers**: Up-to-date GPU drivers with support for:
  - Vulkan `tba`
  - OpenGL `tba`
  - DirectX `tba` (Windows only)
  - Metal `tba` (macOS only)

### Building the Engine

The engine uses CMake Presets to simplify configuration and building across different platforms and
environments. We recommend performing an out-of-source build, which the presets handle automatically
to keep your project directory clean.

> **Important Prerequisites**: Our Linux presets specifically require the Clang compiler
(`clang` and `clang++`) as well as `ccache` (compiler cache) to be installed on your system prior to
building. For Windows, `clang-cl` is required, and for macOS, the default `clang` compiler is sufficient.

**1. List available presets:**

First, check the available configure presets for your specific platform (Linux, Windows, or macOS):

```bash
cmake --list-presets
```

_Example output:_

```text
Available configure presets:

  "linux-release"
  "linux-debug"
  "linux-profile"
  "linux-development"
```

> (Note: You will see `windows-...` or `macos-...` prefixes depending on your operating system).

For detailed information on what each configuration entails, please refer to the
[Build Type documentation](https://docs.graphical-playground.com/docs/gp-engine/Programming%20With%20C++/GP%20Build%20Tool/Build%20Type).

**2. Configure the project:**

Select the appropriate preset for your environment and generate the build files. For example, to
configure a release build on Linux:

```bash
cmake --preset linux-development
```

> Note: You can still append flags like `-DGP_USE_VULKAN=ON` or `-DGP_USE_OPENGL=ON` to your preset
  command depending on the rendering backend you wish to target. Read the [documentation](#documentation)
  for more details on configuring rendering backends.

**3. Compile the project:**

Build the engine using the corresponding build preset. You can also append the `-j` flag to utilize
multiple CPU cores and speed up the process:

```bash
cmake --build --preset linux-development -j$(nproc)
```

Once the build successfully completes, the compiled binaries and sample experimentation projects
will be located in the `binaries/bin/` directory. You can run one of the basic sample executables
to verify that the rendering pipeline and windowing context have initialized correctly.

### Gallery

_Screenshots and clips of `gp-engine` in action are coming soon. In the meantime, check the
[Learning Paths](https://graphical-playground.com/discover) for a preview of what you'll be building._

## Architecture

`gp-engine` is organized as a set of independent, composable systems rather than a single monolithic
runtime. Each system is designed to be studied and swapped out in isolation, in keeping with the
project's Deconstructionist Pedagogy. The engine targets Windows, Linux, macOS, iOS, Android, and
WASM, and is built under strict portability constraints: no RTTI, no exceptions, and correctness
across both endianness and architecture (x86_64 and ARM64).

### Core Systems

The engine's foundation is a collection of hand-built, dependency-light core systems:

- **Math Library**: A high-performance, SIMD-optimized math library for vectors, matrices, and
  quaternions.
- **Memory Management**: A custom memory allocator and pool system designed for low-latency
  graphics workloads.
- **Foundational Containers**: Lightweight, cache-friendly data structures for graphics programming.
- **Cryptography & Hashing**: A set of cryptographic primitives and hashing functions for secure
  resource management.
- **Error & Diagnostics**: A robust error handling and logging system for debugging and profiling.
- **Concepts & Type Traits**: A collection of C++ concepts and type traits to enforce compile-time
  correctness and improve code clarity.

### Progressive Abstractions

Rather than exposing a single fixed API, `gp-engine` layers its abstractions so learners can enter at
the level that matches their experience and descend further as they're ready:

1. **Foundations**: core algorithms and data structures with no rendering API in sight.
2. **Abstracted Rendering**: a simplified, engine-level API for getting pixels on screen quickly.
3. **Native APIs**: direct access to OpenGL and Vulkan calls for learners who want to work as close
to the hardware as possible.

Each layer is documented so you can trace exactly how a call at the top eventually reaches the GPU.

### Rendering Pipelines (OpenGL / Vulkan / DirectX / ...)

`gp-engine` currently ships with DirectX and Vulkan rendering backends, selectable at configure time
via CMake flags (see [Building the Engine](#building-the-engine)). OpenGL (Linux/macOS) and Metal
(macOS) backends are planned to round out native support on every target platform. Each backend
implements the same engine-facing abstraction, so sample projects and learning material remain
portable across APIs.

## Usage & Experimentation

Once built, `gp-engine` is meant to be explored hands-on: run the sample projects, read through the
corresponding source, then modify or reimplement pieces yourself.

### Sample Projects

The `samples/` directory is organized to mirror the Learning Paths, starting with minimal
"hello triangle"-style projects and building up toward full rendering pipelines. Check the directory
itself for the current list of runnable samples, and see the
[Learning Paths](https://graphical-playground.com/discover) for the recommended order to work
through them.

## Documentation

Comprehensive documentation for `gp-engine` is hosted on our main documentation portal. Whether you
are building your first triangle or writing a custom Vulkan rendering pass, our guides are designed
to support your experimentation.

- [**Main Documentation Portal**](https://docs.graphical-playground.com)
- [**Engine Introduction**](https://docs.graphical-playground.com/docs/engine/intro)

### API Reference

Our detailed C++ API documentation outlines the core classes, rendering pipelines, and math
libraries that make up `gp-engine`. If you are extending the engine or modifying its core
components, please review our C++ guides and formatting rules to ensure your code aligns
with the project's architecture.

- [**C++ API Reference**](https://docs.graphical-playground.com/docs/engine/cpp-api-reference)
- [**C++ Programming Guide**](https://docs.graphical-playground.com/docs/engine/programming-with-cpp)
- [**C++ Coding Standard**](https://docs.graphical-playground.com/docs/engine/programming-with-cpp/coding-standard)

### Learning Paths

Graphical Playground is built around a Deconstructionist Pedagogy, we want you to tear the engine
apart and learn how it works. To help guide your studies, we have curated structured learning
paths that take you from foundational graphics programming concepts to advanced engine architecture.

- [**Explore the Learning Paths**](https://graphical-playground.com/discover)

<!-- gp:protected:end -->
## Contributing

We welcome contributions from everybody! Whether you are fixing a bug, implementing a new features,
or improving our documentation, your help is appreciated. Please see our full
[CONTRIBUTING.md](./CONTRIBUTING.md) guide for detailed information on our standards and the pull
request review process.

### Code of Conduct

To ensure a welcoming, collaborative, and inclusive environment for everyone learning and
building within the Graphical Playground ecosystem, all contributors and participants are
expected to adhere to our [Code of Conduct](./CODE_OF_CONDUCT.md). Please review it before engaging
in community discussions or submitting code.

### Security

If you discover a security vulnerability within `gp-engine`, please do not report
it by opening a public issue. Instead, refer to our [Security Policy](./SECURITY.md) for
instructions on how to securely disclose the vulnerability to the maintainers.

### License

`gp-engine` is open-source software. Please see the [LICENSE.md](./LICENSE.md) file in the root
directory for full terms regarding modification, distribution, and use in your own projects.

### Donations

If you find `gp-engine` helpful for your learning, academic research, or game development journey,
please consider supporting the project. Maintaining a modern C++ graphics engine takes
significant time and resources!

You can sponsor the Graphical Playground project through the following links:

- [**Buy Me A Coffee**](https://www.buymeacoffee.com/GraphicalPlayground)
- [**GitHub Sponsors**](https://github.com/sponsors/GraphicalPlayground)
- [**Direct Donation**](https://graphical-playground.com/donate)

You can see the full list of sponsors and supporters on our
[Sponsors Page](https://graphical-playground.com/sponsors) or in [DONORS.md](./DONORS.md).
Your support helps us continue to develop high-quality educational resources and maintain the engine
for the next generation of graphics engineers.

## Contact

If you have any questions, suggestions, or want to share your projects built with `gp-engine`, we
would love to hear from you! You can reach out to us through the following channels:

- **GitHub Discussions**: [Join the Conversation](https://github.com/orgs/GraphicalPlayground/discussions)
- **Email**:
  - [support@graphical-playground.com](mailto:support@graphical-playground.com)
  - [security@graphical-playground.com](mailto:security@graphical-playground.com)
  - [contact@graphical-playground.com](mailto:contact@graphical-playground.com)
- **Social Media**:
  - [LinkedIn](https://www.linkedin.com/company/graphical-playground)
  - [Discord](https://discord.graphical-playground.com)

---
© 2026 Graphical Playground. Built for the next generation of graphics engineers.

![Graphical Playground](https://github.com/GraphicalPlayground/.github/blob/main/assets/misc/gplayd-footer.svg)
