// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "ApplicationDesc.hpp"
#include "ApplicationTypes.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"   // IWYU pragma: keep
#include "containers/strings/FixedString.hpp"
#include "logging/sinks/MemoryRingSink.hpp"
#include "memory/MemoryOrder.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "modules/Modules.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/base/system/Thread.hpp"
#include "templates/ScopeExit.hpp"        // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"   // IWYU pragma: keep
#include "threading/jobs/JobGraph.hpp"
#include "threading/jobs/JobSystem.hpp"
#include "threading/jobs/ParallelFor.hpp"   // IWYU pragma: keep
#include "threading/roles/NamedThreadDispatcher.hpp"
#include "threading/roles/ScopedThreadRole.hpp"
#include "threading/roles/ThreadRole.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "vfs/MemoryMountBackend.hpp"
#include "vfs/VirtualFileSystem.hpp"
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
    desc.rhi.forceNullRHI = true;
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
    desc.rhi.forceNullRHI = true;
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
    desc.rhi.forceNullRHI = true;
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
    desc.rhi.forceNullRHI = true;
    desc.jobSystem.workerThreadCount = 2u;
    desc.jobSystem.standardFiberCount = 8u;
    desc.jobSystem.largeFiberCount = 2u;

    detail::FailingInitApp app(desc);
    EXPECT_EQ(app.run(), -2);
    EXPECT_FALSE(app.getJobSystem().isInitialized());
    EXPECT_FALSE(app.getCurrentFrameArena().isValid());
    EXPECT_FALSE(app.getPreviousFrameArena().isValid());
}

TEST(ApplicationLoggingTest, InitializesLoggerRoutesLifecycleLogsAndCleansUpSinks)
{
    CommandLine::shutdown();
    log::Logger& logger = Application::getLogger();
    logger.shutdown();
    logger.clearSinks();

    log::MemoryRingSink ringSink(log::LogLevel::Trace);
    ASSERT_EQ(logger.addSink(&ringSink), log::LogResult::Success);

    LogApplication.setActiveLevel(log::LogLevel::Debug);

    ApplicationDesc desc{};
    desc.backend.appName = "TestLoggingHost";
    desc.backend.appVersion = "2.0.0";
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableJobSystem = false;
    desc.enableConsoleLogSink = false;
    desc.enableFileLogSink = false;
    desc.enableDebugOutputLogSink = false;
    desc.enableTracyLogSink = false;
    desc.logger.mode = log::LogExecutionMode::Synchronous;

    detail::CommandLineProbeApp app(desc);
    EXPECT_EQ(app.run(), 0);

    // Verify lifecycle log messages were emitted under LogApplication and captured by ringSink.
    EXPECT_GE(ringSink.getSize(), 2u);
    EXPECT_EQ(ringSink.getEntry(0u).toView().categoryName, "LogApplication");
    EXPECT_TRUE(ringSink.getEntry(0u).toView().message.contains("TestLoggingHost"));

    // Verify Application shut down the Logger it initialized and left only our test sink registered.
    EXPECT_FALSE(logger.isInitialized());
    EXPECT_EQ(logger.getSinkCount(), 1u);

    LogApplication.resetToDefaultLevel();
    logger.clearSinks();
}

TEST(ApplicationConsoleWiringTest, FrameSyncLatchFlushSinksAndRuntimeControls)
{
    CommandLine::shutdown();

    ApplicationDesc desc{};
    desc.backend.appName = "ConsoleWiringApp";
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableLogging = false;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;
    desc.frameArenaBytes = 128ULL * 1024ULL;
    desc.jobSystem.workerThreadCount = 1u;
    desc.jobSystem.standardFiberCount = 4u;
    desc.jobSystem.largeFiberCount = 2u;

    class ConsoleSyncProbeApp final : public Application
    {
    public:
        explicit ConsoleSyncProbeApp(const ApplicationDesc& inDesc) noexcept
            : Application(inDesc)
            , m_renderSyncVar("test.AppRenderSync", 10, "render sync probe")
            , m_latchedVar("test.AppLatched", 1, "latched probe", console::ConsoleVarFlags::Latched)
            , m_sink(
                  console::ConsoleSinkDelegate::createLambda(
                      [this]() noexcept -> void
        {
            ++m_sinkHits;
        }
                  )
              )
        {}

        [[nodiscard]] bool hadPreRenderStaleRenderSlotOnFrame1() const noexcept
        {
            return m_sawStaleRenderSlotDuringUpdate;
        }

        [[nodiscard]] bool hadSyncedRenderSlotOnFrame1Render() const noexcept
        {
            return m_sawSyncedRenderSlotInRender;
        }

        [[nodiscard]] bool hadCommittedLatchedOnFrame2() const noexcept
        {
            return m_sawCommittedLatchedOnFrame2;
        }

        [[nodiscard]] gp::Int32 getSinkHitCount() const noexcept
        {
            return m_sinkHits;
        }

        [[nodiscard]] bool wasClockPausedAfterCommand() const noexcept
        {
            return m_clockPausedObserved;
        }

        [[nodiscard]] gp::Float64 getObservedTimeScale() const noexcept
        {
            return m_observedTimeScale;
        }

        [[nodiscard]] time::DeltaSmoothingMode getObservedSmoothingMode() const noexcept
        {
            return m_observedSmoothingMode;
        }

        [[nodiscard]] gp::StringView getAppStatsLine() const noexcept
        {
            return m_appStatsOut.view();
        }

        [[nodiscard]] gp::StringView getJobsStatsLine() const noexcept
        {
            return m_jobsStatsOut.view();
        }

    protected:
        [[nodiscard]] virtual bool onInitialize() noexcept override
        {
            const console::IConsoleManager& console = getConsoleManager();
            EXPECT_NE(console.findConsoleVariable("app.TargetFps"), nullptr);
            EXPECT_NE(console.findConsoleVariable("app.BackgroundFps"), nullptr);
            EXPECT_NE(console.findConsoleVariable("app.SleepWhenMinimized"), nullptr);
            EXPECT_NE(console.findConsoleVariable("t.TimeScale"), nullptr);
            EXPECT_NE(console.findConsoleVariable("t.Paused"), nullptr);
            EXPECT_NE(console.findConsoleVariable("t.SmoothingMode"), nullptr);
            EXPECT_NE(console.findConsoleCommand("t.Step"), nullptr);
            EXPECT_NE(console.findConsoleCommand("app.Quit"), nullptr);
            EXPECT_NE(console.findConsoleCommand("app.Stats"), nullptr);
            EXPECT_NE(console.findConsoleCommand("jobs.Stats"), nullptr);
            return true;
        }

        virtual void onUpdate(const gp::time::FrameTimeStats& timeStats) noexcept override
        {
            if (timeStats.frameIndex == 1ULL)
            {
                EXPECT_EQ(m_renderSyncVar.set(42), console::ConsoleResult::Success);
                EXPECT_EQ(m_renderSyncVar.getValueOnGameThread(), 42);
                m_sawStaleRenderSlotDuringUpdate = (m_renderSyncVar.getValueOnRenderThread() == 10);

                EXPECT_EQ(m_latchedVar.set(99), console::ConsoleResult::Success);
                EXPECT_EQ(m_latchedVar.getValueOnGameThread(), 1);
            }
            else if (timeStats.frameIndex == 2ULL)
            {
                m_sawCommittedLatchedOnFrame2 = (m_latchedVar.getValueOnGameThread() == 99);

                console::ConsoleOutputDelegate appOut = console::ConsoleOutputDelegate::createLambda(
                    [this](const gp::StringView line) noexcept -> void
                {
                    m_appStatsOut.assign(line.substr(0u, m_appStatsOut.capacity()));
                }
                );
                console::ConsoleOutputDelegate jobsOut = console::ConsoleOutputDelegate::createLambda(
                    [this](const gp::StringView line) noexcept -> void
                {
                    m_jobsStatsOut.assign(line.substr(0u, m_jobsStatsOut.capacity()));
                }
                );

                EXPECT_EQ(executeConsoleCommand("t.TimeScale 0.25"), console::ConsoleResult::Success);
                EXPECT_EQ(executeConsoleCommand("t.SmoothingMode EMA"), console::ConsoleResult::Success);
                EXPECT_EQ(executeConsoleCommand("t.Paused 1"), console::ConsoleResult::Success);
                EXPECT_EQ(executeConsoleCommand("t.Step"), console::ConsoleResult::Success);
                EXPECT_EQ(executeConsoleCommand("app.Stats", &appOut), console::ConsoleResult::Success);
                EXPECT_EQ(executeConsoleCommand("jobs.Stats", &jobsOut), console::ConsoleResult::Success);

                m_clockPausedObserved = getClock().getGameplayClock().isLocallyPaused();
                m_observedTimeScale = getClock().getGameplayClock().getLocalTimeScale();
                m_observedSmoothingMode = getClock().getSmoothingMode();

                EXPECT_EQ(executeConsoleCommand("app.Quit"), console::ConsoleResult::Success);
            }
        }

        virtual void onRender(const gp::time::FrameTimeStats& timeStats) noexcept override
        {
            if (timeStats.frameIndex == 1ULL)
            {
                m_sawSyncedRenderSlotInRender = (m_renderSyncVar.getValueOnRenderThread() == 42);
            }
        }

    private:
        console::AutoConsoleVariable<gp::Int32> m_renderSyncVar;
        console::AutoConsoleVariable<gp::Int32> m_latchedVar;
        console::AutoConsoleVariableSink m_sink;
        gp::FixedString<383> m_appStatsOut{};
        gp::FixedString<383> m_jobsStatsOut{};
        gp::Int32 m_sinkHits{ 0 };
        gp::Float64 m_observedTimeScale{ 1.0 };
        time::DeltaSmoothingMode m_observedSmoothingMode{ time::DeltaSmoothingMode::OutlierRejectedAverage };
        bool m_sawStaleRenderSlotDuringUpdate{ false };
        bool m_sawSyncedRenderSlotInRender{ false };
        bool m_sawCommittedLatchedOnFrame2{ false };
        bool m_clockPausedObserved{ false };
    };

    {
        ConsoleSyncProbeApp app(desc);
        EXPECT_EQ(app.run(), 0);
        EXPECT_TRUE(app.hadPreRenderStaleRenderSlotOnFrame1());
        EXPECT_TRUE(app.hadSyncedRenderSlotOnFrame1Render());
        EXPECT_TRUE(app.hadCommittedLatchedOnFrame2());
        EXPECT_GE(app.getSinkHitCount(), 1);
        EXPECT_TRUE(app.wasClockPausedAfterCommand());
        EXPECT_DOUBLE_EQ(app.getObservedTimeScale(), 0.25);
        EXPECT_EQ(app.getObservedSmoothingMode(), time::DeltaSmoothingMode::ExponentialMovingAverage);
        EXPECT_NE(app.getAppStatsLine().find("ConsoleWiringApp"), StringView::npos);
        EXPECT_NE(app.getJobsStatsLine().find("Jobs[workers=1"), StringView::npos);
    }

    // All dynamic Application CVars & commands must be cleanly unregistered on shutdown.
    const console::IConsoleManager& console = Application::getConsoleManager();
    EXPECT_EQ(console.findConsoleVariable("app.TargetFps"), nullptr);
    EXPECT_EQ(console.findConsoleVariable("t.TimeScale"), nullptr);
    EXPECT_EQ(console.findConsoleCommand("app.Quit"), nullptr);
    EXPECT_EQ(console.findConsoleCommand("jobs.Stats"), nullptr);
}

TEST(ApplicationConsoleWiringTest, CommandLineOverridesApplyToAppAndClockOnBootstrap)
{
    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableLogging = false;
    desc.enableJobSystem = false;
    desc.targetForegroundFps = 120.0;
    desc.targetBackgroundFps = 30.0;
    desc.sleepWhenMinimized = true;

    CommandLine::initialize(
        "gp-tests +app.TargetFps=0 +app.SleepWhenMinimized=0 +t.TimeScale=0.5 -t.SmoothingMode=Raw"
    );

    class ConsoleCliProbeApp final : public Application
    {
    public:
        explicit ConsoleCliProbeApp(const ApplicationDesc& inDesc) noexcept
            : Application(inDesc)
        {}

        gp::Float64 observedTimeScale{ 1.0 };
        time::DeltaSmoothingMode observedMode{ time::DeltaSmoothingMode::OutlierRejectedAverage };
        gp::FixedString<127> unpausedStepMsg{};
        gp::FixedString<127> disabledJobsMsg{};

    protected:
        virtual void onUpdate(const gp::time::FrameTimeStats&) noexcept override
        {
            observedTimeScale = getClock().getGameplayClock().getLocalTimeScale();
            observedMode = getClock().getSmoothingMode();

            console::ConsoleOutputDelegate stepOut = console::ConsoleOutputDelegate::createLambda(
                [this](const gp::StringView line) noexcept -> void
            {
                unpausedStepMsg.assign(line.substr(0u, unpausedStepMsg.capacity()));
            }
            );
            console::ConsoleOutputDelegate jobsOut = console::ConsoleOutputDelegate::createLambda(
                [this](const gp::StringView line) noexcept -> void
            {
                disabledJobsMsg.assign(line.substr(0u, disabledJobsMsg.capacity()));
            }
            );
            EXPECT_EQ(executeConsoleCommand("t.Step", &stepOut), console::ConsoleResult::Success);
            EXPECT_EQ(executeConsoleCommand("jobs.Stats", &jobsOut), console::ConsoleResult::Success);
            EXPECT_EQ(executeConsoleCommand("app.BackgroundFps 0"), console::ConsoleResult::Success);
            requestQuit();
        }
    };

    ConsoleCliProbeApp app(desc);
    EXPECT_EQ(app.run(), 0);
    EXPECT_DOUBLE_EQ(app.observedTimeScale, 0.5);
    EXPECT_EQ(app.observedMode, time::DeltaSmoothingMode::None);
    EXPECT_NE(app.unpausedStepMsg.view().find("not paused"), StringView::npos);
    EXPECT_EQ(app.disabledJobsMsg.view(), StringView("JobSystem is disabled."));

    CommandLine::shutdown();
}

TEST(ApplicationThreadRoleWiringTest, SingleThreadedModeClaimsAllRolesAndPumpsPerPhase)
{
    CommandLine::shutdown();

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableLogging = false;
    desc.enableJobSystem = false;
    desc.singleThreadedRenderAndRHI = true;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;

    class SingleThreadRoleProbeApp final : public Application
    {
    public:
        explicit SingleThreadRoleProbeApp(const ApplicationDesc& inDesc) noexcept
            : Application(inDesc)
        {}

        bool initRolesOk{ false };
        bool gameTaskRanBeforeFrame2Update{ false };
        bool renderTaskRanBeforeRender{ false };
        bool rhiTaskRanBeforeEndFrame{ false };
        bool shutdownTaskDrained{ false };

    protected:
        [[nodiscard]] virtual bool onInitialize() noexcept override
        {
            initRolesOk = thread::isInGameThread() && thread::isInRenderThread() && thread::isInRHIThread() &&
                          thread::isInParallelRenderThread() && getNamedThreadDispatcher().isInitialized();
            return initRolesOk;
        }

        virtual void onUpdate(const gp::time::FrameTimeStats& timeStats) noexcept override
        {
            if (timeStats.frameIndex == 1ULL)
            {
                EXPECT_EQ(
                    getNamedThreadDispatcher().dispatch(
                        thread::NamedThread::Game,
                        [](void* rawFlag)
                {
                    *static_cast<bool*>(rawFlag) = thread::isInGameThread();
                },
                        &m_gameTaskRan
                    ),
                    thread::ThreadRoleResult::Success
                );
                EXPECT_EQ(
                    getNamedThreadDispatcher().dispatch(
                        thread::NamedThread::Render,
                        [](void* rawFlag)
                {
                    *static_cast<bool*>(rawFlag) = thread::isInRenderThread();
                },
                        &m_renderTaskRan
                    ),
                    thread::ThreadRoleResult::Success
                );
            }
            else if (timeStats.frameIndex == 2ULL)
            {
                gameTaskRanBeforeFrame2Update = m_gameTaskRan;
                requestQuit();
            }
        }

        virtual void onRender(const gp::time::FrameTimeStats& timeStats) noexcept override
        {
            if (timeStats.frameIndex == 1ULL)
            {
                renderTaskRanBeforeRender = m_renderTaskRan;
                EXPECT_EQ(
                    getNamedThreadDispatcher().dispatch(
                        thread::NamedThread::RHI,
                        [](void* rawFlag)
                {
                    *static_cast<bool*>(rawFlag) = thread::isInRHIThread();
                },
                        &m_rhiTaskRan
                    ),
                    thread::ThreadRoleResult::Success
                );
            }
        }

        virtual void onEndFrame(const ApplicationFrameContext& frameContext) noexcept override
        {
            EXPECT_EQ(&frameContext.namedThreadDispatcher, &getNamedThreadDispatcher());
            if (frameContext.getFrameIndex() == 1ULL)
            {
                rhiTaskRanBeforeEndFrame = m_rhiTaskRan;
            }
        }

        virtual void onShutdown() noexcept override
        {
            EXPECT_EQ(
                getNamedThreadDispatcher().dispatch(
                    thread::NamedThread::Game,
                    [](void* rawFlag)
            {
                *static_cast<bool*>(rawFlag) = true;
            },
                    &shutdownTaskDrained
                ),
                thread::ThreadRoleResult::Success
            );
        }

    private:
        bool m_gameTaskRan{ false };
        bool m_renderTaskRan{ false };
        bool m_rhiTaskRan{ false };
    };

    SingleThreadRoleProbeApp app(desc);
    EXPECT_EQ(app.run(), 0);
    EXPECT_TRUE(app.initRolesOk);
    EXPECT_TRUE(app.gameTaskRanBeforeFrame2Update);
    EXPECT_TRUE(app.renderTaskRanBeforeRender);
    EXPECT_TRUE(app.rhiTaskRanBeforeEndFrame);
    EXPECT_TRUE(app.shutdownTaskDrained);

    EXPECT_FALSE(app.getNamedThreadDispatcher().isInitialized());
    EXPECT_EQ(thread::ThreadRoleRegistry::getCurrentRoles(), thread::ThreadRole::None);
    EXPECT_FALSE(thread::ThreadRoleRegistry::isAttached(thread::NamedThread::Game));
    EXPECT_FALSE(thread::ThreadRoleRegistry::isAttached(thread::NamedThread::Render));
    EXPECT_FALSE(thread::ThreadRoleRegistry::isAttached(thread::NamedThread::RHI));
}

TEST(ApplicationThreadRoleWiringTest, DedicatedRenderModeLeavesRenderAndRHIUnclaimed)
{
    CommandLine::shutdown();

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableLogging = false;
    desc.enableJobSystem = false;
    desc.singleThreadedRenderAndRHI = false;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;

    class DedicatedRenderProbeApp final : public Application
    {
    public:
        explicit DedicatedRenderProbeApp(const ApplicationDesc& inDesc) noexcept
            : Application(inDesc)
        {}

        bool rolesVerified{ false };

    protected:
        virtual void onUpdate(const gp::time::FrameTimeStats&) noexcept override
        {
            rolesVerified = thread::isInGameThread() && !thread::isInRenderThread() && !thread::isInRHIThread() &&
                            !thread::ThreadRoleRegistry::isAttached(thread::NamedThread::Render) &&
                            !thread::ThreadRoleRegistry::isAttached(thread::NamedThread::RHI);
            requestQuit();
        }
    };

    DedicatedRenderProbeApp app(desc);
    EXPECT_EQ(app.run(), 0);
    EXPECT_TRUE(app.rolesVerified);
    EXPECT_EQ(thread::ThreadRoleRegistry::getCurrentRoles(), thread::ThreadRole::None);
}

TEST(ApplicationThreadRoleWiringTest, FailsInitIfAnotherThreadAlreadyOwnsGameRole)
{
    CommandLine::shutdown();

    struct HolderState
    {
        Atomic<gp::UInt32> isHolding{ 0u };
        Atomic<gp::UInt32> mayExit{ 0u };
    };

    HolderState state{};
    gp::platform::Thread holder{};
    gp::platform::ThreadDesc holderDesc{};
    holderDesc.name = "GP_AppGameRoleHolder";
    holderDesc.entry = [](void* rawState) -> gp::UInt32
    {
        HolderState& holderState = *static_cast<HolderState*>(rawState);
        const thread::ScopedNamedThread guard(thread::NamedThread::Game);
        holderState.isHolding.store(1u, gp::MemoryOrder::Release);
        while (holderState.mayExit.load(gp::MemoryOrder::Acquire) == 0u)
        {
            gp::platform::Thread::spinWait();
        }
        return 0u;
    };
    holderDesc.userData = &state;
    ASSERT_EQ(holder.start(holderDesc), gp::platform::ThreadResult::Success);

    while (state.isHolding.load(gp::MemoryOrder::Acquire) == 0u)
    {
        gp::platform::Thread::spinWait();
    }

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableLogging = false;
    desc.enableJobSystem = false;

    Application app(desc);
    EXPECT_EQ(app.run(), -1);
    EXPECT_FALSE(app.getNamedThreadDispatcher().isInitialized());

    state.mayExit.store(1u, gp::MemoryOrder::Release);
    gp::UInt32 exitCode = 1u;
    EXPECT_EQ(holder.join(&exitCode), gp::platform::ThreadResult::Success);
    EXPECT_EQ(exitCode, 0u);
    EXPECT_FALSE(thread::ThreadRoleRegistry::isAttached(thread::NamedThread::Game));
}

TEST(ApplicationVfsWiringTest, DefaultMountsLifecycleAndConsoleDiagnostics)
{
    CommandLine::shutdown();
    vfs::VirtualFileSystem::shutdown();

    auto cleanup = gp::makeScopeExit(
        []() noexcept -> void
    {
        vfs::VirtualFileSystem::shutdown();
        CommandLine::shutdown();
    }
    );

    ApplicationDesc desc{};
    desc.backend.appName = "GPAppVfsLifecycleUnitTest";
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableLogging = false;
    desc.enableJobSystem = false;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;

    class VfsLifecycleProbeApp final : public Application
    {
    public:
        explicit VfsLifecycleProbeApp(const ApplicationDesc& inDesc) noexcept
            : Application(inDesc)
        {}

        bool allSchemesMountedInInit{ false };
        bool vfsStillActiveInShutdown{ false };
        bool consoleStatusReportedSix{ false };
        gp::PathBuffer tempResolvedPath{};

    protected:
        [[nodiscard]] virtual bool onInitialize() noexcept override
        {
            allSchemesMountedInInit = vfs::VirtualFileSystem::isSchemeMounted("engine://") &&
                                      vfs::VirtualFileSystem::isSchemeMounted("game://") &&
                                      vfs::VirtualFileSystem::isSchemeMounted("user://") &&
                                      vfs::VirtualFileSystem::isSchemeMounted("appdata://") &&
                                      vfs::VirtualFileSystem::isSchemeMounted("cache://") &&
                                      vfs::VirtualFileSystem::isSchemeMounted("temp://") &&
                                      (vfs::VirtualFileSystem::getMountCount() == 6u);

            const gp::Expected<gp::PathBuffer, vfs::VfsError> tempRes =
                vfs::VirtualFileSystem::resolveWritablePhysicalPath("temp://probe.txt");
            if (tempRes.hasValue())
            {
                tempResolvedPath = tempRes.value();
            }

            gp::String output{};
            console::ConsoleOutputDelegate capture = console::ConsoleOutputDelegate::createLambda(
                [&output](const gp::StringView line) noexcept -> void
            {
                output.append(line);
                output.pushBack('\n');
            }
            );
            const console::ConsoleResult cmdRes = executeConsoleCommand("vfs.Status", &capture);
            consoleStatusReportedSix =
                (cmdRes == console::ConsoleResult::Success) && output.view().contains("total=6/");
            return true;
        }

        virtual void onUpdate(const gp::time::FrameTimeStats&) noexcept override
        {
            requestQuit();
        }

        virtual void onShutdown() noexcept override
        {
            vfsStillActiveInShutdown = (vfs::VirtualFileSystem::getMountCount() == 6u);
        }
    };

    VfsLifecycleProbeApp app(desc);
    EXPECT_EQ(app.run(), 0);
    EXPECT_TRUE(app.allSchemesMountedInInit);
    EXPECT_TRUE(app.consoleStatusReportedSix);
    EXPECT_TRUE(app.vfsStillActiveInShutdown);
    EXPECT_TRUE(app.tempResolvedPath.view().contains("GPAppVfsLifecycleUnitTest"));

    // Application owned VFS initialization, so shutdownInternal() must unmount all default schemes cleanly.
    EXPECT_EQ(vfs::VirtualFileSystem::getMountCount(), 0u);
}

TEST(ApplicationVfsWiringTest, CommandLineNoVfsDisablesDefaultMountsAndPreservesPreMountedBackends)
{
    CommandLine::shutdown();
    vfs::VirtualFileSystem::shutdown();

    auto cleanup = gp::makeScopeExit(
        []() noexcept -> void
    {
        vfs::VirtualFileSystem::shutdown();
        CommandLine::shutdown();
    }
    );

    // 1. Verify -no-vfs disables default mounts cleanly.
    CommandLine::initialize("gp_app_test -no-vfs");
    ASSERT_TRUE(CommandLine::isInitialized());
    {
        ApplicationDesc desc{};
        desc.createMainWindow = false;
        desc.rhi.forceNullRHI = true;
        desc.enableLogging = false;
        desc.enableJobSystem = false;
        desc.targetForegroundFps = 0.0;
        desc.targetBackgroundFps = 0.0;

        class NoVfsProbeApp final : public Application
        {
        public:
            explicit NoVfsProbeApp(const ApplicationDesc& inDesc) noexcept
                : Application(inDesc)
            {}

            gp::USize mountCountInInit{ 99u };

        protected:
            [[nodiscard]] virtual bool onInitialize() noexcept override
            {
                mountCountInInit = vfs::VirtualFileSystem::getMountCount();
                return true;
            }

            virtual void onUpdate(const gp::time::FrameTimeStats&) noexcept override
            {
                requestQuit();
            }
        };

        NoVfsProbeApp app(desc);
        EXPECT_EQ(app.run(), 0);
        EXPECT_EQ(app.mountCountInInit, 0u);
        EXPECT_EQ(vfs::VirtualFileSystem::getMountCount(), 0u);
    }

    // 2. Verify pre-mounted caller backends are preserved and not torn down by Application::shutdownInternal().
    CommandLine::shutdown();
    vfs::MemoryMountBackend customMem{};
    ASSERT_TRUE(
        vfs::VirtualFileSystem::mountBackend(
            "custom://", &customMem, vfs::MountPriority::Base, vfs::MountAccess::ReadWrite
        )
            .hasValue()
    );

    {
        ApplicationDesc desc{};
        desc.createMainWindow = false;
        desc.rhi.forceNullRHI = true;
        desc.enableLogging = false;
        desc.enableJobSystem = false;
        desc.targetForegroundFps = 0.0;
        desc.targetBackgroundFps = 0.0;

        class PreMountedProbeApp final : public Application
        {
        public:
            explicit PreMountedProbeApp(const ApplicationDesc& inDesc) noexcept
                : Application(inDesc)
            {}

            bool customMounted{ false };
            gp::USize totalMounts{ 0u };

        protected:
            [[nodiscard]] virtual bool onInitialize() noexcept override
            {
                customMounted = vfs::VirtualFileSystem::isSchemeMounted("custom://");
                totalMounts = vfs::VirtualFileSystem::getMountCount();
                return true;
            }

            virtual void onUpdate(const gp::time::FrameTimeStats&) noexcept override
            {
                requestQuit();
            }
        };

        PreMountedProbeApp app(desc);
        EXPECT_EQ(app.run(), 0);
        EXPECT_TRUE(app.customMounted);
        EXPECT_EQ(app.totalMounts, 1u);
    }

    EXPECT_TRUE(vfs::VirtualFileSystem::isSchemeMounted("custom://"));
    EXPECT_EQ(vfs::VirtualFileSystem::getMountCount(), 1u);
    EXPECT_TRUE(vfs::VirtualFileSystem::unmountBackend("custom://", &customMem));
}

TEST(ApplicationVfsWiringTest, CommandLineVfsAppAndVfsLogFileUriResolution)
{
    CommandLine::shutdown();
    vfs::VirtualFileSystem::shutdown();

    const gp::PathBuffer expectedRoot =
        gp::platform::Paths::combine(gp::platform::Paths::tempDir(), "GPAppVfsLogUriUnitTest");
    gp::platform::FileSystem::removeDirectoryTree(expectedRoot.view());

    auto cleanup = gp::makeScopeExit(
        [&expectedRoot]() noexcept -> void
    {
        vfs::VirtualFileSystem::shutdown();
        CommandLine::shutdown();
        gp::platform::FileSystem::removeDirectoryTree(expectedRoot.view());
    }
    );

    CommandLine::initialize("gp_app_test -vfs-app=GPAppVfsLogUriUnitTest -log-file=temp://logs/vfs_boot.log");
    ASSERT_TRUE(CommandLine::isInitialized());

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableLogging = true;
    desc.enableConsoleLogSink = false;
    desc.enableDebugOutputLogSink = false;
    desc.enableTracyLogSink = false;
    desc.enableFileLogSink = false;   // Enabled by -log-file CLI override
    desc.logger.mode = log::LogExecutionMode::Synchronous;
    desc.logger.applyCommandLineOverrides = false;
    desc.enableJobSystem = false;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;

    class VfsLogProbeApp final : public Application
    {
    public:
        explicit VfsLogProbeApp(const ApplicationDesc& inDesc) noexcept
            : Application(inDesc)
        {}

        bool fileSinkWasOpen{ false };
        gp::PathBuffer openedLogPath{};

    protected:
        [[nodiscard]] virtual bool onInitialize() noexcept override
        {
            fileSinkWasOpen = getFileSink().isOpen();
            openedLogPath.assign(getFileSink().getFilePath());
            GP_LOG_INFO(LogApplication, "VfsLogProbeApp marker message");
            return true;
        }

        virtual void onUpdate(const gp::time::FrameTimeStats&) noexcept override
        {
            requestQuit();
        }
    };

    VfsLogProbeApp app(desc);
    EXPECT_EQ(app.run(), 0);
    EXPECT_TRUE(app.fileSinkWasOpen);
    EXPECT_TRUE(app.openedLogPath.view().contains("GPAppVfsLogUriUnitTest"));
    EXPECT_TRUE(app.openedLogPath.view().endsWith("logs/vfs_boot.log"));
    EXPECT_EQ(vfs::VirtualFileSystem::getMountCount(), 0u);

    gp::String diskLog{};
    ASSERT_TRUE(gp::platform::FileSystem::readAllText(app.openedLogPath.view(), diskLog).hasValue());
    EXPECT_TRUE(diskLog.view().contains("VfsLogProbeApp marker message"));
}

namespace
{

class AppTestRhiBackendModule final : public modules::IModule
{
public:
    static inline gp::UInt32 s_startupOrder{ 0u };
    static inline gp::UInt32 s_shutdownOrder{ 0u };
    static inline gp::UInt32 s_orderCounter{ 0u };
    static inline gp::memory::Malloc* s_observedShadowMalloc{ nullptr };

    virtual bool onStartupModule() noexcept override
    {
        s_observedShadowMalloc = gp::memory::getInlineMalloc();
        s_startupOrder = ++s_orderCounter;
        return true;
    }

    virtual void onShutdownModule() noexcept override
    {
        s_shutdownOrder = ++s_orderCounter;
    }
};

class AppTestAudioBackendModule final : public modules::IModule
{
public:
    static inline gp::UInt32 s_startupOrder{ 0u };
    static inline gp::UInt32 s_shutdownOrder{ 0u };

    virtual bool onStartupModule() noexcept override
    {
        s_startupOrder = ++AppTestRhiBackendModule::s_orderCounter;
        return true;
    }

    virtual void onShutdownModule() noexcept override
    {
        s_shutdownOrder = ++AppTestRhiBackendModule::s_orderCounter;
    }
};

}   // namespace

TEST(ApplicationModuleWiringTest, StartupModulesCliOverridesConsoleCommandsAndReverseLifoShutdown)
{
    CommandLine::shutdown();
    modules::ModuleManager& moduleMgr = Application::getModuleManager();
    moduleMgr.resetForTesting();

    auto cleanup = gp::makeScopeExit(
        [&moduleMgr]() noexcept -> void
    {
        moduleMgr.resetForTesting();
        CommandLine::shutdown();
    }
    );

    AppTestRhiBackendModule::s_startupOrder = 0u;
    AppTestRhiBackendModule::s_shutdownOrder = 0u;
    AppTestRhiBackendModule::s_orderCounter = 0u;
    AppTestRhiBackendModule::s_observedShadowMalloc = nullptr;
    AppTestAudioBackendModule::s_startupOrder = 0u;
    AppTestAudioBackendModule::s_shutdownOrder = 0u;

    ASSERT_TRUE(moduleMgr.registerStaticModule(
        "rhi/app_test_backend",
        &modules::StaticModuleRegistrant<AppTestRhiBackendModule>::createInstance,
        &modules::StaticModuleRegistrant<AppTestRhiBackendModule>::destroyInstance
    ));
    ASSERT_TRUE(moduleMgr.registerStaticModule(
        "audio/app_test_backend",
        &modules::StaticModuleRegistrant<AppTestAudioBackendModule>::createInstance,
        &modules::StaticModuleRegistrant<AppTestAudioBackendModule>::destroyInstance
    ));

    // Pass the second startup module via the -load-module CLI flag while configuring the first in ApplicationDesc.
    CommandLine::initialize("gp_application_tests -load-module=audio/app_test_backend");

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.enableLogging = false;
    desc.enableJobSystem = false;
    desc.enableRHI = false;
    desc.targetForegroundFps = 0.0;
    desc.targetBackgroundFps = 0.0;
    EXPECT_TRUE(desc.addStartupModule("rhi/app_test_backend"));

    class ModuleLifecycleProbeApp final : public Application
    {
    public:
        explicit ModuleLifecycleProbeApp(const ApplicationDesc& inDesc) noexcept
            : Application(inDesc)
        {}

        bool bothLoadedInOnInitialize{ false };
        bool bothStillLoadedInOnShutdown{ false };
        bool consoleStatusSucceeded{ false };
        bool reloadSucceededInOnUpdate{ false };

    protected:
        [[nodiscard]] virtual bool onInitialize() noexcept override
        {
            bothLoadedInOnInitialize =
                isModuleLoaded("rhi/app_test_backend") && isModuleLoaded("audio/app_test_backend");
            return bothLoadedInOnInitialize;
        }

        virtual void onUpdate(const gp::time::FrameTimeStats&) noexcept override
        {
            gp::String statusOutput{};
            console::ConsoleOutputDelegate capture = console::ConsoleOutputDelegate::createLambda(
                [&statusOutput](const gp::StringView line) noexcept -> void
            {
                statusOutput.append(line);
            }
            );
            consoleStatusSucceeded =
                (executeConsoleCommand("module.Status", &capture) == console::ConsoleResult::Success) &&
                statusOutput.view().contains("loaded=2");

            modules::ModuleLoadResult reloadRes = modules::ModuleLoadResult::NotFound;
            reloadSucceededInOnUpdate = (reloadModule("audio/app_test_backend", &reloadRes) != nullptr) &&
                                        (reloadRes == modules::ModuleLoadResult::Success);
            requestQuit();
        }

        virtual void onShutdown() noexcept override
        {
            // Modules must remain active throughout onShutdown() and only unload inside shutdownInternal() afterward.
            bothStillLoadedInOnShutdown =
                isModuleLoaded("rhi/app_test_backend") && isModuleLoaded("audio/app_test_backend");
        }
    };

    ModuleLifecycleProbeApp app(desc);
    EXPECT_EQ(app.run(), 0);
    EXPECT_TRUE(app.bothLoadedInOnInitialize);
    EXPECT_TRUE(app.consoleStatusSucceeded);
    EXPECT_TRUE(app.reloadSucceededInOnUpdate);
    EXPECT_TRUE(app.bothStillLoadedInOnShutdown);
    EXPECT_EQ(AppTestRhiBackendModule::s_observedShadowMalloc, gp::memory::getGlobalMalloc());

    // After app.run() returns, shutdownInternal() must have unloaded all modules in reverse-LIFO order
    // (audio/app_test_backend shut down BEFORE rhi/app_test_backend).
    EXPECT_EQ(moduleMgr.getLoadedModuleCount(), 0u);
    EXPECT_LT(AppTestAudioBackendModule::s_shutdownOrder, AppTestRhiBackendModule::s_shutdownOrder);
}

TEST(ApplicationModuleWiringTest, FailingRequiredStartupModuleAbortsRunAndRollsBackCleanly)
{
    CommandLine::shutdown();
    modules::ModuleManager& moduleMgr = Application::getModuleManager();
    moduleMgr.resetForTesting();

    auto cleanup = gp::makeScopeExit(
        [&moduleMgr]() noexcept -> void
    {
        moduleMgr.resetForTesting();
        CommandLine::shutdown();
    }
    );

    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.rhi.forceNullRHI = true;
    desc.enableLogging = false;
    desc.enableJobSystem = false;
    desc.failOnStartupModuleLoadError = true;
    EXPECT_TRUE(desc.addStartupModule("rhi/nonexistent_required_startup_module_xyz"));

    Application app(desc);
    EXPECT_EQ(app.run(), -1);
    EXPECT_EQ(moduleMgr.getLoadedModuleCount(), 0u);
    EXPECT_EQ(thread::ThreadRoleRegistry::getCurrentRoles(), thread::ThreadRole::None);
}

}   // namespace gp::tests
