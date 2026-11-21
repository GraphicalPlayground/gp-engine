// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/roles/ScopedThreadRole.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/roles/ThreadRole.hpp"
#include "threading/roles/ThreadRoleEnums.hpp"
#include "threading/roles/ThreadRoleTypes.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::thread::tests
{

namespace
{

/// @brief Context passed to a worker that attempts to claim an already-owned `NamedThread` via `ScopedNamedThread`.
struct ScopedWorkerContext
{
    NamedThread target{ NamedThread::Render };
    ThreadRoleResult observedResult{ ThreadRoleResult::Success };
    bool observedAttached{ true };
    NamedThread observedTarget{ NamedThread::Count };
    bool observedInRole{ true };
};

/// @brief Worker entry that constructs a `ScopedNamedThread` and records its state.
gp::UInt32 scopedWorkerEntry(void* rawContext)
{
    ScopedWorkerContext& context = *static_cast<ScopedWorkerContext*>(rawContext);
    const ScopedNamedThread guard(context.target);
    context.observedResult = guard.getResult();
    context.observedAttached = guard.isAttached();
    context.observedTarget = guard.getNamedThread();
    context.observedInRole = isInThreadRole(context.target);
    return 0u;
}

}   // namespace

TEST(ScopedThreadRoleTest, GrantsAndRestoresScopedRole)
{
    EXPECT_FALSE(isInParallelRenderThread());

    {
        const ScopedThreadRole guard(ThreadRole::ParallelRender);
        EXPECT_EQ(guard.getResult(), ThreadRoleResult::Success);
        EXPECT_TRUE(guard.isActive());
        EXPECT_TRUE(isInParallelRenderThread());
        EXPECT_FALSE(isInRenderThread());
    }

    EXPECT_FALSE(isInParallelRenderThread());
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::None);
}

TEST(ScopedThreadRoleTest, NestedGuardsUnwindCleanly)
{
    {
        const ScopedThreadRole outer(ThreadRole::ParallelRender);
        EXPECT_TRUE(outer.isActive());
        EXPECT_TRUE(isInParallelRenderThread());

        {
            const ScopedThreadRole inner(ThreadRole::ParallelRender);
            EXPECT_TRUE(inner.isActive());
            EXPECT_TRUE(isInParallelRenderThread());
        }

        // Inner exited, but outer still holds ParallelRender.
        EXPECT_TRUE(isInParallelRenderThread());
    }

    EXPECT_FALSE(isInParallelRenderThread());
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::None);
}

TEST(ScopedThreadRoleTest, InvalidRoleLeavesMaskUntouched)
{
    {
        const ScopedThreadRole emptyGuard(ThreadRole::None);
        EXPECT_EQ(emptyGuard.getResult(), ThreadRoleResult::InvalidArguments);
        EXPECT_FALSE(emptyGuard.isActive());
        EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::None);
    }

    {
        const ScopedThreadRole exclusiveGuard(ThreadRole::Game);
        EXPECT_EQ(exclusiveGuard.getResult(), ThreadRoleResult::InvalidArguments);
        EXPECT_FALSE(exclusiveGuard.isActive());
        EXPECT_FALSE(isInGameThread());
        EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::None);
    }
}

TEST(ScopedNamedThreadTest, AttachesAndDetachesOnScopeExit)
{
    EXPECT_FALSE(isInGameThread());
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(NamedThread::Game));

    {
        const ScopedNamedThread guard(NamedThread::Game);
        EXPECT_EQ(guard.getResult(), ThreadRoleResult::Success);
        EXPECT_TRUE(guard.isAttached());
        EXPECT_EQ(guard.getNamedThread(), NamedThread::Game);
        EXPECT_TRUE(isInGameThread());
        EXPECT_TRUE(ThreadRoleRegistry::isAttached(NamedThread::Game));
    }

    EXPECT_FALSE(isInGameThread());
    EXPECT_FALSE(ThreadRoleRegistry::isAttached(NamedThread::Game));
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::None);
}

TEST(ScopedNamedThreadTest, NestedGuardDoesNotDetachOuterOwner)
{
    const ScopedNamedThread outer(NamedThread::Render);
    ASSERT_TRUE(outer.isAttached());

    {
        const ScopedNamedThread inner(NamedThread::Render);
        EXPECT_EQ(inner.getResult(), ThreadRoleResult::AlreadyAttached);
        EXPECT_FALSE(inner.isAttached());
        EXPECT_EQ(inner.getNamedThread(), NamedThread::Render);
        EXPECT_TRUE(isInRenderThread());
    }

    // Inner failed with AlreadyAttached, so outer's claim must remain intact.
    EXPECT_TRUE(isInRenderThread());
    EXPECT_TRUE(ThreadRoleRegistry::isAttached(NamedThread::Render));
}

TEST(ScopedThreadRoleTest, PreservesExclusiveRolesAcrossScope)
{
    const ScopedNamedThread gameGuard(NamedThread::Game);
    ASSERT_TRUE(gameGuard.isAttached());

    {
        const ScopedThreadRole parallelGuard(ThreadRole::ParallelRender);
        ASSERT_TRUE(parallelGuard.isActive());
        EXPECT_TRUE(isInGameThread());
        EXPECT_TRUE(isInParallelRenderThread());
        EXPECT_FALSE(isInRenderThread());
    }

    EXPECT_TRUE(isInGameThread());
    EXPECT_FALSE(isInParallelRenderThread());
}

TEST(ScopedNamedThreadTest, InvalidNamedThreadReportsInvalidArguments)
{
    const ScopedNamedThread guard(NamedThread::Count);
    EXPECT_EQ(guard.getResult(), ThreadRoleResult::InvalidArguments);
    EXPECT_FALSE(guard.isAttached());
    EXPECT_EQ(guard.getNamedThread(), NamedThread::Count);
    EXPECT_EQ(ThreadRoleRegistry::getCurrentRoles(), ThreadRole::None);
}

TEST(ScopedNamedThreadTest, ContendedWorkerSeesRoleClaimedByOtherThread)
{
    const ScopedNamedThread owner(NamedThread::RHI);
    ASSERT_TRUE(owner.isAttached());

    ScopedWorkerContext context{};
    context.target = NamedThread::RHI;

    gp::platform::Thread worker{};
    gp::platform::ThreadDesc desc{};
    desc.name = "GP_ScopedNamedThreadTestWorker";
    desc.entry = &scopedWorkerEntry;
    desc.userData = &context;
    ASSERT_EQ(worker.start(desc), gp::platform::ThreadResult::Success);

    gp::UInt32 exitCode = 1u;
    EXPECT_EQ(worker.join(&exitCode), gp::platform::ThreadResult::Success);
    EXPECT_EQ(exitCode, 0u);

    EXPECT_EQ(context.observedResult, ThreadRoleResult::RoleClaimedByOtherThread);
    EXPECT_FALSE(context.observedAttached);
    EXPECT_EQ(context.observedTarget, NamedThread::RHI);
    EXPECT_FALSE(context.observedInRole);
    EXPECT_TRUE(isInRHIThread());
}

}   // namespace gp::thread::tests
