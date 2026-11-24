// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/CommandEnums.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::command::tests
{

static_assert(kRenderTargetLoadActionCount == 3u, "RenderTargetLoadAction must have 3 enumerators.");
static_assert(kRenderTargetLoadActionNumBits == 2u, "RenderTargetLoadAction must fit in 2 bits.");
static_assert(kRenderTargetStoreActionCount == 3u, "RenderTargetStoreAction must have 3 enumerators.");
static_assert(kRenderTargetStoreActionNumBits == 2u, "RenderTargetStoreAction must fit in 2 bits.");

TEST(CommandEnumsTest, DecodeRenderTargetModeCoversAllModesWithFullWrite)
{
    RenderTargetLoadAction colorLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction colorStore = RenderTargetStoreAction::NoAction;
    RenderTargetLoadAction depthLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction depthStore = RenderTargetStoreAction::NoAction;
    RenderTargetLoadAction stencilLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction stencilStore = RenderTargetStoreAction::NoAction;

    const resource::ExclusiveDepthStencil fullWrite = resource::ExclusiveDepthStencil::kDepthWriteStencilWrite;

    decodeRenderTargetMode(
        SimpleRenderTargetMode::ExistingColorAndDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(colorStore, RenderTargetStoreAction::Store);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(depthStore, RenderTargetStoreAction::Store);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(stencilStore, RenderTargetStoreAction::Store);

    decodeRenderTargetMode(
        SimpleRenderTargetMode::UninitializedColorAndDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::NoAction);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::NoAction);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::NoAction);
    EXPECT_EQ(depthStore, RenderTargetStoreAction::Store);

    decodeRenderTargetMode(
        SimpleRenderTargetMode::UninitializedColorExistingDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::NoAction);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Load);

    decodeRenderTargetMode(
        SimpleRenderTargetMode::UninitializedColorClearDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::NoAction);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Clear);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Clear);

    decodeRenderTargetMode(
        SimpleRenderTargetMode::ClearColorExistingDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::Clear);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Load);

    decodeRenderTargetMode(
        SimpleRenderTargetMode::ClearColorAndDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::Clear);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Clear);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Clear);

    decodeRenderTargetMode(
        SimpleRenderTargetMode::ExistingContentsNoDepthStore,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(depthStore, RenderTargetStoreAction::NoAction);
    EXPECT_EQ(stencilStore, RenderTargetStoreAction::Store);

    decodeRenderTargetMode(
        SimpleRenderTargetMode::ExistingColorAndClearDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Clear);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Clear);

    decodeRenderTargetMode(
        SimpleRenderTargetMode::ExistingColorAndDepthAndClearStencil,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        fullWrite
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Load);
}

TEST(CommandEnumsTest, DecodeRenderTargetModeMasksUnusedAndReadOnlyPlanes)
{
    RenderTargetLoadAction colorLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction colorStore = RenderTargetStoreAction::NoAction;
    RenderTargetLoadAction depthLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction depthStore = RenderTargetStoreAction::NoAction;
    RenderTargetLoadAction stencilLoad = RenderTargetLoadAction::NoAction;
    RenderTargetStoreAction stencilStore = RenderTargetStoreAction::NoAction;

    // Depth read-only + stencil unused drops depth store and both stencil actions.
    decodeRenderTargetMode(
        SimpleRenderTargetMode::ClearColorAndDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        resource::ExclusiveDepthStencil::kDepthReadStencilNop
    );
    EXPECT_EQ(colorLoad, RenderTargetLoadAction::Clear);
    EXPECT_EQ(colorStore, RenderTargetStoreAction::Store);
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::Clear);
    EXPECT_EQ(depthStore, RenderTargetStoreAction::NoAction);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::NoAction);
    EXPECT_EQ(stencilStore, RenderTargetStoreAction::NoAction);

    // Depth unused + stencil read-only drops both depth actions and stencil store.
    decodeRenderTargetMode(
        SimpleRenderTargetMode::ExistingColorAndDepth,
        colorLoad,
        colorStore,
        depthLoad,
        depthStore,
        stencilLoad,
        stencilStore,
        resource::ExclusiveDepthStencil::kDepthNopStencilRead
    );
    EXPECT_EQ(depthLoad, RenderTargetLoadAction::NoAction);
    EXPECT_EQ(depthStore, RenderTargetStoreAction::NoAction);
    EXPECT_EQ(stencilLoad, RenderTargetLoadAction::Load);
    EXPECT_EQ(stencilStore, RenderTargetStoreAction::NoAction);
}

}   // namespace gp::rhi::command::tests
