// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "ImGuiLayer.hpp"
#include "imgui.h"
#include "ImGuiLog.hpp"
#include "ImGuiScopedContext.hpp"

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
    ImGui::NewFrame();
    m_isFrameOpen = true;
    m_hasPendingFrame = false;
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
