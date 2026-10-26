// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "input/GenericSpatialDevice.hpp"
#include <gtest/gtest.h>

namespace gp::hal::input::generic::tests
{

TEST(GenericSpatialDeviceTest, DefaultState)
{
    SpatialDevice spatial;

    EXPECT_EQ(spatial.getDeviceId(), 1u);
    EXPECT_EQ(spatial.getDeviceType(), InputDeviceType::Touch);
    EXPECT_EQ(spatial.getDeviceName(), "System Spatial Touch/Pen Device");
    EXPECT_TRUE(spatial.isConnected());
    EXPECT_EQ(spatial.getBatteryPercent(), 100u);
    EXPECT_EQ(spatial.getActiveTouchCount(), 0u);
    EXPECT_EQ(spatial.getTouchContact(0u), nullptr);

    const PenStateSnapshot& pen = spatial.getPenState();
    EXPECT_FALSE(pen.isInProximity);
    EXPECT_EQ(pen.tipState, ButtonStateFlags::None);
    EXPECT_EQ(pen.heldButtons, PenButton::None);
}

TEST(GenericSpatialDeviceTest, MultiTouchLifecycle)
{
    SpatialDevice spatial;

    // Contact 0 Down
    event::TouchEvent ev{};
    ev.touchDeviceId = 1u;
    ev.fingerId = 10u;
    ev.windowId = 1u;
    ev.x = 200.0f;
    ev.y = 300.0f;
    ev.deltaX = 0.0f;
    ev.deltaY = 0.0f;
    ev.pressure = 0.8f;
    ev.state = TouchFingerState::Down;
    ev.isCancelled = 0u;

    spatial.onTouchEvent(ev);

    EXPECT_EQ(spatial.getActiveTouchCount(), 1u);
    const TouchContactPoint* contact0 = spatial.getTouchContact(0u);
    ASSERT_NE(contact0, nullptr);
    EXPECT_EQ(contact0->fingerId, 10u);
    EXPECT_FLOAT_EQ(contact0->x, 200.0f);
    EXPECT_FLOAT_EQ(contact0->y, 300.0f);
    EXPECT_FLOAT_EQ(contact0->pressure, 0.8f);
    EXPECT_TRUE((contact0->flags & ButtonStateFlags::Held) != ButtonStateFlags::None);
    EXPECT_TRUE((contact0->flags & ButtonStateFlags::PressedThisFrame) != ButtonStateFlags::None);

    // Motion on contact 0
    ev.state = TouchFingerState::Motion;
    ev.x = 210.0f;
    ev.y = 305.0f;
    ev.deltaX = 10.0f;
    ev.deltaY = 5.0f;
    spatial.onTouchEvent(ev);

    EXPECT_FLOAT_EQ(contact0->x, 210.0f);
    EXPECT_FLOAT_EQ(contact0->deltaX, 10.0f);
    EXPECT_FLOAT_EQ(contact0->deltaY, 5.0f);

    // beginFrame clears delta and pressed edge
    spatial.beginFrame();
    EXPECT_FLOAT_EQ(contact0->deltaX, 0.0f);
    EXPECT_FLOAT_EQ(contact0->deltaY, 0.0f);
    EXPECT_FALSE((contact0->flags & ButtonStateFlags::PressedThisFrame) != ButtonStateFlags::None);
    EXPECT_TRUE((contact0->flags & ButtonStateFlags::Held) != ButtonStateFlags::None);

    // Up transition
    ev.state = TouchFingerState::Up;
    spatial.onTouchEvent(ev);

    EXPECT_TRUE((contact0->flags & ButtonStateFlags::ReleasedThisFrame) != ButtonStateFlags::None);
    EXPECT_FALSE((contact0->flags & ButtonStateFlags::Held) != ButtonStateFlags::None);
    EXPECT_EQ(spatial.getActiveTouchCount(), 0u);

    // Next frame reclaims inactive contact slot
    spatial.beginFrame();
    EXPECT_EQ(spatial.getTouchContact(0u), nullptr);
}

TEST(GenericSpatialDeviceTest, PenStylusProximityAndMotion)
{
    SpatialDevice spatial;

    // Proximity In switches device type to Pen
    event::PenEvent ev{};
    ev.action = event::PenAction::ProximityIn;
    ev.windowId = 1u;
    ev.penId = 100u;
    spatial.onPenEvent(ev);

    EXPECT_TRUE(spatial.getPenState().isInProximity);
    EXPECT_EQ(spatial.getDeviceType(), InputDeviceType::Pen);

    // Tip Down
    ev.action = event::PenAction::TipChanged;
    ev.state = KeyState::Pressed;
    ev.x = 400.0f;
    ev.y = 500.0f;
    ev.buttons = PenButton::Barrel;
    ev.isEraser = 0u;
    spatial.onPenEvent(ev);

    EXPECT_TRUE((spatial.getPenState().tipState & ButtonStateFlags::Held) != ButtonStateFlags::None);
    EXPECT_TRUE((spatial.getPenState().tipState & ButtonStateFlags::PressedThisFrame) != ButtonStateFlags::None);
    EXPECT_EQ(spatial.getPenState().heldButtons, PenButton::Barrel);

    // Pen Move
    ev.action = event::PenAction::Moved;
    ev.x = 415.0f;
    ev.y = 520.0f;
    ev.axes[static_cast<gp::UInt8>(PenAxis::Pressure)] = 0.75f;
    spatial.onPenEvent(ev);

    EXPECT_FLOAT_EQ(spatial.getPenState().deltaX, 15.0f);
    EXPECT_FLOAT_EQ(spatial.getPenState().deltaY, 20.0f);
    EXPECT_FLOAT_EQ(spatial.getPenState().axes[static_cast<gp::UInt8>(PenAxis::Pressure)], 0.75f);

    // Proximity Out synthesizes release and switches back to Touch
    ev.action = event::PenAction::ProximityOut;
    spatial.onPenEvent(ev);

    EXPECT_FALSE(spatial.getPenState().isInProximity);
    EXPECT_EQ(spatial.getDeviceType(), InputDeviceType::Touch);
    EXPECT_TRUE((spatial.getPenState().tipState & ButtonStateFlags::ReleasedThisFrame) != ButtonStateFlags::None);
    EXPECT_EQ(spatial.getPenState().heldButtons, PenButton::None);
}

TEST(GenericSpatialDeviceTest, SensorReadings)
{
    SpatialDevice spatial;

    EXPECT_FALSE(spatial.hasSensor(GamepadSensorType::Accelerometer));
    EXPECT_FALSE(spatial.hasSensor(GamepadSensorType::Gyroscope));

    event::SensorEvent ev{};
    ev.type = GamepadSensorType::Accelerometer;
    ev.timestampNs = 999999ULL;
    ev.values[0] = 0.1f;
    ev.values[1] = 0.2f;
    ev.values[2] = 9.8f;
    spatial.onSensorEvent(ev);

    EXPECT_TRUE(spatial.hasSensor(GamepadSensorType::Accelerometer));
    EXPECT_TRUE(spatial.isSensorEnabled(GamepadSensorType::Accelerometer));
    EXPECT_FLOAT_EQ(spatial.getAccelerometerReading().value.z, 9.8f);

    EXPECT_FALSE(spatial.setSensorEnabled(GamepadSensorType::Accelerometer, true));
}

TEST(GenericSpatialDeviceTest, ReleaseAllInputsAndReset)
{
    SpatialDevice spatial;

    // Add touch contact
    event::TouchEvent ev{};
    ev.touchDeviceId = 1u;
    ev.fingerId = 1u;
    ev.x = 10.0f;
    ev.y = 10.0f;
    ev.pressure = 1.0f;
    ev.state = TouchFingerState::Down;
    spatial.onTouchEvent(ev);

    spatial.releaseAllInputs();

    const TouchContactPoint* contact = spatial.getTouchContact(0u);
    ASSERT_NE(contact, nullptr);
    EXPECT_FALSE((contact->flags & ButtonStateFlags::Held) != ButtonStateFlags::None);
    EXPECT_TRUE((contact->flags & ButtonStateFlags::ReleasedThisFrame) != ButtonStateFlags::None);
    EXPECT_FLOAT_EQ(contact->pressure, 0.0f);

    spatial.reset();

    EXPECT_EQ(spatial.getActiveTouchCount(), 0u);
    EXPECT_EQ(spatial.getTouchContact(0u), nullptr);
    EXPECT_FALSE(spatial.hasSensor(GamepadSensorType::Accelerometer));
}

}   // namespace gp::hal::input::generic::tests
