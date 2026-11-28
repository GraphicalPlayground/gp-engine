// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

// Dear ImGui opaque types. `ImGuiKey` has a fixed underlying type in `imgui.h` (`enum ImGuiKey : int`), so it can be
// forward-declared here without pulling the full 4000-line ImGui header into every engine translation unit.
struct ImGuiContext;
struct ImDrawData;
struct ImTextureData;
enum ImGuiKey : int;

namespace gp::imgui
{

struct ImGuiRendererDesc;
struct ImGuiPlatformDesc;
struct ImGuiLayerDesc;
struct ImGuiRendererStats;

class ImGuiRHIRenderer;
class ImGuiHALPlatform;
class ImGuiLayer;

}   // namespace gp::imgui
