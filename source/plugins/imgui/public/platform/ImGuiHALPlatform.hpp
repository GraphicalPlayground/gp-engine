// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "HALForward.hpp"
#include "ImGuiForward.hpp"
#include "ImGuiPlugin.hpp"
#include "platform/ImGuiPlatformDesc.hpp"
#include "window/CursorEnums.hpp"

namespace gp::hal::event
{

struct KeyboardEvent;
struct MouseEvent;
struct TextInputEvent;
struct WindowEvent;

}   // namespace gp::hal::event

namespace gp::imgui
{

/// @brief Dear ImGui *platform* backend built on the engine HAL (`IWindow` + HAL event packets).
/// @details Mirrors the split used by the official ImGui backends (`imgui_impl_sdl3` + `imgui_impl_vulkan`): the
///          platform half owns everything that flows *into* ImGui (display size, DPI scale, delta time, keyboard,
///          mouse, text, focus) and the few things that flow back *out* to the OS (cursor shape, IME toggling).
///          It is deliberately push-based. The application forwards the HAL event packets it already drains
///          (`Application::onKeyboardEvent()` and friends) instead of the backend polling the OS, so ImGui sees exactly
///          the same ordered input stream as the rest of the engine, with no second event pump.
/// @note All methods must run on the thread that owns the ImGui context (the Game/Main thread in `gp-engine`).
///       The ImGui context targeted is the one current at `initialize()` time.
class GP_PLUGIN_IMGUI_API ImGuiHALPlatform final
{
public:
    /// @brief Constructs an uninitialized platform backend.
    ImGuiHALPlatform() noexcept = default;

    /// @brief Shuts down the backend if it is still initialized.
    ~ImGuiHALPlatform() noexcept;

    ImGuiHALPlatform(const ImGuiHALPlatform&) = delete;
    ImGuiHALPlatform& operator=(const ImGuiHALPlatform&) = delete;
    ImGuiHALPlatform(ImGuiHALPlatform&&) = delete;
    ImGuiHALPlatform& operator=(ImGuiHALPlatform&&) = delete;

public:
    /// @brief Binds the backend to the current ImGui context and the given HAL window.
    /// @details Registers `io.BackendPlatformName` / `io.BackendPlatformUserData` and advertises
    ///          `ImGuiBackendFlags_HasMouseCursors` when cursor shapes are enabled.
    /// @param[in] window Window ImGui renders into; must outlive the backend. `nullptr` is accepted for headless
    ///                   use (unit tests), in which case display metrics must be supplied through `newFrame()`.
    /// @param[in] desc Platform configuration.
    /// @return True on success; false if no ImGui context is current or a platform backend is already bound.
    [[nodiscard]] bool initialize(gp::hal::IWindow* window, const ImGuiPlatformDesc& desc = {}) noexcept;

    /// @brief Unbinds the backend from the ImGui context and stops any OS text input it started.
    void shutdown() noexcept;

    /// @brief Publishes per-frame display metrics and delta time, then syncs cursor shape and text input state.
    /// @details Must be called before `ImGui::NewFrame()`. Logical window size becomes `io.DisplaySize`; the ratio of
    ///          framebuffer pixels to logical units becomes `io.DisplayFramebufferScale`, which the renderer uses to
    ///          convert clip rectangles into pixel scissors on high-DPI displays.
    /// @param[in] deltaSeconds Unscaled frame delta in seconds (clamped to a small positive minimum).
    void newFrame(gp::Float32 deltaSeconds) noexcept;

    /// @brief Overrides the display metrics used by `newFrame()` (headless/offscreen rendering, tests).
    /// @param[in] width Logical display width.
    /// @param[in] height Logical display height.
    /// @param[in] framebufferScale Framebuffer pixels per logical unit.
    void setDisplayOverride(gp::Float32 width, gp::Float32 height, gp::Float32 framebufferScale = 1.0f) noexcept;

    /// @brief Forwards a HAL keyboard packet (modifiers + key down/up) to ImGui.
    /// @param[in] ev Decoded keyboard event.
    /// @return True if ImGui wants exclusive keyboard capture (`io.WantCaptureKeyboard`).
    bool processKeyboardEvent(const gp::hal::event::KeyboardEvent& ev) noexcept;

    /// @brief Forwards a HAL mouse packet (motion, button, wheel) to ImGui.
    /// @param[in] ev Decoded mouse event.
    /// @return True if ImGui wants exclusive mouse capture (`io.WantCaptureMouse`).
    bool processMouseEvent(const gp::hal::event::MouseEvent& ev) noexcept;

    /// @brief Forwards committed UTF-8 text to ImGui (IME composition packets are ignored).
    /// @param[in] ev Decoded text input descriptor.
    /// @param[in] utf8Text Trailing UTF-8 payload (not null-terminated).
    /// @return True if ImGui wants text input (`io.WantTextInput`).
    bool processTextInputEvent(const gp::hal::event::TextInputEvent& ev, gp::StringView utf8Text) noexcept;

    /// @brief Forwards focus gain/loss and mouse-leave notifications to ImGui.
    /// @param[in] ev Decoded window event.
    void processWindowEvent(const gp::hal::event::WindowEvent& ev) noexcept;

    /// @brief Overrides the OS content scale (DPI factor, `1.0` = 96 DPI). Also fed by `WindowAction::DPIChanged`.
    /// @param[in] contentScale Desktop scaling factor (`1.5` for "150 %"); non-positive values reset to `1.0`.
    void setContentScale(gp::Float32 contentScale) noexcept;

    /// @brief Returns the scale ImGui sizes and fonts must be multiplied by, as computed by the last `newFrame()`.
    /// @details `contentScale / framebufferScale`. The division removes the part of the DPI factor the OS already
    ///          applies through the coordinate system:
    ///          - Windows / X11: window units are physical pixels (framebuffer scale `1`), so 150 % yields `1.5`.
    ///          - macOS Retina / Wayland: window units are points and the framebuffer is 2x (content scale `2`), so
    ///            the result is `1.0`; the renderer's `DisplayFramebufferScale` already produces crisp pixels.
    /// @return UI scale factor, clamped to `[0.5, 4]`.
    [[nodiscard]] gp::Float32 getUIScale() const noexcept
    {
        return m_uiScale;
    }

    /// @brief Checks whether the backend is bound to an ImGui context.
    /// @return True between a successful `initialize()` and `shutdown()`.
    [[nodiscard]] bool isInitialized() const noexcept
    {
        return m_context != nullptr;
    }

private:
    /// @brief Pushes `ImGui::GetMouseCursor()` to the HAL window when it changed since the last frame.
    void updateCursorShape() noexcept;

    /// @brief Starts or stops OS text input to follow `io.WantTextInput`.
    void updateTextInput() noexcept;

    /// @brief Emits `ImGuiMod_*` key events reflecting the HAL modifier bitmask.
    /// @param[in] modifiers Raw `gp::hal::input::KeyModifier` bits.
    void updateKeyModifiers(gp::UInt16 modifiers) noexcept;

private:
    ImGuiContext* m_context{ nullptr };
    gp::hal::IWindow* m_window{ nullptr };
    ImGuiPlatformDesc m_desc{};
    gp::Float32 m_overrideWidth{ 0.0f };
    gp::Float32 m_overrideHeight{ 0.0f };
    gp::Float32 m_overrideScale{ 1.0f };
    gp::Float32 m_contentScale{ 1.0f };
    gp::Float32 m_uiScale{ 1.0f };
    gp::hal::CursorShape m_lastCursorShape{ gp::hal::CursorShape::Count };
    bool m_isCursorHidden{ false };
    bool m_hasDisplayOverride{ false };
    bool m_startedTextInput{ false };
};

}   // namespace gp::imgui
