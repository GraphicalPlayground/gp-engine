<!-- markdownlint-disable MD041 -->
![Graphical Playground - Contribution Guidelines](https://github.com/GraphicalPlayground/.github/blob/main/assets/banners/gplayd-contributing.svg)

🌎 Read this in: [English](CONTRIBUTING.md) | [Español](translations/es/CONTRIBUTING.md) | [Français](translations/fr/CONTRIBUTING.md) | [简体中文](translations/zh-cn/CONTRIBUTING.md)

**Table of content**  
[Code of Conduct](#code-of-conduct)  
[Overview](#overview)  
[Getting Started](#getting-started)  
┕ [Prerequisites & Dependencies](#prerequisites--dependencies)  
┕ [Setting up the Development Environment](#setting-up-the-development-environment)  
[Building the Engine](#building-the-engine)  
┕ [Windows](#windows)  
┕ [Linux](#linux)  
┕ [MacOS](#macos)  
┕ [Troubleshooting](#troubleshooting)  
[How to Contribute](#how-to-contribute)  
┕ [Reporting Bugs](#reporting-bugs)  
┕ [Suggesting Enhancements](#suggesting-enhancements)  
┕ [Tackling Good First Issues](#tackling-good-first-issues)  
[Engine Architecture & Structure](#engine-architecture--structure)  
┕ [Core Modules](#core-modules)  
┕ [Rendering Pipeline](#rendering-pipeline)  
┕ [Directory Structure](#directory-structure)  
[Development Workflow](#development-workflow)  
┕ [Branching Strategy](#branching-strategy)  
┕ [Commit Message Guidelines](#commit-message-guidelines)  
┕ [Pull Request Process](#pull-request-process)  
┕ [Code Review](#code-review)  
[Coding Standards](#coding-standards)  
┕ [Style Guide](#style-guide)  
┕ [Naming Conventions](#naming-conventions)  
┕ [Performance & Memory Guidelines](#performance--memory-guidelines)  
[Testing](#testing)  
┕ [Writing Unit Tests](#writing-unit-tests)  
┕ [Running the Test Suite](#running-the-test-suite)  
[Documentation](#documentation)  
┕ [Inline Code Documentation](#inline-code-documentation)  
┕ [Writing Tutorials & Examples](#writing-tutorials--examples)  
[Community & Getting Help](#community--getting-help)  

## Code of Conduct

We are committed to fostering a welcoming, respectful, and harassment-free environment for everyone,
regardless of experience level, background, or identity.

To ensure that the Graphical Playground community remains a safe space to learn, collaborate, and
build, we expect all maintainers, contributors, and community members to adhere to our community
standards.

Before participating in the `gp-engine` project, whether by opening an issue, submitting a pull
request, or joining our community channels, please read our full
[Code of Conduct](./CODE_OF_CONDUCT.md). By participating in this project, you agree to abide by its
terms.

## Overview

Welcome to the `gp-engine` (Graphical Playground) contributor's guide!

First and foremost, thank you for your interest in contributing. Open-source projects thrive on the
diverse skills, perspectives, and time of their communities. Whether you are a rendering expert, a
technical writer, or a student looking for your first open-source contribution, we are thrilled to
have you here.

## Getting Started

### Prerequisites & Dependencies

`gp-engine` is built strictly using C++23 and utilizes a custom CMake-based toolchain, the
Graphical Playground Build Tool (GPBT). To ensure a consistent and highly optimized build
environment, we heavily enforce the use of LLVM/Clang.

Before you begin, ensure your system has the following core tools installed:

- [**CMake**](https://cmake.org): v3.28 or newer.
- **Compiler Toolchain ([LLVM/Clang v22+](https://llvm.org) Recommended)**:
  - **Linux/macOS**: You are strictly obligated to use `clang` and `clang++`. You must also link
    against LLVM's `libc++` and `libc++abi`. `lld` is the default linker for Linux.
  - **Windows**: You must use `clang-cl` via the MSVC x64 Native Tools Command Prompt.
- **Build Generator**: [`ninja`](https://ninja-build.org) is strictly required.
- **Caching (Optional but recommended)**: [`ccache`](https://ccache.dev) (Automatically utilized by
  our CMake presets).
- **Code Quality Tools**: [`clang-format`](https://clang.llvm.org/docs/ClangFormat.html),
  [`clang-tidy`](https://clang.llvm.org/extra/clang-tidy/), and [`clangd`](https://clangd.llvm.org)
  (v22+).
- **Documentation (Optional)**: [`doxygen`](https://www.doxygen.nl) and [`graphviz`](https://graphviz.org).

> **Note on Engine Dependencies**: Third-party libraries (e.g., SDL3, Vulkan headers, TBB, Tracy) are
**automatically fetched and built** (or downloaded as binaries) via the GPBT. You only need to install
the system-level libraries required to compile them.

### Setting up the Development Environment

#### 1. OS-Level Dependencies (Linux)

If you are on Linux, we provide helper scripts to install all necessary packages, headers, and build
tools for Wayland, Pipewire, X11, Vulkan, Tracy, and LLVM.

Run the script matching your distribution from the project root:

```bash
# For Ubuntu/Debian (22.04+)
./toolchain/dependencies/ubuntu-debian.sh

# For Fedora (34+)
./toolchain/dependencies/fedora.sh
```

#### 2. VS Code Integration

We highly recommend using Visual Studio Code. The repository includes a `.vscode` directory
populated with recommended extensions and code snippets.

- **Crucial**: We strongly advise using the clangd extension for code completion and navigation
  instead of the default Microsoft C/C++ IntelliSense engine.

#### 3. Devcontainer (Alternative)

If you prefer an isolated, pre-configured environment, the repository includes an **Ubuntu 24-based
[Devcontainer](https://containers.dev)**. Opening the project in VS Code with the DevContainers
extension will automatically build a ready-to-use C++23/Clang 22 environment.

## Building the Engine
<!-- markdownlint-disable MD001 MD024 MD033 -->

The build process is orchestrated by our custom `gp-build-tool`, which is integrated as a Git
submodule ([GraphicalPlayground/gp-build-tool](https://github.com/GraphicalPlayground/gp-build-tool)).

<details>
<summary id="windows"><h3>Windows</h3></summary>

#### 1. Cloning the Repository

When you clone the repository, you should initialize submodules:

```bash
git clone --recursive https://github.com/GraphicalPlayground/gp-engine.git
cd gp-engine
```

> **Submodule Management**: Our `CMakeLists.txt` is designed to automatically ensure the
`gp-build-tool` submodule is initialized and up to date upon configuration. If you are developing
the build tool locally or working offline, you can bypass this forced update by passing
`-DGP_SKIP_SUBMODULE_UPDATE=ON` to CMake.

#### 2. Configuring the Project via Presets

We utilize CMake Presets to standardize the build process across operating systems. Configuration
flags, compile commands, and build directories (`build/`) are automatically handled.

To see available presets for Windows, run:

```bash
cmake --list-presets
```

_Example output:_

```text
Available configure presets:

  "windows-release"
  "windows-debug"
  "windows-profile"
  "windows-development"
  "windows-asan"
```

For detailed information on what each configuration entails, please refer to the
[Build Type documentation][gpbt-build-type].

#### 3. Compiling

1. Open the x64 Native Tools Command Prompt for VS (required to ensure the correct architecture and
   tools are in your PATH).
2. Run the CMake commands using the Windows presets:

```bash
# Configure the project
cmake --preset windows-development

# Build the project
cmake --build --preset windows-development
```

#### 4. Finding the Binaries

While CMake generates its intermediate files in the `build/` directory, the final compiled
executables and shared libraries are automatically routed to the `binaries/` directory at the
project root:

- Executables: `binaries/bin/`
- Libraries: `binaries/lib/`

For advanced compiler flags, custom GPBT arguments, and deeper configuration options, please consult
the [GP Build Tool Configuration Guide][gpbt-config-guide].

</details>

<details>
<summary id="linux"><h3>Linux</h3></summary>

#### 1. Cloning the Repository

When you clone the repository, you should initialize submodules:

```bash
git clone --recursive https://github.com/GraphicalPlayground/gp-engine.git
cd gp-engine
```

> **Submodule Management**: Our `CMakeLists.txt` is designed to automatically ensure the
`gp-build-tool` submodule is initialized and up to date upon configuration. If you are developing
the build tool locally or working offline, you can bypass this forced update by passing
`-DGP_SKIP_SUBMODULE_UPDATE=ON` to CMake.

#### 2. Configuring the Project via Presets

We utilize CMake Presets to standardize the build process across operating systems. Configuration
flags, compile commands, and build directories (`build/`) are automatically handled.

To see available presets for Linux, run:

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
  "linux-asan"
  "linux-ubsan"
  "linux-tsan"
  "linux-msan"
```

For detailed information on what each configuration entails, please refer to the
[Build Type documentation][gpbt-build-type].

#### 3. Compiling

Ensure you are in the project root. Configure and build using your desired preset (e.g.,
`linux-development`):

```bash
# Configure the project
cmake --preset linux-development

# Build the project utilizing all available CPU cores
cmake --build --preset linux-development -j$(nproc)
```

#### 4. Finding the Binaries

While CMake generates its intermediate files in the `build/` directory, the final compiled
executables and shared libraries are automatically routed to the `binaries/` directory at the
project root:

- Executables: `binaries/bin/`
- Libraries: `binaries/lib/`

For advanced compiler flags, custom GPBT arguments, and deeper configuration options, please consult
the [GP Build Tool Configuration Guide][gpbt-config-guide].

</details>

<details>
<summary id="macos"><h3>MacOS</h3></summary>

#### 1. Cloning the Repository

When you clone the repository, you should initialize submodules:

```bash
git clone --recursive https://github.com/GraphicalPlayground/gp-engine.git
cd gp-engine
```

> **Submodule Management**: Our `CMakeLists.txt` is designed to automatically ensure the
`gp-build-tool` submodule is initialized and up to date upon configuration. If you are developing
the build tool locally or working offline, you can bypass this forced update by passing
`-DGP_SKIP_SUBMODULE_UPDATE=ON` to CMake.

#### 2. Configuring the Project via Presets

We utilize CMake Presets to standardize the build process across operating systems. Configuration
flags, compile commands, and build directories (`build/`) are automatically handled.

To see available presets for MacOS, run:

```bash
cmake --list-presets
```

_Example output:_

```text
Available configure presets:

  "macos-release"
  "macos-debug"
  "macos-profile"
  "macos-development"
  "macos-asan"
  "macos-ubsan"
  "macos-tsan"
```

For detailed information on what each configuration entails, please refer to the
[Build Type documentation][gpbt-build-type].

#### 3. Compiling

Ensure you are in the project root. Configure and build using your desired preset (e.g.,
`macos-development`):

```bash
# Configure the project
cmake --preset macos-development

# Build the project utilizing all available CPU cores
cmake --build --preset macos-development -j$(nproc)
```

#### 4. Finding the Binaries

While CMake generates its intermediate files in the `build/` directory, the final compiled
executables and shared libraries are automatically routed to the `binaries/` directory at the
project root:

- Executables: `binaries/bin/`
- Libraries: `binaries/lib/`

For advanced compiler flags, custom GPBT arguments, and deeper configuration options, please consult
the [GP Build Tool Configuration Guide][gpbt-config-guide].

</details>

[gpbt-build-type]: https://docs.graphical-playground.com/docs/gp-engine/Programming%20With%20C++/GP%20Build%20Tool/Build%20Type
[gpbt-config-guide]: https://docs.graphical-playground.com/docs/gp-engine/Programming%20With%20C++/GP%20Build%20Tool/Configuration

<details>
<summary id="troubleshooting"><h3>Troubleshooting</h3></summary>

Even with automated scripts and presets, environment quirks can happen. Here are the most common
issues contributors encounter and how to fix them.

<details>
<summary><h4>1. CMake fails with: "gp-build-tool is completely missing"</h4></summary>

- **The Issue**: CMake cannot find the build tool required to configure the project because the Git
  submodule was not initialized.
- **The Fix**: Run the following command in the project root to fetch the build tool:

```bash
git submodule update --init --recursive
```

</details>

<details>
<summary><h4>2. VS Code is flooded with red "IntelliSense" errors</h4></summary>

- **The Issue**: The default Microsoft C/C++ extension is trying (and failing) to parse C++23
  features, conflicting with our required `clangd` setup.
- **The Fix**:
  1. Ensure the `clangd` extension is installed.
  2. Disable the default IntelliSense engine. Open your VS Code settings (JSON) and add: `"C_Cpp.intelliSenseEngine": "disabled"`.
  3. Open the command palette (`Ctrl+Shift+P` / `Cmd+Shift+P`) and run `"clangd: Restart language server"`.

</details>

<details>
<summary><h4>3. Build fails with <kbd>&lt;format&gt;</kbd> or standard library errors (Linux)</h4></summary>

- **The Issue**: The build system is accidentally picking up `GCC` or `libstdc++` instead of our
  mandated LLVM toolchain. gp-engine strictly requires `libc++` and `libc++abi`.
- **The Fix**: Ensure you are using the official CMake presets (e.g., `--preset linux-development`).
  If you are running CMake manually, ensure your environment variables aren't forcing `GCC`. You can
  verify your compiler version by running `clang++ --version` (must be v22+).

</details>

<details>
<summary><h4>4. CMake cannot find the compiler (Windows)</h4></summary>

- **The Issue**: You are running the build commands in a standard PowerShell or Command Prompt
  window, which doesn't know where `clang-cl` is located.
- **The Fix**: You must run all CMake and Ninja commands from the x64 Native Tools Command Prompt
  for VS. You can search for this in your Windows Start menu.

</details>

<details>
<summary><h4>5. Missing header errors for Vulkan, X11, or Wayland (Linux)</h4></summary>

- **The Issue**: The GPBT automatically fetches the libraries (like SDL3), but your operating system
  is missing the required underlying display server or graphics API development headers.
- **The Fix**: Re-run the dependency script for your distribution located in `toolchain/dependencies/`.
  If the script completes but you still see errors, check that your package manager successfully
  refreshed its cache (`sudo apt-get update` or `sudo dnf makecache`).

</details>

**Still stuck?** If you've run into an issue not listed here, please drop into our
[community channels](#community--getting-help) or open an issue on GitHub with the `build-system`
label. Include your OS, Clang version, and the full CMake output log.

</details>

<!-- markdownlint-enable MD001 MD024 MD033 -->

## How to Contribute

_wip..._

### Reporting Bugs

_wip..._

### Suggesting Enhancements

_wip..._

### Tackling Good First Issues

_wip..._

## Engine Architecture & Structure

_wip..._

### Core Modules

_wip..._

### Rendering Pipeline

_wip..._

### Directory Structure

_wip..._

## Development Workflow

_wip..._

### Branching Strategy

_wip..._

### Commit Message Guidelines

_wip..._

### Pull Request Process

_wip..._

### Code Review

_wip..._

## Coding Standards

_wip..._

### Style Guide

_wip..._

### Naming Conventions

_wip..._

### Performance & Memory Guidelines

_wip..._

## Testing

_wip..._

### Writing Unit Tests

_wip..._

### Running the Test Suite

_wip..._

## Documentation

_wip..._

### Inline Code Documentation

_wip..._

### Writing Tutorials & Examples

_wip..._

## Community & Getting Help

Getting stuck is a normal part of working on a complex C++ engine! Whether you need help configuring
the GP Build Tool, want feedback on a new shader you're writing, or just want to show off a project
you built with `gp-engine`, we are here to support you.

**Where to Connect:**

- **Discord**: [Join our Discord Server](https://discord.graphical-playground.com) for real-time chat
  with the maintainers and other developers. This is the best place for quick questions and informal
  technical discussions.
- **GitHub Discussions**: For longer-form questions, architectural proposals, or sharing your
  showcases, head over to [GitHub Discussions](https://github.com/orgs/GraphicalPlayground/discussions).
- **Social Media**: Follow our updates and community highlights on [LinkedIn](https://www.linkedin.com/company/graphical-playground).

**Reporting Security Vulnerabilities:**

If you discover a security vulnerability within `gp-engine`, please do not report it by opening a
public GitHub issue. We take security seriously. Instead, refer to our [Security Policy](./SECURITY.md)
and email us directly at <security@graphical-playground.com> so we can address it safely.

**General Inquiries:**

For non-technical support, business inquiries, or general questions, you can reach the core team via
email:

- <support@graphical-playground.com>
- <contact@graphical-playground.com>

**Supporting the Project:**

Maintaining a modern C++ graphics engine and creating high-quality educational resources takes a
massive amount of time and effort. If you find `gp-engine` valuable for your learning, research, or
development journey, please consider supporting the project!

You can sponsor the Graphical Playground project through the following links:

- [**Buy Me A Coffee**](https://www.buymeacoffee.com/GraphicalPlayground)
- [**GitHub Sponsors**](https://github.com/sponsors/GraphicalPlayground)
- [**Direct Donation**](https://graphical-playground.com/donate)

---

_Thank you for being a part of the Graphical Playground. We can't wait to see what you build!_

---
© 2026 Graphical Playground. Built for the next generation of graphics engineers.

![Graphical Playground](https://github.com/GraphicalPlayground/.github/blob/main/assets/misc/gplayd-footer.svg)
