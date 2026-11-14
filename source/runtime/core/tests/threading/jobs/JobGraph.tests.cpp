// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/jobs/JobGraph.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/jobs/JobSystem.hpp"
#include "threading/jobs/ParallelFor.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>
#include <utility>

namespace gp::thread::tests
{

namespace detail
{

/// @brief No-op `JobEntryFunction` used for topology and cycle-detection tests.
/// @param[in] userData Unused payload pointer.
static void noopGraphPassEntry([[maybe_unused]] void* const userData) noexcept
{}

/// @brief Shared timeline and execution tracker for the 9-pass simulated RDG diamond pipeline test.
struct RDGPassExecutionTracker
{
    static constexpr gp::UInt32 kPassCount = 9u;
    static constexpr gp::UInt32 kGBufferDrawCount = 512u;

    JobSystem* jobSystem{ nullptr };
    Atomic<gp::UInt32> globalTimelineTicket{ 1u };
    Atomic<gp::UInt32> passStartTicket[kPassCount]{};
    Atomic<gp::UInt32> passFinishTicket[kPassCount]{};
    Atomic<gp::UInt32> passExecutionCount[kPassCount]{};
    Atomic<gp::UInt32> gbufferRecordedDraws{ 0u };
};

/// @brief Per-node argument pairing a `RDGPassExecutionTracker*` with its `passIndex`.
struct RDGPassNodeArg
{
    RDGPassExecutionTracker* tracker{ nullptr };
    gp::UInt32 passIndex{ 0u };
};

/// @brief Nested `parallelFor` batch callback invoked inside Pass `4` (`GBufferPass`) to simulate parallel draw
///        recording.
/// @param[in] startIndex Batch starting index (unused).
/// @param[in] count Number of draws in the batch.
/// @param[in] rawTracker Pointer to `RDGPassExecutionTracker`.
/// @param[in] context Active worker and fiber context.
static void gbufferDrawRecordBatch(
    [[maybe_unused]] const gp::UInt32 startIndex,
    const gp::UInt32 count,
    void* const rawTracker,
    const JobContext& context
) noexcept
{
    RDGPassExecutionTracker* const tracker = static_cast<RDGPassExecutionTracker*>(rawTracker);
    EXPECT_TRUE(context.isValid());
    tracker->gbufferRecordedDraws.fetchAdd(count, gp::MemoryOrder::AcqRel);
}

/// @brief `JobEntryFunction` for each pass in the 9-pass simulated RDG diamond pipeline.
/// @param[in] rawArg Pointer to `RDGPassNodeArg`.
static void rdgPassCallback(void* const rawArg) noexcept
{
    const RDGPassNodeArg* const arg = static_cast<const RDGPassNodeArg*>(rawArg);
    RDGPassExecutionTracker* const tracker = arg->tracker;
    const gp::UInt32 passIdx = arg->passIndex;

    const gp::UInt32 startTicket = tracker->globalTimelineTicket.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->passStartTicket[passIdx].store(startTicket, gp::MemoryOrder::Release);

    // Pass 4 (GBufferPass) fans out into a nested data-parallel parallelFor command-list recording wave.
    if (passIdx == 4u && tracker->jobSystem != nullptr)
    {
        EXPECT_EQ(
            tracker->jobSystem->parallelFor(
                RDGPassExecutionTracker::kGBufferDrawCount, 64u, &gbufferDrawRecordBatch, tracker, JobPriority::High
            ),
            JobSystemResult::Success
        );
    }

    tracker->passExecutionCount[passIdx].fetchAdd(1u, gp::MemoryOrder::AcqRel);
    const gp::UInt32 finishTicket = tracker->globalTimelineTicket.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->passFinishTicket[passIdx].store(finishTicket, gp::MemoryOrder::Release);
}

/// @brief Shared tracking state for the 288-pass (`36` tiers x `8` passes) deep RDG stress test.
struct DeepRDGStressContext
{
    static constexpr gp::UInt32 kTierCount = 36u;
    static constexpr gp::UInt32 kPassesPerTier = 8u;
    static constexpr gp::UInt32 kTotalPasses = kTierCount * kPassesPerTier;   // 288 passes (> 256 inline threshold)

    Atomic<gp::UInt32> executedPasses{ 0u };
    Atomic<gp::UInt32> tierCompletionCounts[kTierCount]{};
    Atomic<gp::UInt32> orderingViolations{ 0u };
};

/// @brief Per-node payload for the 288-pass deep RDG stress test.
struct DeepRDGNodePayload
{
    DeepRDGStressContext* ctx{ nullptr };
    gp::UInt32 tierIndex{ 0u };
};

/// @brief `JobEntryFunction` verifying upstream tier completion before recording its own pass execution.
/// @param[in] rawPayload Pointer to `DeepRDGNodePayload`.
static void deepRDGStressPassEntry(void* const rawPayload) noexcept
{
    const DeepRDGNodePayload* const payload = static_cast<const DeepRDGNodePayload*>(rawPayload);
    DeepRDGStressContext* const ctx = payload->ctx;

    if (payload->tierIndex > 0u)
    {
        const gp::UInt32 prevTierDone =
            ctx->tierCompletionCounts[payload->tierIndex - 1u].load(gp::MemoryOrder::Acquire);
        if (prevTierDone < 2u)
        {
            ctx->orderingViolations.fetchAdd(1u, gp::MemoryOrder::Relaxed);
        }
    }

    ctx->tierCompletionCounts[payload->tierIndex].fetchAdd(1u, gp::MemoryOrder::AcqRel);
    ctx->executedPasses.fetchAdd(1u, gp::MemoryOrder::AcqRel);
}

}   // namespace detail

TEST(JobGraphLifecycleTest, ArenaAndBufferInitializationMoveAndValidation)
{
    JobGraph graph{};
    EXPECT_FALSE(graph.isValid());
    EXPECT_TRUE(graph.isEmpty());
    EXPECT_EQ(graph.getNodeCount(), 0u);
    EXPECT_EQ(graph.validateTopology(), JobSystemResult::NotInitialized);

    gp::memory::LinearAllocator arena(64ULL * 1024ULL);
    ASSERT_TRUE(arena.isValid());

    EXPECT_EQ(graph.initialize(arena, 0u, 8u), JobSystemResult::InvalidArguments);
    EXPECT_EQ(graph.initialize(arena, 8u, 0u), JobSystemResult::InvalidArguments);

    ASSERT_EQ(graph.initialize(arena, 4u, 2u), JobSystemResult::Success);
    EXPECT_TRUE(graph.isValid());
    EXPECT_EQ(graph.getMaxNodes(), 4u);
    EXPECT_EQ(graph.getMaxDependentsPerNode(), 2u);

    JobGraphNodeId n0 = kInvalidJobGraphNodeId;
    JobGraphNodeId n1 = kInvalidJobGraphNodeId;
    JobGraphNodeId n2 = kInvalidJobGraphNodeId;
    JobGraphNodeId n3 = kInvalidJobGraphNodeId;
    ASSERT_EQ(graph.addNode(&detail::noopGraphPassEntry, nullptr, &n0), JobSystemResult::Success);
    ASSERT_EQ(graph.addNode(&detail::noopGraphPassEntry, nullptr, &n1), JobSystemResult::Success);
    ASSERT_EQ(graph.addNode(&detail::noopGraphPassEntry, nullptr, &n2), JobSystemResult::Success);
    ASSERT_EQ(graph.addNode(&detail::noopGraphPassEntry, nullptr, &n3), JobSystemResult::Success);
    EXPECT_EQ(graph.getNodeCount(), 4u);

    // Adding a fifth node exceeds the configured capacity of 4.
    EXPECT_EQ(graph.addNode(&detail::noopGraphPassEntry, nullptr), JobSystemResult::CapacityExceeded);

    // Self-loop and out-of-bounds edges must fail with InvalidArguments.
    EXPECT_EQ(graph.addDependency(n0, n0), JobSystemResult::InvalidArguments);
    EXPECT_EQ(graph.addDependency(n0, 99u), JobSystemResult::InvalidArguments);

    // Adding the same dependency edge twice is idempotent.
    ASSERT_EQ(graph.addDependency(n0, n1), JobSystemResult::Success);
    ASSERT_EQ(graph.addDependency(n0, n1), JobSystemResult::Success);
    EXPECT_EQ(graph.getNode(n1)->initialPrerequisites, 1u);
    EXPECT_EQ(graph.getNode(n1)->getUnresolvedPrerequisiteCount(), 1u);
    EXPECT_TRUE(graph.getNode(n0)->isRootNode());
    EXPECT_FALSE(graph.getNode(n1)->isRootNode());

    // A second distinct dependent from n0 succeeds (maxDependentsPerNode == 2).
    ASSERT_EQ(graph.addDependency(n0, n2), JobSystemResult::Success);

    // A third distinct dependent from n0 exceeds maxDependentsPerNode == 2.
    EXPECT_EQ(graph.addDependency(n0, n3), JobSystemResult::CapacityExceeded);

    // Move construction and move assignment transfer ownership cleanly.
    JobGraph movedGraph(std::move(graph));
    EXPECT_FALSE(graph.isValid());
    EXPECT_TRUE(movedGraph.isValid());
    EXPECT_EQ(movedGraph.getNodeCount(), 4u);
    EXPECT_EQ(movedGraph.validateTopology(), JobSystemResult::Success);
    EXPECT_EQ(movedGraph.getNode(n1)->getUnresolvedPrerequisiteCount(), 1u);

    JobGraph assignedGraph{};
    assignedGraph = std::move(movedGraph);
    EXPECT_FALSE(movedGraph.isValid());
    EXPECT_TRUE(assignedGraph.isValid());

    assignedGraph.clear();
    EXPECT_EQ(assignedGraph.getNodeCount(), 0u);
    assignedGraph.shutdown();
}

TEST(JobGraphCycleDetectionTest, DetectsDirectedCyclesBeforeExecution)
{
    alignas(gp::memory::HardwareConstants::cacheLineSize) JobGraphNode nodeStorage[8]{};
    JobGraphNode* edgeStorage[8 * 4]{};

    JobGraph graph{};
    ASSERT_EQ(graph.initializeWithBuffers(nodeStorage, 8u, edgeStorage, 4u), JobSystemResult::Success);

    JobSystem jobSystem{};
    JobSystemDesc sysDesc{};
    sysDesc.workerThreadCount = 2u;
    ASSERT_EQ(jobSystem.initialize(sysDesc), JobSystemResult::Success);

    // Submitting an empty initialized graph is a safe no-op.
    EXPECT_EQ(graph.executeAndWait(jobSystem), JobSystemResult::Success);

    JobGraphNodeId a = 0u;
    JobGraphNodeId b = 0u;
    JobGraphNodeId c = 0u;
    ASSERT_EQ(graph.addNode(&detail::noopGraphPassEntry, nullptr, &a), JobSystemResult::Success);
    ASSERT_EQ(graph.addNode(&detail::noopGraphPassEntry, nullptr, &b), JobSystemResult::Success);
    ASSERT_EQ(graph.addNode(&detail::noopGraphPassEntry, nullptr, &c), JobSystemResult::Success);

    // Wire a directed cycle: A -> B -> C -> A.
    ASSERT_EQ(graph.addDependency(a, b), JobSystemResult::Success);
    ASSERT_EQ(graph.addDependency(b, c), JobSystemResult::Success);
    ASSERT_EQ(graph.addDependency(c, a), JobSystemResult::Success);

    EXPECT_EQ(graph.validateTopology(), JobSystemResult::GraphCycleDetected);
    EXPECT_EQ(graph.executeAndWait(jobSystem), JobSystemResult::GraphCycleDetected);

    jobSystem.shutdown();
}

TEST(JobGraphRDGPipelineTest, ExecutesMultiPassRDGDiamondAndReplaysAcrossFrames)
{
    JobSystem jobSystem{};
    JobSystemDesc sysDesc{};
    sysDesc.workerThreadCount = 4u;
    sysDesc.standardFiberCount = 32u;
    sysDesc.largeFiberCount = 8u;
    ASSERT_EQ(jobSystem.initialize(sysDesc), JobSystemResult::Success);

    gp::memory::LinearAllocator frameArena(128ULL * 1024ULL);
    ASSERT_TRUE(frameArena.isValid());

    JobGraph rdg{};
    ASSERT_EQ(rdg.initialize(frameArena, 16u, 8u), JobSystemResult::Success);

    detail::RDGPassExecutionTracker tracker{};
    tracker.jobSystem = &jobSystem;

    detail::RDGPassNodeArg passArgs[detail::RDGPassExecutionTracker::kPassCount]{};
    JobGraphNodeId passIds[detail::RDGPassExecutionTracker::kPassCount]{};
    JobCounter perNodeCounter(0u);

    for (gp::UInt32 p = 0u; p < detail::RDGPassExecutionTracker::kPassCount; ++p)
    {
        passArgs[p].tracker = &tracker;
        passArgs[p].passIndex = p;
        const JobDecl nodeDecl = JobDecl::make(
            &detail::rdgPassCallback,
            &passArgs[p],
            &perNodeCounter,
            JobPriority::High,
            FiberStackTier::Standard64KiB,
            static_cast<gp::UInt16>(JobFlags::RDGPass)
        );
        ASSERT_EQ(rdg.addNode(nodeDecl, &passIds[p]), JobSystemResult::Success);
    }

    // Pass 2 (DepthPrePass) -> Pass 3 (AsyncComputeHiZ) and Pass 4 (GBufferPass)
    ASSERT_EQ(rdg.addDependency(passIds[2], passIds[3]), JobSystemResult::Success);
    ASSERT_EQ(rdg.addDependency(passIds[2], passIds[4]), JobSystemResult::Success);

    // Pass 3 (HiZ) and Pass 4 (GBuffer) -> Pass 5 (SSAO)
    const JobGraphNodeId ssaoPrereqs[2] = { passIds[3], passIds[4] };
    ASSERT_EQ(rdg.addDependencies(ssaoPrereqs, 2u, passIds[5]), JobSystemResult::Success);

    // Pass 0 (Shadow0), Pass 1 (Shadow1), Pass 4 (GBuffer), Pass 5 (SSAO) -> Pass 6 (DeferredLighting)
    const JobGraphNodeId lightingPrereqs[4] = { passIds[0], passIds[1], passIds[4], passIds[5] };
    ASSERT_EQ(rdg.addDependencies(lightingPrereqs, 4u, passIds[6]), JobSystemResult::Success);

    // Pass 6 (DeferredLighting) -> Pass 7 (Bloom) -> Pass 8 (ToneMappingPresent)
    ASSERT_EQ(rdg.addDependency(passIds[6], passIds[7]), JobSystemResult::Success);
    ASSERT_EQ(rdg.addDependency(passIds[7], passIds[8]), JobSystemResult::Success);

    EXPECT_EQ(rdg.getRootNodeCount(), 3u);
    ASSERT_EQ(rdg.validateTopology(), JobSystemResult::Success);

    // Replay the compiled RDG across 3 simulated frames using rdg.reset().
    constexpr gp::UInt32 kSimulatedFrames = 3u;
    for (gp::UInt32 frame = 0u; frame < kSimulatedFrames; ++frame)
    {
        rdg.reset();
        ASSERT_EQ(perNodeCounter.reset(detail::RDGPassExecutionTracker::kPassCount), JobSyncResult::Success);

        CacheAlignedJobCounter frameCompletionCounter(0u);
        ASSERT_EQ(rdg.submit(jobSystem, &frameCompletionCounter.counter), JobSystemResult::Success);
        ASSERT_EQ(jobSystem.waitForCounter(frameCompletionCounter, 0u), JobSystemResult::Success);
        EXPECT_EQ(perNodeCounter.getCount(), 0u);

        EXPECT_LT(
            tracker.passFinishTicket[2].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[3].load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            tracker.passFinishTicket[2].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[4].load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            tracker.passFinishTicket[3].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[5].load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            tracker.passFinishTicket[4].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[5].load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            tracker.passFinishTicket[0].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[6].load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            tracker.passFinishTicket[1].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[6].load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            tracker.passFinishTicket[5].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[6].load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            tracker.passFinishTicket[6].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[7].load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            tracker.passFinishTicket[7].load(gp::MemoryOrder::Acquire),
            tracker.passStartTicket[8].load(gp::MemoryOrder::Acquire)
        );
    }

    for (gp::UInt32 p = 0u; p < detail::RDGPassExecutionTracker::kPassCount; ++p)
    {
        EXPECT_EQ(tracker.passExecutionCount[p].load(gp::MemoryOrder::Acquire), kSimulatedFrames);
    }

    EXPECT_EQ(
        tracker.gbufferRecordedDraws.load(gp::MemoryOrder::Acquire),
        kSimulatedFrames * detail::RDGPassExecutionTracker::kGBufferDrawCount
    );

    rdg.shutdown();
    jobSystem.shutdown();
}

TEST(JobGraphRDGStressTest, Executes256PassDAGOverTinyFiberPoolWithZeroFiberExhaustion)
{
    // Configure 6 Standard64KiB fibers + 2 Large512KiB fibers (8 total fibers) and execute a 288-pass DAG
    // (exceeding the 256-node stack-scratch threshold in validateTopology). Because JobGraphNode consumes no fibers
    // until its prerequisites reach 0, all 288 passes execute without exhausting the fiber pool.
    JobSystem jobSystem{};
    JobSystemDesc sysDesc{};
    sysDesc.workerThreadCount = 4u;
    sysDesc.standardFiberCount = 6u;
    sysDesc.largeFiberCount = 2u;
    ASSERT_EQ(jobSystem.initialize(sysDesc), JobSystemResult::Success);

    gp::memory::LinearAllocator arena(256ULL * 1024ULL);
    ASSERT_TRUE(arena.isValid());

    JobGraph rdg{};
    ASSERT_EQ(rdg.initialize(arena, detail::DeepRDGStressContext::kTotalPasses, 8u), JobSystemResult::Success);

    detail::DeepRDGStressContext stressCtx{};
    detail::DeepRDGNodePayload payloads[detail::DeepRDGStressContext::kTotalPasses]{};
    JobGraphNodeId nodeIds[detail::DeepRDGStressContext::kTierCount][detail::DeepRDGStressContext::kPassesPerTier]{};

    for (gp::UInt32 tier = 0u; tier < detail::DeepRDGStressContext::kTierCount; ++tier)
    {
        for (gp::UInt32 idx = 0u; idx < detail::DeepRDGStressContext::kPassesPerTier; ++idx)
        {
            const gp::UInt32 flatIdx = (tier * detail::DeepRDGStressContext::kPassesPerTier) + idx;
            payloads[flatIdx].ctx = &stressCtx;
            payloads[flatIdx].tierIndex = tier;

            ASSERT_EQ(
                rdg.addNode(&detail::deepRDGStressPassEntry, &payloads[flatIdx], &nodeIds[tier][idx]),
                JobSystemResult::Success
            );

            if (tier > 0u)
            {
                const JobGraphNodeId parentA = nodeIds[tier - 1u][idx];
                const JobGraphNodeId parentB =
                    nodeIds[tier - 1u][(idx + 1u) % detail::DeepRDGStressContext::kPassesPerTier];
                ASSERT_EQ(rdg.addDependency(parentA, nodeIds[tier][idx]), JobSystemResult::Success);
                ASSERT_EQ(rdg.addDependency(parentB, nodeIds[tier][idx]), JobSystemResult::Success);
            }
        }
    }

    ASSERT_EQ(rdg.executeAndWait(jobSystem), JobSystemResult::Success);
    EXPECT_EQ(stressCtx.executedPasses.load(gp::MemoryOrder::Acquire), detail::DeepRDGStressContext::kTotalPasses);
    EXPECT_EQ(stressCtx.orderingViolations.load(gp::MemoryOrder::Acquire), 0u);

    rdg.shutdown();
    jobSystem.shutdown();
}

}   // namespace gp::thread::tests
