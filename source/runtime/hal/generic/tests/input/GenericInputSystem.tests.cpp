// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "input/GenericInputSystem.hpp"
#include <gtest/gtest.h>

namespace gp::hal::input::generic::tests
{

TEST(GenericInputSystemTest, DefaultState)
{
    InputSystem inputSystem;

    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Keyboard);
    EXPECT_EQ(inputSystem.getConnectedGamepadCount(), 0u);

    // Verify subsystem accessors
    EXPECT_EQ(inputSystem.getKeyboard().getDeviceType(), InputDeviceType::Keyboard);
    EXPECT_EQ(inputSystem.getMouse().getDeviceType(), InputDeviceType::Mouse);
    EXPECT_EQ(inputSystem.getSpatialDevice().getActiveTouchCount(), 0u);

    for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
    {
        const IGamepad* pad = inputSystem.getGamepad(static_cast<GamepadSlot>(i));
        ASSERT_NE(pad, nullptr);
        EXPECT_FALSE(pad->isConnected());
    }

    const auto invalidSlot = static_cast<GamepadSlot>(99);
    EXPECT_EQ(inputSystem.getGamepad(invalidSlot), nullptr);
}

TEST(GenericInputSystemTest, ActiveDeviceHysteresis)
{
    InputSystem inputSystem;

    // Default is Keyboard
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Keyboard);

    // Sub-threshold mouse movement (< 4.0f squared distance) should NOT switch active device
    event::MouseEvent mouseMoveSmall{};
    mouseMoveSmall.action = event::MouseAction::Moved;
    mouseMoveSmall.deltaX = 1.0f;
    mouseMoveSmall.deltaY = 1.0f;
    inputSystem.onMouseEvent(mouseMoveSmall);
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Keyboard);

    // Above-threshold mouse movement (3.0^2 + 0.0^2 = 9.0 >= 4.0) switches to Mouse
    event::MouseEvent mouseMoveLarge{};
    mouseMoveLarge.action = event::MouseAction::Moved;
    mouseMoveLarge.deltaX = 3.0f;
    mouseMoveLarge.deltaY = 0.0f;
    inputSystem.onMouseEvent(mouseMoveLarge);
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Mouse);

    // Keyboard press switches back to Keyboard
    event::KeyboardEvent keyPress{};
    keyPress.scancode = Scancode::Space;
    keyPress.keycode = Keycode::Space;
    keyPress.state = KeyState::Pressed;
    inputSystem.onKeyboardEvent(keyPress);
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Keyboard);

    // Touch event switches to Touch
    event::TouchEvent touchEv{};
    touchEv.touchDeviceId = 1u;
    touchEv.fingerId = 1u;
    touchEv.state = TouchFingerState::Down;
    inputSystem.onTouchEvent(touchEv);
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Touch);

    // Pen event switches to Pen
    event::PenEvent penEv{};
    penEv.action = event::PenAction::ProximityIn;
    inputSystem.onPenEvent(penEv);
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Pen);

    // Gamepad button press switches to Gamepad
    event::GamepadEvent padButton{};
    padButton.deviceId = 100u;
    padButton.action = event::GamepadAction::ButtonChanged;
    padButton.control.button = GamepadButton::South;
    padButton.state = KeyState::Pressed;
    inputSystem.onGamepadEvent(padButton);
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Gamepad);
}

TEST(GenericInputSystemTest, GamepadHotplugAndRouting)
{
    InputSystem inputSystem;

    // Connect Gamepad 10u
    event::DeviceChangeEvent connectEv{};
    connectEv.domain = event::DeviceDomain::Input;
    connectEv.inputType = InputDeviceType::Gamepad;
    connectEv.action = event::DeviceAction::Connected;
    connectEv.deviceId = 10u;

    inputSystem.onDeviceChangeEvent(connectEv);

    EXPECT_EQ(inputSystem.getConnectedGamepadCount(), 1u);
    IGamepad* pad0 = inputSystem.getGamepad(GamepadSlot::Player1);
    ASSERT_NE(pad0, nullptr);
    EXPECT_TRUE(pad0->isConnected());
    EXPECT_EQ(pad0->getDeviceId(), 10u);

    // Send button event to connected gamepad
    event::GamepadEvent padEv{};
    padEv.deviceId = 10u;
    padEv.action = event::GamepadAction::ButtonChanged;
    padEv.control.button = GamepadButton::RightShoulder;
    padEv.state = KeyState::Pressed;
    inputSystem.onGamepadEvent(padEv);

    EXPECT_TRUE(inputSystem.isGamepadButtonDown(GamepadButton::RightShoulder, GamepadSlot::Player1));

    // Disconnect event
    event::DeviceChangeEvent disconnectEv{};
    disconnectEv.domain = event::DeviceDomain::Input;
    disconnectEv.inputType = InputDeviceType::Gamepad;
    disconnectEv.action = event::DeviceAction::Disconnected;
    disconnectEv.deviceId = 10u;

    inputSystem.onDeviceChangeEvent(disconnectEv);

    EXPECT_EQ(inputSystem.getConnectedGamepadCount(), 0u);
    EXPECT_FALSE(pad0->isConnected());
}

TEST(GenericInputSystemTest, SensorRouting)
{
    InputSystem inputSystem;

    // Connect gamepad with ID 25u
    event::DeviceChangeEvent connectEv{};
    connectEv.domain = event::DeviceDomain::Input;
    connectEv.inputType = InputDeviceType::Gamepad;
    connectEv.action = event::DeviceAction::Connected;
    connectEv.deviceId = 25u;
    inputSystem.onDeviceChangeEvent(connectEv);

    // Sensor event targeted to gamepad device ID 25u
    event::SensorEvent gamepadSensor{};
    gamepadSensor.deviceId = 25u;
    gamepadSensor.type = GamepadSensorType::Accelerometer;
    gamepadSensor.values[0] = 1.0f;
    gamepadSensor.values[1] = 2.0f;
    gamepadSensor.values[2] = 3.0f;

    inputSystem.onSensorEvent(gamepadSensor);

    IGamepad* pad = inputSystem.getGamepad(GamepadSlot::Player1);
    ASSERT_NE(pad, nullptr);
    EXPECT_FLOAT_EQ(pad->getAccelerometerReading().value.x, 1.0f);
    EXPECT_FLOAT_EQ(pad->getAccelerometerReading().value.y, 2.0f);
    EXPECT_FLOAT_EQ(pad->getAccelerometerReading().value.z, 3.0f);

    // Standalone sensor event targeted to device ID 99u (routes to spatial device)
    event::SensorEvent spatialSensor{};
    spatialSensor.deviceId = 99u;
    spatialSensor.type = GamepadSensorType::Gyroscope;
    spatialSensor.values[0] = 0.5f;
    spatialSensor.values[1] = 0.25f;
    spatialSensor.values[2] = 0.125f;

    inputSystem.onSensorEvent(spatialSensor);

    EXPECT_FLOAT_EQ(inputSystem.getSpatialDevice().getGyroscopeReading().value.x, 0.5f);
    EXPECT_FLOAT_EQ(inputSystem.getSpatialDevice().getGyroscopeReading().value.y, 0.25f);
}

TEST(GenericInputSystemTest, FrameLifecycleCoordination)
{
    InputSystem inputSystem;

    // Press a key and a mouse button
    event::KeyboardEvent keyEv{};
    keyEv.scancode = Scancode::Z;
    keyEv.keycode = Keycode::Z;
    keyEv.state = KeyState::Pressed;
    inputSystem.onKeyboardEvent(keyEv);

    event::MouseEvent mouseEv{};
    mouseEv.action = event::MouseAction::ButtonChanged;
    mouseEv.button = MouseButton::Left;
    mouseEv.state = KeyState::Pressed;
    inputSystem.onMouseEvent(mouseEv);

    EXPECT_TRUE(inputSystem.isScancodeDown(Scancode::Z));
    EXPECT_TRUE(inputSystem.isMouseButtonDown(MouseButton::Left));
    EXPECT_TRUE(inputSystem.wasScancodeJustPressed(Scancode::Z));

    // beginFrame propagates
    inputSystem.beginFrame();
    EXPECT_TRUE(inputSystem.isScancodeDown(Scancode::Z));
    EXPECT_TRUE(inputSystem.isMouseButtonDown(MouseButton::Left));
    EXPECT_FALSE(inputSystem.wasScancodeJustPressed(Scancode::Z));

    // releaseAllInputs propagates
    inputSystem.releaseAllInputs();
    EXPECT_FALSE(inputSystem.isScancodeDown(Scancode::Z));
    EXPECT_FALSE(inputSystem.isMouseButtonDown(MouseButton::Left));

    // reset restores initial active device type
    inputSystem.onMouseEvent(mouseEv);
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Mouse);
    inputSystem.reset();
    EXPECT_EQ(inputSystem.getActiveDeviceType(), InputDeviceType::Keyboard);
}

}   // namespace gp::hal::input::generic::tests
