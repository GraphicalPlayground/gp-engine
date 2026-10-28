// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "display/GenericDisplay.hpp"
#include <gtest/gtest.h>

namespace gp::hal::generic::tests
{

TEST(GenericDisplayTest, DefaultConstruction)
{
    const Display display;

    EXPECT_EQ(display.getId(), kInvalidDisplayId);
    EXPECT_EQ(display.getName(), "Generic Display");
    EXPECT_FALSE(display.isConnected());
    EXPECT_FALSE(display.isValid());
    EXPECT_FALSE(display.isPrimary());
    EXPECT_FALSE(display.isHDREnabled());
    EXPECT_EQ(display.getSupportedModeCount(), 0u);
}

TEST(GenericDisplayTest, DescriptorInitialization)
{
    DisplayDesc desc{};
    desc.id = 42u;
    desc.name = "UltraWide Monitor";
    desc.bounds = DisplayRect(0, 0, 3440u, 1440u);
    desc.workArea = DisplayRect(0, 0, 3440u, 1400u);
    desc.contentScale = 1.25f;
    desc.orientation = DisplayOrientation::Landscape;
    desc.flags = DisplayFlags::Connected | DisplayFlags::Primary;
    desc.currentMode = DisplayMode(Extent2D(3440u, 1440u), 144.0f, 1.25f, DisplayPixelFormat::RGBA8888);
    desc.desktopMode = desc.currentMode;

    const Display display(desc);

    EXPECT_EQ(display.getId(), 42u);
    EXPECT_EQ(display.getName(), "UltraWide Monitor");
    EXPECT_TRUE(display.isConnected());
    EXPECT_TRUE(display.isValid());
    EXPECT_TRUE(display.isPrimary());
    EXPECT_FLOAT_EQ(display.getContentScale(), 1.25f);
    EXPECT_EQ(display.getOrientation(), DisplayOrientation::Landscape);
    EXPECT_EQ(display.getBounds(), desc.bounds);
    EXPECT_EQ(display.getWorkArea(), desc.workArea);
    EXPECT_EQ(display.getSupportedModeCount(), 1u);
}

TEST(GenericDisplayTest, FindClosestMode)
{
    DisplayDesc desc{};
    desc.id = 1u;
    desc.flags = DisplayFlags::Connected;
    desc.currentMode = DisplayMode(Extent2D(1920u, 1080u), 60.0f);
    desc.desktopMode = desc.currentMode;

    Display display(desc);

    DisplayMode resolved{};
    EXPECT_TRUE(display.findClosestMode(Extent2D(1920u, 1080u), 60.0f, resolved));
    EXPECT_EQ(resolved.resolution.width, 1920u);
    EXPECT_EQ(resolved.resolution.height, 1080u);
    EXPECT_FLOAT_EQ(resolved.refreshRate, 60.0f);
}

TEST(GenericDisplayTest, SetPrimaryFlag)
{
    Display display;
    EXPECT_FALSE(display.isPrimary());

    display.setPrimary(true);
    EXPECT_TRUE(display.isPrimary());

    display.setPrimary(false);
    EXPECT_FALSE(display.isPrimary());
}

TEST(GenericDisplayTest, OnDisplayEventUpdatesShadowState)
{
    DisplayDesc desc{};
    desc.id = 5u;
    desc.flags = DisplayFlags::Connected;
    desc.bounds = DisplayRect(0, 0, 1920u, 1080u);

    Display display(desc);

    // Moved event
    event::DisplayEvent movedEv{};
    movedEv.displayId = 5u;
    movedEv.action = event::DisplayAction::Moved;
    movedEv.position = Point2D(1920, 0);
    display.onDisplayEvent(movedEv);
    EXPECT_EQ(display.getBounds().origin.x, 1920);

    // Resized event
    event::DisplayEvent resizedEv{};
    resizedEv.displayId = 5u;
    resizedEv.action = event::DisplayAction::Resized;
    resizedEv.extent = Extent2D(2560u, 1440u);
    display.onDisplayEvent(resizedEv);
    EXPECT_EQ(display.getBounds().extent.width, 2560u);

    // Scale changed
    event::DisplayEvent scaleEv{};
    scaleEv.displayId = 5u;
    scaleEv.action = event::DisplayAction::ContentScaleChanged;
    scaleEv.contentScale = 1.5f;
    display.onDisplayEvent(scaleEv);
    EXPECT_FLOAT_EQ(display.getContentScale(), 1.5f);

    // HDR state changed
    event::DisplayEvent hdrEv{};
    hdrEv.displayId = 5u;
    hdrEv.action = event::DisplayAction::HDRStateChanged;
    hdrEv.isHDREnabled = 1u;
    display.onDisplayEvent(hdrEv);
    EXPECT_TRUE(display.isHDREnabled());

    // Disconnected event
    event::DisplayEvent discEv{};
    discEv.displayId = 5u;
    discEv.action = event::DisplayAction::Disconnected;
    display.onDisplayEvent(discEv);
    EXPECT_FALSE(display.isConnected());
}

TEST(GenericDisplayTest, MoveConstructionAndAssignment)
{
    DisplayDesc desc{};
    desc.id = 7u;
    desc.name = "Source Display";
    desc.flags = DisplayFlags::Connected;

    Display src(desc);
    Display moved(std::move(src));

    EXPECT_EQ(moved.getId(), 7u);
    EXPECT_EQ(moved.getName(), "Source Display");
    EXPECT_TRUE(moved.isConnected());

    // Source must be reset
    EXPECT_EQ(src.getId(), kInvalidDisplayId);

    Display assigned;
    assigned = std::move(moved);
    EXPECT_EQ(assigned.getId(), 7u);
    EXPECT_EQ(assigned.getName(), "Source Display");
    EXPECT_TRUE(assigned.isConnected());
}

}   // namespace gp::hal::generic::tests
