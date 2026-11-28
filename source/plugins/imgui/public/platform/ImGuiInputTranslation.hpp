// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "ImGuiForward.hpp"
#include "ImGuiPlugin.hpp"
#include "input/InputEnums.hpp"
#include "window/CursorEnums.hpp"

namespace gp::imgui
{

/// @brief Sentinel returned by `toImGuiMouseButton()` for buttons ImGui does not track.
inline constexpr gp::Int32 kInvalidImGuiMouseButton = -1;

/// @brief Translates a physical USB HID scancode into the matching `ImGuiKey`.
/// @details Scancodes are used instead of layout-dependent keycodes: ImGui shortcuts (Ctrl+C, Ctrl+Z, ...) are
///          positional in most AAA editors so they work across AZERTY/QWERTZ layouts, while actual characters always
///          arrive through `TextInputEvent`.
/// @param[in] scancode Physical key identifier reported by the HAL.
/// @return Matching `ImGuiKey`, or `ImGuiKey_None` when the key has no ImGui equivalent.
[[nodiscard]] GP_PLUGIN_IMGUI_API ImGuiKey toImGuiKey(gp::hal::input::Scancode scancode) noexcept;

/// @brief Translates a HAL mouse button into ImGui's button index (0 = left, 1 = right, 2 = middle, 3/4 = extra).
/// @param[in] button HAL mouse button identifier.
/// @return ImGui mouse button index, or `kInvalidImGuiMouseButton` for unknown buttons.
[[nodiscard]] GP_PLUGIN_IMGUI_API gp::Int32 toImGuiMouseButton(gp::hal::input::MouseButton button) noexcept;

/// @brief Translates an `ImGuiMouseCursor_` value into the closest HAL cursor shape.
/// @param[in] imguiCursor Value returned by `ImGui::GetMouseCursor()` (must not be `ImGuiMouseCursor_None`).
/// @return HAL cursor shape; unknown values fall back to `CursorShape::Arrow`.
[[nodiscard]] GP_PLUGIN_IMGUI_API gp::hal::CursorShape toHALCursorShape(gp::Int32 imguiCursor) noexcept;

}   // namespace gp::imgui
