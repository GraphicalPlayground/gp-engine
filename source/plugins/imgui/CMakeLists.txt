# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool)

# Opt-in Dear ImGui integration. Any target that adds `gpAddDependency(PRIVATE plugin/imgui)` gets:
#   - the full Dear ImGui API (`#include "imgui.h"`), re-exported through the PUBLIC thirdparty dependency;
#   - `gp::imgui::ImGuiLayer`, the one-object integration (context + HAL platform + DynamicRHI renderer);
#   - the lower-level `ImGuiHALPlatform` / `ImGuiRHIRenderer` backends for custom render pipelines.
gpStartModule(plugin/imgui)
  gpEnableTests()

  gpAddDependency(PUBLIC core)
  gpAddDependency(PUBLIC hal/base)
  gpAddDependency(PUBLIC rhi/base)
  # Wrapped in BUILD_INTERFACE like core/tracy: the generated ImGui target is not part of the GPTargets export set.
  gpAddDependency(PUBLIC "$<BUILD_INTERFACE:gp::thirdparty::imgui>")
gpEndModule()
