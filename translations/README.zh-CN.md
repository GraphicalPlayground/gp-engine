![Graphical Playground - Engine](https://github.com/GraphicalPlayground/.github/blob/main/assets/banners/gp-engine.svg)

[![探索平台](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-explore-platform.svg)](https://graphical-playground.com)
[![阅读手册](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-read-handbook.svg)](https://handbook.graphical-playground.com)
[![文档](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-documentation.svg)](https://docs.graphical-playground.com)
[![了解许可证](https://github.com/GraphicalPlayground/.github/blob/main/assets/cta/cta-learn-licensing.svg)](https://graphical-playground.com/licensing)

# [@GraphicalPlayground](https://github.com/GraphicalPlayground)/gp-engine
<!-- gp:protected:start -->

🌎 阅读语言：[English](../README.md) | [Español](README.es.md) | [Français](README.fr.md) | [简体中文](README.zh-CN.md)

**目录**  
[概述](#概述)  
┕ [快速开始](#快速开始)  
┕ [前置条件](#前置条件)  
┕ [构建引擎](#构建引擎)  
┕ [图库](#图库)  
[架构](#架构)  
┕ [核心系统](#核心系统)  
┕ [渐进式抽象](#渐进式抽象)  
┕ [渲染管线（OpenGL / Vulkan / DirectX / ...）](#渲染管线opengl--vulkan--directx--)  
[使用与实验](#使用与实验)  
┕ [示例项目](#示例项目)  
┕ [Shader 编程](#shader-编程)  
[文档](#文档)  
┕ [API 参考](#api-参考)  
┕ [学习路径](#学习路径)  
[贡献](#贡献)  
┕ [行为准则](#行为准则)  
┕ [安全](#安全)  
┕ [许可证](#许可证)  
┕ [捐赠](#捐赠)  
[联系](#联系)  

## 概述

**gp-engine** 是一个专为学习、实验和教授现代渲染架构而设计的 C++ 图形引擎。

引擎以**解构主义教学法**为核心，在高层创意工具与底层硬件编程之间架起桥梁。`gp-engine` 不会将复杂性隐藏在黑盒系统背后，而是在现代渲染 API（OpenGL 和 Vulkan）之上提供渐进式抽象层，让用户能够拆解、研究、修改并干净地重新实现核心图形概念——从基础渲染算法到先进的 GPU 架构，无需掌握数百万行的遗留代码。

### 快速开始

欢迎来到 Graphical Playground 生态系统。要开始使用 `gp-engine` 进行实验，你需要克隆仓库并配置构建环境。引擎基于现代 C++23 工具链构建，采用模块化的生产级架构。

首先，克隆仓库及其子模块：

```bash
git clone --recursive https://github.com/GraphicalPlayground/gp-engine.git
cd gp-engine
```

### 前置条件

由于 `gp-engine` 充分利用了现代图形 API 和最新的 C++ 特性，请确保你的开发环境满足以下要求：

- **编译器**：兼容 C++23 的编译器（GCC 13+、Clang 16+ 或 MSVC 19.38+）
- **构建系统**：CMake（3.20 或更高版本）
- **显卡驱动**：支持以下 API 的最新 GPU 驱动：
  - Vulkan `tba`
  - OpenGL `tba`
  - DirectX `tba`（仅限 Windows）
  - Metal `tba`（仅限 macOS）

### 构建引擎

引擎使用 CMake Presets 来简化不同平台和环境下的配置与构建流程。我们建议执行源外构建（out-of-source build），预设会自动处理，以保持项目目录整洁。

**Linux 重要前置条件**：我们的 Linux 预设特别要求在构建前已在系统中安装 Clang 编译器（`clang` 和 `clang++`）以及 `ccache`（编译器缓存）。

**1. 列出可用预设：**

首先，查看适用于你所在平台（Linux、Windows 或 macOS）的可用配置预设：

```bash
cmake --list-presets
```

_示例输出：_

```text
Available configure presets:

  "linux-release"
  "linux-debug"
  "linux-profile"
  "linux-development"
```

> （注意：根据你的操作系统，你会看到 `windows-...` 或 `macos-...` 前缀）。

有关每种配置的详细说明，请参阅
[构建类型文档](https://docs.graphical-playground.com/docs/gp-engine/Programming%20With%20C++/GP%20Build%20Tool/Build%20Type)。

**2. 配置项目：**

根据你的环境选择合适的预设并生成构建文件。例如，在 Linux 上配置开发构建：

```bash
cmake --preset linux-development
```

> 注意：你仍可以根据想要使用的渲染后端，在预设命令后附加 `-DGP_USE_VULKAN=ON` 或 `-DGP_USE_OPENGL=ON` 等标志。有关配置渲染后端的详细信息，请阅读[文档](#文档)。

**3. 编译项目：**

使用对应的构建预设编译引擎。你也可以附加 `-j` 标志以利用多个 CPU 核心加速编译过程：

```bash
cmake --build --preset linux-development -j$(nproc)
```

构建成功完成后，编译好的二进制文件和示例实验项目将位于 `binaries/bin/` 目录中。你可以运行一个基本示例可执行文件，以验证渲染管线和窗口上下文是否已正确初始化。

### 图库

_建设中..._

## 架构

_建设中..._

### 核心系统

_建设中..._

### 渐进式抽象

_建设中..._

### 渲染管线（OpenGL / Vulkan / DirectX / ...）

_建设中..._

## 使用与实验

_建设中..._

### 示例项目

_建设中..._

### Shader 编程

_建设中..._

## 文档

`gp-engine` 的完整文档托管在我们的主文档门户上。无论你是在绘制第一个三角形，还是编写自定义 Vulkan 渲染通道，我们的指南都旨在支持你的实验探索。

- [**主文档门户**](https://docs.graphical-playground.com)
- [**引擎介绍**](https://docs.graphical-playground.com/docs/engine/intro)

### API 参考

我们详细的 C++ API 文档概述了构成 `gp-engine` 的核心类、渲染管线和数学库。如果你在扩展引擎或修改其核心组件，请查阅我们的 C++ 指南和格式规范，以确保你的代码符合项目架构。

- [**C++ API 参考**](https://docs.graphical-playground.com/docs/engine/cpp-api-reference)
- [**C++ 编程指南**](https://docs.graphical-playground.com/docs/engine/programming-with-cpp)
- [**C++ 编码规范**](https://docs.graphical-playground.com/docs/engine/programming-with-cpp/coding-standard)

### 学习路径

Graphical Playground 围绕解构主义教学法构建，我们希望你拆解引擎并了解其工作原理。为了指导你的学习，我们精心设计了结构化学习路径，带你从图形编程基础概念一路深入到高级引擎架构。

- [**探索学习路径**](https://graphical-playground.com/discover)

<!-- gp:protected:end -->
## 贡献

我们欢迎所有人的贡献！无论你是在修复 bug、实现新功能还是改进我们的文档，你的帮助都备受感激。请查阅我们完整的 [CONTRIBUTING.md](./CONTRIBUTING.md) 指南，了解我们的标准和 pull request 审查流程的详细信息。

### 行为准则

为了确保所有在 Graphical Playground 生态系统中学习和构建的人都能拥有一个友好、协作且包容的环境，所有贡献者和参与者都必须遵守我们的[行为准则](./CODE_OF_CONDUCT.md)。在参与社区讨论或提交代码之前，请先阅读它。

### 安全

如果你在 `gp-engine` 中发现安全漏洞，请不要通过开 public issue 的方式报告。请参阅我们的[安全政策](./SECURITY.md)，获取如何安全地向维护者披露漏洞的说明。

### 许可证

`gp-engine` 是开源软件。请查阅根目录中的 [LICENSE.md](./LICENSE.md) 文件，了解有关修改、分发和在你自己项目中使用的完整条款。

### 捐赠

如果你发现 `gp-engine` 对你的学习、学术研究或游戏开发之旅有所帮助，欢迎考虑支持本项目。维护一个现代 C++ 图形引擎需要大量的时间和资源！

你可以通过以下链接赞助 Graphical Playground 项目：

- [**Buy Me A Coffee**](https://www.buymeacoffee.com/GraphicalPlayground)
- [**GitHub Sponsors**](https://github.com/sponsors/GraphicalPlayground)
- [**直接捐赠**](https://graphical-playground.com/donate)

你可以在我们的[赞助者页面](https://graphical-playground.com/sponsors)或 [DONORS.md](./DONORS.md) 中查看完整的赞助者和支持者名单。
你的支持帮助我们继续开发高质量的教育资源，并为下一代图形工程师维护引擎。

## 联系

如果你有任何问题、建议，或想分享使用 `gp-engine` 构建的项目，我们非常乐意听到你的声音！你可以通过以下渠道联系我们：

- **GitHub Discussions**：[加入讨论](https://github.com/orgs/GraphicalPlayground/discussions)
- **电子邮件**：
  - [support@graphical-playground.com](mailto:support@graphical-playground.com)
  - [security@graphical-playground.com](mailto:security@graphical-playground.com)
  - [contact@graphical-playground.com](mailto:contact@graphical-playground.com)
- **社交媒体**：
  - [LinkedIn](https://www.linkedin.com/company/graphical-playground)
  - [Discord](https://discord.graphical-playground.com)

---
© 2026 Graphical Playground. 为下一代图形工程师而生。

![Graphical Playground](https://github.com/GraphicalPlayground/.github/blob/main/assets/misc/gplayd-footer.svg)
