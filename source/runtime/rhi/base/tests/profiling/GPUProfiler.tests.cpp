// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "profiling/GPUProfiler.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"
#include "diagnostics/Breadcrumbs.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "profiling/GpuProfilerTrace.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::profiling::tests
{

TEST(RHIGPUProfilerTests, MetadataSerializerAndGpuProfilerTraceRecordSpans)
{
    GpuProfilerTrace::reset();

    MetadataSerializer serializer{};
    serializer.appendValue("pass", "ShadowDepths");
    serializer.appendValue("draws", static_cast<gp::UInt64>(48u));
    serializer.appendValue("ms", 1.25);
    const gp::String meta = serializer.toString();
    EXPECT_EQ(serializer.getFieldCount(), 3u);
    EXPECT_NE(meta.view().find("pass=\"ShadowDepths\""), gp::StringView::npos);
    EXPECT_NE(meta.view().find("draws=48"), gp::StringView::npos);

    GpuProfilerTrace::setEnabled(true);
    const Queue gfxQueue(Queue::Type::Graphics, 0u, 0u);
    GpuProfilerTrace::initQueue(gfxQueue.value, "Graphics0");
    GpuProfilerTrace::beginFrame(gfxQueue.value, 1u);
    GpuProfilerTrace::beginEvent(gfxQueue.value, "Frame", 100u, meta.view());
    GpuProfilerTrace::beginEvent(gfxQueue.value, "BasePass", 120u);
    GpuProfilerTrace::endEvent(gfxQueue.value, 220u);
    GpuProfilerTrace::endEvent(gfxQueue.value, 300u);
    GpuProfilerTrace::endFrame(gfxQueue.value);

    gp::Vector<GpuProfilerTrace::RecordedSpan> spans{};
    GpuProfilerTrace::getRecordedSpans(spans);
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0].name, "BasePass");
    EXPECT_EQ(spans[0].depth, 1u);
    EXPECT_EQ(spans[0].endTimestampUs - spans[0].startTimestampUs, 100u);
    EXPECT_EQ(spans[1].name, "Frame");
    EXPECT_EQ(spans[1].depth, 0u);

    GpuProfilerTrace::reset();
}

TEST(RHIGPUProfilerTests, EventStreamProcessEventsComputesInclusiveExclusiveTimingsAndCaptureJson)
{
    resetGPUProfiler();
    GpuProfilerTrace::reset();

    const Queue queues[2] = {
        Queue(Queue::Type::Graphics, 0u, 0u),
        Queue(Queue::Type::Compute, 0u, 0u),
    };
    initializeQueues(gp::ConstVectorView<Queue>(queues, 2u));

    gp::SharedRef<diagnostics::BreadcrumbAllocator> bcAlloc = diagnostics::BreadcrumbAllocator::create();
    diagnostics::BreadcrumbNode* const outerNode = bcAlloc->allocBreadcrumb(diagnostics::BreadcrumbData("Lighting"));
    diagnostics::BreadcrumbNode* const innerNode = bcAlloc->allocBreadcrumb(diagnostics::BreadcrumbData("DirectLight"));
    innerNode->setParent(outerNode);

    struct ObserverLog
    {
        gp::Vector<GPUStatTimingResult> results{};
        gp::String capturedJson{};
    } log{};

    setGlobalTimingObserver(
        [](const GPUStatTimingResult& res, void* const user) noexcept -> void
    {
        static_cast<ObserverLog*>(user)->results.emplaceBack(res);
    },
        &log
    );

    setProfileCaptureCallback(
        [](const gp::StringView json, void* const user) noexcept -> void
    {
        static_cast<ObserverLog*>(user)->capturedJson = gp::String(json);
    },
        &log
    );

    profileNextFrame();

    bool frameStatNotified = false;
    GPUStat* const oneShotStat = new GPUStat(
        gp::Name("FrameStat"),
        [&frameStatNotified](const GPUStatTimingResult& res) noexcept -> OnTimingResultsAction
    {
        frameStatNotified = (res.inclusiveMicroseconds == 500u && res.numDraws == 12u);
        return OnTimingResultsAction::Delete;
    }
    );

    // Frame 0->1 boundary arms capture and registers `oneShotStat` for Frame 1.
    EventStream armStream{};
    armStream.emplace(Event::FrameBoundary{ 1u, nullptr, oneShotStat });
    processEvents(queues[0], std::move(armStream));
    EXPECT_TRUE(isProfiling());

    // Frame 1 work + nested breadcrumbs + Frame 1->2 boundary.
    EventStream frame1{};
    diagnostics::BreadcrumbAllocatorArray allocArray{};
    allocArray.addUnique(bcAlloc.get());
    frame1.retainBreadcrumbAllocators(allocArray);

    frame1.emplace(Event::BeginWork{ 1000u, 1000u });
    frame1.emplace(Event::BeginBreadcrumb{ outerNode, 1050u });
    frame1.emplace(Event::Stats{ 4u, 400u, 0u });
    frame1.emplace(Event::BeginBreadcrumb{ innerNode, 1100u });
    frame1.emplace(Event::Stats{ 8u, 800u, 1u });
    frame1.emplace(Event::EndBreadcrumb{ innerNode, 1300u });
    frame1.emplace(Event::EndBreadcrumb{ outerNode, 1450u });
    frame1.emplace(Event::SignalFence{ 1460u, 77u });
    frame1.emplace(Event::EndWork{ 1500u });
    frame1.emplace(Event::FrameBoundary{ 2u, nullptr, nullptr });

    processEvents(queues[0], std::move(frame1));

    EXPECT_FALSE(isProfiling());
    EXPECT_TRUE(frameStatNotified);
    EXPECT_EQ(getLastGPUFrameMicroseconds(0u), 500u);
    EXPECT_EQ(getGPUFrameCycles(0u), 500u);

    ASSERT_EQ(log.results.size(), 2u);
    // Inner ("DirectLight"): 1300 - 1100 = 200 us inclusive, 200 us exclusive.
    EXPECT_EQ(log.results[0].statName, gp::Name("DirectLight"));
    EXPECT_EQ(log.results[0].inclusiveMicroseconds, 200u);
    EXPECT_EQ(log.results[0].exclusiveMicroseconds, 200u);
    EXPECT_EQ(log.results[0].numDraws, 8u);

    // Outer ("Lighting"): 1450 - 1050 = 400 us inclusive, 400 - 200 = 200 us exclusive, 4 + 8 = 12 draws.
    EXPECT_EQ(log.results[1].statName, gp::Name("Lighting"));
    EXPECT_EQ(log.results[1].inclusiveMicroseconds, 400u);
    EXPECT_EQ(log.results[1].exclusiveMicroseconds, 200u);
    EXPECT_EQ(log.results[1].numDraws, 12u);

    EXPECT_NE(log.capturedJson.view().find("\"Lighting\""), gp::StringView::npos);
    EXPECT_NE(log.capturedJson.view().find("\"DirectLight\""), gp::StringView::npos);

    gp::UInt32 poppedCycles = 0u;
    // Frame 0 (from `armStream` boundary) had 0 busy us; Frame 1 (from `frame1` boundary) had 500 busy us.
    EXPECT_EQ(g_rhiGPUFrameTimeHistory[0].popFrameCycles(poppedCycles), GPUFrameTimeHistory::Result::Ok);
    EXPECT_EQ(poppedCycles, 0u);
    EXPECT_EQ(g_rhiGPUFrameTimeHistory[0].popFrameCycles(poppedCycles), GPUFrameTimeHistory::Result::Ok);
    EXPECT_EQ(poppedCycles, 500u);

    g_rhiGPUFrameTimeHistory[0].pushFrameCycles(0.0, 999u);
    EXPECT_EQ(g_rhiGPUFrameTimeHistory[0].popFrameCycles(poppedCycles), GPUFrameTimeHistory::Result::Disjoint);
    EXPECT_EQ(g_rhiGPUFrameTimeHistory[0].popFrameCycles(poppedCycles), GPUFrameTimeHistory::Result::Empty);

    resetGPUProfiler();
}

}   // namespace gp::rhi::profiling::tests
