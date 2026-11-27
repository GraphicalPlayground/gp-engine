// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/Breadcrumbs.hpp"
#include "commands/CommandList.hpp"
#include "commands/CommandListExecutor.hpp"    // IWYU pragma: keep
#include "commands/CommandListImmediate.hpp"   // IWYU pragma: keep
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "containers/strings/String.hpp"
#include "diagnostics/BreadcrumbEnums.hpp"     // IWYU pragma: keep
#include "diagnostics/BreadcrumbTypes.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "sync/SyncEnums.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::diagnostics::tests
{

#if GP_WITH_RHI_BREADCRUMBS

TEST(RHIBreadcrumbsTests, NodeHierarchyLevelAncestorAndFullPath)
{
    gp::SharedRef<BreadcrumbAllocator> alloc = BreadcrumbAllocator::create();

    BreadcrumbNode* const sentinel = BreadcrumbNode::getSentinel();
    ASSERT_NE(sentinel, nullptr);
    EXPECT_EQ(sentinel->id, BreadcrumbNode::kSentinelId);
    EXPECT_EQ(BreadcrumbNode::getLevel(nullptr), 0u);
    EXPECT_EQ(BreadcrumbNode::getLevel(sentinel), 0u);
    EXPECT_EQ(BreadcrumbNode::getNonNullRoot(nullptr), nullptr);
    EXPECT_EQ(BreadcrumbNode::getNonNullRoot(sentinel), nullptr);

    BreadcrumbNode* const frameNode = alloc->allocFormattedBreadcrumb(BreadcrumbData("Frame", "Frame %u"), 42u);
    BreadcrumbNode* const basePass = alloc->allocBreadcrumb(BreadcrumbData("BasePass"));
    BreadcrumbNode* const opaque = alloc->allocFormattedBreadcrumb(BreadcrumbData("Opaque", "Opaque Batch %d"), 3);
    BreadcrumbNode* const shadowPass = alloc->allocBreadcrumb(BreadcrumbData("ShadowDepths"));

    ASSERT_NE(frameNode, nullptr);
    ASSERT_NE(basePass, nullptr);
    ASSERT_NE(opaque, nullptr);
    ASSERT_NE(shadowPass, nullptr);
    EXPECT_NE(frameNode->id, BreadcrumbNode::kSentinelId);
    EXPECT_NE(frameNode->id, basePass->id);

    basePass->setParent(frameNode);
    opaque->setParent(basePass);
    shadowPass->setParent(frameNode);

    EXPECT_EQ(BreadcrumbNode::getLevel(frameNode), 1u);
    EXPECT_EQ(BreadcrumbNode::getLevel(basePass), 2u);
    EXPECT_EQ(BreadcrumbNode::getLevel(opaque), 3u);
    EXPECT_EQ(BreadcrumbNode::getNonNullRoot(opaque), frameNode);
    EXPECT_EQ(BreadcrumbNode::findCommonAncestor(opaque, shadowPass), frameNode);
    EXPECT_EQ(BreadcrumbNode::findCommonAncestor(opaque, basePass), basePass);

    const gp::String fullPath = opaque->getFullPath();
    EXPECT_EQ(fullPath.view(), gp::StringView("Frame 42/BasePass/Opaque Batch 3"));
    EXPECT_GT(alloc->getUsedBytes(), 0u);
}

TEST(RHIBreadcrumbsTests, ListIterateUnlinkAndRangeInsertAfter)
{
    gp::SharedRef<BreadcrumbAllocator> alloc = BreadcrumbAllocator::create();
    BreadcrumbNode* const n0 = alloc->allocBreadcrumb(BreadcrumbData("N0"));
    BreadcrumbNode* const n1 = alloc->allocBreadcrumb(BreadcrumbData("N1"));
    BreadcrumbNode* const n2 = alloc->allocBreadcrumb(BreadcrumbData("N2"));
    BreadcrumbNode* const mid = alloc->allocBreadcrumb(BreadcrumbData("Mid"));

    BreadcrumbList list{};
    list.append(n0);
    list.append(n1);
    list.append(n2);

    const BreadcrumbRange initialRange = list.getRange();
    EXPECT_TRUE(static_cast<bool>(initialRange));
    EXPECT_EQ(initialRange.first, n0);
    EXPECT_EQ(initialRange.last, n2);

    gp::Vector<BreadcrumbNode*> unlinked{};
    list.iterateAndUnlink(
        [&unlinked](BreadcrumbNode* const node) noexcept -> void
    {
        unlinked.emplaceBack(node);
    }
    );
    ASSERT_EQ(unlinked.size(), 3u);
    EXPECT_EQ(unlinked[0], n0);
    EXPECT_EQ(unlinked[1], n1);
    EXPECT_EQ(unlinked[2], n2);
    EXPECT_EQ(n0->getNextPtr(), nullptr);
    EXPECT_EQ(n1->getNextPtr(), nullptr);

    // Build [n0, n2] and splice [mid, mid] after n0 -> [n0, mid, n2].
    BreadcrumbList outerList{};
    outerList.append(n0);
    outerList.append(n2);
    BreadcrumbRange range = outerList.getRange();

    const BreadcrumbRange subRange(mid, mid);
    range.insertAfter(subRange, n0);

    gp::Vector<BreadcrumbNode*> visited{};
    range.enumerate(
        [&visited](BreadcrumbNode* const node) noexcept -> void
    {
        visited.emplaceBack(node);
    }
    );
    ASSERT_EQ(visited.size(), 3u);
    EXPECT_EQ(visited[0], n0);
    EXPECT_EQ(visited[1], mid);
    EXPECT_EQ(visited[2], n2);
}

TEST(RHIBreadcrumbsTests, DeepParentAllocatorChainDestructsIterativelyWithoutStackOverflow)
{
    gp::SharedPtr<BreadcrumbAllocator> head = BreadcrumbAllocator::create();
    constexpr gp::UInt32 kChainDepth = 2048u;

    for (gp::UInt32 idx = 0u; idx < kChainDepth; ++idx)
    {
        gp::SharedRef<BreadcrumbAllocator> next = BreadcrumbAllocator::create();
        next->addParentReference(head.get());
        head = gp::SharedPtr<BreadcrumbAllocator>(std::move(next));
    }

    // Releasing `head` must flatten the 2,048-deep chain iteratively inside `~BreadcrumbAllocator()`.
    head.reset();
    EXPECT_FALSE(head.isValid());
}

TEST(RHIBreadcrumbsTests, BreadcrumbStateDumpActiveTreeClassifiesNodes)
{
    gp::SharedRef<BreadcrumbAllocator> alloc = BreadcrumbAllocator::create();
    BreadcrumbNode* const frame = alloc->allocBreadcrumb(BreadcrumbData("Frame 7"));
    BreadcrumbNode* const prePass = alloc->allocBreadcrumb(BreadcrumbData("PrePass"));
    BreadcrumbNode* const basePass = alloc->allocBreadcrumb(BreadcrumbData("BasePass"));
    BreadcrumbNode* const postFx = alloc->allocBreadcrumb(BreadcrumbData("PostProcessing"));

    prePass->setParent(frame);
    basePass->setParent(frame);
    postFx->setParent(frame);

    BreadcrumbList list{};
    list.append(frame);
    list.append(prePass);
    list.append(basePass);
    list.append(postFx);

    BreadcrumbState state{};
    const BreadcrumbQueueId gfxQueue(0u, sync::PipelineStage::Graphics);
    // GPU entered `basePass` (markerIn) and finished `prePass` (markerOut), while `postFx` has not started.
    state.linkRange(gfxQueue, list.getRange(), basePass->id, prePass->id);

    const gp::String dump = state.dumpActiveBreadcrumbs(true);
    EXPECT_NE(dump.view().find("[Active] Frame 7"), gp::StringView::npos);
    EXPECT_NE(dump.view().find("[Finished] PrePass"), gp::StringView::npos);
    EXPECT_NE(dump.view().find("[Active] BasePass"), gp::StringView::npos);
    EXPECT_NE(dump.view().find("[NotStarted] PostProcessing"), gp::StringView::npos);

    const gp::String activeOnly = state.dumpActiveBreadcrumbs(false);
    EXPECT_NE(activeOnly.view().find("[Active] BasePass"), gp::StringView::npos);
    EXPECT_EQ(activeOnly.view().find("PrePass"), gp::StringView::npos);
    EXPECT_EQ(activeOnly.view().find("PostProcessing"), gp::StringView::npos);
}

TEST(RHIBreadcrumbsTests, CommandListScopeAndManualEventRecordCPUAndGPUMarkers)
{
    command::CommandList cmdList{};
    EXPECT_EQ(cmdList.getCurrentBreadcrumbCPU(), nullptr);

    {
        GP_RHI_BREADCRUMB_EVENT_F(cmdList, "SceneRender", "SceneRender View=%d", 1);
        BreadcrumbNode* const outer = cmdList.getCurrentBreadcrumbCPU();
        ASSERT_NE(outer, nullptr);
        EXPECT_EQ(outer->getParent(), BreadcrumbNode::getSentinel());

        BreadcrumbEventManual manualPass(cmdList, BreadcrumbData("GBufferPass"), true);
        ASSERT_TRUE(manualPass.isActive());
        EXPECT_EQ(cmdList.getCurrentBreadcrumbCPU(), manualPass.getNode());
        EXPECT_EQ(manualPass.getNode()->getParent(), outer);

        const BreadcrumbNodeRef exportedRef = cmdList.getCurrentBreadcrumbRef();
        EXPECT_TRUE(static_cast<bool>(exportedRef));
        EXPECT_TRUE(exportedRef.getAllocatorRef().isValid());

        manualPass.end(cmdList);
        EXPECT_FALSE(manualPass.isActive());
        EXPECT_EQ(cmdList.getCurrentBreadcrumbCPU(), outer);
    }

    EXPECT_EQ(cmdList.getCurrentBreadcrumbCPU(), nullptr);
    const BreadcrumbRange recordedRange = cmdList.exportBreadcrumbRange(sync::PipelineStage::Graphics);
    EXPECT_TRUE(static_cast<bool>(recordedRange));
    cmdList.reset();
}

#endif

}   // namespace gp::rhi::diagnostics::tests
