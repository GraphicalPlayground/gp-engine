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
submodule ([GraphicalPlayground/gp-build-tool][gpbt]).

<details>
<summary id="windows"><h3><img src="https://thesvg.org/icons/windows/default.svg" height="16" alt="Windows"/> Windows</h3></summary>

#### 0. Prerequisites

Before you can build the engine on Windows, ensure you have the following installed:

- [Visual Studio 2022](https://visualstudio.microsoft.com) (v17.6+) with the following workloads:
  - **Desktop development with C++**
  - **Game development with C++**
- [Chocolatey](https://chocolatey.org) (Optional but recommended for package management)
- [LLVM](https://llvm.org) (v22+) installed via Chocolatey or manually.
  - Ensure `clang-cl`, `clangd`, `clang-format` are in your PATH.
- [Ninja](https://ninja-build.org) (v1.11+) installed via Chocolatey or manually.
- [CMake](https://cmake.org) (v3.28+) installed via Chocolatey or manually.
- [Git](https://git-scm.com) (v2.40+) installed via Chocolatey or manually.

<details>
<summary id="chocolatey"><h5>Installing with Chocolatey</h5></summary>

1. Install Chocolatey by following the instructions on [Chocolatey's website](https://chocolatey.org/install).
2. Open an **elevated PowerShell** (Run as Administrator) and execute:

```powershell
choco install llvm cmake ninja -y
```

</details>

> [!TIP]
> You may need to enable Win32 Long Paths in Windows 10/11 to avoid issues with long file paths
> during the build process. See [Microsoft's documentation](https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)
> for instructions.

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
<summary id="linux"><h3><img src="https://thesvg.org/icons/linux/default.svg" height="16" alt="Linux"/> Linux</h3></summary>

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
<summary id="macos"><h3><img src="https://thesvg.org/icons/apple/default.svg" height="16" alt="Apple"/> MacOS</h3></summary>

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

As a pedagogical engine meant to help you learn AAA game engine design, the `gp-engine` repository
is organized to clearly reflect a modern, scalable engine architecture. Our directory layout
separates core engine code, platform-specific layers, tools, and build configurations to keep the
learning curve manageable while exposing you to industry-standard project organization.

Below is an overview of the main directories and their roles:

**Principal Directories:**

- `/source/`: The heart of the engine. This is where all C++ source code, headers, and core
  logic live.
  - `/runtime/`: Contains the core engine systems used at runtime (e.g., `/core/`, `/rhi/`,
    `/renderer/`, `/physics/`, `/audio/`). Exploring this directory gives you a deep dive into how
    an engine ticks frame-by-frame.
  - `/launch/`: Contains the entry points for the engine applications. It is split into targets
    like `/editor/` (the authoring tool) and `/standalone/` (the packaged game executable). This
    demonstrates how engines separate their development tools from the final shipped product.
  - `/shaders/`: Houses all HLSL/GLSL shader code. We separate `/public/`
    (shared interfaces/includes) from `/private/` (actual shader implementations) to teach proper
    shader resource management and encapsulation.
  - `/plugins/`: An ecosystem for extensible engine modules. We use this to demonstrate how to build
    a modular architecture where features can be loaded or unloaded without modifying the core
    `/runtime/`.
- `/examples/`: Practical, stripped-down examples and sample projects. These are designed to isolate
  specific engine features (like a rendering pass or an input system) so you can study them without
  being overwhelmed by the entire engine context.
- `/cmake/`: Contains the [`gp-build-tool`][gpbt] git submodule, orchestrating our robust build
  process. This exposes you to advanced, modular CMake practices used in large-scale C++ projects.
- `/toolchain/`: Scripts and configuration files for setting up the development environment
  across platforms (Windows, Linux, MacOS). This includes CMake presets to guarantee a unified
  build experience.
- `/thirdparty/`: Contains CMake scripts and licenses for external dependencies (e.g., SDL3,
  Vulkan headers). The actual source code is fetched automatically by the GPBT, teaching you modern
  dependency management that avoids repository bloat.
- `/docs/`: High-level guides, architectural overviews, and tutorials. Scattered `docs/` folders
  throughout the repository dive into specific modules. All these are aggregated into our
  [documentation website](https://docs.graphical-playground.com/docs/gp-engine/Introduction).
- `/.devcontainer/`: Configuration for a Docker-based Devcontainer. It provides a standardized,
  pre-configured development environment, ensuring every contributor has the same toolchain
  instantly.
- `/.github/`: GitHub Actions workflows for our CI/CD pipeline, PR templates, and issue tracking.
- `/translations/`: Multilingual support for our documentation, making the pedagogical resources
  accessible to a broader global audience.
- `/.vscode/`: Recommended workspace settings, tasks, and extensions for Visual Studio Code to
  enforce coding styles and streamline debugging.

> [!TIP]
> You can read the article ["Engine Architecture and Directory Layout: A Principal Engineer's Guide"](https://docs.graphical-playground.com/blog/engine-architecture-layout)
> for a deeper dive into the reasoning behind our directory structure and how it reflects modern AAA
> engine design principles.

## Development Workflow

_wip..._

### Branching Strategy

Our repository follows a structured branching model to ensure stability and smooth collaboration.

**Main Branches:**

- `main`: This is the current stable development branch.
- `dev`: This is the current experimental branch where active integration happens.

**Release Branches:**

- `release-<version>`: We use specific branches for major releases (e.g., `release-1.0.0`),
  isolating them for final polishing and bug fixes.

**Working Branches:**

- **Personal Prefixes:** Everyone works on their own branches, which must be prefixed with the
  author's initials. For example:
  - `ms/...` (Mallory Scotton)
  - `hc/...` (Hugo Cathelain)
  - `nf/...` (Nathan Fievet)
- **One Feature Per Branch:** Keep your work focused. Each branch should encompass a single feature
  or fix.
- **Automated Workflows:** Before a pull request can be merged, all CI/CD workflows (formatting,
  build, tags, etc.) must pass successfully.
- **Cleanup:** Once merged, working branches are deleted automatically.

### Commit Message Guidelines

We follow a structured convention for commit messages to ensure a clear and well-documented project
history.

**General Rules:**

- **Prefer using lower case** for the commit message subject.
- **Use a prefix** to indicate the type of commit (e.g., `add`, `update`, `chore`, `fix`, `hotfix`,
  `bug`, `docs`, etc.).
- **Sub-categories (Optional):** You can add a specific sub-category or scope in parentheses to
  provide more context. For example: `chore(format): ...`.
- **Be Explicit:** The commit message should explicitly state what the commit is actually
  implementing.
- **Small, Atomic Commits:** Prefer doing multiple focused commits instead of one giant one.
- **Co-authors:** If someone helps you with a commit or code, think about adding them as a
  co-author if it's relevant (e.g., `Co-authored-by: Name <email@example.com>` at the end of the
  commit message body).

**Classic Commit Guidelines:**

- Separate the subject line from the body with a blank line.
- Limit the subject line to 50-72 characters.
- Do not end the subject line with a period.
- Use the imperative mood in the subject line (e.g., `add core rendering module`, not `added` or
  `adds`).
- Wrap the body text at 72 characters.
- Use the body to explain _what_ you did and _why_, rather than _how_ you did it.

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
- [**Open Collective**](https://opencollective.com/graphical-playground)
- [**Thanks Dev**](https://thanks.dev/u/gh/GraphicalPlayground)
- [**Direct donation**](https://graphical-playground.com/donate)

---

_Thank you for being a part of the Graphical Playground. We can't wait to see what you build!_

---
© 2026 Graphical Playground. Built for the next generation of graphics engineers.

![Graphical Playground](https://github.com/GraphicalPlayground/.github/blob/main/assets/misc/gplayd-footer.svg)

[gpbt]: https://github.com/GraphicalPlayground/gp-build-tool
