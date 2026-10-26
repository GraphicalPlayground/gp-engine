// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "event/data/ClipboardEvent.hpp"        // IWYU pragma: keep
#include "event/data/DropEvent.hpp"             // IWYU pragma: keep
#include "event/data/TextInputEvent.hpp"        // IWYU pragma: keep
#include "event/device/DeviceChangeEvent.hpp"   // IWYU pragma: keep
#include "event/EventCategory.hpp"
#include "event/EventHeader.hpp"
#include "event/input/GamepadEvent.hpp"
#include "event/input/KeyboardEvent.hpp"
#include "event/input/MouseEvent.hpp"
#include "event/input/PenEvent.hpp"            // IWYU pragma: keep
#include "event/input/SensorEvent.hpp"         // IWYU pragma: keep
#include "event/input/TouchEvent.hpp"          // IWYU pragma: keep
#include "event/system/ApplicationEvent.hpp"   // IWYU pragma: keep
#include "event/system/DisplayEvent.hpp"
#include "event/system/WindowEvent.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::hal::event::tests
{

static_assert(std::is_scoped_enum_v<EventCategory>, "EventCategory must be an enum class.");

TEST(EventsTest, EventCategoryEnumerators)
{
    EXPECT_EQ(static_cast<gp::UInt16>(EventCategory::None), 0u);
    EXPECT_NE(EventCategory::Window, EventCategory::Keyboard);
    EXPECT_NE(EventCategory::Mouse, EventCategory::Gamepad);
    EXPECT_EQ(static_cast<gp::UInt16>(EventCategory::Custom), 0xFFFFu);
}

TEST(EventsTest, EventHeaderDefaultState)
{
    const EventHeader header{ EventCategory::Window, static_cast<gp::UInt16>(sizeof(WindowEvent)) };
    EXPECT_EQ(header.category, EventCategory::Window);
    EXPECT_EQ(header.payloadSize, static_cast<gp::UInt16>(sizeof(WindowEvent)));
}

TEST(EventsTest, WindowEventInitialization)
{
    WindowEvent ev{};
    ev.action = WindowAction::Resized;
    ev.extent = Extent2D(1920u, 1080u);

    EXPECT_EQ(ev.action, WindowAction::Resized);
    EXPECT_EQ(ev.extent.width, 1920u);
    EXPECT_EQ(ev.extent.height, 1080u);
}

TEST(EventsTest, DisplayEventInitialization)
{
    DisplayEvent ev{};
    ev.displayId = 1u;
    ev.action = DisplayAction::Connected;
    ev.contentScale = 1.25f;

    EXPECT_EQ(ev.displayId, 1u);
    EXPECT_EQ(ev.action, DisplayAction::Connected);
    EXPECT_FLOAT_EQ(ev.contentScale, 1.25f);
}

TEST(EventsTest, InputEventsInitialization)
{
    KeyboardEvent keyEv{};
    keyEv.state = input::KeyState::Pressed;
    keyEv.scancode = input::Scancode::W;
    keyEv.keycode = input::Keycode::W;
    EXPECT_EQ(keyEv.state, input::KeyState::Pressed);
    EXPECT_EQ(keyEv.scancode, input::Scancode::W);
    EXPECT_EQ(keyEv.keycode, input::Keycode::W);

    MouseEvent mouseEv{};
    mouseEv.action = MouseAction::Moved;
    mouseEv.x = 150.0f;
    mouseEv.y = 250.0f;
    EXPECT_EQ(mouseEv.action, MouseAction::Moved);
    EXPECT_FLOAT_EQ(mouseEv.x, 150.0f);
    EXPECT_FLOAT_EQ(mouseEv.y, 250.0f);

    GamepadEvent padEv{};
    padEv.action = GamepadAction::ButtonChanged;
    padEv.control.button = input::GamepadButton::South;
    padEv.state = input::KeyState::Pressed;
    EXPECT_EQ(padEv.action, GamepadAction::ButtonChanged);
    EXPECT_EQ(padEv.control.button, input::GamepadButton::South);
    EXPECT_EQ(padEv.state, input::KeyState::Pressed);
}

}   // namespace gp::hal::event::tests
