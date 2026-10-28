// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "backend/GenericApplicationBackend.hpp"
#include <gtest/gtest.h>

namespace gp::hal::generic::tests
{

/// @brief Test harness exposing protected backend display registration methods for unit verification.
class TestApplicationBackend : public ApplicationBackend
{
public:
    using ApplicationBackend::registerDisplay;
    using ApplicationBackend::unregisterDisplayById;
};

TEST(GenericApplicationBackendTest, UninitializedState)
{
    ApplicationBackend backend;

    EXPECT_FALSE(backend.isInitialized());
    EXPECT_EQ(backend.getBackendName(), "Generic");
    EXPECT_EQ(backend.getDisplayCount(), 0u);
    EXPECT_EQ(backend.createInputSystem(), nullptr);

    WindowDesc winDesc{};
    EXPECT_EQ(backend.createWindow(winDesc), nullptr);

    EventPumpStats stats{};
    gp::SPSCByteRingBuffer dummyBuffer;
    EXPECT_EQ(backend.pumpEvents(dummyBuffer, &stats), BackendResult::NotInitialized);
    EXPECT_FALSE(backend.waitEventsTimeout(10u));

    // wakeEventLoop should execute safely without crash
    backend.wakeEventLoop();

    // Dialog and Clipboard accessors are available even in uninitialized state
    EXPECT_EQ(backend.getClipboard().getSequenceNumber(), 0u);
}

TEST(GenericApplicationBackendTest, InitializationAndShutdownLifecycle)
{
    ApplicationBackend backend;

    ApplicationBackendDesc desc{};
    desc.appName = "Unit Test Application";

    EXPECT_EQ(backend.initialize(desc), BackendResult::Success);
    EXPECT_TRUE(backend.isInitialized());

    // Double initialization protection
    EXPECT_EQ(backend.initialize(desc), BackendResult::AlreadyInitialized);

    // Initializing with no physical monitors provides synthetic headless fallback display
    EXPECT_EQ(backend.getDisplayCount(), 1u);
    const IDisplay* primary = backend.getPrimaryDisplay();
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->getId(), ApplicationBackend::kHeadlessDisplayId);
    EXPECT_EQ(primary->getBounds().extent.width, 1920u);
    EXPECT_EQ(primary->getBounds().extent.height, 1080u);

    // Component factories succeed when initialized
    auto inputSystem = backend.createInputSystem();
    EXPECT_NE(inputSystem, nullptr);

    auto window = backend.createWindow(WindowDesc{});
    EXPECT_NE(window, nullptr);

    // Event pump succeeds when initialized
    EventPumpStats stats{};
    gp::SPSCByteRingBuffer dummyBuffer;
    EXPECT_EQ(backend.pumpEvents(dummyBuffer, &stats), BackendResult::Success);

    // Shutdown
    backend.shutdown();
    EXPECT_FALSE(backend.isInitialized());
    EXPECT_EQ(backend.getDisplayCount(), 0u);
}

TEST(GenericApplicationBackendTest, SpatialDisplayPointLookup)
{
    TestApplicationBackend backend;
    EXPECT_EQ(backend.initialize(ApplicationBackendDesc{}), BackendResult::Success);

    // Create two mock monitors side-by-side: Monitor 1 at [0, 0, 1920, 1080], Monitor 2 at [1920, 0, 1920, 1080]
    DisplayDesc desc1{};
    desc1.id = 1u;
    desc1.name = "Left Monitor";
    desc1.bounds = DisplayRect(Point2D(0, 0), Extent2D(1920u, 1080u));
    desc1.currentMode = DisplayMode{ Extent2D(1920u, 1080u), 60.0f, 1.0f, DisplayPixelFormat::RGBA8888 };
    desc1.flags = DisplayFlags::Primary | DisplayFlags::Connected;
    Display disp1(desc1);

    DisplayDesc desc2{};
    desc2.id = 2u;
    desc2.name = "Right Monitor";
    desc2.bounds = DisplayRect(Point2D(1920, 0), Extent2D(1920u, 1080u));
    desc2.currentMode = DisplayMode{ Extent2D(1920u, 1080u), 144.0f, 1.0f, DisplayPixelFormat::RGBA8888 };
    desc2.flags = DisplayFlags::Connected;
    Display disp2(desc2);

    EXPECT_TRUE(backend.registerDisplay(&disp1));
    EXPECT_TRUE(backend.registerDisplay(&disp2));
    EXPECT_EQ(backend.getDisplayCount(), 2u);

    // Exact containment lookup
    const IDisplay* foundLeft = backend.getDisplayForPoint(Point2D(500, 500));
    ASSERT_NE(foundLeft, nullptr);
    EXPECT_EQ(foundLeft->getId(), 1u);

    const IDisplay* foundRight = backend.getDisplayForPoint(Point2D(2500, 500));
    ASSERT_NE(foundRight, nullptr);
    EXPECT_EQ(foundRight->getId(), 2u);

    // Point outside all monitors clamps to closest display
    const IDisplay* foundOutsideRight = backend.getDisplayForPoint(Point2D(5000, 500));
    ASSERT_NE(foundOutsideRight, nullptr);
    EXPECT_EQ(foundOutsideRight->getId(), 2u);

    backend.shutdown();
}

TEST(GenericApplicationBackendTest, SpatialDisplayRectOverlapLookup)
{
    TestApplicationBackend backend;
    EXPECT_EQ(backend.initialize(ApplicationBackendDesc{}), BackendResult::Success);

    DisplayDesc desc1{};
    desc1.id = 10u;
    desc1.bounds = DisplayRect(Point2D(0, 0), Extent2D(1920u, 1080u));
    desc1.flags = DisplayFlags::Primary | DisplayFlags::Connected;
    Display disp1(desc1);

    DisplayDesc desc2{};
    desc2.id = 20u;
    desc2.bounds = DisplayRect(Point2D(1920, 0), Extent2D(1920u, 1080u));
    desc2.flags = DisplayFlags::Connected;
    Display disp2(desc2);

    backend.registerDisplay(&disp1);
    backend.registerDisplay(&disp2);

    // Window spanning both monitors with 75% on Monitor 2 (e.g. from 1800 to 2600, overlap with disp1 is 120px, with
    // disp2 is 680px)
    DisplayRect spanningRect(Point2D(1800, 100), Extent2D(800u, 600u));
    const IDisplay* bestDisplay = backend.getDisplayForRect(spanningRect);
    ASSERT_NE(bestDisplay, nullptr);
    EXPECT_EQ(bestDisplay->getId(), 20u);

    backend.shutdown();
}

TEST(GenericApplicationBackendTest, UnregisterRestoresHeadlessFallback)
{
    TestApplicationBackend backend;
    EXPECT_EQ(backend.initialize(ApplicationBackendDesc{}), BackendResult::Success);

    DisplayDesc desc{};
    desc.id = 100u;
    desc.bounds = DisplayRect(Point2D(0, 0), Extent2D(1920u, 1080u));
    desc.flags = DisplayFlags::Primary | DisplayFlags::Connected;
    Display physical(desc);

    backend.registerDisplay(&physical);
    EXPECT_EQ(backend.getDisplayCount(), 1u);
    EXPECT_EQ(backend.getPrimaryDisplay()->getId(), 100u);

    // Unregister the only physical display
    backend.unregisterDisplayById(100u);

    // Table automatically restores the synthetic headless fallback display
    EXPECT_EQ(backend.getDisplayCount(), 1u);
    EXPECT_EQ(backend.getPrimaryDisplay()->getId(), ApplicationBackend::kHeadlessDisplayId);

    backend.shutdown();
}

TEST(GenericApplicationBackendTest, DisplayEventNotification)
{
    TestApplicationBackend backend;
    EXPECT_EQ(backend.initialize(ApplicationBackendDesc{}), BackendResult::Success);

    DisplayDesc desc{};
    desc.id = 77u;
    desc.bounds = DisplayRect(Point2D(0, 0), Extent2D(1920u, 1080u));
    desc.flags = DisplayFlags::Primary | DisplayFlags::Connected;
    Display physical(desc);

    backend.registerDisplay(&physical);

    // Send geometry move event
    event::DisplayEvent ev{};
    ev.displayId = 77u;
    ev.action = event::DisplayAction::Moved;
    ev.position = Point2D(100, 50);

    backend.notifyDisplayEvent(ev);

    EXPECT_EQ(physical.getBounds().origin.x, 100);
    EXPECT_EQ(physical.getBounds().origin.y, 50);

    // Disconnect event unregisters display
    ev.action = event::DisplayAction::Disconnected;
    backend.notifyDisplayEvent(ev);

    EXPECT_FALSE(physical.isConnected());
    EXPECT_EQ(backend.findDisplayById(77u), nullptr);
    EXPECT_EQ(backend.getPrimaryDisplay()->getId(), ApplicationBackend::kHeadlessDisplayId);

    backend.shutdown();
}

}   // namespace gp::hal::generic::tests
