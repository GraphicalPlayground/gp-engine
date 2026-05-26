![Graphical Playground - Engine](.github/assets/repository-title.svg)

[![Explore the Platform](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-explore-platform.svg)](https://graphical-playground.com)
[![Read the Handbook](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-read-handbook.svg)](https://handbook.graphical-playground.com)
[![Documentation](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-documentation.svg)](https://docs.graphical-playground.com)
[![Learn about Licensing](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-learn-licensing.svg)](https://graphical-playground.com/licensing)

# [@GraphicalPlayground](https://github.com/GraphicalPlayground)/gp-engine
<!-- gp:protected:start -->

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

The engine uses standard CMake build procedures. We recommend performing an out-of-source build
to keep your project directory clean.

**1. Generate build files:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

> (Note: You can pass `-DGP_USE_VULKAN=ON` or `-DGP_USE_OPENGL=ON` depending on the rendering
backend you wish to target.). Read the [documentation](#documentation) for more details on
configuring rendering backends.

**2. Compile the project:**

```bash
cmake --build build --config Release -j$(nproc)
```

Once the build successfully completes, the compiled binaries and sample experimentation projects
will be located in the `binaries/bin/` directory. You can run one of the basic sample
executables to verify that the rendering pipeline and windowing context have initialized correctly.

### Gallery

_wip..._

## Architecture

_wip..._

### Core Systems

_wip..._

### Progressive Abstractions

_wip..._

### Rendering Pipelines (OpenGL / Vulkan / DirectX / ...)

_wip..._

## Usage & Experimentation

_wip..._

### Sample Projects

_wip..._

### Shader Programming

_wip..._

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
