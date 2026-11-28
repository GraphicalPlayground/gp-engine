// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platform/ImGuiHALPlatform.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "event/data/TextInputEvent.hpp"
#include "event/input/KeyboardEvent.hpp"
#include "event/input/MouseEvent.hpp"
#include "event/system/WindowEvent.hpp"
#include "imgui.h"
#include "ImGuiScopedContext.hpp"
#include "input/InputEnums.hpp"
#include "platform/ImGuiInputTranslation.hpp"
#include "window/IWindow.hpp"
#include "window/WindowTypes.hpp"
#include <cfloat>

namespace gp::imgui
{

namespace
{

/// @brief Backend identifier published in `io.BackendPlatformName` (visible in ImGui's About/Metrics windows).
constexpr const char* kBackendPlatformName = "gp-engine HAL";

/// @brief Smallest delta time forwarded to ImGui, which asserts on non-positive values.
constexpr gp::Float32 kMinimumDeltaSeconds = 1.0f / 10000.0f;

/// @brief Lower bound of the UI scale (guards against bogus OS values and tiny unreadable UI).
constexpr gp::Float32 kMinimumUIScale = 0.5f;

/// @brief Upper bound of the UI scale (8K at 400 % still fits; prevents runaway sizes on broken drivers).
constexpr gp::Float32 kMaximumUIScale = 4.0f;

/// @brief Byte capacity of the stack buffer used to null-terminate UTF-8 text chunks for ImGui.
constexpr gp::USize kTextChunkCapacity = 256u;

using detail::ScopedImGuiContext;

/// @brief Checks whether any bit of `mask` is set in `modifiers`.
/// @param[in] modifiers Raw modifier bits.
/// @param[in] mask Modifier mask to test.
/// @return True if at least one bit of `mask` is set.
[[nodiscard]] constexpr bool hasModifier(const gp::UInt16 modifiers, const gp::hal::input::KeyModifier mask) noexcept
{
    return (modifiers & static_cast<gp::UInt16>(mask)) != 0u;
}

}   // namespace

ImGuiHALPlatform::~ImGuiHALPlatform() noexcept
{
    shutdown();
}

bool ImGuiHALPlatform::initialize(gp::hal::IWindow* const window, const ImGuiPlatformDesc& desc) noexcept
{
    ImGuiContext* const context = ImGui::GetCurrentContext();
    if (context == nullptr || m_context != nullptr)
    {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendPlatformUserData != nullptr)
    {
        return false;
    }

    m_context = context;
    m_window = window;
    m_desc = desc;
    m_lastCursorShape = gp::hal::CursorShape::Count;
    m_isCursorHidden = false;
    m_startedTextInput = false;
    m_contentScale = 1.0f;
    m_uiScale = 1.0f;
    if (m_window != nullptr)
    {
        setContentScale(m_window->getContentScale());
    }

    io.BackendPlatformName = kBackendPlatformName;
    io.BackendPlatformUserData = this;
    if (m_desc.enableCursorShapes && m_window != nullptr)
    {
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    }

    return true;
}

void ImGuiHALPlatform::shutdown() noexcept
{
    if (m_context == nullptr)
    {
        return;
    }

    if (m_window != nullptr)
    {
        if (m_startedTextInput)
        {
            m_window->setTextInputActive(false);
        }
        if (m_isCursorHidden)
        {
            m_window->setCursorMode(gp::hal::CursorMode::Normal);
        }
    }

    {
        const ScopedImGuiContext scope(m_context);
        ImGuiIO& io = ImGui::GetIO();
        io.BackendPlatformName = nullptr;
        io.BackendPlatformUserData = nullptr;
        io.BackendFlags &= ~ImGuiBackendFlags_HasMouseCursors;
    }

    m_context = nullptr;
    m_window = nullptr;
    m_startedTextInput = false;
    m_isCursorHidden = false;
    m_hasDisplayOverride = false;
}

void ImGuiHALPlatform::setDisplayOverride(
    const gp::Float32 width, const gp::Float32 height, const gp::Float32 framebufferScale
) noexcept
{
    m_overrideWidth = width;
    m_overrideHeight = height;
    m_overrideScale = framebufferScale;
    m_hasDisplayOverride = true;
}

void ImGuiHALPlatform::setContentScale(const gp::Float32 contentScale) noexcept
{
    m_contentScale = (contentScale > 0.0f) ? contentScale : 1.0f;
}

void ImGuiHALPlatform::newFrame(const gp::Float32 deltaSeconds) noexcept
{
    if (m_context == nullptr)
    {
        return;
    }

    const ScopedImGuiContext scope(m_context);
    ImGuiIO& io = ImGui::GetIO();

    if (m_hasDisplayOverride || m_window == nullptr)
    {
        io.DisplaySize = ImVec2(m_overrideWidth, m_overrideHeight);
        io.DisplayFramebufferScale = ImVec2(m_overrideScale, m_overrideScale);
    }
    else
    {
        // Deconstructive note: HAL mouse coordinates are in logical window units, so ImGui must lay out in the same
        // space. The framebuffer/logical ratio is exported separately; the renderer multiplies clip rectangles by it
        // to produce pixel-exact scissors on 150%/200% DPI displays.
        const gp::hal::Extent2D logicalSize = m_window->getSize();
        const gp::hal::Extent2D pixelSize = m_window->getFramebufferSize();
        const gp::Float32 logicalWidth = static_cast<gp::Float32>(logicalSize.width);
        const gp::Float32 logicalHeight = static_cast<gp::Float32>(logicalSize.height);

        io.DisplaySize = ImVec2(logicalWidth, logicalHeight);
        if (logicalSize.width > 0u && logicalSize.height > 0u && pixelSize.width > 0u && pixelSize.height > 0u)
        {
            io.DisplayFramebufferScale = ImVec2(
                static_cast<gp::Float32>(pixelSize.width) / logicalWidth,
                static_cast<gp::Float32>(pixelSize.height) / logicalHeight
            );
        }
        else
        {
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        }
    }

    // Only the DPI factor the coordinate system does not already absorb is applied to ImGui sizes (see getUIScale()).
    const gp::Float32 framebufferScale = (io.DisplayFramebufferScale.x > 0.0f) ? io.DisplayFramebufferScale.x : 1.0f;
    const gp::Float32 uiScale = m_contentScale / framebufferScale;
    m_uiScale =
        (uiScale < kMinimumUIScale) ? kMinimumUIScale : ((uiScale > kMaximumUIScale) ? kMaximumUIScale : uiScale);

    io.DeltaTime = (deltaSeconds > kMinimumDeltaSeconds) ? deltaSeconds : kMinimumDeltaSeconds;

    updateCursorShape();
    updateTextInput();
}

bool ImGuiHALPlatform::processKeyboardEvent(const gp::hal::event::KeyboardEvent& ev) noexcept
{
    if (m_context == nullptr)
    {
        return false;
    }

    const ScopedImGuiContext scope(m_context);
    ImGuiIO& io = ImGui::GetIO();

    updateKeyModifiers(static_cast<gp::UInt16>(ev.modifiers));

    const ImGuiKey key = toImGuiKey(ev.scancode);
    if (key != ImGuiKey_None)
    {
        const bool isDown = ev.state == gp::hal::input::KeyState::Pressed;
        io.AddKeyEvent(key, isDown);
        io.SetKeyEventNativeData(key, static_cast<int>(ev.keycode), static_cast<int>(ev.scancode));
    }

    return io.WantCaptureKeyboard;
}

bool ImGuiHALPlatform::processMouseEvent(const gp::hal::event::MouseEvent& ev) noexcept
{
    if (m_context == nullptr)
    {
        return false;
    }

    const ScopedImGuiContext scope(m_context);
    ImGuiIO& io = ImGui::GetIO();

    switch (ev.action)
    {
    case gp::hal::event::MouseAction::Moved:
    {
        io.AddMousePosEvent(ev.x, ev.y);
        break;
    }
    case gp::hal::event::MouseAction::ButtonChanged:
    {
        const gp::Int32 button = toImGuiMouseButton(ev.button);
        if (button != kInvalidImGuiMouseButton)
        {
            io.AddMousePosEvent(ev.x, ev.y);
            io.AddMouseButtonEvent(button, ev.state == gp::hal::input::KeyState::Pressed);
        }
        break;
    }
    case gp::hal::event::MouseAction::WheelScrolled:
    {
        // Same convention as the official SDL3 backend: horizontal wheel is mirrored, vertical is forwarded as-is.
        io.AddMouseWheelEvent(-ev.deltaX, ev.deltaY);
        break;
    }
    default:
        break;
    }

    return io.WantCaptureMouse;
}

bool ImGuiHALPlatform::processTextInputEvent(
    const gp::hal::event::TextInputEvent& ev, const gp::StringView utf8Text
) noexcept
{
    if (m_context == nullptr)
    {
        return false;
    }

    const ScopedImGuiContext scope(m_context);
    ImGuiIO& io = ImGui::GetIO();

    if (ev.action != gp::hal::event::TextInputAction::Committed || utf8Text.isEmpty())
    {
        return io.WantTextInput;
    }

    // ImGui expects null-terminated UTF-8. The HAL payload is a non-terminated view into the event ring, so it is
    // copied in stack chunks; chunk boundaries are moved back to a code point start so no sequence is ever split.
    char chunk[kTextChunkCapacity + 1u]{};
    const char* cursor = utf8Text.data();
    gp::USize remaining = utf8Text.size();
    while (remaining > 0u)
    {
        gp::USize count = (remaining < kTextChunkCapacity) ? remaining : kTextChunkCapacity;
        if (count < remaining)
        {
            while (count > 0u && (static_cast<gp::UInt8>(cursor[count]) & 0xC0u) == 0x80u)
            {
                --count;
            }
            if (count == 0u)
            {
                count = (remaining < kTextChunkCapacity) ? remaining : kTextChunkCapacity;
            }
        }

        for (gp::USize i = 0u; i < count; ++i)
        {
            chunk[i] = cursor[i];
        }
        chunk[count] = '\0';
        io.AddInputCharactersUTF8(chunk);

        cursor += count;
        remaining -= count;
    }

    return io.WantTextInput;
}

void ImGuiHALPlatform::processWindowEvent(const gp::hal::event::WindowEvent& ev) noexcept
{
    if (m_context == nullptr)
    {
        return;
    }

    const ScopedImGuiContext scope(m_context);
    ImGuiIO& io = ImGui::GetIO();

    switch (ev.action)
    {
    case gp::hal::event::WindowAction::FocusGained:
        io.AddFocusEvent(true);
        break;
    case gp::hal::event::WindowAction::FocusLost:
        io.AddFocusEvent(false);
        break;
    case gp::hal::event::WindowAction::MouseLeft:
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    case gp::hal::event::WindowAction::DPIChanged:
        // Window dragged to another monitor or desktop scaling changed: the next newFrame() picks up the new factor.
        setContentScale(ev.dpi.scaleX);
        break;
    default:
        break;
    }
}

void ImGuiHALPlatform::updateCursorShape() noexcept
{
    if (!m_desc.enableCursorShapes || m_window == nullptr)
    {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) != 0)
    {
        return;
    }

    const ImGuiMouseCursor imguiCursor = ImGui::GetMouseCursor();
    const bool shouldHide = io.MouseDrawCursor || imguiCursor == ImGuiMouseCursor_None;
    if (shouldHide != m_isCursorHidden)
    {
        m_window->setCursorMode(shouldHide ? gp::hal::CursorMode::Hidden : gp::hal::CursorMode::Normal);
        m_isCursorHidden = shouldHide;
    }

    if (shouldHide)
    {
        return;
    }

    const gp::hal::CursorShape shape = toHALCursorShape(imguiCursor);
    if (shape != m_lastCursorShape)
    {
        m_window->setCursorShape(shape);
        m_lastCursorShape = shape;
    }
}

void ImGuiHALPlatform::updateTextInput() noexcept
{
    if (!m_desc.enableTextInputToggling || m_window == nullptr)
    {
        return;
    }

    const bool wantsTextInput = ImGui::GetIO().WantTextInput;
    if (wantsTextInput && !m_window->isTextInputActive())
    {
        m_window->setTextInputActive(true);
        m_startedTextInput = m_window->isTextInputActive();
    }
    else if (!wantsTextInput && m_startedTextInput)
    {
        // Only stop text input this backend started, so an application-owned text field outside ImGui keeps its IME.
        m_window->setTextInputActive(false);
        m_startedTextInput = false;
    }
}

void ImGuiHALPlatform::updateKeyModifiers(const gp::UInt16 modifiers) noexcept
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, hasModifier(modifiers, gp::hal::input::KeyModifier::Ctrl));
    io.AddKeyEvent(ImGuiMod_Shift, hasModifier(modifiers, gp::hal::input::KeyModifier::Shift));
    io.AddKeyEvent(ImGuiMod_Alt, hasModifier(modifiers, gp::hal::input::KeyModifier::Alt));
    io.AddKeyEvent(ImGuiMod_Super, hasModifier(modifiers, gp::hal::input::KeyModifier::Gui));
}

}   // namespace gp::imgui
