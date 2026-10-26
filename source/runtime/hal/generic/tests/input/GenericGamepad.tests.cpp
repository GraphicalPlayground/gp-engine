// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "input/GenericGamepad.hpp"
#include <gtest/gtest.h>

namespace gp::hal::input::generic::tests
{

TEST(GenericGamepadTest, DefaultUnattachedState)
{
    Gamepad gamepad;

    EXPECT_EQ(gamepad.getDeviceId(), kInvalidInputDeviceId);
    EXPECT_EQ(gamepad.getDeviceType(), InputDeviceType::Gamepad);
    EXPECT_EQ(gamepad.getSlot(), GamepadSlot::Player1);
    EXPECT_FALSE(gamepad.isConnected());
    EXPECT_EQ(gamepad.getBatteryPercent(), 100u);
    EXPECT_EQ(gamepad.getHatPosition(), HatPosition::Centered);
    EXPECT_EQ(gamepad.getTouchpadState(), TouchFingerState::Up);
    EXPECT_EQ(gamepad.getActiveTouchCount(), 0u);
    EXPECT_EQ(gamepad.getTouchContact(0u), nullptr);

    EXPECT_FALSE(gamepad.isButtonDown(GamepadButton::South));
    EXPECT_EQ(gamepad.getAxis(GamepadAxis::LeftX), 0.0f);
    EXPECT_EQ(gamepad.getRawAxis(GamepadAxis::LeftX), 0.0f);
}

TEST(GenericGamepadTest, AttachAndDetachLifecycle)
{
    Gamepad gamepad;

    gamepad.attach(42u, GamepadSlot::Player2, "Xbox Wireless Controller");

    EXPECT_TRUE(gamepad.isConnected());
    EXPECT_EQ(gamepad.getDeviceId(), 42u);
    EXPECT_EQ(gamepad.getSlot(), GamepadSlot::Player2);
    EXPECT_EQ(gamepad.getDeviceName(), "Xbox Wireless Controller");

    // Press button while attached
    event::GamepadEvent ev{};
    ev.action = event::GamepadAction::ButtonChanged;
    ev.control.button = GamepadButton::South;
    ev.state = KeyState::Pressed;
    gamepad.onGamepadEvent(ev);

    EXPECT_TRUE(gamepad.isButtonDown(GamepadButton::South));

    // releaseAllInputs synthesizes release flags
    gamepad.releaseAllInputs();
    EXPECT_FALSE(gamepad.isButtonDown(GamepadButton::South));
    EXPECT_TRUE(gamepad.wasButtonJustReleased(GamepadButton::South));

    // Detach marks disconnected
    gamepad.detach();
    EXPECT_FALSE(gamepad.isConnected());
}

TEST(GenericGamepadTest, ButtonEventsAndEdgeLatching)
{
    Gamepad gamepad;
    gamepad.attach(1u, GamepadSlot::Player1);

    event::GamepadEvent ev{};
    ev.action = event::GamepadAction::ButtonChanged;
    ev.control.button = GamepadButton::East;
    ev.state = KeyState::Pressed;

    gamepad.onGamepadEvent(ev);

    EXPECT_TRUE(gamepad.isButtonDown(GamepadButton::East));
    EXPECT_TRUE(gamepad.wasButtonJustPressed(GamepadButton::East));
    EXPECT_FALSE(gamepad.wasButtonJustReleased(GamepadButton::East));

    gamepad.beginFrame();
    EXPECT_TRUE(gamepad.isButtonDown(GamepadButton::East));
    EXPECT_FALSE(gamepad.wasButtonJustPressed(GamepadButton::East));

    ev.state = KeyState::Released;
    gamepad.onGamepadEvent(ev);

    EXPECT_FALSE(gamepad.isButtonDown(GamepadButton::East));
    EXPECT_TRUE(gamepad.wasButtonJustReleased(GamepadButton::East));
}

TEST(GenericGamepadTest, AnalogAxesAndRadialDeadzones)
{
    Gamepad gamepad;
    gamepad.attach(1u, GamepadSlot::Player1);

    // Deflect left stick below inner deadzone (0.12f default)
    event::GamepadEvent ev{};
    ev.action = event::GamepadAction::AxisMoved;
    ev.control.axis = GamepadAxis::LeftX;
    ev.value = 0.08f;
    gamepad.onGamepadEvent(ev);

    EXPECT_EQ(gamepad.getRawAxis(GamepadAxis::LeftX), 0.08f);
    EXPECT_EQ(gamepad.getAxis(GamepadAxis::LeftX), 0.0f);
    EXPECT_EQ(gamepad.getLeftStick().x, 0.0f);

    // Deflect left stick above inner deadzone
    ev.value = 0.5f;
    gamepad.onGamepadEvent(ev);

    EXPECT_EQ(gamepad.getRawAxis(GamepadAxis::LeftX), 0.5f);
    EXPECT_GT(gamepad.getAxis(GamepadAxis::LeftX), 0.0f);
    EXPECT_GT(gamepad.getLeftStick().x, 0.0f);

    // Disable deadzones
    AnalogDeadzoneConfig noDeadzone{ 0.0f, 1.0f, DeadzoneMode::None };
    gamepad.setDeadzones(noDeadzone, noDeadzone, noDeadzone);

    EXPECT_FLOAT_EQ(gamepad.getAxis(GamepadAxis::LeftX), 0.5f);
    EXPECT_FLOAT_EQ(gamepad.getLeftStick().x, 0.5f);
}

TEST(GenericGamepadTest, TriggerAxialDeadzones)
{
    Gamepad gamepad;
    gamepad.attach(1u, GamepadSlot::Player1);

    // Small trigger deflection below default 0.05f deadzone
    event::GamepadEvent ev{};
    ev.action = event::GamepadAction::AxisMoved;
    ev.control.axis = GamepadAxis::RightTrigger;
    ev.value = 0.02f;
    gamepad.onGamepadEvent(ev);

    EXPECT_EQ(gamepad.getRawAxis(GamepadAxis::RightTrigger), 0.02f);
    EXPECT_EQ(gamepad.getAxis(GamepadAxis::RightTrigger), 0.0f);

    // Substantial deflection
    ev.value = 0.515f;
    gamepad.onGamepadEvent(ev);

    EXPECT_EQ(gamepad.getRawAxis(GamepadAxis::RightTrigger), 0.515f);
    EXPECT_GT(gamepad.getAxis(GamepadAxis::RightTrigger), 0.0f);
}

TEST(GenericGamepadTest, HatAndTouchpadEvents)
{
    Gamepad gamepad;
    gamepad.attach(1u, GamepadSlot::Player1);

    // Hat change
    event::GamepadEvent evHat{};
    evHat.action = event::GamepadAction::HatChanged;
    evHat.control.hatPosition = HatPosition::Up;
    gamepad.onGamepadEvent(evHat);

    EXPECT_EQ(gamepad.getHatPosition(), HatPosition::Up);

    // Touchpad change
    event::GamepadEvent evTouch{};
    evTouch.action = event::GamepadAction::TouchpadChanged;
    evTouch.control.touchState = TouchFingerState::Down;
    evTouch.value = 0.4f;
    evTouch.secondaryValue = 0.8f;
    gamepad.onGamepadEvent(evTouch);

    EXPECT_EQ(gamepad.getTouchpadState(), TouchFingerState::Down);
    EXPECT_FLOAT_EQ(gamepad.getTouchpadPosition().x, 0.4f);
    EXPECT_FLOAT_EQ(gamepad.getTouchpadPosition().y, 0.8f);
    EXPECT_EQ(gamepad.getActiveTouchCount(), 1u);
    ASSERT_NE(gamepad.getTouchContact(0u), nullptr);
    EXPECT_FLOAT_EQ(gamepad.getTouchContact(0u)->x, 0.4f);
    EXPECT_FLOAT_EQ(gamepad.getTouchContact(0u)->y, 0.8f);
}

TEST(GenericGamepadTest, SensorReadings)
{
    Gamepad gamepad;
    gamepad.attach(1u, GamepadSlot::Player1);

    event::SensorEvent ev{};
    ev.type = GamepadSensorType::Accelerometer;
    ev.side = ActuatorSide::Both;
    ev.timestampNs = 12345678ULL;
    ev.values[0] = 0.0f;
    ev.values[1] = 9.81f;
    ev.values[2] = 0.0f;

    gamepad.onSensorEvent(ev);

    const SensorReading& accel = gamepad.getAccelerometerReading();
    EXPECT_TRUE(accel.isValid);
    EXPECT_EQ(accel.timestampNs, 12345678ULL);
    EXPECT_FLOAT_EQ(accel.value.y, 9.81f);

    ev.type = GamepadSensorType::Gyroscope;
    ev.values[0] = 1.5f;
    ev.values[1] = 0.0f;
    ev.values[2] = -0.5f;

    gamepad.onSensorEvent(ev);

    const SensorReading& gyro = gamepad.getGyroscopeReading();
    EXPECT_TRUE(gyro.isValid);
    EXPECT_FLOAT_EQ(gyro.value.x, 1.5f);
    EXPECT_FLOAT_EQ(gyro.value.z, -0.5f);
}

TEST(GenericGamepadTest, BatteryAndHapticsStubs)
{
    Gamepad gamepad;
    gamepad.attach(1u, GamepadSlot::Player1);

    event::GamepadEvent ev{};
    ev.action = event::GamepadAction::BatteryUpdated;
    ev.batteryPercent = 75u;
    gamepad.onGamepadEvent(ev);

    EXPECT_EQ(gamepad.getBatteryPercent(), 75u);

    EXPECT_FALSE(gamepad.supportsRumble());
    EXPECT_FALSE(gamepad.supportsTriggerRumble());
    EXPECT_FALSE(gamepad.supportsLEDColor());
    EXPECT_FALSE(gamepad.setRumble(HapticRumbleDesc{}));
    EXPECT_FALSE(gamepad.setTriggerRumble(HapticTriggerRumbleDesc{}));
    EXPECT_FALSE(gamepad.setLEDColor(DeviceLEDColor{}));

    // Setting player LED updates logical slot
    EXPECT_FALSE(gamepad.setPlayerLED(GamepadSlot::Player3));
    EXPECT_EQ(gamepad.getSlot(), GamepadSlot::Player3);
}

TEST(GenericGamepadTest, ResetClearsAllState)
{
    Gamepad gamepad;
    gamepad.attach(5u, GamepadSlot::Player2);

    event::GamepadEvent ev{};
    ev.action = event::GamepadAction::ButtonChanged;
    ev.control.button = GamepadButton::West;
    ev.state = KeyState::Pressed;
    gamepad.onGamepadEvent(ev);

    gamepad.reset();

    EXPECT_FALSE(gamepad.isConnected());
    EXPECT_EQ(gamepad.getDeviceId(), kInvalidInputDeviceId);
    EXPECT_FALSE(gamepad.isButtonDown(GamepadButton::West));
    EXPECT_FALSE(gamepad.wasButtonJustReleased(GamepadButton::West));
}

}   // namespace gp::hal::input::generic::tests
