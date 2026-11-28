// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "ImGuiLayer.hpp"
#include "imgui.h"
#include "ImGuiLog.hpp"
#include "ImGuiScopedContext.hpp"
#include "memory/pointers/UniquePtr.hpp"

namespace gp::imgui
{

namespace
{

/// @brief Applies one of the built-in ImGui color themes to the current context.
/// @param[in] theme Theme to apply.
void applyTheme(const ImGuiTheme theme) noexcept
{
    switch (theme)
    {
    case ImGuiTheme::Light:
        ImGui::StyleColorsLight();
        break;
    case ImGuiTheme::Classic:
        ImGui::StyleColorsClassic();
        break;
    case ImGuiTheme::Dark:
    default:
        ImGui::StyleColorsDark();
        break;
    }
}

}   // namespace

ImGuiLayer::ImGuiLayer() noexcept = default;

ImGuiLayer::~ImGuiLayer() noexcept
{
    shutdown();
}

bool ImGuiLayer::initialize(gp::hal::IWindow* const window, const ImGuiLayerDesc& desc) noexcept
{
    if (m_context != nullptr)
    {
        GP_LOG_WARNING(detail::LogImGui, "ImGuiLayer::initialize() called twice; ignoring.");
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGuiContext* const context = ImGui::CreateContext();
    if (context == nullptr)
    {
        GP_LOG_ERROR(detail::LogImGui, "ImGui::CreateContext() failed.");
        return false;
    }
    ImGui::SetCurrentContext(context);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = desc.iniFilename;
    if (desc.enableKeyboardNavigation)
    {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }
    if (desc.enableDocking)
    {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
    applyTheme(desc.theme);

    // Platform first: the renderer reads nothing from it, but the platform publishes DisplaySize which the very first
    // frame needs. Any failure rolls back in reverse order so a half-initialized context never escapes.
    if (!m_platform.initialize(window, desc.platform))
    {
        ImGui::DestroyContext(context);
        return false;
    }

    if (!m_renderer.initialize(desc.renderer))
    {
        m_platform.shutdown();
        ImGui::DestroyContext(context);
        return false;
    }

    m_context = context;
    m_isFrameOpen = false;
    m_hasPendingFrame = false;
    m_referenceStyle.reset();
    m_uiScaleOverride = desc.uiScaleOverride;
    m_uiScaleMultiplier = (desc.uiScaleMultiplier > 0.0f) ? desc.uiScaleMultiplier : 1.0f;
    m_appliedUIScale = 0.0f;
    return true;
}

void ImGuiLayer::shutdown() noexcept
{
    if (m_context == nullptr)
    {
        return;
    }

    {
        const detail::ScopedImGuiContext scope(m_context);
        if (m_isFrameOpen)
        {
            ImGui::EndFrame();
        }
        m_renderer.shutdown();
        m_platform.shutdown();
    }

    ImGui::DestroyContext(m_context);
    m_context = nullptr;
    m_referenceStyle.reset();
    m_appliedUIScale = 0.0f;
    m_isFrameOpen = false;
    m_hasPendingFrame = false;
}

void ImGuiLayer::makeCurrent() const noexcept
{
    if (m_context != nullptr)
    {
        ImGui::SetCurrentContext(m_context);
    }
}

void ImGuiLayer::beginFrame(const gp::Float32 deltaSeconds) noexcept
{
    if (m_context == nullptr || m_isFrameOpen)
    {
        return;
    }

    // Deliberately leaves the context current: user widget code runs between beginFrame() and endFrame().
    ImGui::SetCurrentContext(m_context);
    m_platform.newFrame(deltaSeconds);
    updateUIScale();
    ImGui::NewFrame();
    m_isFrameOpen = true;
    m_hasPendingFrame = false;
}

void ImGuiLayer::setUIScaleMultiplier(const gp::Float32 multiplier) noexcept
{
    m_uiScaleMultiplier = (multiplier > 0.0f) ? multiplier : 1.0f;
}

void ImGuiLayer::rebaseStyle() noexcept
{
    if (m_context == nullptr)
    {
        return;
    }

    const detail::ScopedImGuiContext scope(m_context);
    if (m_appliedUIScale <= 0.0f)
    {
        // Nothing scaled yet: the next beginFrame() snapshots the current style as-is.
        m_referenceStyle.reset();
        return;
    }

    // The live style is scaled by m_appliedUIScale; divide it back out to recover the unscaled reference.
    ImGuiStyle unscaled = ImGui::GetStyle();
    unscaled.ScaleAllSizes(1.0f / m_appliedUIScale);
    unscaled.FontScaleDpi = 1.0f;
    m_referenceStyle = gp::makeUnique<ImGuiStyle>(unscaled);
    m_appliedUIScale = 0.0f;   // Force updateUIScale() to re-apply from the new reference.
}

void ImGuiLayer::updateUIScale() noexcept
{
    const gp::Float32 baseScale = (m_uiScaleOverride > 0.0f) ? m_uiScaleOverride : m_platform.getUIScale();
    const gp::Float32 targetScale = baseScale * m_uiScaleMultiplier;

    ImGuiStyle& style = ImGui::GetStyle();
    if (!m_referenceStyle)
    {
        // Lazy snapshot: picks up theme and any size tweaks the application made between initialize() and now.
        m_referenceStyle = gp::makeUnique<ImGuiStyle>(style);
    }
    if (targetScale == m_appliedUIScale)
    {
        return;
    }

    // Deconstructive note: ScaleAllSizes() truncates to whole pixels, so repeatedly rescaling the live style would
    // drift. Always derive from the pristine reference instead (the approach recommended by Dear ImGui's FAQ).
    // Colors are copied from the live style because applications edit them at runtime (accent pickers, themes).
    ImGuiStyle scaled = *m_referenceStyle;
    for (int colorIndex = 0; colorIndex < ImGuiCol_COUNT; ++colorIndex)
    {
        scaled.Colors[colorIndex] = style.Colors[colorIndex];
    }
    scaled.ScaleAllSizes(targetScale);
    scaled.FontScaleDpi = targetScale;
    style = scaled;

    GP_LOG_INFO(detail::LogImGui, "ImGui UI scale set to {:.2f}.", static_cast<gp::Float64>(targetScale));
    m_appliedUIScale = targetScale;
}

void ImGuiLayer::endFrame() noexcept
{
    if (m_context == nullptr || !m_isFrameOpen)
    {
        return;
    }

    const detail::ScopedImGuiContext scope(m_context);
    ImGui::Render();
    m_isFrameOpen = false;
    m_hasPendingFrame = true;
}

void ImGuiLayer::prepareRender(gp::rhi::command::CommandListImmediate& cmdList) noexcept
{
    if (m_context == nullptr || !m_hasPendingFrame)
    {
        return;
    }

    const detail::ScopedImGuiContext scope(m_context);
    m_renderer.prepareDrawData(cmdList, ImGui::GetDrawData());
}

void ImGuiLayer::renderInPass(gp::rhi::command::CommandListImmediate& cmdList) noexcept
{
    if (m_context == nullptr || !m_hasPendingFrame)
    {
        return;
    }

    const detail::ScopedImGuiContext scope(m_context);
    m_renderer.drawInRenderPass(cmdList, ImGui::GetDrawData());
    m_hasPendingFrame = false;
}

void ImGuiLayer::render(
    gp::rhi::command::CommandListImmediate& cmdList, gp::rhi::resource::Texture* const renderTarget
) noexcept
{
    if (m_context == nullptr || !m_hasPendingFrame)
    {
        return;
    }

    const detail::ScopedImGuiContext scope(m_context);
    m_renderer.renderDrawData(cmdList, ImGui::GetDrawData(), renderTarget);
    m_hasPendingFrame = false;
}

bool ImGuiLayer::processKeyboardEvent(const gp::hal::event::KeyboardEvent& ev) noexcept
{
    return m_platform.processKeyboardEvent(ev);
}

bool ImGuiLayer::processMouseEvent(const gp::hal::event::MouseEvent& ev) noexcept
{
    return m_platform.processMouseEvent(ev);
}

bool ImGuiLayer::processTextInputEvent(const gp::hal::event::TextInputEvent& ev, const gp::StringView utf8Text) noexcept
{
    return m_platform.processTextInputEvent(ev, utf8Text);
}

void ImGuiLayer::processWindowEvent(const gp::hal::event::WindowEvent& ev) noexcept
{
    m_platform.processWindowEvent(ev);
}

bool ImGuiLayer::wantsMouseCapture() const noexcept
{
    if (m_context == nullptr)
    {
        return false;
    }

    const detail::ScopedImGuiContext scope(m_context);
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::wantsKeyboardCapture() const noexcept
{
    if (m_context == nullptr)
    {
        return false;
    }

    const detail::ScopedImGuiContext scope(m_context);
    return ImGui::GetIO().WantCaptureKeyboard;
}

}   // namespace gp::imgui
