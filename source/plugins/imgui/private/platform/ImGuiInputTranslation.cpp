// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platform/ImGuiInputTranslation.hpp"
#include "imgui.h"

namespace gp::imgui
{

ImGuiKey toImGuiKey(const gp::hal::input::Scancode scancode) noexcept
{
    using gp::hal::input::Scancode;

    const gp::UInt16 raw = static_cast<gp::UInt16>(scancode);

    // Deconstructive note: USB HID usage IDs are contiguous for letters (A..Z = 4..29), the digit row
    // (1..9,0 = 30..39), F1..F12 (58..69) and the keypad digits (1..9,0 = 89..98). ImGuiKey is contiguous for the
    // same groups, so those ranges translate with one subtraction instead of 60 switch cases.
    if (raw >= static_cast<gp::UInt16>(Scancode::A) && raw <= static_cast<gp::UInt16>(Scancode::Z))
    {
        return static_cast<ImGuiKey>(ImGuiKey_A + (raw - static_cast<gp::UInt16>(Scancode::A)));
    }
    if (raw >= static_cast<gp::UInt16>(Scancode::Num1) && raw <= static_cast<gp::UInt16>(Scancode::Num9))
    {
        return static_cast<ImGuiKey>(ImGuiKey_1 + (raw - static_cast<gp::UInt16>(Scancode::Num1)));
    }
    if (raw >= static_cast<gp::UInt16>(Scancode::F1) && raw <= static_cast<gp::UInt16>(Scancode::F12))
    {
        return static_cast<ImGuiKey>(ImGuiKey_F1 + (raw - static_cast<gp::UInt16>(Scancode::F1)));
    }
    if (raw >= static_cast<gp::UInt16>(Scancode::Kp1) && raw <= static_cast<gp::UInt16>(Scancode::Kp9))
    {
        return static_cast<ImGuiKey>(ImGuiKey_Keypad1 + (raw - static_cast<gp::UInt16>(Scancode::Kp1)));
    }

    switch (scancode)
    {
    case Scancode::Num0:
        return ImGuiKey_0;
    case Scancode::Kp0:
        return ImGuiKey_Keypad0;
    case Scancode::Return:
        return ImGuiKey_Enter;
    case Scancode::Escape:
        return ImGuiKey_Escape;
    case Scancode::Backspace:
        return ImGuiKey_Backspace;
    case Scancode::Tab:
        return ImGuiKey_Tab;
    case Scancode::Space:
        return ImGuiKey_Space;
    case Scancode::Minus:
        return ImGuiKey_Minus;
    case Scancode::Equals:
        return ImGuiKey_Equal;
    case Scancode::LeftBracket:
        return ImGuiKey_LeftBracket;
    case Scancode::RightBracket:
        return ImGuiKey_RightBracket;
    case Scancode::Backslash:
    case Scancode::NonUsHash:
        return ImGuiKey_Backslash;
    case Scancode::Semicolon:
        return ImGuiKey_Semicolon;
    case Scancode::Apostrophe:
        return ImGuiKey_Apostrophe;
    case Scancode::Grave:
        return ImGuiKey_GraveAccent;
    case Scancode::Comma:
        return ImGuiKey_Comma;
    case Scancode::Period:
        return ImGuiKey_Period;
    case Scancode::Slash:
        return ImGuiKey_Slash;
    case Scancode::CapsLock:
        return ImGuiKey_CapsLock;
    case Scancode::PrintScreen:
        return ImGuiKey_PrintScreen;
    case Scancode::ScrollLock:
        return ImGuiKey_ScrollLock;
    case Scancode::Pause:
        return ImGuiKey_Pause;
    case Scancode::Insert:
        return ImGuiKey_Insert;
    case Scancode::Home:
        return ImGuiKey_Home;
    case Scancode::PageUp:
        return ImGuiKey_PageUp;
    case Scancode::Delete:
        return ImGuiKey_Delete;
    case Scancode::End:
        return ImGuiKey_End;
    case Scancode::PageDown:
        return ImGuiKey_PageDown;
    case Scancode::Right:
        return ImGuiKey_RightArrow;
    case Scancode::Left:
        return ImGuiKey_LeftArrow;
    case Scancode::Down:
        return ImGuiKey_DownArrow;
    case Scancode::Up:
        return ImGuiKey_UpArrow;
    case Scancode::NumLock:
        return ImGuiKey_NumLock;
    case Scancode::KpDivide:
        return ImGuiKey_KeypadDivide;
    case Scancode::KpMultiply:
        return ImGuiKey_KeypadMultiply;
    case Scancode::KpMinus:
        return ImGuiKey_KeypadSubtract;
    case Scancode::KpPlus:
        return ImGuiKey_KeypadAdd;
    case Scancode::KpEnter:
        return ImGuiKey_KeypadEnter;
    case Scancode::KpPeriod:
        return ImGuiKey_KeypadDecimal;
    case Scancode::NonUsBackslash:
        return ImGuiKey_Oem102;
    case Scancode::Application:
        return ImGuiKey_Menu;
    case Scancode::LeftCtrl:
        return ImGuiKey_LeftCtrl;
    case Scancode::LeftShift:
        return ImGuiKey_LeftShift;
    case Scancode::LeftAlt:
        return ImGuiKey_LeftAlt;
    case Scancode::LeftGui:
        return ImGuiKey_LeftSuper;
    case Scancode::RightCtrl:
        return ImGuiKey_RightCtrl;
    case Scancode::RightShift:
        return ImGuiKey_RightShift;
    case Scancode::RightAlt:
        return ImGuiKey_RightAlt;
    case Scancode::RightGui:
        return ImGuiKey_RightSuper;
    default:
        break;
    }

    return ImGuiKey_None;
}

gp::Int32 toImGuiMouseButton(const gp::hal::input::MouseButton button) noexcept
{
    switch (button)
    {
    case gp::hal::input::MouseButton::Left:
        return ImGuiMouseButton_Left;
    case gp::hal::input::MouseButton::Right:
        return ImGuiMouseButton_Right;
    case gp::hal::input::MouseButton::Middle:
        return ImGuiMouseButton_Middle;
    case gp::hal::input::MouseButton::X1:
        return 3;
    case gp::hal::input::MouseButton::X2:
        return 4;
    default:
        break;
    }

    return kInvalidImGuiMouseButton;
}

gp::hal::CursorShape toHALCursorShape(const gp::Int32 imguiCursor) noexcept
{
    switch (imguiCursor)
    {
    case ImGuiMouseCursor_Arrow:
        return gp::hal::CursorShape::Arrow;
    case ImGuiMouseCursor_TextInput:
        return gp::hal::CursorShape::IBeam;
    case ImGuiMouseCursor_ResizeAll:
        return gp::hal::CursorShape::ResizeAll;
    case ImGuiMouseCursor_ResizeNS:
        return gp::hal::CursorShape::ResizeNS;
    case ImGuiMouseCursor_ResizeEW:
        return gp::hal::CursorShape::ResizeWE;
    case ImGuiMouseCursor_ResizeNESW:
        return gp::hal::CursorShape::ResizeNESW;
    case ImGuiMouseCursor_ResizeNWSE:
        return gp::hal::CursorShape::ResizeNWSE;
    case ImGuiMouseCursor_Hand:
        return gp::hal::CursorShape::Hand;
    case ImGuiMouseCursor_Wait:
        return gp::hal::CursorShape::Wait;
    case ImGuiMouseCursor_Progress:
        return gp::hal::CursorShape::WaitArrow;
    case ImGuiMouseCursor_NotAllowed:
        return gp::hal::CursorShape::No;
    default:
        break;
    }

    return gp::hal::CursorShape::Arrow;
}

}   // namespace gp::imgui
