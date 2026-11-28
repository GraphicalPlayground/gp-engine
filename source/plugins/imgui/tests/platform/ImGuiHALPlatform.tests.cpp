// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platform/ImGuiHALPlatform.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "event/data/TextInputEvent.hpp"
#include "event/input/KeyboardEvent.hpp"
#include "event/input/MouseEvent.hpp"
#include "event/system/WindowEvent.hpp"
#include "imgui.h"
#include "input/InputEnums.hpp"
#include <gtest/gtest.h>

namespace gp::imgui::tests
{

namespace
{

/// @brief Headless fixture: a private ImGui context driven by an `ImGuiHALPlatform` without a window.
/// @details With no window the platform uses its display override, so the whole input path (HAL event -> ImGui IO
///          queue -> `NewFrame()` state) is verifiable without SDL or a GPU.
class ImGuiHALPlatformTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_context = ImGui::CreateContext();
        ImGui::SetCurrentContext(m_context);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        // No renderer in this test: claim the modern texture protocol so NewFrame() does not require a built atlas.
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        ASSERT_TRUE(m_platform.initialize(nullptr));
        m_platform.setDisplayOverride(1280.0f, 720.0f, 2.0f);
    }

    virtual void TearDown() override
    {
        if (m_isFrameOpen)
        {
            ImGui::EndFrame();
        }
        m_platform.shutdown();
        ImGui::DestroyContext(m_context);
        m_context = nullptr;
    }

    /// @brief Runs `newFrame()` + `ImGui::NewFrame()` so queued input events are applied to IO state.
    void beginFrame()
    {
        m_platform.newFrame(1.0f / 60.0f);
        ImGui::NewFrame();
        m_isFrameOpen = true;
    }

protected:
    ImGuiContext* m_context{ nullptr };
    ImGuiHALPlatform m_platform;
    bool m_isFrameOpen{ false };
};

}   // namespace

TEST(ImGuiHALPlatformLifecycleTest, InitializeRequiresCurrentContext)
{
    ImGui::SetCurrentContext(nullptr);
    ImGuiHALPlatform platform;

    EXPECT_FALSE(platform.initialize(nullptr));
    EXPECT_FALSE(platform.isInitialized());
}

TEST(ImGuiHALPlatformLifecycleTest, EventsAreIgnoredWhenUninitialized)
{
    ImGuiHALPlatform platform;
    gp::hal::event::MouseEvent mouse{};
    mouse.action = gp::hal::event::MouseAction::Moved;

    EXPECT_FALSE(platform.processMouseEvent(mouse));
    EXPECT_FALSE(platform.processKeyboardEvent(gp::hal::event::KeyboardEvent{}));
}

TEST_F(ImGuiHALPlatformTest, InitializePublishesBackendIdentity)
{
    const ImGuiIO& io = ImGui::GetIO();

    EXPECT_TRUE(m_platform.isInitialized());
    EXPECT_EQ(io.BackendPlatformUserData, &m_platform);
    ASSERT_NE(io.BackendPlatformName, nullptr);
    // Without a window there is nothing to change the cursor on, so the capability must not be advertised.
    EXPECT_EQ(io.BackendFlags & ImGuiBackendFlags_HasMouseCursors, 0);
}

TEST_F(ImGuiHALPlatformTest, SecondPlatformOnSameContextIsRejected)
{
    ImGuiHALPlatform second;

    EXPECT_FALSE(second.initialize(nullptr));
}

TEST_F(ImGuiHALPlatformTest, ShutdownClearsBackendIdentity)
{
    m_platform.shutdown();
    const ImGuiIO& io = ImGui::GetIO();

    EXPECT_FALSE(m_platform.isInitialized());
    EXPECT_EQ(io.BackendPlatformUserData, nullptr);
    EXPECT_EQ(io.BackendPlatformName, nullptr);
}

TEST_F(ImGuiHALPlatformTest, NewFrameAppliesDisplayOverrideAndDeltaTime)
{
    beginFrame();
    const ImGuiIO& io = ImGui::GetIO();

    EXPECT_FLOAT_EQ(io.DisplaySize.x, 1280.0f);
    EXPECT_FLOAT_EQ(io.DisplaySize.y, 720.0f);
    EXPECT_FLOAT_EQ(io.DisplayFramebufferScale.x, 2.0f);
    EXPECT_FLOAT_EQ(io.DisplayFramebufferScale.y, 2.0f);
    EXPECT_NEAR(io.DeltaTime, 1.0f / 60.0f, 1e-6f);
}

TEST_F(ImGuiHALPlatformTest, NonPositiveDeltaTimeIsClamped)
{
    m_platform.newFrame(0.0f);

    EXPECT_GT(ImGui::GetIO().DeltaTime, 0.0f);
}

TEST_F(ImGuiHALPlatformTest, MouseMoveAndButtonReachImGui)
{
    gp::hal::event::MouseEvent move{};
    move.action = gp::hal::event::MouseAction::Moved;
    move.x = 100.0f;
    move.y = 50.0f;
    m_platform.processMouseEvent(move);

    gp::hal::event::MouseEvent press{};
    press.action = gp::hal::event::MouseAction::ButtonChanged;
    press.button = gp::hal::input::MouseButton::Right;
    press.state = gp::hal::input::KeyState::Pressed;
    press.x = 100.0f;
    press.y = 50.0f;
    m_platform.processMouseEvent(press);

    beginFrame();
    const ImGuiIO& io = ImGui::GetIO();

    EXPECT_FLOAT_EQ(io.MousePos.x, 100.0f);
    EXPECT_FLOAT_EQ(io.MousePos.y, 50.0f);
    EXPECT_TRUE(io.MouseDown[ImGuiMouseButton_Right]);
    EXPECT_FALSE(io.MouseDown[ImGuiMouseButton_Left]);
}

TEST_F(ImGuiHALPlatformTest, WheelMirrorsHorizontalAxis)
{
    gp::hal::event::MouseEvent wheel{};
    wheel.action = gp::hal::event::MouseAction::WheelScrolled;
    wheel.deltaX = 1.0f;
    wheel.deltaY = -2.0f;
    m_platform.processMouseEvent(wheel);

    beginFrame();
    const ImGuiIO& io = ImGui::GetIO();

    EXPECT_FLOAT_EQ(io.MouseWheelH, -1.0f);
    EXPECT_FLOAT_EQ(io.MouseWheel, -2.0f);
}

TEST_F(ImGuiHALPlatformTest, KeyPressAndModifiersReachImGui)
{
    gp::hal::event::KeyboardEvent key{};
    key.scancode = gp::hal::input::Scancode::A;
    key.state = gp::hal::input::KeyState::Pressed;
    key.modifiers = gp::hal::input::KeyModifier::LeftCtrl;
    m_platform.processKeyboardEvent(key);

    beginFrame();
    const ImGuiIO& io = ImGui::GetIO();

    EXPECT_TRUE(ImGui::IsKeyDown(ImGuiKey_A));
    EXPECT_TRUE(io.KeyCtrl);
    EXPECT_FALSE(io.KeyShift);
}

TEST_F(ImGuiHALPlatformTest, KeyReleaseClearsKeyState)
{
    gp::hal::event::KeyboardEvent key{};
    key.scancode = gp::hal::input::Scancode::Escape;
    key.state = gp::hal::input::KeyState::Pressed;
    m_platform.processKeyboardEvent(key);
    key.state = gp::hal::input::KeyState::Released;
    m_platform.processKeyboardEvent(key);

    beginFrame();
    ImGui::EndFrame();
    m_isFrameOpen = false;
    beginFrame();

    EXPECT_FALSE(ImGui::IsKeyDown(ImGuiKey_Escape));
}

TEST_F(ImGuiHALPlatformTest, CommittedTextIsQueuedAsCodepoints)
{
    gp::hal::event::TextInputEvent text{};
    text.action = gp::hal::event::TextInputAction::Committed;
    // "aé€" : 1-, 2- and 3-byte UTF-8 sequences, non-null-terminated view.
    const char utf8[] = "a\xC3\xA9\xE2\x82\xAC";
    m_platform.processTextInputEvent(text, gp::StringView(utf8, sizeof(utf8) - 1u));

    // Characters travel through ImGui's input event queue and land in InputQueueCharacters during NewFrame().
    beginFrame();
    const ImGuiIO& io = ImGui::GetIO();
    ASSERT_EQ(io.InputQueueCharacters.Size, 3);
    EXPECT_EQ(io.InputQueueCharacters[0], static_cast<ImWchar>(0x61));
    EXPECT_EQ(io.InputQueueCharacters[1], static_cast<ImWchar>(0xE9));
    EXPECT_EQ(io.InputQueueCharacters[2], static_cast<ImWchar>(0x20AC));
}

TEST_F(ImGuiHALPlatformTest, LongTextIsChunkedWithoutSplittingCodepoints)
{
    // 200 two-byte code points = 400 bytes: crosses the 256-byte chunk boundary in the middle of a sequence.
    constexpr gp::USize kCodepointCount = 200u;
    char utf8[kCodepointCount * 2u]{};
    for (gp::USize i = 0u; i < kCodepointCount; ++i)
    {
        utf8[i * 2u] = '\xC3';
        utf8[i * 2u + 1u] = '\xA9';
    }

    gp::hal::event::TextInputEvent text{};
    text.action = gp::hal::event::TextInputAction::Committed;
    m_platform.processTextInputEvent(text, gp::StringView(utf8, sizeof(utf8)));

    beginFrame();
    const ImGuiIO& io = ImGui::GetIO();
    ASSERT_EQ(io.InputQueueCharacters.Size, static_cast<int>(kCodepointCount));
    for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
    {
        EXPECT_EQ(io.InputQueueCharacters[i], static_cast<ImWchar>(0xE9));
    }
}

TEST_F(ImGuiHALPlatformTest, EditingTextIsNotQueued)
{
    gp::hal::event::TextInputEvent text{};
    text.action = gp::hal::event::TextInputAction::Editing;
    const char utf8[] = "ka";
    m_platform.processTextInputEvent(text, gp::StringView(utf8, 2u));

    beginFrame();
    EXPECT_EQ(ImGui::GetIO().InputQueueCharacters.Size, 0);
}

TEST_F(ImGuiHALPlatformTest, MouseLeftInvalidatesMousePosition)
{
    gp::hal::event::MouseEvent move{};
    move.action = gp::hal::event::MouseAction::Moved;
    move.x = 10.0f;
    move.y = 10.0f;
    m_platform.processMouseEvent(move);

    gp::hal::event::WindowEvent leave{};
    leave.action = gp::hal::event::WindowAction::MouseLeft;
    m_platform.processWindowEvent(leave);

    beginFrame();

    EXPECT_FALSE(ImGui::IsMousePosValid());
}

TEST_F(ImGuiHALPlatformTest, UIScaleDividesContentScaleByFramebufferScale)
{
    // macOS-like: 2x framebuffer already absorbs a 2.0 content scale -> no extra UI scaling.
    m_platform.setContentScale(2.0f);
    beginFrame();
    EXPECT_FLOAT_EQ(m_platform.getUIScale(), 1.0f);
}

TEST_F(ImGuiHALPlatformTest, DPIChangedEventDrivesUIScale)
{
    // Windows-like: window units are physical pixels, so a 150 % desktop must scale the UI by 1.5.
    m_platform.setDisplayOverride(2560.0f, 1600.0f, 1.0f);

    gp::hal::event::WindowEvent dpiChanged{};
    dpiChanged.action = gp::hal::event::WindowAction::DPIChanged;
    dpiChanged.dpi.scaleX = 1.5f;
    dpiChanged.dpi.scaleY = 1.5f;
    m_platform.processWindowEvent(dpiChanged);

    beginFrame();
    EXPECT_FLOAT_EQ(m_platform.getUIScale(), 1.5f);
}

TEST_F(ImGuiHALPlatformTest, UIScaleIsClampedAndInvalidContentScaleResets)
{
    m_platform.setDisplayOverride(1280.0f, 720.0f, 1.0f);

    m_platform.setContentScale(32.0f);
    m_platform.newFrame(1.0f / 60.0f);
    EXPECT_FLOAT_EQ(m_platform.getUIScale(), 4.0f);

    m_platform.setContentScale(-1.0f);
    m_platform.newFrame(1.0f / 60.0f);
    EXPECT_FLOAT_EQ(m_platform.getUIScale(), 1.0f);
}

TEST_F(ImGuiHALPlatformTest, EventsTargetOwnContextWhenAnotherIsCurrent)
{
    ImGuiContext* const other = ImGui::CreateContext();
    ImGui::SetCurrentContext(other);

    gp::hal::event::MouseEvent move{};
    move.action = gp::hal::event::MouseAction::Moved;
    move.x = 42.0f;
    move.y = 24.0f;
    m_platform.processMouseEvent(move);

    // The scoped context guard must restore the caller's context.
    EXPECT_EQ(ImGui::GetCurrentContext(), other);
    ImGui::DestroyContext(other);

    ImGui::SetCurrentContext(m_context);
    beginFrame();
    EXPECT_FLOAT_EQ(ImGui::GetIO().MousePos.x, 42.0f);
    EXPECT_FLOAT_EQ(ImGui::GetIO().MousePos.y, 24.0f);
}

}   // namespace gp::imgui::tests
