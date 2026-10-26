// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "input/GenericKeyboard.hpp"
#include <gtest/gtest.h>

namespace gp::hal::input::generic::tests
{

TEST(GenericKeyboardTest, DefaultState)
{
    Keyboard keyboard;

    EXPECT_EQ(keyboard.getDeviceId(), 1u);
    EXPECT_EQ(keyboard.getDeviceType(), InputDeviceType::Keyboard);
    EXPECT_EQ(keyboard.getDeviceName(), "System Keyboard");
    EXPECT_TRUE(keyboard.isConnected());
    EXPECT_EQ(keyboard.getBatteryPercent(), 100u);
    EXPECT_EQ(keyboard.getModifiers(), KeyModifier::None);
    EXPECT_TRUE(keyboard.hasModifiers(KeyModifier::None));

    EXPECT_FALSE(keyboard.isScancodeDown(Scancode::A));
    EXPECT_FALSE(keyboard.wasScancodeJustPressed(Scancode::A));
    EXPECT_FALSE(keyboard.wasScancodeJustReleased(Scancode::A));
    EXPECT_FALSE(keyboard.wasScancodeRepeated(Scancode::A));
    EXPECT_EQ(keyboard.getScancodeState(Scancode::A), ButtonStateFlags::None);

    EXPECT_FALSE(keyboard.isKeyDown(Keycode::A));
    EXPECT_FALSE(keyboard.wasKeyJustPressed(Keycode::A));
    EXPECT_FALSE(keyboard.wasKeyJustReleased(Keycode::A));
    EXPECT_EQ(keyboard.getKeyState(Keycode::A), ButtonStateFlags::None);
}

TEST(GenericKeyboardTest, KeyEventTransitions)
{
    Keyboard keyboard;

    event::KeyboardEvent ev{};
    ev.scancode = Scancode::A;
    ev.keycode = Keycode::A;
    ev.state = KeyState::Pressed;
    ev.isRepeat = 0u;
    ev.modifiers = KeyModifier::None;

    keyboard.onKeyboardEvent(ev);

    EXPECT_TRUE(keyboard.isScancodeDown(Scancode::A));
    EXPECT_TRUE(keyboard.wasScancodeJustPressed(Scancode::A));
    EXPECT_FALSE(keyboard.wasScancodeJustReleased(Scancode::A));
    EXPECT_FALSE(keyboard.wasScancodeRepeated(Scancode::A));

    EXPECT_TRUE(keyboard.isKeyDown(Keycode::A));
    EXPECT_TRUE(keyboard.wasKeyJustPressed(Keycode::A));
    EXPECT_FALSE(keyboard.wasKeyJustReleased(Keycode::A));

    // Next frame: edge cleared, held maintained
    keyboard.beginFrame();

    EXPECT_TRUE(keyboard.isScancodeDown(Scancode::A));
    EXPECT_FALSE(keyboard.wasScancodeJustPressed(Scancode::A));
    EXPECT_FALSE(keyboard.wasScancodeJustReleased(Scancode::A));
    EXPECT_TRUE(keyboard.isKeyDown(Keycode::A));
    EXPECT_FALSE(keyboard.wasKeyJustPressed(Keycode::A));

    // Key repeat event
    ev.isRepeat = 1u;
    keyboard.onKeyboardEvent(ev);

    EXPECT_TRUE(keyboard.isScancodeDown(Scancode::A));
    EXPECT_TRUE(keyboard.wasScancodeRepeated(Scancode::A));

    // Key release event
    ev.state = KeyState::Released;
    ev.isRepeat = 0u;
    keyboard.onKeyboardEvent(ev);

    EXPECT_FALSE(keyboard.isScancodeDown(Scancode::A));
    EXPECT_TRUE(keyboard.wasScancodeJustReleased(Scancode::A));
    EXPECT_FALSE(keyboard.isKeyDown(Keycode::A));
    EXPECT_TRUE(keyboard.wasKeyJustReleased(Keycode::A));

    // Begin next frame clears release edge
    keyboard.beginFrame();
    EXPECT_FALSE(keyboard.wasScancodeJustReleased(Scancode::A));
    EXPECT_FALSE(keyboard.wasKeyJustReleased(Keycode::A));
}

TEST(GenericKeyboardTest, ModifierHandling)
{
    Keyboard keyboard;

    // Press LeftShift
    event::KeyboardEvent evShift{};
    evShift.scancode = Scancode::LeftShift;
    evShift.keycode = Keycode::LeftShift;
    evShift.state = KeyState::Pressed;
    evShift.modifiers = KeyModifier::LeftShift;

    keyboard.onKeyboardEvent(evShift);

    EXPECT_TRUE(keyboard.isScancodeDown(Scancode::LeftShift));
    EXPECT_TRUE((keyboard.getModifiers() & KeyModifier::LeftShift) != KeyModifier::None);
    EXPECT_TRUE(keyboard.hasModifiers(KeyModifier::Shift));
    EXPECT_TRUE(keyboard.hasModifiers(KeyModifier::LeftShift));
    EXPECT_FALSE(keyboard.hasModifiers(KeyModifier::RightShift));
    EXPECT_FALSE(keyboard.hasModifiers(KeyModifier::Ctrl));

    // Press RightCtrl
    event::KeyboardEvent evCtrl{};
    evCtrl.scancode = Scancode::RightCtrl;
    evCtrl.keycode = Keycode::RightCtrl;
    evCtrl.state = KeyState::Pressed;
    evCtrl.modifiers = KeyModifier::LeftShift | KeyModifier::RightCtrl;

    keyboard.onKeyboardEvent(evCtrl);

    EXPECT_TRUE(keyboard.hasModifiers(KeyModifier::Shift | KeyModifier::Ctrl));
    EXPECT_TRUE(keyboard.hasModifiers(KeyModifier::LeftShift | KeyModifier::RightCtrl));

    // Release LeftShift
    evShift.state = KeyState::Released;
    evShift.modifiers = KeyModifier::RightCtrl;
    keyboard.onKeyboardEvent(evShift);

    EXPECT_FALSE(keyboard.isScancodeDown(Scancode::LeftShift));
    EXPECT_FALSE(keyboard.hasModifiers(KeyModifier::Shift));
    EXPECT_TRUE(keyboard.hasModifiers(KeyModifier::Ctrl));
}

TEST(GenericKeyboardTest, ReleaseAllInputsAndReset)
{
    Keyboard keyboard;

    event::KeyboardEvent ev{};
    ev.scancode = Scancode::W;
    ev.keycode = Keycode::W;
    ev.state = KeyState::Pressed;
    keyboard.onKeyboardEvent(ev);

    EXPECT_TRUE(keyboard.isScancodeDown(Scancode::W));

    keyboard.releaseAllInputs();

    EXPECT_FALSE(keyboard.isScancodeDown(Scancode::W));
    EXPECT_TRUE(keyboard.wasScancodeJustReleased(Scancode::W));
    EXPECT_EQ(keyboard.getModifiers(), KeyModifier::None);

    keyboard.reset();

    EXPECT_FALSE(keyboard.isScancodeDown(Scancode::W));
    EXPECT_FALSE(keyboard.wasScancodeJustReleased(Scancode::W));
    EXPECT_EQ(keyboard.getScancodeState(Scancode::W), ButtonStateFlags::None);
}

TEST(GenericKeyboardTest, OverflowKeycodeHandling)
{
    Keyboard keyboard;

    // Synthetic keycode outside standard ASCII/Scancode mapped range
    const auto highKeycode = static_cast<Keycode>(0x70000001u);

    event::KeyboardEvent ev{};
    ev.scancode = Scancode::Unknown;
    ev.keycode = highKeycode;
    ev.state = KeyState::Pressed;

    keyboard.onKeyboardEvent(ev);

    EXPECT_TRUE(keyboard.isKeyDown(highKeycode));
    EXPECT_TRUE(keyboard.wasKeyJustPressed(highKeycode));

    keyboard.beginFrame();
    EXPECT_TRUE(keyboard.isKeyDown(highKeycode));
    EXPECT_FALSE(keyboard.wasKeyJustPressed(highKeycode));

    ev.state = KeyState::Released;
    keyboard.onKeyboardEvent(ev);

    EXPECT_FALSE(keyboard.isKeyDown(highKeycode));
    EXPECT_TRUE(keyboard.wasKeyJustReleased(highKeycode));

    keyboard.reset();
    EXPECT_FALSE(keyboard.isKeyDown(highKeycode));
    EXPECT_EQ(keyboard.getKeyState(highKeycode), ButtonStateFlags::None);
}

TEST(GenericKeyboardTest, OutOfBoundsScancode)
{
    Keyboard keyboard;

    const auto invalidScancode = static_cast<Scancode>(9999);
    EXPECT_FALSE(keyboard.isScancodeDown(invalidScancode));
    EXPECT_FALSE(keyboard.wasScancodeJustPressed(invalidScancode));
    EXPECT_FALSE(keyboard.wasScancodeJustReleased(invalidScancode));
    EXPECT_FALSE(keyboard.wasScancodeRepeated(invalidScancode));
    EXPECT_EQ(keyboard.getScancodeState(invalidScancode), ButtonStateFlags::None);
}

}   // namespace gp::hal::input::generic::tests
