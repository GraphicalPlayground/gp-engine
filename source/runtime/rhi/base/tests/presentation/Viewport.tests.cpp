// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/Viewport.hpp"
#include "commands/CommandForward.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "presentation/CustomPresent.hpp"
#include "presentation/PresentArgs.hpp"
#include "presentation/PresentationForward.hpp"
#include "templates/Function.hpp"
#include "templates/Interval.hpp"
#include "threading/atomics/Atomic.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <type_traits>

namespace gp::rhi::presentation::tests
{

static_assert(gp::concepts::IsRefCounted<CustomPresent>, "CustomPresent must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<Viewport>, "Viewport must satisfy IsRefCounted.");

static_assert(sizeof(CustomPresentRef) == sizeof(CustomPresent*), "CustomPresentRef must remain a single pointer.");
static_assert(sizeof(ViewportRef) == sizeof(Viewport*), "ViewportRef must remain a single pointer.");

static_assert(std::is_abstract_v<CustomPresent>, "CustomPresent must be an abstract base class.");
static_assert(!std::is_abstract_v<Viewport>, "Viewport must be instantiable with default virtual behavior.");

static_assert(!std::is_copy_constructible_v<CustomPresent>, "CustomPresent must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<CustomPresent>, "CustomPresent must not be copy assignable.");
static_assert(!std::is_move_constructible_v<CustomPresent>, "CustomPresent must not be move constructible.");
static_assert(!std::is_move_assignable_v<CustomPresent>, "CustomPresent must not be move assignable.");

static_assert(!std::is_copy_constructible_v<Viewport>, "Viewport must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<Viewport>, "Viewport must not be copy assignable.");
static_assert(!std::is_move_constructible_v<Viewport>, "Viewport must not be move constructible.");
static_assert(!std::is_move_assignable_v<Viewport>, "Viewport must not be move assignable.");

namespace
{

constexpr bool verifyPresentArgsConstexpr() noexcept
{
    const PresentArgs defaultArgs{};
    if (!defaultArgs.shouldPresent() || defaultArgs.isLockedToVsync())
    {
        return false;
    }

    const PresentArgs syncedArgs(true, true);
    const PresentArgs skipArgs(false, false);
    const PresentArgs skipSyncedArgs(false, true);

    if (!syncedArgs.shouldPresent() || !syncedArgs.isLockedToVsync())
    {
        return false;
    }
    if (skipArgs.shouldPresent() || skipArgs.isLockedToVsync())
    {
        return false;
    }
    if (skipSyncedArgs.shouldPresent() || !skipSyncedArgs.isLockedToVsync())
    {
        return false;
    }

    if (!(defaultArgs == PresentArgs(true, false)) || (defaultArgs == syncedArgs) || (syncedArgs == skipSyncedArgs))
    {
        return false;
    }

    const PresentArgsWithStats defaultStats{};
    if (!defaultStats.shouldPresent() || defaultStats.isLockedToVsync() || defaultStats.frameCounter != 0u ||
        defaultStats.inputCycles != 0u)
    {
        return false;
    }

    const PresentArgsWithStats populatedStats(syncedArgs, 42u, 9001u);
    const PresentArgsWithStats sameStats(PresentArgs(true, true), 42u, 9001u);
    const PresentArgsWithStats diffFrameStats(syncedArgs, 43u, 9001u);
    const PresentArgsWithStats diffInputStats(syncedArgs, 42u, 9002u);
    const PresentArgsWithStats diffFlagStats(defaultArgs, 42u, 9001u);

    return (populatedStats == sameStats) && !(populatedStats == diffFrameStats) &&
           !(populatedStats == diffInputStats) && !(populatedStats == diffFlagStats);
}

static_assert(verifyPresentArgsConstexpr(), "Constexpr PresentArgs and PresentArgsWithStats checks failed.");

class MinimalCustomPresent final : public CustomPresent
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    explicit MinimalCustomPresent(const bool inNeedsNativePresent) noexcept
        : m_needsNativePresent(inNeedsNativePresent)
    {}

    virtual ~MinimalCustomPresent() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc CustomPresent::onBackBufferResize()
    virtual void onBackBufferResize() noexcept override
    {
        ++m_resizeCount;
    }

    /// @copydoc CustomPresent::needsNativePresent()
    [[nodiscard]] virtual bool needsNativePresent() noexcept override
    {
        return m_needsNativePresent;
    }

    [[nodiscard]] gp::UInt32 getResizeCount() const noexcept
    {
        return m_resizeCount;
    }

private:
    bool m_needsNativePresent{ true };
    gp::UInt32 m_resizeCount{ 0u };
};

class FullMockCustomPresent final : public CustomPresent
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    FullMockCustomPresent(
        const bool inNeedsNativePresent,
        const bool inNeedsAdvanceBackbuffer,
        const bool inPresentReturnValue,
        const gp::Int32 inOverrideSyncInterval
    ) noexcept
        : m_needsNativePresent(inNeedsNativePresent)
        , m_needsAdvanceBackbuffer(inNeedsAdvanceBackbuffer)
        , m_presentReturnValue(inPresentReturnValue)
        , m_overrideSyncInterval(inOverrideSyncInterval)
    {}

    virtual ~FullMockCustomPresent() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc CustomPresent::onBackBufferResize()
    virtual void onBackBufferResize() noexcept override
    {
        ++m_resizeCalls;
    }

    /// @copydoc CustomPresent::needsNativePresent()
    [[nodiscard]] virtual bool needsNativePresent() noexcept override
    {
        return m_needsNativePresent;
    }

    /// @copydoc CustomPresent::needsAdvanceBackbuffer()
    [[nodiscard]] virtual bool needsAdvanceBackbuffer() noexcept override
    {
        return m_needsAdvanceBackbuffer;
    }

    /// @copydoc CustomPresent::present()
    [[nodiscard]] virtual bool present(
        Viewport* const viewport, command::ICommandContext& cmdContext, gp::Int32& inOutSyncInterval
    ) noexcept override
    {
        ++m_presentCalls;
        m_lastViewport = viewport;
        m_lastContextPtr = &cmdContext;
        m_receivedSyncInterval = inOutSyncInterval;
        inOutSyncInterval = m_overrideSyncInterval;
        return m_presentReturnValue;
    }

    /// @copydoc CustomPresent::postPresent()
    virtual void postPresent() noexcept override
    {
        ++m_postPresentCalls;
    }

    /// @copydoc CustomPresent::onAcquireThreadOwnership()
    virtual void onAcquireThreadOwnership() noexcept override
    {
        ++m_acquireOwnershipCalls;
    }

    /// @copydoc CustomPresent::onReleaseThreadOwnership()
    virtual void onReleaseThreadOwnership() noexcept override
    {
        ++m_releaseOwnershipCalls;
    }

public:
    gp::UInt32 m_resizeCalls{ 0u };
    gp::UInt32 m_presentCalls{ 0u };
    gp::UInt32 m_postPresentCalls{ 0u };
    gp::UInt32 m_acquireOwnershipCalls{ 0u };
    gp::UInt32 m_releaseOwnershipCalls{ 0u };
    Viewport* m_lastViewport{ nullptr };
    const command::ICommandContext* m_lastContextPtr{ nullptr };
    gp::Int32 m_receivedSyncInterval{ -1 };

private:
    bool m_needsNativePresent{ false };
    bool m_needsAdvanceBackbuffer{ false };
    bool m_presentReturnValue{ true };
    gp::Int32 m_overrideSyncInterval{ 0 };
};

class MockViewport final : public Viewport
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };
    static constexpr gp::USize kMaxRecordedLayerRanges = 4u;

    MockViewport(
        void* const swapChain,
        void* const backBufferTex,
        void* const backBufferRT,
        void* const nativeWindow,
        void* const windowAddParam,
        resource::Texture* const sdrBackBuffer
    ) noexcept
        : m_nativeSwapChain(swapChain)
        , m_nativeBackBufferTexture(backBufferTex)
        , m_nativeBackBufferRT(backBufferRT)
        , m_nativeWindow(nativeWindow)
        , m_windowAddParam(windowAddParam)
        , m_sdrBackBuffer(sdrBackBuffer)
    {}

    virtual ~MockViewport() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc Viewport::getNativeSwapChain()
    [[nodiscard]] virtual void* getNativeSwapChain() const noexcept override
    {
        return m_nativeSwapChain;
    }

    /// @copydoc Viewport::getNativeBackBufferTexture()
    [[nodiscard]] virtual void* getNativeBackBufferTexture() const noexcept override
    {
        return m_nativeBackBufferTexture;
    }

    /// @copydoc Viewport::getNativeBackBufferRT()
    [[nodiscard]] virtual void* getNativeBackBufferRT() const noexcept override
    {
        return m_nativeBackBufferRT;
    }

    /// @copydoc Viewport::getNativeWindow()
    [[nodiscard]] virtual void* getNativeWindow(void** const outAddParam) const noexcept override
    {
        if (outAddParam != nullptr)
        {
            *outAddParam = m_windowAddParam;
        }
        return m_nativeWindow;
    }

    /// @copydoc Viewport::setCustomPresent()
    virtual void setCustomPresent(CustomPresent* const customPresent) noexcept override
    {
        m_customPresent = customPresent;
    }

    /// @copydoc Viewport::createNativeLayer()
    virtual void createNativeLayer(
        const gp::Int32 newNativeLayer,
        void* const nativeViewHandle,
        const gp::ConstVectorView<gp::Interval<gp::Int32>> layerRanges
    ) noexcept override
    {
        m_lastCreatedLayer = newNativeLayer;
        m_lastNativeViewHandle = nativeViewHandle;
        m_recordedLayerRangeCount =
            (layerRanges.size() <= kMaxRecordedLayerRanges) ? layerRanges.size() : kMaxRecordedLayerRanges;
        for (gp::USize i = 0u; i < m_recordedLayerRangeCount; ++i)
        {
            m_recordedLayerRanges[i] = layerRanges[i];
        }
    }

    /// @copydoc Viewport::deleteNativeLayer()
    virtual void deleteNativeLayer(const gp::Int32 oldNativeLayer) noexcept override
    {
        m_lastDeletedLayer = oldNativeLayer;
    }

    /// @copydoc Viewport::setDefaultNativeLayer()
    virtual void setDefaultNativeLayer(const gp::Int32 nativeLayer) noexcept override
    {
        m_defaultNativeLayer = nativeLayer;
    }

    /// @copydoc Viewport::setWindowCropSize()
    virtual void setWindowCropSize(const resource::IntRect& newCropWindow) noexcept override
    {
        m_cropWindow = newCropWindow;
    }

    /// @copydoc Viewport::getCustomPresent()
    [[nodiscard]] virtual CustomPresent* getCustomPresent() const noexcept override
    {
        return m_customPresent.get();
    }

    /// @copydoc Viewport::getOptionalSDRBackBuffer()
    [[nodiscard]] virtual resource::Texture*
        getOptionalSDRBackBuffer(resource::Texture* const backBuffer) const noexcept override
    {
        m_lastQueriedBackBuffer = backBuffer;
        return (backBuffer != nullptr) ? m_sdrBackBuffer : nullptr;
    }

    /// @copydoc Viewport::waitForFrameEventCompletion()
    virtual void waitForFrameEventCompletion() noexcept override
    {
        ++m_waitFrameEventCalls;
    }

    /// @copydoc Viewport::issueFrameEvent()
    virtual void issueFrameEvent() noexcept override
    {
        ++m_issueFrameEventCalls;
    }

    /// @brief Exercises the protected latency callbacks around a simulated swapchain present.
    void simulatePresent() noexcept
    {
        m_presentBeginLatencyCallback.executeIfBound();
        ++m_simulatedPresentCount;
        m_presentEndLatencyCallback.executeIfBound();
    }

    [[nodiscard]] bool hasBeginLatencyCallback() const noexcept
    {
        return m_presentBeginLatencyCallback.isBound();
    }

    [[nodiscard]] bool hasEndLatencyCallback() const noexcept
    {
        return m_presentEndLatencyCallback.isBound();
    }

public:
    gp::Int32 m_lastCreatedLayer{ -1 };
    gp::Int32 m_lastDeletedLayer{ -1 };
    gp::Int32 m_defaultNativeLayer{ -1 };
    void* m_lastNativeViewHandle{ nullptr };
    gp::USize m_recordedLayerRangeCount{ 0u };
    gp::Array<gp::Interval<gp::Int32>, kMaxRecordedLayerRanges> m_recordedLayerRanges{};
    resource::IntRect m_cropWindow{};
    mutable resource::Texture* m_lastQueriedBackBuffer{ nullptr };
    gp::UInt32 m_waitFrameEventCalls{ 0u };
    gp::UInt32 m_issueFrameEventCalls{ 0u };
    gp::UInt32 m_simulatedPresentCount{ 0u };

private:
    void* m_nativeSwapChain{ nullptr };
    void* m_nativeBackBufferTexture{ nullptr };
    void* m_nativeBackBufferRT{ nullptr };
    void* m_nativeWindow{ nullptr };
    void* m_windowAddParam{ nullptr };
    resource::Texture* m_sdrBackBuffer{ nullptr };
    CustomPresentRef m_customPresent{};
};

class ViewportTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        MinimalCustomPresent::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        FullMockCustomPresent::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        MockViewport::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }
};

}   // namespace

TEST(PresentArgsTest, DefaultAndExplicitConstructionFlags)
{
    const PresentArgs defaultArgs{};
    EXPECT_EQ(defaultArgs.present, 1u);
    EXPECT_EQ(defaultArgs.lockToVsync, 0u);
    EXPECT_TRUE(defaultArgs.shouldPresent());
    EXPECT_FALSE(defaultArgs.isLockedToVsync());

    const PresentArgs vsyncArgs(true, true);
    EXPECT_EQ(vsyncArgs.present, 1u);
    EXPECT_EQ(vsyncArgs.lockToVsync, 1u);
    EXPECT_TRUE(vsyncArgs.shouldPresent());
    EXPECT_TRUE(vsyncArgs.isLockedToVsync());

    const PresentArgs skipArgs(false, false);
    EXPECT_EQ(skipArgs.present, 0u);
    EXPECT_EQ(skipArgs.lockToVsync, 0u);
    EXPECT_FALSE(skipArgs.shouldPresent());
    EXPECT_FALSE(skipArgs.isLockedToVsync());

    EXPECT_NE(defaultArgs, vsyncArgs);
    EXPECT_NE(defaultArgs, skipArgs);
    EXPECT_EQ(defaultArgs, PresentArgs(true, false));
}

TEST(PresentArgsTest, EqualityIgnoresUninitializedPaddingBits)
{
    alignas(PresentArgs) gp::Byte storageAllZero[sizeof(PresentArgs)]{};
    alignas(PresentArgs) gp::Byte storageAllOnes[sizeof(PresentArgs)]{};
    std::memset(storageAllZero, 0x00, sizeof(storageAllZero));
    std::memset(storageAllOnes, 0xFF, sizeof(storageAllOnes));

    PresentArgs* const argsZeroPad = std::construct_at(reinterpret_cast<PresentArgs*>(storageAllZero), true, false);
    PresentArgs* const argsOnesPad = std::construct_at(reinterpret_cast<PresentArgs*>(storageAllOnes), true, false);

    EXPECT_EQ(*argsZeroPad, *argsOnesPad);
    EXPECT_TRUE(argsZeroPad->shouldPresent());
    EXPECT_FALSE(argsZeroPad->isLockedToVsync());
    EXPECT_TRUE(argsOnesPad->shouldPresent());
    EXPECT_FALSE(argsOnesPad->isLockedToVsync());

    std::destroy_at(argsOnesPad);
    std::destroy_at(argsZeroPad);
}

TEST(PresentArgsTest, PresentArgsWithStatsPreservesBaseFlagsAndCounters)
{
    const PresentArgsWithStats defaultStats{};
    EXPECT_TRUE(defaultStats.shouldPresent());
    EXPECT_FALSE(defaultStats.isLockedToVsync());
    EXPECT_EQ(defaultStats.frameCounter, 0u);
    EXPECT_EQ(defaultStats.inputCycles, 0u);

    const PresentArgs baseArgs(false, true);
    const PresentArgsWithStats statsA(baseArgs, 128u, 987654321u);
    const PresentArgsWithStats statsB(PresentArgs(false, true), 128u, 987654321u);
    const PresentArgsWithStats statsDiffFrame(baseArgs, 129u, 987654321u);
    const PresentArgsWithStats statsDiffInput(baseArgs, 128u, 111111111u);
    const PresentArgsWithStats statsDiffBase(PresentArgs(true, true), 128u, 987654321u);

    EXPECT_FALSE(statsA.shouldPresent());
    EXPECT_TRUE(statsA.isLockedToVsync());
    EXPECT_EQ(statsA.frameCounter, 128u);
    EXPECT_EQ(statsA.inputCycles, 987654321u);

    EXPECT_EQ(statsA, statsB);
    EXPECT_NE(statsA, statsDiffFrame);
    EXPECT_NE(statsA, statsDiffInput);
    EXPECT_NE(statsA, statsDiffBase);
}

TEST_F(ViewportTestFixture, CustomPresentDefaultMethodsAndDeferredDeletion)
{
    {
        MinimalCustomPresent* const rawHook = new MinimalCustomPresent(true);
        CustomPresentRef hook(rawHook);

        EXPECT_EQ(hook->getType(), resource::ResourceType::CustomPresent);
        EXPECT_TRUE(hook->needsNativePresent());
        EXPECT_FALSE(hook->needsAdvanceBackbuffer());

        hook->onBackBufferResize();
        EXPECT_EQ(rawHook->getResizeCount(), 1u);

        // Default virtual no-op methods must execute cleanly.
        hook->postPresent();
        hook->onAcquireThreadOwnership();
        hook->onReleaseThreadOwnership();

        // Upcast to ResourceRef shares intrusive ref counting.
        resource::ResourceRef baseRef = hook;
        EXPECT_EQ(hook->getRefCount(), 2u);
        baseRef.reset();
        EXPECT_EQ(hook->getRefCount(), 1u);
        EXPECT_EQ(MinimalCustomPresent::s_destructCount.load(), 0u);
    }

    EXPECT_EQ(MinimalCustomPresent::s_destructCount.load(), 0u);
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MinimalCustomPresent::s_destructCount.load(), 1u);
}

TEST_F(ViewportTestFixture, CustomPresentFullOverrideContract)
{
    ViewportRef viewport(new Viewport());
    FullMockCustomPresent* const rawHook = new FullMockCustomPresent(false, true, false, 2);
    CustomPresentRef hook(rawHook);

    gp::UInt64 dummyContextStorage = 0u;
    command::ICommandContext& dummyContext = *reinterpret_cast<command::ICommandContext*>(&dummyContextStorage);

    EXPECT_FALSE(hook->needsNativePresent());
    EXPECT_TRUE(hook->needsAdvanceBackbuffer());

    gp::Int32 syncInterval = 1;
    EXPECT_FALSE(hook->present(viewport.get(), dummyContext, syncInterval));
    EXPECT_EQ(syncInterval, 2);
    EXPECT_EQ(rawHook->m_presentCalls, 1u);
    EXPECT_EQ(rawHook->m_lastViewport, viewport.get());
    EXPECT_EQ(rawHook->m_lastContextPtr, &dummyContext);
    EXPECT_EQ(rawHook->m_receivedSyncInterval, 1);

    hook->onBackBufferResize();
    hook->postPresent();
    hook->onAcquireThreadOwnership();
    hook->onReleaseThreadOwnership();

    EXPECT_EQ(rawHook->m_resizeCalls, 1u);
    EXPECT_EQ(rawHook->m_postPresentCalls, 1u);
    EXPECT_EQ(rawHook->m_acquireOwnershipCalls, 1u);
    EXPECT_EQ(rawHook->m_releaseOwnershipCalls, 1u);
}

TEST_F(ViewportTestFixture, BaseViewportReportsCorrectTypeAndAllDefaultImplementations)
{
    ViewportRef viewport(new Viewport());
    EXPECT_EQ(viewport->getType(), resource::ResourceType::Viewport);

    gp::UInt64 addParamSentinelStorage = 0u;
    void* addParamSentinel = &addParamSentinelStorage;
    EXPECT_EQ(viewport->getNativeSwapChain(), nullptr);
    EXPECT_EQ(viewport->getNativeBackBufferTexture(), nullptr);
    EXPECT_EQ(viewport->getNativeBackBufferRT(), nullptr);
    EXPECT_EQ(viewport->getNativeWindow(), nullptr);
    EXPECT_EQ(viewport->getNativeWindow(&addParamSentinel), nullptr);
    EXPECT_EQ(addParamSentinel, &addParamSentinelStorage);
    EXPECT_EQ(viewport->getCustomPresent(), nullptr);
    EXPECT_EQ(viewport->getOptionalSDRBackBuffer(nullptr), nullptr);

    const gp::Array<gp::Interval<gp::Int32>, 2u> ranges{ gp::Interval<gp::Int32>::closed(0, 10),
                                                         gp::Interval<gp::Int32>::closed(20, 30) };
    viewport->setCustomPresent(nullptr);
    viewport->createNativeLayer(1, nullptr, gp::ConstVectorView<gp::Interval<gp::Int32>>(ranges));
    viewport->deleteNativeLayer(1);
    viewport->setDefaultNativeLayer(0);
    viewport->setWindowCropSize(resource::IntRect(0, 0, 1920, 1080));
    viewport->waitForFrameEventCompletion();
    viewport->issueFrameEvent();

    resource::ResourceRef baseRef = viewport;
    EXPECT_EQ(viewport->getRefCount(), 2u);
    baseRef.reset();
    EXPECT_EQ(viewport->getRefCount(), 1u);
}

TEST_F(ViewportTestFixture, MockViewportOverridesCustomPresentLayersCropAndDeferredDeletion)
{
    gp::UInt64 swapChainTag = 0u;
    gp::UInt64 backBufferTexTag = 0u;
    gp::UInt64 backBufferRTTag = 0u;
    gp::UInt64 nativeWindowTag = 0u;
    gp::UInt64 addParamTag = 0u;
    gp::UInt64 nativeViewTag = 0u;
    gp::UInt64 hdrBackBufferTag = 0u;
    gp::UInt64 sdrBackBufferTag = 0u;

    resource::Texture* const fakeHdrBackBuffer = reinterpret_cast<resource::Texture*>(&hdrBackBufferTag);
    resource::Texture* const fakeSdrBackBuffer = reinterpret_cast<resource::Texture*>(&sdrBackBufferTag);

    {
        MockViewport* const rawViewport = new MockViewport(
            &swapChainTag, &backBufferTexTag, &backBufferRTTag, &nativeWindowTag, &addParamTag, fakeSdrBackBuffer
        );
        ViewportRef viewport(rawViewport);

        EXPECT_EQ(viewport->getType(), resource::ResourceType::Viewport);
        EXPECT_EQ(viewport->getNativeSwapChain(), &swapChainTag);
        EXPECT_EQ(viewport->getNativeBackBufferTexture(), &backBufferTexTag);
        EXPECT_EQ(viewport->getNativeBackBufferRT(), &backBufferRTTag);

        void* outAddParam = nullptr;
        EXPECT_EQ(viewport->getNativeWindow(&outAddParam), &nativeWindowTag);
        EXPECT_EQ(outAddParam, &addParamTag);
        EXPECT_EQ(viewport->getNativeWindow(nullptr), &nativeWindowTag);

        EXPECT_EQ(viewport->getOptionalSDRBackBuffer(fakeHdrBackBuffer), fakeSdrBackBuffer);
        EXPECT_EQ(rawViewport->m_lastQueriedBackBuffer, fakeHdrBackBuffer);
        EXPECT_EQ(viewport->getOptionalSDRBackBuffer(nullptr), nullptr);

        // Bind a CustomPresent hook and verify the viewport retains a reference to it.
        FullMockCustomPresent* const rawHook = new FullMockCustomPresent(true, false, true, 1);
        CustomPresentRef hook(rawHook);
        viewport->setCustomPresent(hook.get());
        EXPECT_EQ(viewport->getCustomPresent(), hook.get());
        EXPECT_EQ(hook->getRefCount(), 2u);
        hook.reset();
        EXPECT_EQ(rawHook->getRefCount(), 1u);

        // Exercise native layer management, crop window updates, and frame event synchronization.
        const gp::Array<gp::Interval<gp::Int32>, 2u> layerRanges{ gp::Interval<gp::Int32>::closed(0, 7),
                                                                  gp::Interval<gp::Int32>::closed(16, 31) };
        viewport->createNativeLayer(3, &nativeViewTag, gp::ConstVectorView<gp::Interval<gp::Int32>>(layerRanges));
        EXPECT_EQ(rawViewport->m_lastCreatedLayer, 3);
        EXPECT_EQ(rawViewport->m_lastNativeViewHandle, &nativeViewTag);
        ASSERT_EQ(rawViewport->m_recordedLayerRangeCount, 2u);
        EXPECT_EQ(rawViewport->m_recordedLayerRanges[0], gp::Interval<gp::Int32>::closed(0, 7));
        EXPECT_EQ(rawViewport->m_recordedLayerRanges[1], gp::Interval<gp::Int32>::closed(16, 31));

        viewport->setDefaultNativeLayer(3);
        EXPECT_EQ(rawViewport->m_defaultNativeLayer, 3);

        viewport->deleteNativeLayer(3);
        EXPECT_EQ(rawViewport->m_lastDeletedLayer, 3);

        const resource::IntRect cropRect(16, 32, 1296, 752);
        viewport->setWindowCropSize(cropRect);
        EXPECT_EQ(rawViewport->m_cropWindow, cropRect);

        viewport->waitForFrameEventCompletion();
        viewport->issueFrameEvent();
        viewport->issueFrameEvent();
        EXPECT_EQ(rawViewport->m_waitFrameEventCalls, 1u);
        EXPECT_EQ(rawViewport->m_issueFrameEventCalls, 2u);

        EXPECT_EQ(MockViewport::s_destructCount.load(), 0u);
        EXPECT_EQ(FullMockCustomPresent::s_destructCount.load(), 0u);
    }

    // Releasing the last ViewportRef defers destruction until pending deletes flush.
    EXPECT_EQ(MockViewport::s_destructCount.load(), 0u);
    EXPECT_EQ(FullMockCustomPresent::s_destructCount.load(), 0u);

    // Flushing deletes the MockViewport and cascades to its retained CustomPresentRef.
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockViewport::s_destructCount.load(), 1u);
    EXPECT_EQ(FullMockCustomPresent::s_destructCount.load(), 1u);
}

TEST_F(ViewportTestFixture, PresentLatencyCallbacksExecuteInOrderAndCanBeCleared)
{
    MockViewport* const rawViewport = new MockViewport(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    ViewportRef viewport(rawViewport);

    EXPECT_FALSE(rawViewport->hasBeginLatencyCallback());
    EXPECT_FALSE(rawViewport->hasEndLatencyCallback());

    // Simulating present with unbound callbacks must not crash.
    rawViewport->simulatePresent();
    EXPECT_EQ(rawViewport->m_simulatedPresentCount, 1u);

    gp::Array<gp::Int32, 4u> callOrder{};
    gp::USize callCount = 0u;
    gp::UInt32 beginCalls = 0u;
    gp::UInt32 endCalls = 0u;

    viewport->setPresentLatencyCallbacks(
        gp::Function<void()>(
            [&callOrder, &callCount, &beginCalls]() -> void
    {
        callOrder[callCount++] = 1;
        ++beginCalls;
    }
        ),
        gp::Function<void()>(
            [&callOrder, &callCount, &endCalls]() -> void
    {
        callOrder[callCount++] = 2;
        ++endCalls;
    }
        )
    );

    EXPECT_TRUE(rawViewport->hasBeginLatencyCallback());
    EXPECT_TRUE(rawViewport->hasEndLatencyCallback());

    rawViewport->simulatePresent();
    EXPECT_EQ(rawViewport->m_simulatedPresentCount, 2u);
    EXPECT_EQ(beginCalls, 1u);
    EXPECT_EQ(endCalls, 1u);
    ASSERT_EQ(callCount, 2u);
    EXPECT_EQ(callOrder[0], 1);
    EXPECT_EQ(callOrder[1], 2);

    // Replacing with empty functions unbinds both callbacks.
    viewport->setPresentLatencyCallbacks(gp::Function<void()>{}, gp::Function<void()>{});
    EXPECT_FALSE(rawViewport->hasBeginLatencyCallback());
    EXPECT_FALSE(rawViewport->hasEndLatencyCallback());

    rawViewport->simulatePresent();
    EXPECT_EQ(rawViewport->m_simulatedPresentCount, 3u);
    EXPECT_EQ(beginCalls, 1u);
    EXPECT_EQ(endCalls, 1u);
}

}   // namespace gp::rhi::presentation::tests
