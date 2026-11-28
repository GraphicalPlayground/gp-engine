// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platform/ImGuiInputTranslation.hpp"
#include "imgui.h"
#include "input/InputEnums.hpp"
#include "window/CursorEnums.hpp"
#include <gtest/gtest.h>

namespace gp::imgui::tests
{

using gp::hal::input::MouseButton;
using gp::hal::input::Scancode;

TEST(ImGuiInputTranslationTest, LetterRangeMapsContiguously)
{
    EXPECT_EQ(toImGuiKey(Scancode::A), ImGuiKey_A);
    EXPECT_EQ(toImGuiKey(Scancode::M), ImGuiKey_M);
    EXPECT_EQ(toImGuiKey(Scancode::Z), ImGuiKey_Z);
}

TEST(ImGuiInputTranslationTest, DigitRowMapsWithZeroSpecialCase)
{
    // HID orders the digit row 1..9,0 while ImGuiKey orders it 0..9: zero must not fall into the contiguous range.
    EXPECT_EQ(toImGuiKey(Scancode::Num1), ImGuiKey_1);
    EXPECT_EQ(toImGuiKey(Scancode::Num9), ImGuiKey_9);
    EXPECT_EQ(toImGuiKey(Scancode::Num0), ImGuiKey_0);
}

TEST(ImGuiInputTranslationTest, KeypadDigitsMapWithZeroSpecialCase)
{
    EXPECT_EQ(toImGuiKey(Scancode::Kp1), ImGuiKey_Keypad1);
    EXPECT_EQ(toImGuiKey(Scancode::Kp9), ImGuiKey_Keypad9);
    EXPECT_EQ(toImGuiKey(Scancode::Kp0), ImGuiKey_Keypad0);
}

TEST(ImGuiInputTranslationTest, FunctionKeysMapContiguously)
{
    EXPECT_EQ(toImGuiKey(Scancode::F1), ImGuiKey_F1);
    EXPECT_EQ(toImGuiKey(Scancode::F6), ImGuiKey_F6);
    EXPECT_EQ(toImGuiKey(Scancode::F12), ImGuiKey_F12);
}

TEST(ImGuiInputTranslationTest, NavigationAndEditingKeysMap)
{
    EXPECT_EQ(toImGuiKey(Scancode::Return), ImGuiKey_Enter);
    EXPECT_EQ(toImGuiKey(Scancode::Escape), ImGuiKey_Escape);
    EXPECT_EQ(toImGuiKey(Scancode::Backspace), ImGuiKey_Backspace);
    EXPECT_EQ(toImGuiKey(Scancode::Tab), ImGuiKey_Tab);
    EXPECT_EQ(toImGuiKey(Scancode::Left), ImGuiKey_LeftArrow);
    EXPECT_EQ(toImGuiKey(Scancode::Right), ImGuiKey_RightArrow);
    EXPECT_EQ(toImGuiKey(Scancode::Up), ImGuiKey_UpArrow);
    EXPECT_EQ(toImGuiKey(Scancode::Down), ImGuiKey_DownArrow);
    EXPECT_EQ(toImGuiKey(Scancode::Home), ImGuiKey_Home);
    EXPECT_EQ(toImGuiKey(Scancode::End), ImGuiKey_End);
    EXPECT_EQ(toImGuiKey(Scancode::Delete), ImGuiKey_Delete);
    EXPECT_EQ(toImGuiKey(Scancode::KpEnter), ImGuiKey_KeypadEnter);
}

TEST(ImGuiInputTranslationTest, ModifierKeysMapPerSide)
{
    EXPECT_EQ(toImGuiKey(Scancode::LeftCtrl), ImGuiKey_LeftCtrl);
    EXPECT_EQ(toImGuiKey(Scancode::RightCtrl), ImGuiKey_RightCtrl);
    EXPECT_EQ(toImGuiKey(Scancode::LeftShift), ImGuiKey_LeftShift);
    EXPECT_EQ(toImGuiKey(Scancode::RightAlt), ImGuiKey_RightAlt);
    EXPECT_EQ(toImGuiKey(Scancode::LeftGui), ImGuiKey_LeftSuper);
}

TEST(ImGuiInputTranslationTest, UnknownScancodeMapsToNone)
{
    EXPECT_EQ(toImGuiKey(Scancode::Unknown), ImGuiKey_None);
}

TEST(ImGuiInputTranslationTest, MouseButtonsMapToImGuiIndices)
{
    EXPECT_EQ(toImGuiMouseButton(MouseButton::Left), static_cast<gp::Int32>(ImGuiMouseButton_Left));
    EXPECT_EQ(toImGuiMouseButton(MouseButton::Right), static_cast<gp::Int32>(ImGuiMouseButton_Right));
    EXPECT_EQ(toImGuiMouseButton(MouseButton::Middle), static_cast<gp::Int32>(ImGuiMouseButton_Middle));
    EXPECT_EQ(toImGuiMouseButton(MouseButton::X1), 3);
    EXPECT_EQ(toImGuiMouseButton(MouseButton::X2), 4);
    EXPECT_EQ(toImGuiMouseButton(MouseButton::Unknown), kInvalidImGuiMouseButton);
}

TEST(ImGuiInputTranslationTest, CursorShapesMapToHAL)
{
    EXPECT_EQ(toHALCursorShape(ImGuiMouseCursor_Arrow), gp::hal::CursorShape::Arrow);
    EXPECT_EQ(toHALCursorShape(ImGuiMouseCursor_TextInput), gp::hal::CursorShape::IBeam);
    EXPECT_EQ(toHALCursorShape(ImGuiMouseCursor_ResizeEW), gp::hal::CursorShape::ResizeWE);
    EXPECT_EQ(toHALCursorShape(ImGuiMouseCursor_ResizeNS), gp::hal::CursorShape::ResizeNS);
    EXPECT_EQ(toHALCursorShape(ImGuiMouseCursor_Hand), gp::hal::CursorShape::Hand);
    EXPECT_EQ(toHALCursorShape(ImGuiMouseCursor_NotAllowed), gp::hal::CursorShape::No);
}

TEST(ImGuiInputTranslationTest, OutOfRangeCursorFallsBackToArrow)
{
    EXPECT_EQ(toHALCursorShape(ImGuiMouseCursor_COUNT), gp::hal::CursorShape::Arrow);
    EXPECT_EQ(toHALCursorShape(ImGuiMouseCursor_None), gp::hal::CursorShape::Arrow);
}

}   // namespace gp::imgui::tests
