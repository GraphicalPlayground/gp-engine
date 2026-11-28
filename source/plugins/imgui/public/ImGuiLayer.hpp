// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/ContainerForward.hpp"
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "HALForward.hpp"
#include "ImGuiForward.hpp"
#include "ImGuiLayerDesc.hpp"
#include "ImGuiPlugin.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "platform/ImGuiHALPlatform.hpp"
#include "renderer/ImGuiRHIRenderer.hpp"

namespace gp::imgui
{

/// @brief One-object Dear ImGui integration: owns the ImGui context, the HAL platform backend and the RHI renderer.
/// @details Typical frame (all on the main thread):
/// @code
///   // Application event hooks:        layer.processMouseEvent(ev);  // returns true when ImGui captured it
///   // Application::onUpdate():        layer.beginFrame(dt); ImGui::ShowDemoWindow(); layer.endFrame();
///   // Application::onRender():        layer.render(cmdList, backBuffer);       // own LoadStore pass
///   //                         or:     layer.prepareRender(cmdList);  /* begin pass */  layer.renderInPass(cmdList);
/// @endcode
///          The layer is a thin orchestrator: every call forwards to `ImGuiHALPlatform`, `ImGuiRHIRenderer` or the
///          ImGui API itself, so it can be swapped for the two backends when an engine needs a custom pipeline (for
///          example rendering ImGui into an offscreen texture, or multiple contexts for in-world UI).
/// @note The context created by the layer is made current during `initialize()` and stays current until
///       `shutdown()`. Code that juggles several contexts should call `makeCurrent()` before using ImGui.
class GP_PLUGIN_IMGUI_API ImGuiLayer final
{
public:
    /// @brief Constructs an inactive layer.
    /// @note Defined out of line: `gp::UniquePtr<ImGuiStyle>` needs the complete type, which only the `.cpp` sees.
    ImGuiLayer() noexcept;

    /// @brief Shuts down the layer if it is still active.
    ~ImGuiLayer() noexcept;

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

public:
    /// @brief Creates the ImGui context, applies configuration and theme, and initializes both backends.
    /// @param[in] window Window ImGui renders into and receives input from; must outlive the layer.
    /// @param[in] desc Layer configuration.
    /// @return True when ImGui is fully operational; false (with everything rolled back) on any failure.
    [[nodiscard]] bool initialize(gp::hal::IWindow* window, const ImGuiLayerDesc& desc = {}) noexcept;

    /// @brief Shuts down both backends and destroys the ImGui context.
    void shutdown() noexcept;

    /// @brief Makes the layer's ImGui context current.
    void makeCurrent() const noexcept;

    /// @brief Starts a new UI frame (`ImGuiHALPlatform::newFrame()` + `ImGui::NewFrame()`).
    /// @param[in] deltaSeconds Unscaled frame delta in seconds.
    void beginFrame(gp::Float32 deltaSeconds) noexcept;

    /// @brief Finalizes the UI frame (`ImGui::Render()`); draw data becomes available for rendering.
    void endFrame() noexcept;

    /// @brief Upload phase for the last finished frame (see `ImGuiRHIRenderer::prepareDrawData()`).
    /// @param[in,out] cmdList Immediate command list, outside any render pass.
    void prepareRender(gp::rhi::command::CommandListImmediate& cmdList) noexcept;

    /// @brief Raster phase inside the caller's render pass (see `ImGuiRHIRenderer::drawInRenderPass()`).
    /// @param[in,out] cmdList Immediate command list with an open render pass.
    void renderInPass(gp::rhi::command::CommandListImmediate& cmdList) noexcept;

    /// @brief Uploads and draws the last finished frame in its own `LoadStore` pass on `renderTarget`.
    /// @param[in,out] cmdList Immediate command list, outside any render pass.
    /// @param[in] renderTarget Color target in the `RTV` access state.
    void render(gp::rhi::command::CommandListImmediate& cmdList, gp::rhi::resource::Texture* renderTarget) noexcept;

    /// @copydoc ImGuiHALPlatform::processKeyboardEvent()
    bool processKeyboardEvent(const gp::hal::event::KeyboardEvent& ev) noexcept;

    /// @copydoc ImGuiHALPlatform::processMouseEvent()
    bool processMouseEvent(const gp::hal::event::MouseEvent& ev) noexcept;

    /// @copydoc ImGuiHALPlatform::processTextInputEvent()
    bool processTextInputEvent(const gp::hal::event::TextInputEvent& ev, gp::StringView utf8Text) noexcept;

    /// @copydoc ImGuiHALPlatform::processWindowEvent()
    void processWindowEvent(const gp::hal::event::WindowEvent& ev) noexcept;

    /// @brief Checks whether ImGui currently wants exclusive mouse input (hovering or dragging a window).
    /// @return `io.WantCaptureMouse`, or false when inactive.
    [[nodiscard]] bool wantsMouseCapture() const noexcept;

    /// @brief Checks whether ImGui currently wants exclusive keyboard input (a widget has keyboard focus).
    /// @return `io.WantCaptureKeyboard`, or false when inactive.
    [[nodiscard]] bool wantsKeyboardCapture() const noexcept;

    /// @brief Returns the scale currently applied to ImGui sizes and fonts (DPI scale x multiplier, or the override).
    /// @return Applied scale; `1.0` before the first `beginFrame()`. Multiply hard-coded pixel sizes (initial window
    ///         sizes, column widths) by it so layouts match the fonts.
    [[nodiscard]] gp::Float32 getUIScale() const noexcept
    {
        return (m_appliedUIScale > 0.0f) ? m_appliedUIScale : 1.0f;
    }

    /// @brief Sets the user multiplier applied on top of the DPI scale (see `ImGuiLayerDesc::uiScaleMultiplier`).
    /// @param[in] multiplier Positive multiplier; takes effect on the next `beginFrame()`.
    void setUIScaleMultiplier(gp::Float32 multiplier) noexcept;

    /// @brief Re-captures the current style as the unscaled reference used for DPI scaling.
    /// @details Style *sizes* (padding, rounding, ...) are scaled from a reference snapshot taken at the first
    ///          `beginFrame()`, so tweaks made after `initialize()` and before that frame are included automatically.
    ///          Call this after changing sizes later on; the current style is assumed to be scaled by `getUIScale()`.
    ///          Colors are never scaled and can be edited freely at any time.
    void rebaseStyle() noexcept;

    /// @brief Checks whether the layer is operational.
    /// @return True between a successful `initialize()` and `shutdown()`.
    [[nodiscard]] bool isInitialized() const noexcept
    {
        return m_context != nullptr;
    }

    /// @brief Checks whether a frame was finalized by `endFrame()` and not rendered yet.
    /// @return True when `getDrawData()` is valid for rendering.
    [[nodiscard]] bool hasPendingDrawData() const noexcept
    {
        return m_hasPendingFrame;
    }

    /// @brief Returns the owned ImGui context.
    /// @return Context pointer, or `nullptr` when inactive.
    [[nodiscard]] ImGuiContext* getContext() const noexcept
    {
        return m_context;
    }

    /// @brief Returns the platform backend.
    /// @return Mutable platform backend.
    [[nodiscard]] ImGuiHALPlatform& getPlatform() noexcept
    {
        return m_platform;
    }

    /// @brief Returns the renderer backend.
    /// @return Mutable renderer backend.
    [[nodiscard]] ImGuiRHIRenderer& getRenderer() noexcept
    {
        return m_renderer;
    }

    /// @brief Returns the renderer backend.
    /// @return Immutable renderer backend.
    [[nodiscard]] const ImGuiRHIRenderer& getRenderer() const noexcept
    {
        return m_renderer;
    }

private:
    /// @brief Re-derives `ImGui::GetStyle()` from the reference style when the target UI scale changed.
    /// @note Runs before `ImGui::NewFrame()`, so the font atlas sees the new `FontScaleDpi` in the same frame and,
    ///       with `ImGuiBackendFlags_RendererHasTextures`, rasterizes glyphs at the exact new size (no blurry upscale).
    void updateUIScale() noexcept;

private:
    ImGuiContext* m_context{ nullptr };
    ImGuiHALPlatform m_platform;
    ImGuiRHIRenderer m_renderer;

    /// @brief Unscaled style snapshot; heap-allocated so `imgui.h` stays out of this header.
    gp::UniquePtr<ImGuiStyle> m_referenceStyle;
    gp::Float32 m_uiScaleOverride{ 0.0f };
    gp::Float32 m_uiScaleMultiplier{ 1.0f };
    gp::Float32 m_appliedUIScale{ 0.0f };
    bool m_isFrameOpen{ false };
    bool m_hasPendingFrame{ false };
};

}   // namespace gp::imgui
