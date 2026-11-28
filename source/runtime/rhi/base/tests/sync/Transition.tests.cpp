// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/Transition.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "memory/MemoryBase.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>

namespace gp::rhi::sync::tests
{

static_assert(!std::is_copy_constructible_v<Transition>, "Transition must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<Transition>, "Transition must not be copy assignable.");
static_assert(!std::is_move_constructible_v<Transition>, "Transition must not be move constructible.");
static_assert(!std::is_move_assignable_v<Transition>, "Transition must not be move assignable.");

namespace
{

struct alignas(64) MockBackendPrivateData
{
    gp::UInt64 magic{ 0u };
    gp::USize transitionCount{ 0u };
    PipelineStage srcPipelines{ PipelineStage::None };
    PipelineStage dstPipelines{ PipelineStage::None };
};

class TransitionTestFixture : public ::testing::Test
{
protected:
    static inline gp::thread::Atomic<gp::UInt32> s_createHookCalls{ 0u };
    static inline gp::thread::Atomic<gp::UInt32> s_releaseHookCalls{ 0u };
    static inline gp::thread::Atomic<gp::UInt32> s_validMagicReleases{ 0u };

    static constexpr gp::UInt64 kExpectedMagic = 0x5452414E53495449ULL;

    static void resetCounters() noexcept
    {
        s_createHookCalls.store(0u, gp::memory::MemoryOrder::SeqCst);
        s_releaseHookCalls.store(0u, gp::memory::MemoryOrder::SeqCst);
        s_validMagicReleases.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    static void mockOnCreate(Transition* const transition, const TransitionCreateInfo& createInfo) noexcept
    {
        s_createHookCalls.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        MockBackendPrivateData* const data = transition->getPrivateData<MockBackendPrivateData>();
        data->magic = kExpectedMagic;
        data->transitionCount = createInfo.transitionInfos.size();
        data->srcPipelines = createInfo.srcPipelines;
        data->dstPipelines = createInfo.dstPipelines;
    }

    static void mockOnRelease(Transition* const transition) noexcept
    {
        s_releaseHookCalls.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        const Transition* const constTransition = transition;
        const MockBackendPrivateData* const data = constTransition->getPrivateData<MockBackendPrivateData>();
        if (data->magic == kExpectedMagic)
        {
            s_validMagicReleases.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        }
    }

    virtual void SetUp() override
    {
        TransitionController::resetBackend();
        resetCounters();
    }

    virtual void TearDown() override
    {
        TransitionController::resetBackend();
        resetCounters();
    }
};

}   // namespace

TEST_F(TransitionTestFixture, ZeroByteBackendLifecycleAndFlags)
{
    EXPECT_EQ(TransitionController::getPrivateDataSizeInBytes(), 0u);
    EXPECT_EQ(TransitionController::getPrivateDataAlignInBytes(), 0u);
    EXPECT_EQ(TransitionController::getTotalAllocationSize(), sizeof(Transition));
    EXPECT_EQ(TransitionController::getAlignment(), alignof(Transition));

    const TransitionCreateInfo createInfo(
        PipelineStage::Graphics, PipelineStage::Graphics, TransitionCreateFlags::AllowDuringRenderPass
    );

    Transition* const transition = TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);
    EXPECT_TRUE(transition->allowInRenderPass());
    EXPECT_EQ(transition->getFlags(), TransitionCreateFlags::AllowDuringRenderPass);
    EXPECT_NE(transition->getPendingStateMask(), 0u);

    TransitionController::markBegin(transition, PipelineStage::Graphics);
    EXPECT_NE(transition->getPendingStateMask(), 0u);

    // Final markEnd clears the last bit and frees the transition cleanly.
    TransitionController::markEnd(transition, PipelineStage::Graphics);
}

TEST_F(TransitionTestFixture, TrailingOverAlignedBackendPrivateDataAndCallbacks)
{
    TransitionBackendHooks hooks{};
    hooks.privateDataSizeInBytes = sizeof(MockBackendPrivateData);
    hooks.privateDataAlignInBytes = alignof(MockBackendPrivateData);
    hooks.onCreate = &TransitionTestFixture::mockOnCreate;
    hooks.onRelease = &TransitionTestFixture::mockOnRelease;

    TransitionController::configureBackend(hooks);

    EXPECT_EQ(TransitionController::getPrivateDataSizeInBytes(), sizeof(MockBackendPrivateData));
    EXPECT_EQ(TransitionController::getPrivateDataAlignInBytes(), 64u);
    EXPECT_GE(TransitionController::getAlignment(), 64u);
    EXPECT_EQ(
        TransitionController::getTotalAllocationSize(),
        gp::memory::align(sizeof(Transition), 64u) + sizeof(MockBackendPrivateData)
    );

    resource::Buffer* const fakeBuf = reinterpret_cast<resource::Buffer*>(static_cast<gp::UIntPtr>(0x1000u));
    const TransitionInfo infos[2] = {
        TransitionInfo(fakeBuf, ResourceAccess::CopyDest, ResourceAccess::SRVGraphics),
        TransitionInfo(fakeBuf, ResourceAccess::SRVGraphics, ResourceAccess::UAVCompute)
    };
    const TransitionCreateInfo createInfo(
        PipelineStage::Graphics, PipelineStage::AsyncCompute, TransitionCreateFlags::None, infos
    );

    Transition* const transition = TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);
    EXPECT_FALSE(transition->allowInRenderPass());
    EXPECT_EQ(s_createHookCalls.load(), 1u);
    EXPECT_EQ(s_releaseHookCalls.load(), 0u);

    const MockBackendPrivateData* const privateData = transition->getPrivateData<MockBackendPrivateData>();
    ASSERT_NE(privateData, nullptr);
    EXPECT_TRUE(gp::memory::isAligned(privateData, alignof(MockBackendPrivateData)));
    EXPECT_EQ(privateData->magic, kExpectedMagic);
    EXPECT_EQ(privateData->transitionCount, 2u);
    EXPECT_EQ(privateData->srcPipelines, PipelineStage::Graphics);
    EXPECT_EQ(privateData->dstPipelines, PipelineStage::AsyncCompute);

    TransitionController::markBegin(transition, PipelineStage::Graphics);
    EXPECT_EQ(s_releaseHookCalls.load(), 0u);

    TransitionController::markEnd(transition, PipelineStage::AsyncCompute);
    EXPECT_EQ(s_releaseHookCalls.load(), 1u);
    EXPECT_EQ(s_validMagicReleases.load(), 1u);
}

TEST_F(TransitionTestFixture, MultiPipelineSplitTransitionRetiresOnlyWhenAllBitsClear)
{
    TransitionBackendHooks hooks{};
    hooks.privateDataSizeInBytes = sizeof(MockBackendPrivateData);
    hooks.privateDataAlignInBytes = alignof(MockBackendPrivateData);
    hooks.onCreate = &TransitionTestFixture::mockOnCreate;
    hooks.onRelease = &TransitionTestFixture::mockOnRelease;
    TransitionController::configureBackend(hooks);

    const TransitionCreateInfo createInfo(PipelineStage::All, PipelineStage::All);
    Transition* const transition = TransitionController::create(createInfo);
    ASSERT_NE(transition, nullptr);

    // Complete begin/end in interleaved order across Graphics and AsyncCompute.
    TransitionController::markBegin(transition, PipelineStage::Graphics);
    EXPECT_EQ(s_releaseHookCalls.load(), 0u);

    TransitionController::markEnd(transition, PipelineStage::AsyncCompute);
    EXPECT_EQ(s_releaseHookCalls.load(), 0u);

    TransitionController::markEnd(transition, PipelineStage::Graphics);
    EXPECT_EQ(s_releaseHookCalls.load(), 0u);

    // Final remaining bit triggers backend release and cleanup.
    TransitionController::markBegin(transition, PipelineStage::AsyncCompute);
    EXPECT_EQ(s_releaseHookCalls.load(), 1u);
    EXPECT_EQ(s_validMagicReleases.load(), 1u);
}

TEST_F(TransitionTestFixture, ConcurrentSplitBeginAndEndRetireEveryTransitionOnce)
{
    TransitionBackendHooks hooks{};
    hooks.privateDataSizeInBytes = sizeof(MockBackendPrivateData);
    hooks.privateDataAlignInBytes = alignof(MockBackendPrivateData);
    hooks.onCreate = &TransitionTestFixture::mockOnCreate;
    hooks.onRelease = &TransitionTestFixture::mockOnRelease;
    TransitionController::configureBackend(hooks);

    constexpr gp::UInt32 kTransitionCount = 2000u;
    gp::Vector<Transition*> transitions{};
    transitions.reserve(kTransitionCount);

    const TransitionCreateInfo createInfo(PipelineStage::All, PipelineStage::All);
    for (gp::UInt32 i = 0u; i < kTransitionCount; ++i)
    {
        Transition* const transition = TransitionController::create(createInfo);
        ASSERT_NE(transition, nullptr);
        transitions.pushBack(transition);
    }

    EXPECT_EQ(s_createHookCalls.load(), kTransitionCount);

    gp::thread::Atomic<bool> startLatch{ false };

    std::thread graphicsThread(
        [&transitions, &startLatch]() -> void
    {
        while (!startLatch.load(gp::memory::MemoryOrder::Acquire))
        {
            std::this_thread::yield();
        }
        for (gp::UInt32 i = 0u; i < kTransitionCount; ++i)
        {
            TransitionController::markBegin(transitions[i], PipelineStage::Graphics);
            TransitionController::markEnd(transitions[i], PipelineStage::Graphics);
        }
    }
    );

    std::thread asyncComputeThread(
        [&transitions, &startLatch]() -> void
    {
        while (!startLatch.load(gp::memory::MemoryOrder::Acquire))
        {
            std::this_thread::yield();
        }
        for (gp::UInt32 i = 0u; i < kTransitionCount; ++i)
        {
            TransitionController::markBegin(transitions[i], PipelineStage::AsyncCompute);
            TransitionController::markEnd(transitions[i], PipelineStage::AsyncCompute);
        }
    }
    );

    startLatch.store(true, gp::memory::MemoryOrder::Release);
    graphicsThread.join();
    asyncComputeThread.join();

    EXPECT_EQ(s_releaseHookCalls.load(gp::memory::MemoryOrder::Acquire), kTransitionCount);
    EXPECT_EQ(s_validMagicReleases.load(gp::memory::MemoryOrder::Acquire), kTransitionCount);
}

TEST_F(TransitionTestFixture, OneSidedPipelinesAllStageMaskAndAlignmentFloor)
{
    // Backend requesting 0 bytes with 16-byte power-of-two alignment preserves alignment and aligns allocation size.
    TransitionBackendHooks zeroSizeHooks{};
    zeroSizeHooks.privateDataSizeInBytes = 0u;
    zeroSizeHooks.privateDataAlignInBytes = 16u;
    TransitionController::configureBackend(zeroSizeHooks);
    EXPECT_EQ(TransitionController::getPrivateDataSizeInBytes(), 0u);
    EXPECT_EQ(TransitionController::getPrivateDataAlignInBytes(), 16u);
    EXPECT_EQ(TransitionController::getTotalAllocationSize(), gp::memory::align(sizeof(Transition), 16u));

    // Backend requesting alignment smaller than alignof(Transition) keeps alignof(Transition).
    TransitionBackendHooks smallAlignHooks{};
    smallAlignHooks.privateDataSizeInBytes = 4u;
    smallAlignHooks.privateDataAlignInBytes = 1u;
    smallAlignHooks.onCreate = [](Transition* const transition, const TransitionCreateInfo&) noexcept -> void
    {
        s_createHookCalls.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        *transition->getPrivateData<gp::UInt32>() = 0xDEADBEEFu;
    };
    smallAlignHooks.onRelease = [](Transition* const transition) noexcept -> void
    {
        s_releaseHookCalls.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        if (*transition->getPrivateData<gp::UInt32>() == 0xDEADBEEFu)
        {
            s_validMagicReleases.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
        }
    };
    TransitionController::configureBackend(smallAlignHooks);
    EXPECT_EQ(TransitionController::getAlignment(), alignof(Transition));

    // srcPipelines == None retires immediately on markEnd(dst).
    {
        const TransitionCreateInfo endOnlyInfo(PipelineStage::None, PipelineStage::Graphics);
        Transition* const transition = TransitionController::create(endOnlyInfo);
        ASSERT_NE(transition, nullptr);
        TransitionController::markEnd(transition, PipelineStage::Graphics);
        EXPECT_EQ(s_releaseHookCalls.load(), 1u);
        EXPECT_EQ(s_validMagicReleases.load(), 1u);
    }

    // dstPipelines == None retires immediately on markBegin(src).
    {
        const TransitionCreateInfo beginOnlyInfo(PipelineStage::AsyncCompute, PipelineStage::None);
        Transition* const transition = TransitionController::create(beginOnlyInfo);
        ASSERT_NE(transition, nullptr);
        TransitionController::markBegin(transition, PipelineStage::AsyncCompute);
        EXPECT_EQ(s_releaseHookCalls.load(), 2u);
        EXPECT_EQ(s_validMagicReleases.load(), 2u);
    }

    // Passing PipelineStage::All clears both pipeline bits in a single call.
    {
        const TransitionCreateInfo allInfo(PipelineStage::All, PipelineStage::All);
        Transition* const transition = TransitionController::create(allInfo);
        ASSERT_NE(transition, nullptr);
        TransitionController::markBegin(transition, PipelineStage::All);
        EXPECT_EQ(s_releaseHookCalls.load(), 2u);
        TransitionController::markEnd(transition, PipelineStage::All);
        EXPECT_EQ(s_releaseHookCalls.load(), 3u);
        EXPECT_EQ(s_validMagicReleases.load(), 3u);
    }
}

}   // namespace gp::rhi::sync::tests
