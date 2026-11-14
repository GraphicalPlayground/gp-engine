// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "ApplicationDesc.hpp"
#include "ApplicationTypes.hpp"
#include "memory/MemoryOrder.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "threading/atomics/Atomic.hpp"   // IWYU pragma: keep
#include "threading/jobs/JobGraph.hpp"
#include "threading/jobs/JobSystem.hpp"
#include "threading/jobs/ParallelFor.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::tests
{

namespace detail
{

/// @brief Cross-frame canary payload allocated inside `getCurrentFrameArena()` on Frame `N` and verified via
///        `getPreviousFrameArena()` on Frame `N + 1`.
struct FrameArenaCanary
{
    /// @brief Frame index (`timeStats.frameIndex`) during which this canary was allocated.
    gp::UInt64 birthFrameIndex{ 0ULL };

    /// @brief Deterministic magic pattern XORed with `birthFrameIndex`.
    gp::UInt64 magicValue{ 0ULL };
};

/// @brief Stateless `ParallelForFunction` used during `onInitialize` and `onUpdate` to sum `startIndex .. +count`.
/// @param[in] startIndex Inclusive batch start index.
/// @param[in] count Batch element count.
/// @param[in] rawAccumulator Pointer to `Atomic<gp::UInt64>`.
/// @param[in] context Active worker and fiber execution context.
static void parallelSumBatchCallback(
    const gp::UInt32 startIndex, const gp::UInt32 count, void* const rawAccumulator, const thread::JobContext& context
) noexcept
{
    EXPECT_TRUE(context.isValid());
    Atomic<gp::UInt64>* const accumulator = static_cast<Atomic<gp::UInt64>*>(rawAccumulator);
    gp::UInt64 localSum = 0ULL;
    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        localSum += static_cast<gp::UInt64>(startIndex + i + 1u);
    }
    accumulator->fetchAdd(localSum, gp::MemoryOrder::AcqRel);
}

/// @brief Headless test application verifying `JobSystem` availability in `onInitialize`/`onShutdown` and
///        double-buffered `LinearAllocator` arena preservation across frames.
class ArenaAndJobLifecycleApp final : public Application
{
public:
    /// @brief Constructs the test application.
    /// @param[in] desc Application configuration descriptor.
    explicit ArenaAndJobLifecycleApp(const ApplicationDesc& desc) noexcept
        : Application(desc)
    {}

    /// @brief Destructor.
    virtual ~ArenaAndJobLifecycleApp() noexcept override = default;

    /// @brief Queries the sum accumulated by `parallelFor` inside `onInitialize()`.
    /// @return Accumulated initialization sum.
    [[nodiscard]] gp::UInt64 getInitParallelSum() const noexcept
    {
        return m_initParallelSum.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Queries the number of frames where the previous frame's arena canary was verified intact.
    /// @return Verified cross-frame canary count.
    [[nodiscard]] gp::UInt32 getVerifiedPreviousFrameCanaries() const noexcept
    {
        return m_verifiedPreviousFrameCanaries;
    }

    /// @brief Queries whether `getJobSystem().isInitialized()` was still true when `onShutdown()` executed.
    /// @return True if the job system was active during `onShutdown()`.
    [[nodiscard]] bool wasJobSystemActiveInShutdown() const noexcept
    {
        return m_jobSystemActiveInShutdown;
    }

protected:
    /// @copydoc Application::onInitialize()
    [[nodiscard]] virtual bool onInitialize() noexcept override
    {
        if (!getJobSystem().isInitialized() || !getCurrentFrameArena().isValid() ||
            !getPreviousFrameArena().isValid() || !getFrameGraph().isValid())
        {
            return false;
        }

        // Dispatch a parallelFor wave right inside onInitialize() to verify startup readiness.
        const thread::JobSystemResult pfRes =
            getJobSystem().parallelFor(64u, 8u, &parallelSumBatchCallback, &m_initParallelSum);
        return pfRes == thread::JobSystemResult::Success;
    }

    /// @copydoc Application::onUpdate()
    virtual void onUpdate(const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        // Verify that the canary allocated in getCurrentFrameArena() during Frame N-1 is still intact in
        // getPreviousFrameArena() during Frame N.
        if (m_lastCanary != nullptr && timeStats.frameIndex > 1ULL)
        {
            const gp::UInt64 expectedFrame = timeStats.frameIndex - 1ULL;
            const gp::UInt64 expectedMagic = 0x55AA55AA12345678ULL ^ expectedFrame;
            EXPECT_TRUE(getPreviousFrameArena().owns(m_lastCanary));
            EXPECT_FALSE(getCurrentFrameArena().owns(m_lastCanary));
            if (m_lastCanary->birthFrameIndex == expectedFrame && m_lastCanary->magicValue == expectedMagic)
            {
                ++m_verifiedPreviousFrameCanaries;
            }
        }

        // Allocate a fresh canary from the active frame's arena for the next frame to inspect.
        void* const rawMem = getCurrentFrameArena().allocate(sizeof(FrameArenaCanary), alignof(FrameArenaCanary));
        ASSERT_NE(rawMem, nullptr);
        FrameArenaCanary* const canary = static_cast<FrameArenaCanary*>(rawMem);
        canary->birthFrameIndex = timeStats.frameIndex;
        canary->magicValue = 0x55AA55AA12345678ULL ^ timeStats.frameIndex;
        m_lastCanary = canary;

        if (timeStats.frameIndex >= 4ULL)
        {
            requestQuit();
        }
    }

    /// @copydoc Application::onShutdown()
    virtual void onShutdown() noexcept override
    {
        m_jobSystemActiveInShutdown = getJobSystem().isInitialized();
    }

private:
    Atomic<gp::UInt64> m_initParallelSum{ 0ULL };
    const FrameArenaCanary* m_lastCanary{ nullptr };
    gp::UInt32 m_verifiedPreviousFrameCanaries{ 0u };
    bool m_jobSystemActiveInShutdown{ false };
};

/// @brief Shared state updated by the per-frame update DAG and render DAG passes in `PhasedDAGTestApp`.
struct FrameDAGSharedTracker
{
    thread::JobSystem* jobSystem{ nullptr };
    Atomic<gp::UInt32> timelineCursor{ 1u };
    Atomic<gp::UInt32> simPassFinishTicket{ 0u };
    Atomic<gp::UInt32> cullPassStartTicket{ 0u };
    Atomic<gp::UInt32> cullPassFinishTicket{ 0u };
    Atomic<gp::UInt32> extractPassStartTicket{ 0u };
    Atomic<gp::UInt32> culledObjectSum{ 0u };
    Atomic<gp::UInt32> depthPrePassFinishTicket{ 0u };
    Atomic<gp::UInt32> gbufferPassStartTicket{ 0u };
    Atomic<gp::UInt32> hizPassStartTicket{ 0u };
    Atomic<gp::UInt32> lightingPassStartTicket{ 0u };
    Atomic<gp::UInt32> completedUpdateGraphs{ 0u };
    Atomic<gp::UInt32> completedRenderGraphs{ 0u };
};

/// @brief Batch callback invoked inside `cullPassEntry` to simulate parallel frustum culling.
/// @param[in] startIndex Batch starting index (unused).
/// @param[in] count Number of objects in this batch.
/// @param[in] rawTracker Pointer to `FrameDAGSharedTracker`.
/// @param[in] context Active worker and fiber context.
static void frustumCullBatchCallback(
    [[maybe_unused]] const gp::UInt32 startIndex,
    const gp::UInt32 count,
    void* const rawTracker,
    const thread::JobContext& context
) noexcept
{
    EXPECT_TRUE(context.isValid());
    FrameDAGSharedTracker* const tracker = static_cast<FrameDAGSharedTracker*>(rawTracker);
    tracker->culledObjectSum.fetchAdd(count, gp::MemoryOrder::AcqRel);
}

/// @brief Root pass in `onBuildFrameGraph` simulating transform/kinematic updates.
/// @param[in] rawTracker Pointer to `FrameDAGSharedTracker`.
static void simPassEntry(void* const rawTracker) noexcept
{
    FrameDAGSharedTracker* const tracker = static_cast<FrameDAGSharedTracker*>(rawTracker);
    const gp::UInt32 finishTicket = tracker->timelineCursor.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->simPassFinishTicket.store(finishTicket, gp::MemoryOrder::Release);
}

/// @brief Middle pass in `onBuildFrameGraph` that depends on `simPassEntry` and fans out a nested `parallelFor`.
/// @param[in] rawTracker Pointer to `FrameDAGSharedTracker`.
static void cullPassEntry(void* const rawTracker) noexcept
{
    FrameDAGSharedTracker* const tracker = static_cast<FrameDAGSharedTracker*>(rawTracker);
    const gp::UInt32 startTicket = tracker->timelineCursor.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->cullPassStartTicket.store(startTicket, gp::MemoryOrder::Release);

    EXPECT_EQ(
        tracker->jobSystem->parallelFor(128u, 16u, &frustumCullBatchCallback, tracker, thread::JobPriority::High),
        thread::JobSystemResult::Success
    );

    const gp::UInt32 finishTicket = tracker->timelineCursor.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->cullPassFinishTicket.store(finishTicket, gp::MemoryOrder::Release);
}

/// @brief Terminal pass in `onBuildFrameGraph` that depends on `cullPassEntry`.
/// @param[in] rawTracker Pointer to `FrameDAGSharedTracker`.
static void extractPassEntry(void* const rawTracker) noexcept
{
    FrameDAGSharedTracker* const tracker = static_cast<FrameDAGSharedTracker*>(rawTracker);
    const gp::UInt32 startTicket = tracker->timelineCursor.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->extractPassStartTicket.store(startTicket, gp::MemoryOrder::Release);
    tracker->completedUpdateGraphs.fetchAdd(1u, gp::MemoryOrder::AcqRel);
}

/// @brief Root pass in `onBuildRenderGraph` simulating RDG Depth Pre-Pass.
/// @param[in] rawTracker Pointer to `FrameDAGSharedTracker`.
static void rdgDepthPrePassEntry(void* const rawTracker) noexcept
{
    FrameDAGSharedTracker* const tracker = static_cast<FrameDAGSharedTracker*>(rawTracker);
    const gp::UInt32 finishTicket = tracker->timelineCursor.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->depthPrePassFinishTicket.store(finishTicket, gp::MemoryOrder::Release);
}

/// @brief Parallel RDG GBuffer pass depending on `rdgDepthPrePassEntry`.
/// @param[in] rawTracker Pointer to `FrameDAGSharedTracker`.
static void rdgGBufferPassEntry(void* const rawTracker) noexcept
{
    FrameDAGSharedTracker* const tracker = static_cast<FrameDAGSharedTracker*>(rawTracker);
    const gp::UInt32 startTicket = tracker->timelineCursor.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->gbufferPassStartTicket.store(startTicket, gp::MemoryOrder::Release);
}

/// @brief Parallel RDG Hi-Z pyramid pass depending on `rdgDepthPrePassEntry`.
/// @param[in] rawTracker Pointer to `FrameDAGSharedTracker`.
static void rdgHiZPassEntry(void* const rawTracker) noexcept
{
    FrameDAGSharedTracker* const tracker = static_cast<FrameDAGSharedTracker*>(rawTracker);
    const gp::UInt32 startTicket = tracker->timelineCursor.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->hizPassStartTicket.store(startTicket, gp::MemoryOrder::Release);
}

/// @brief Join RDG lighting pass depending on both `rdgGBufferPassEntry` and `rdgHiZPassEntry`.
/// @param[in] rawTracker Pointer to `FrameDAGSharedTracker`.
static void rdgLightingPassEntry(void* const rawTracker) noexcept
{
    FrameDAGSharedTracker* const tracker = static_cast<FrameDAGSharedTracker*>(rawTracker);
    const gp::UInt32 startTicket = tracker->timelineCursor.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    tracker->lightingPassStartTicket.store(startTicket, gp::MemoryOrder::Release);
    tracker->completedRenderGraphs.fetchAdd(1u, gp::MemoryOrder::AcqRel);
}

/// @brief Headless test application exercising `onBuildFrameGraph`, `onBuildRenderGraph`, and `onEndFrame`.
class PhasedDAGTestApp final : public Application
{
public:
    /// @brief Constructs the DAG test application.
    /// @param[in] desc Application configuration descriptor.
    explicit PhasedDAGTestApp(const ApplicationDesc& desc) noexcept
        : Application(desc)
    {}

    /// @brief Destructor.
    virtual ~PhasedDAGTestApp() noexcept override = default;

    /// @brief Retrieves the shared DAG tracker state.
    /// @return Const reference to `FrameDAGSharedTracker`.
    [[nodiscard]] const FrameDAGSharedTracker& getTracker() const noexcept
    {
        return m_tracker;
    }

    /// @brief Queries the number of frames verified inside `onEndFrame()`.
    /// @return End-of-frame verification count.
    [[nodiscard]] gp::UInt32 getEndFrameCount() const noexcept
    {
        return m_endFrameCount;
    }

protected:
    /// @copydoc Application::onInitialize()
    [[nodiscard]] virtual bool onInitialize() noexcept override
    {
        m_tracker.jobSystem = &getJobSystem();
        return true;
    }

    /// @copydoc Application::onBuildFrameGraph()
    virtual void onBuildFrameGraph(
        gp::thread::JobGraph& frameGraph, [[maybe_unused]] const ApplicationFrameContext& frameContext
    ) noexcept override
    {
        thread::JobGraphNodeId simNode = thread::kInvalidJobGraphNodeId;
        thread::JobGraphNodeId cullNode = thread::kInvalidJobGraphNodeId;
        thread::JobGraphNodeId extractNode = thread::kInvalidJobGraphNodeId;

        ASSERT_EQ(frameGraph.addNode(&simPassEntry, &m_tracker, &simNode), thread::JobSystemResult::Success);
        ASSERT_EQ(frameGraph.addNode(&cullPassEntry, &m_tracker, &cullNode), thread::JobSystemResult::Success);
        ASSERT_EQ(frameGraph.addNode(&extractPassEntry, &m_tracker, &extractNode), thread::JobSystemResult::Success);

        ASSERT_EQ(frameGraph.addDependency(simNode, cullNode), thread::JobSystemResult::Success);
        ASSERT_EQ(frameGraph.addDependency(cullNode, extractNode), thread::JobSystemResult::Success);
    }

    /// @copydoc Application::onBuildRenderGraph()
    virtual void onBuildRenderGraph(
        gp::thread::JobGraph& renderGraph, [[maybe_unused]] const ApplicationFrameContext& frameContext
    ) noexcept override
    {
        thread::JobGraphNodeId depthNode = thread::kInvalidJobGraphNodeId;
        thread::JobGraphNodeId gbufferNode = thread::kInvalidJobGraphNodeId;
        thread::JobGraphNodeId hizNode = thread::kInvalidJobGraphNodeId;
        thread::JobGraphNodeId lightingNode = thread::kInvalidJobGraphNodeId;

        ASSERT_EQ(renderGraph.addNode(&rdgDepthPrePassEntry, &m_tracker, &depthNode), thread::JobSystemResult::Success);
        ASSERT_EQ(
            renderGraph.addNode(&rdgGBufferPassEntry, &m_tracker, &gbufferNode), thread::JobSystemResult::Success
        );
        ASSERT_EQ(renderGraph.addNode(&rdgHiZPassEntry, &m_tracker, &hizNode), thread::JobSystemResult::Success);
        ASSERT_EQ(
            renderGraph.addNode(&rdgLightingPassEntry, &m_tracker, &lightingNode), thread::JobSystemResult::Success
        );

        ASSERT_EQ(renderGraph.addDependency(depthNode, gbufferNode), thread::JobSystemResult::Success);
        ASSERT_EQ(renderGraph.addDependency(depthNode, hizNode), thread::JobSystemResult::Success);

        const thread::JobGraphNodeId lightingPrereqs[2] = { gbufferNode, hizNode };
        ASSERT_EQ(renderGraph.addDependencies(lightingPrereqs, 2u, lightingNode), thread::JobSystemResult::Success);
    }

    /// @copydoc Application::onEndFrame()
    virtual void onEndFrame(const ApplicationFrameContext& frameContext) noexcept override
    {
        EXPECT_LT(
            m_tracker.simPassFinishTicket.load(gp::MemoryOrder::Acquire),
            m_tracker.cullPassStartTicket.load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            m_tracker.cullPassFinishTicket.load(gp::MemoryOrder::Acquire),
            m_tracker.extractPassStartTicket.load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            m_tracker.extractPassStartTicket.load(gp::MemoryOrder::Acquire),
            m_tracker.depthPrePassFinishTicket.load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            m_tracker.depthPrePassFinishTicket.load(gp::MemoryOrder::Acquire),
            m_tracker.gbufferPassStartTicket.load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            m_tracker.depthPrePassFinishTicket.load(gp::MemoryOrder::Acquire),
            m_tracker.hizPassStartTicket.load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            m_tracker.gbufferPassStartTicket.load(gp::MemoryOrder::Acquire),
            m_tracker.lightingPassStartTicket.load(gp::MemoryOrder::Acquire)
        );
        EXPECT_LT(
            m_tracker.hizPassStartTicket.load(gp::MemoryOrder::Acquire),
            m_tracker.lightingPassStartTicket.load(gp::MemoryOrder::Acquire)
        );

        ++m_endFrameCount;
        if (frameContext.getFrameIndex() >= 3ULL)
        {
            requestQuit();
        }
    }

private:
    FrameDAGSharedTracker m_tracker{};
    gp::UInt32 m_endFrameCount{ 0u };
};

/// @brief Minimal headless application that exits after one frame and captures `JobSystem` state for CLI tests.
class CommandLineProbeApp final : public Application
{
public:
    /// @brief Constructs the CLI probe application.
    /// @param[in] desc Application configuration descriptor.
    explicit CommandLineProbeApp(const ApplicationDesc& desc) noexcept
        : Application(desc)
    {}

    /// @brief Destructor.
    virtual ~CommandLineProbeApp() noexcept override = default;

    /// @brief Queries whether `getJobSystem().isInitialized()` was true during `onInitialize()`.
    /// @return True if `JobSystem` was initialized.
    [[nodiscard]] bool wasJobSystemInitialized() const noexcept
    {
        return m_wasInitialized;
    }

    /// @brief Queries the worker thread count observed during `onInitialize()`.
    /// @return Observed worker thread count.
    [[nodiscard]] gp::UInt32 getObservedWorkerCount() const noexcept
    {
        return m_observedWorkerCount;
    }

protected:
    /// @copydoc Application::onInitialize()
    [[nodiscard]] virtual bool onInitialize() noexcept override
    {
        m_wasInitialized = getJobSystem().isInitialized();
        m_observedWorkerCount = getJobSystem().getWorkerThreadCount();
        return true;
    }

    /// @copydoc Application::onUpdate()
    virtual void onUpdate([[maybe_unused]] const gp::time::FrameTimeStats& timeStats) noexcept override
    {
        requestQuit();
    }

private:
    gp::UInt32 m_observedWorkerCount{ 0u };
    bool m_wasInitialized{ false };
};

/// @brief Headless application that aborts initialization in `onInitialize()` to verify cleanup and exit codes.
class FailingInitApp final : public Application
{
public:
    /// @brief Constructs the failing-init test application.
    /// @param[in] desc Application configuration descriptor.
    explicit FailingInitApp(const ApplicationDesc& desc) noexcept
        : Application(desc)
    {}

    /// @brief Destructor.
    virtual ~FailingInitApp() noexcept override = default;

protected:
    /// @copydoc Application::onInitialize()
    [[nodiscard]] virtual bool onInitialize() noexcept override
    {
        return false;
    }
};

}   // namespace detail

TEST(ApplicationLifecycleTest, HeadlessJobSystemAndDoubleBufferedFrameArenas)
{
    CommandLine::shutdown();

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;
    desc.frameArenaBytes = 256ULL * 1024ULL;
    desc.jobSystem.workerThreadCount = 2u;
    desc.jobSystem.standardFiberCount = 16u;
    desc.jobSystem.largeFiberCount = 4u;

    detail::ArenaAndJobLifecycleApp app(desc);
    EXPECT_EQ(app.run(), 0);

    // Sum of 1..64 == (64 * 65) / 2 == 2080.
    EXPECT_EQ(app.getInitParallelSum(), 2080ULL);
    // Frames 2, 3, and 4 each verified the canary allocated in the previous frame's arena.
    EXPECT_EQ(app.getVerifiedPreviousFrameCanaries(), 3u);
    EXPECT_TRUE(app.wasJobSystemActiveInShutdown());
    EXPECT_FALSE(app.getJobSystem().isInitialized());
}

TEST(ApplicationFrameGraphTest, ExecutesUpdateAndRenderDAGPhasesAcrossFrames)
{
    CommandLine::shutdown();

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;
    desc.frameArenaBytes = 256ULL * 1024ULL;
    desc.maxFrameGraphNodes = 64u;
    desc.maxFrameGraphDependentsPerNode = 8u;
    desc.jobSystem.workerThreadCount = 3u;
    desc.jobSystem.standardFiberCount = 24u;
    desc.jobSystem.largeFiberCount = 4u;

    detail::PhasedDAGTestApp app(desc);
    EXPECT_EQ(app.run(), 0);

    EXPECT_EQ(app.getEndFrameCount(), 3u);
    EXPECT_EQ(app.getTracker().completedUpdateGraphs.load(gp::MemoryOrder::Acquire), 3u);
    EXPECT_EQ(app.getTracker().completedRenderGraphs.load(gp::MemoryOrder::Acquire), 3u);
    EXPECT_EQ(app.getTracker().culledObjectSum.load(gp::MemoryOrder::Acquire), 3u * 128u);
}

TEST(ApplicationCommandLineTest, AppliesThreadsAndNoJobSystemOverrides)
{
    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;
    desc.frameArenaBytes = 128ULL * 1024ULL;
    desc.jobSystem.workerThreadCount = 4u;
    desc.jobSystem.standardFiberCount = 8u;
    desc.jobSystem.largeFiberCount = 2u;

    CommandLine::initialize("gp-tests -threads=2 -no-core-affinity");
    {
        detail::CommandLineProbeApp app(desc);
        EXPECT_EQ(app.run(), 0);
        EXPECT_TRUE(app.wasJobSystemInitialized());
        EXPECT_EQ(app.getObservedWorkerCount(), 2u);
    }

    CommandLine::initialize("gp-tests -no-jobsystem");
    {
        detail::CommandLineProbeApp app(desc);
        EXPECT_EQ(app.run(), 0);
        EXPECT_FALSE(app.wasJobSystemInitialized());
        EXPECT_EQ(app.getObservedWorkerCount(), 0u);
    }

    CommandLine::shutdown();
}

TEST(ApplicationLifecycleTest, AbortOnInitializeReturnsNegativeExitCodeAndCleansUp)
{
    CommandLine::shutdown();

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.jobSystem.workerThreadCount = 2u;
    desc.jobSystem.standardFiberCount = 8u;
    desc.jobSystem.largeFiberCount = 2u;

    detail::FailingInitApp app(desc);
    EXPECT_EQ(app.run(), -2);
    EXPECT_FALSE(app.getJobSystem().isInitialized());
    EXPECT_FALSE(app.getCurrentFrameArena().isValid());
    EXPECT_FALSE(app.getPreviousFrameArena().isValid());
}

}   // namespace gp::tests
