// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::imgui
{

/// @brief Plain-old-data configuration for `ImGuiHALPlatform::initialize()`.
struct ImGuiPlatformDesc
{
    /// @brief True to let ImGui drive the OS cursor shape through `IWindow::setCursorShape()`.
    bool enableCursorShapes{ true };

    /// @brief True to start/stop OS text input (IME) automatically whenever ImGui requests text input.
    bool enableTextInputToggling{ true };
};

}   // namespace gp::imgui
