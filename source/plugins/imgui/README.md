# ImGui Plugin

**Table of content**  
[Overview](#overview)  
┕ [Responsibilities](#responsibilities)  
┕ [Integration](#integration)  
┕ [Dependencies](#dependencies)  
┕ [CMake Targets](#cmake-targets)  
[Regenerating Shaders](#regenerating-shaders)  
[License](#license)  

## Overview

Opt-in [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) integration for any gp-engine
target. It drives ImGui through the engine's own HAL and DynamicRHI, so it does not use any of the
official ImGui platform or renderer backends.

### Responsibilities

| Component            | Role                                                                                         |
|----------------------|----------------------------------------------------------------------------------------------|
| `ImGuiLayer`         | Single object that owns the ImGui context and both backends. Most applications only need this. |
| `ImGuiHALPlatform`   | Turns HAL keyboard, mouse, text and window events into ImGui IO. Also drives the cursor shape and starts/stops text input (IME). |
| `ImGuiRHIRenderer`   | Renders `ImDrawData` through `CommandListImmediate`. It streams ImGui 1.92+ textures, packs all draw lists into one vertex buffer and one index buffer, and caches a PSO for each render-pass attachment signature. |

Rendering happens in two phases, because RHI uploads are not allowed inside a render pass:

1. `prepareRender(cmdList)` runs **outside** a pass. It handles texture create/update/destroy
   requests and copies the geometry.
2. `renderInPass(cmdList)` runs **inside** the caller's pass. It sets scissors and issues draw calls.

`render(cmdList, target)` combines both phases and draws into its own `LoadStore` pass.

### Integration

```cmake
gpAddDependency(PRIVATE plugin/imgui)
```

```cpp
#include "imgui.h"
#include "ImGuiLayer.hpp"

gp::imgui::ImGuiLayer m_imgui;

// onInitialize():   m_imgui.initialize(getMainWindow());
// onMouseEvent():   m_imgui.processMouseEvent(ev);   // same for Keyboard / TextInput / Window events
// onUpdate():       m_imgui.beginFrame(dt); ImGui::ShowDemoWindow(); m_imgui.endFrame();
// onRender():       m_imgui.prepareRender(cmdList); /* beginRenderPass */ m_imgui.renderInPass(cmdList);
// onShutdown():     m_imgui.shutdown();               // before the RHI goes away
```

Use `wantsMouseCapture()` / `wantsKeyboardCapture()` to stop engine shortcuts from firing while
ImGui has focus. `source/launch/editor/private/Main.cpp` shows a complete example.

### Dependencies

| Dependency              | Type   | Purpose                                                            |
|-------------------------|--------|--------------------------------------------------------------------|
| `gp::core`              | Public | Core types, containers and logging.                                |
| `gp::hal::base`         | Public | `IWindow` and HAL event structures.                                |
| `gp::rhi::base`         | Public | DynamicRHI, command lists and resources.                           |
| `gp::thirdparty::imgui` | Public | Dear ImGui 1.93.1. Built as a shared library in modular builds so the whole process sees one `GImGui`. |

The renderer currently needs the **Vulkan** RHI, because its embedded shaders are `GPVS` SPIR-V
packages. On any other RHI, `initialize()` fails cleanly and the application keeps running without UI.

### CMake Targets

| Target Name              | Type       | Description                          |
|--------------------------|------------|--------------------------------------|
| `gp-plugin-imgui`        | Library    | The ImGui plugin library target.     |
| `gp-plugin-imgui-tests`  | Executable | GoogleTest suite (headless, no RHI). |

## Regenerating Shaders

The shader sources are `shaders/ImGui.vert` and `shaders/ImGui.frag`. After editing either one, run:

```text
python source/plugins/imgui/tools/compile_imgui_shaders.py
```

This rewrites `private/renderer/ImGuiShaderBytecode.inl`. The script needs the Vulkan SDK tools
`glslc` and `spirv-val`. It finds them on `PATH`, in `%VULKAN_SDK%/Bin`, or under `C:/VulkanSDK/<ver>/Bin`.

## License

Dear ImGui is MIT-licensed (see `thirdparty/imgui`). The plugin code follows the gp-engine license.
