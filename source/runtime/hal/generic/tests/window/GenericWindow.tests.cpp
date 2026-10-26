// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "window/GenericWindow.hpp"
#include <gtest/gtest.h>

namespace gp::hal::generic::tests
{

TEST(GenericWindowTest, DefaultConstruction)
{
    Window window;

    EXPECT_EQ(window.getId(), kInvalidWindowId);
    EXPECT_FALSE(window.isOpen());
    EXPECT_FALSE(window.isValid());
    EXPECT_FALSE(window.isCloseRequested());
    EXPECT_EQ(window.getTitle(), "Graphical Playground");
    EXPECT_EQ(window.getSize().width, 1280u);
    EXPECT_EQ(window.getSize().height, 720u);
}

TEST(GenericWindowTest, ParameterizedConstruction)
{
    WindowDesc desc{};
    desc.title = "Custom Game Window";
    desc.size = Extent2D(1920u, 1080u);
    desc.startVisible = true;
    desc.startFocused = true;

    Window window(desc);

    EXPECT_EQ(window.getTitle(), "Custom Game Window");
    EXPECT_EQ(window.getSize().width, 1920u);
    EXPECT_EQ(window.getSize().height, 1080u);
    EXPECT_TRUE(window.isFocused());
}

TEST(GenericWindowTest, GeometryAccessorsAndSetters)
{
    Window window;

    window.setPosition(Point2D(200, 300));
    EXPECT_EQ(window.getPosition().x, 200);
    EXPECT_EQ(window.getPosition().y, 300);

    window.setSize(Extent2D(800u, 600u));
    EXPECT_EQ(window.getSize().width, 800u);
    EXPECT_EQ(window.getSize().height, 600u);

    window.setMinimumSize(Extent2D(400u, 300u));
    EXPECT_EQ(window.getMinimumSize().width, 400u);
    EXPECT_EQ(window.getMinimumSize().height, 300u);

    window.setMaximumSize(Extent2D(2560u, 1440u));
    EXPECT_EQ(window.getMaximumSize().width, 2560u);
    EXPECT_EQ(window.getMaximumSize().height, 1440u);

    window.setOpacity(0.85f);
    EXPECT_FLOAT_EQ(window.getOpacity(), 0.85f);
}

TEST(GenericWindowTest, TitleMutation)
{
    Window window;

    window.setTitle("Updated Title");
    EXPECT_EQ(window.getTitle(), "Updated Title");
}

TEST(GenericWindowTest, StateTransitions)
{
    Window window;
    window.open();

    window.show();
    EXPECT_TRUE(window.isOpen());
    EXPECT_TRUE(window.isVisible());

    window.minimize();
    EXPECT_TRUE(window.isMinimized());

    window.maximize();
    EXPECT_TRUE(window.isMaximized());
    EXPECT_FALSE(window.isMinimized());

    window.restore();
    EXPECT_FALSE(window.isMaximized());
    EXPECT_FALSE(window.isMinimized());

    window.hide();
    EXPECT_FALSE(window.isVisible());

    window.requestClose();
    EXPECT_TRUE(window.isCloseRequested());

    window.clearCloseRequest();
    EXPECT_FALSE(window.isCloseRequested());

    window.close();
    EXPECT_FALSE(window.isOpen());
}

TEST(GenericWindowTest, OnWindowEventUpdatesState)
{
    WindowDesc desc{};
    Window window(desc);
    window.show();

    event::WindowEvent movedEv{};
    movedEv.action = event::WindowAction::Moved;
    movedEv.position = Point2D(400, 500);
    window.onWindowEvent(movedEv);
    EXPECT_EQ(window.getPosition().x, 400);
    EXPECT_EQ(window.getPosition().y, 500);

    event::WindowEvent resizedEv{};
    resizedEv.action = event::WindowAction::Resized;
    resizedEv.extent = Extent2D(1024u, 768u);
    window.onWindowEvent(resizedEv);
    EXPECT_EQ(window.getSize().width, 1024u);
    EXPECT_EQ(window.getSize().height, 768u);

    event::WindowEvent focusEv{};
    focusEv.action = event::WindowAction::FocusGained;
    window.onWindowEvent(focusEv);
    EXPECT_TRUE(window.isFocused());

    focusEv.action = event::WindowAction::FocusLost;
    window.onWindowEvent(focusEv);
    EXPECT_FALSE(window.isFocused());
}

} // namespace gp::hal::generic::tests

