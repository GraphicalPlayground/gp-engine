// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "ImGuiLayer.hpp"
#include "imgui.h"
#include "renderer/ImGuiRHIRenderer.hpp"
#include <cstdint>
#include <gtest/gtest.h>

namespace gp::imgui::tests
{

TEST(ImGuiLayerTest, DefaultStateIsInactive)
{
    const ImGuiLayer layer;

    EXPECT_FALSE(layer.isInitialized());
    EXPECT_FALSE(layer.hasPendingDrawData());
    EXPECT_EQ(layer.getContext(), nullptr);
    EXPECT_FALSE(layer.wantsMouseCapture());
    EXPECT_FALSE(layer.wantsKeyboardCapture());
}

TEST(ImGuiLayerTest, InitializeWithoutRHIRollsBackCompletely)
{
    // Unit tests run without a DynamicRHI, so the renderer must refuse and the layer must leave no context behind.
    ImGui::SetCurrentContext(nullptr);
    ImGuiLayer layer;

    EXPECT_FALSE(layer.initialize(nullptr));
    EXPECT_FALSE(layer.isInitialized());
    EXPECT_EQ(layer.getContext(), nullptr);
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
    EXPECT_FALSE(layer.getPlatform().isInitialized());
    EXPECT_FALSE(layer.getRenderer().isInitialized());
}

TEST(ImGuiLayerTest, FrameCallsAreNoOpsWhenInactive)
{
    ImGuiLayer layer;

    layer.beginFrame(1.0f / 60.0f);
    layer.endFrame();
    layer.shutdown();

    EXPECT_FALSE(layer.hasPendingDrawData());
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
}

TEST(ImGuiRHIRendererTest, InitializeFailsWithoutDynamicRHI)
{
    ImGuiContext* const context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);

    ImGuiRHIRenderer renderer;
    EXPECT_FALSE(renderer.initialize());
    EXPECT_FALSE(renderer.isInitialized());
    EXPECT_EQ(ImGui::GetIO().BackendRendererUserData, nullptr);
    EXPECT_EQ(ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasTextures, 0);

    ImGui::DestroyContext(context);
}

TEST(ImGuiRHIRendererTest, InitializeFailsWithoutContext)
{
    ImGui::SetCurrentContext(nullptr);
    ImGuiRHIRenderer renderer;

    EXPECT_FALSE(renderer.initialize());
}

TEST(ImGuiRHIRendererTest, TextureIdRoundTripsPointer)
{
    EXPECT_EQ(ImGuiRHIRenderer::toTextureId(nullptr), 0u);

    alignas(16) gp::UInt8 storage[16]{};
    gp::rhi::resource::Texture* const fake = reinterpret_cast<gp::rhi::resource::Texture*>(storage);
    EXPECT_EQ(ImGuiRHIRenderer::toTextureId(fake), static_cast<gp::UInt64>(reinterpret_cast<std::uintptr_t>(storage)));
}

TEST(ImGuiRHIRendererTest, DefaultStatsAreZero)
{
    const ImGuiRHIRenderer renderer;
    const ImGuiRendererStats& stats = renderer.getStats();

    EXPECT_EQ(stats.drawCalls, 0u);
    EXPECT_EQ(stats.vertexCount, 0u);
    EXPECT_EQ(stats.residentTextures, 0u);
}

}   // namespace gp::imgui::tests
