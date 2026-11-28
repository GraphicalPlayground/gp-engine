// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platform/ImGuiPlatformDesc.hpp"
#include "renderer/ImGuiRendererDesc.hpp"

namespace gp::imgui
{

/// @brief Visual theme applied by `ImGuiLayer::initialize()`.
enum class ImGuiTheme : gp::UInt8
{
    Dark = 0,
    Light,
    Classic
};

/// @brief Plain-old-data configuration for `ImGuiLayer::initialize()`.
struct ImGuiLayerDesc
{
    /// @brief Platform backend (HAL input/cursor) configuration.
    ImGuiPlatformDesc platform{};

    /// @brief Renderer backend (DynamicRHI) configuration.
    ImGuiRendererDesc renderer{};

    /// @brief Path of the ImGui `.ini` layout file, or `nullptr` to disable layout persistence.
    /// @note The pointer must outlive the layer; ImGui stores it without copying.
    const char* iniFilename{ nullptr };

    /// @brief Fixed UI scale; `0` (default) follows the window's DPI automatically (`ImGuiHALPlatform::getUIScale()`).
    gp::Float32 uiScaleOverride{ 0.0f };

    /// @brief Extra user multiplier applied on top of the DPI scale (an accessibility "UI size" setting).
    gp::Float32 uiScaleMultiplier{ 1.0f };

    /// @brief Initial color theme.
    ImGuiTheme theme{ ImGuiTheme::Dark };

    /// @brief True to enable docking (`ImGuiConfigFlags_DockingEnable`).
    bool enableDocking{ true };

    /// @brief True to enable keyboard navigation (`ImGuiConfigFlags_NavEnableKeyboard`).
    bool enableKeyboardNavigation{ true };
};

}   // namespace gp::imgui
