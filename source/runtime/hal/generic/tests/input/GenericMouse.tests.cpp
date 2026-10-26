// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "input/GenericMouse.hpp"
#include <gtest/gtest.h>

namespace gp::hal::input::generic::tests
{

TEST(GenericMouseTest, DefaultState)
{
    Mouse mouse;

    EXPECT_EQ(mouse.getDeviceId(), 1u);
    EXPECT_EQ(mouse.getDeviceType(), InputDeviceType::Mouse);
    EXPECT_EQ(mouse.getDeviceName(), "System Mouse");
    EXPECT_TRUE(mouse.isConnected());
    EXPECT_EQ(mouse.getBatteryPercent(), 100u);
    EXPECT_EQ(mouse.getFocusedWindowId(), 0u);
    EXPECT_EQ(mouse.getHeldButtonMask(), MouseButtonMask::None);

    EXPECT_EQ(mouse.getPosition().x, 0.0f);
    EXPECT_EQ(mouse.getPosition().y, 0.0f);
    EXPECT_EQ(mouse.getDelta().x, 0.0f);
    EXPECT_EQ(mouse.getDelta().y, 0.0f);
    EXPECT_EQ(mouse.getWheelDelta().x, 0.0f);
    EXPECT_EQ(mouse.getWheelDelta().y, 0.0f);

    EXPECT_FALSE(mouse.isButtonDown(MouseButton::Left));
    EXPECT_FALSE(mouse.wasButtonJustPressed(MouseButton::Left));
    EXPECT_FALSE(mouse.wasButtonJustReleased(MouseButton::Left));
    EXPECT_FALSE(mouse.wasButtonDoubleClicked(MouseButton::Left));
}

TEST(GenericMouseTest, MotionAndDeltaAccumulation)
{
    Mouse mouse;

    event::MouseEvent ev{};
    ev.action = event::MouseAction::Moved;
    ev.windowId = 42u;
    ev.deviceId = 7u;
    ev.x = 150.0f;
    ev.y = 250.0f;
    ev.deltaX = 10.0f;
    ev.deltaY = -5.0f;
    ev.buttonMask = MouseButtonMask::None;

    mouse.onMouseEvent(ev);

    EXPECT_EQ(mouse.getFocusedWindowId(), 42u);
    EXPECT_EQ(mouse.getDeviceId(), 7u);
    EXPECT_EQ(mouse.getPosition().x, 150.0f);
    EXPECT_EQ(mouse.getPosition().y, 250.0f);
    EXPECT_EQ(mouse.getDelta().x, 10.0f);
    EXPECT_EQ(mouse.getDelta().y, -5.0f);

    // Additional motion in same frame accumulates delta
    ev.x = 160.0f;
    ev.y = 240.0f;
    ev.deltaX = 10.0f;
    ev.deltaY = -10.0f;
    mouse.onMouseEvent(ev);

    EXPECT_EQ(mouse.getPosition().x, 160.0f);
    EXPECT_EQ(mouse.getPosition().y, 240.0f);
    EXPECT_EQ(mouse.getDelta().x, 20.0f);
    EXPECT_EQ(mouse.getDelta().y, -15.0f);

    // beginFrame clears deltas but preserves position
    mouse.beginFrame();
    EXPECT_EQ(mouse.getPosition().x, 160.0f);
    EXPECT_EQ(mouse.getPosition().y, 240.0f);
    EXPECT_EQ(mouse.getDelta().x, 0.0f);
    EXPECT_EQ(mouse.getDelta().y, 0.0f);
}

TEST(GenericMouseTest, ButtonEventsAndDoubleClicks)
{
    Mouse mouse;

    // Left Button Press
    event::MouseEvent ev{};
    ev.action = event::MouseAction::ButtonChanged;
    ev.button = MouseButton::Left;
    ev.state = KeyState::Pressed;
    ev.clicks = 1u;
    ev.x = 100.0f;
    ev.y = 100.0f;

    mouse.onMouseEvent(ev);

    EXPECT_TRUE(mouse.isButtonDown(MouseButton::Left));
    EXPECT_TRUE(mouse.wasButtonJustPressed(MouseButton::Left));
    EXPECT_FALSE(mouse.wasButtonJustReleased(MouseButton::Left));
    EXPECT_FALSE(mouse.wasButtonDoubleClicked(MouseButton::Left));
    EXPECT_EQ(mouse.getHeldButtonMask(), MouseButtonMask::Left);

    // Frame latching
    mouse.beginFrame();
    EXPECT_TRUE(mouse.isButtonDown(MouseButton::Left));
    EXPECT_FALSE(mouse.wasButtonJustPressed(MouseButton::Left));

    // Release
    ev.state = KeyState::Released;
    mouse.onMouseEvent(ev);

    EXPECT_FALSE(mouse.isButtonDown(MouseButton::Left));
    EXPECT_TRUE(mouse.wasButtonJustReleased(MouseButton::Left));
    EXPECT_EQ(mouse.getHeldButtonMask(), MouseButtonMask::None);

    // Double click press
    mouse.beginFrame();
    ev.state = KeyState::Pressed;
    ev.clicks = 2u;
    mouse.onMouseEvent(ev);

    EXPECT_TRUE(mouse.isButtonDown(MouseButton::Left));
    EXPECT_TRUE(mouse.wasButtonJustPressed(MouseButton::Left));
    EXPECT_TRUE(mouse.wasButtonDoubleClicked(MouseButton::Left));
}

TEST(GenericMouseTest, ScrollWheelHandling)
{
    Mouse mouse;

    event::MouseEvent ev{};
    ev.action = event::MouseAction::WheelScrolled;
    ev.deltaX = 0.0f;
    ev.deltaY = 1.0f;
    ev.isWheelFlipped = 0u;

    mouse.onMouseEvent(ev);

    EXPECT_EQ(mouse.getWheelDelta().y, 1.0f);
    EXPECT_EQ(mouse.getWheelDelta().x, 0.0f);

    // Reverse wheel scrolling
    ev.deltaY = 2.0f;
    ev.isWheelFlipped = 1u;
    mouse.onMouseEvent(ev);

    // 1.0f + (2.0f * -1.0f) = -1.0f
    EXPECT_EQ(mouse.getWheelDelta().y, -1.0f);

    // beginFrame clears wheel delta
    mouse.beginFrame();
    EXPECT_EQ(mouse.getWheelDelta().y, 0.0f);
}

TEST(GenericMouseTest, ReleaseAllInputsAndReset)
{
    Mouse mouse;

    event::MouseEvent ev{};
    ev.action = event::MouseAction::ButtonChanged;
    ev.button = MouseButton::Right;
    ev.state = KeyState::Pressed;
    ev.clicks = 1u;
    ev.x = 50.0f;
    ev.y = 50.0f;
    mouse.onMouseEvent(ev);

    EXPECT_TRUE(mouse.isButtonDown(MouseButton::Right));

    mouse.releaseAllInputs();

    EXPECT_FALSE(mouse.isButtonDown(MouseButton::Right));
    EXPECT_TRUE(mouse.wasButtonJustReleased(MouseButton::Right));
    EXPECT_EQ(mouse.getHeldButtonMask(), MouseButtonMask::None);

    mouse.reset();

    EXPECT_FALSE(mouse.isButtonDown(MouseButton::Right));
    EXPECT_FALSE(mouse.wasButtonJustReleased(MouseButton::Right));
    EXPECT_EQ(mouse.getPosition().x, 0.0f);
    EXPECT_EQ(mouse.getPosition().y, 0.0f);
    EXPECT_EQ(mouse.getFocusedWindowId(), 0u);
}

TEST(GenericMouseTest, OutOfBoundsButton)
{
    Mouse mouse;

    const auto invalidButton = static_cast<MouseButton>(250);
    EXPECT_FALSE(mouse.isButtonDown(invalidButton));
    EXPECT_FALSE(mouse.wasButtonJustPressed(invalidButton));
    EXPECT_FALSE(mouse.wasButtonJustReleased(invalidButton));
    EXPECT_FALSE(mouse.wasButtonDoubleClicked(invalidButton));
    EXPECT_EQ(mouse.getButtonState(invalidButton), ButtonStateFlags::None);
}

}   // namespace gp::hal::input::generic::tests
