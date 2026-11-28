// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/DiagnosticBuffer.hpp"
#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::diagnostics::tests
{

TEST(RHIDiagnosticBufferTests, UnmappedAndCleanQueuesReportExpectedSummaries)
{
    clearShaderDiagnosticMessages();

    const DiagnosticBuffer unmapped{};
    const gp::String unmappedMsg = unmapped.getShaderDiagnosticMessages(0u, 0u, "Graphics");
    EXPECT_NE(unmappedMsg.view().find("unmapped"), gp::StringView::npos);

    DiagnosticBuffer::Queue queues[2]{};
    queues[0].markerIn = 11u;
    queues[0].markerOut = 10u;

    DiagnosticBuffer mapped(queues, 2u);
    EXPECT_EQ(mapped.getNumQueues(), 2u);
    EXPECT_NE(mapped.getQueue(0u), nullptr);
    EXPECT_EQ(mapped.getQueue(2u), nullptr);

    const gp::String cleanMsg = mapped.getShaderDiagnosticMessages(0u, 0u, "Graphics");
    EXPECT_NE(cleanMsg.view().find("No shader diagnostics found"), gp::StringView::npos);
    EXPECT_NE(cleanMsg.view().find("markerIn=11"), gp::StringView::npos);
}

TEST(RHIDiagnosticBufferTests, RegisteredAndRawShaderAssertionPayloadsFormatCorrectly)
{
    clearShaderDiagnosticMessages();
    registerShaderDiagnosticMessage(0xCAFE0001u, "ShadowCull.hlsl:84: check(clusterIdx < maxClusters)");
    ASSERT_NE(getShaderDiagnosticMessage(0xCAFE0001u), nullptr);

    DiagnosticBuffer::Queue queue{};
    queue.markerIn = 25u;
    queue.markerOut = 24u;

    queue.lanes[0].counter = 3u;
    queue.lanes[0].messageId = 0xCAFE0001u;
    queue.lanes[0].payload.asUint[0] = 4096u;
    queue.lanes[0].payload.asUint[1] = 2048u;

    queue.lanes[5].counter = 1u;
    queue.lanes[5].messageId = 0x99990002u;
    queue.lanes[5].payload.asFloat[0] = 3.5f;
    queue.lanes[5].payload.asFloat[1] = -1.25f;

    DiagnosticBuffer buffer(&queue, 1u);
    const gp::String report = buffer.getShaderDiagnosticMessages(0u, 0u, "AsyncCompute");

    EXPECT_NE(report.view().find("2 active lane(s)"), gp::StringView::npos);
    EXPECT_NE(report.view().find("ShadowCull.hlsl:84: check(clusterIdx < maxClusters)"), gp::StringView::npos);
    EXPECT_NE(report.view().find("4096"), gp::StringView::npos);
    EXPECT_NE(report.view().find("Unregistered ID 0x99990002"), gp::StringView::npos);
    EXPECT_NE(report.view().find("3.5000"), gp::StringView::npos);

    clearShaderDiagnosticMessages();
    EXPECT_EQ(getShaderDiagnosticMessage(0xCAFE0001u), nullptr);
}

}   // namespace gp::rhi::diagnostics::tests
