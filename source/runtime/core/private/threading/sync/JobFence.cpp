// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/sync/JobFence.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/fibers/Fiber.hpp"

namespace gp::thread
{

namespace detail
{

[[nodiscard]] static GP_FORCEINLINE gp::UIntPtr acquireFenceLock(Atomic<gp::UIntPtr>& state) noexcept
{
    for (;;)
    {
        const gp::UIntPtr prev = state.fetchOr(kJobFenceLockBit, gp::MemoryOrder::Acquire);
        if ((prev & kJobFenceLockBit) == 0ULL) [[likely]]
        {
            return prev;
        }

        while ((state.load(gp::MemoryOrder::Relaxed) & kJobFenceLockBit) != 0ULL)
        {
            gp::platform::Thread::spinWait();
        }
    }
}

}   // namespace detail

JobFence::~JobFence() noexcept
{
    GP_ASSERT(
        !hasWaiters(gp::MemoryOrder::Acquire),
        "gp::thread::JobFence destroyed while fibers or threads are still parked on its wait list."
    );
}

void JobFence::signal() noexcept
{
    // Fast-path check if already signaled with no waiters.
    const gp::UIntPtr current = m_state.load(gp::MemoryOrder::Acquire);
    if ((current & kJobFenceSignaledBit) != 0ULL && (current & kJobFencePointerMask) == 0ULL)
    {
        return;
    }

    const gp::UIntPtr prev = detail::acquireFenceLock(m_state);
    JobWaitNode* const rawHead = reinterpret_cast<JobWaitNode*>(prev & kJobFencePointerMask);

    // Transition to the signaled state, clear the wait-list head pointer, and release kJobFenceLockBit.
    m_state.store(kJobFenceSignaledBit, gp::MemoryOrder::Release);

    if (rawHead == nullptr)
    {
        return;
    }

    // Reverse the LIFO stack list so waiters are woken in FIFO arrival order.
    JobWaitNode* fifoHead = nullptr;
    JobWaitNode* cursor = rawHead;
    while (cursor != nullptr)
    {
        JobWaitNode* const nextNode = cursor->next;
        cursor->next = fifoHead;
        fifoHead = cursor;
        cursor = nextNode;
    }

    JobWaitNode* wakeCursor = fifoHead;
    while (wakeCursor != nullptr)
    {
        JobWaitNode* const nextWake = wakeCursor->next;
        Fiber* const waitingFiber = wakeCursor->waitingFiber;
        const JobWakeFunction wakeCallback = wakeCursor->wakeCallback;
        void* const wakeUserData = wakeCursor->wakeUserData;

        wakeCursor->next = nullptr;
        wakeCursor->state.store(JobWaitNodeState::Signaled, gp::MemoryOrder::Release);

        if (wakeCallback != nullptr)
        {
            wakeCallback(waitingFiber, wakeUserData);
        }

        wakeCursor = nextWake;
    }
}

JobSyncResult JobFence::reset() noexcept
{
    const gp::UIntPtr prev = detail::acquireFenceLock(m_state);
    if ((prev & kJobFencePointerMask) != 0ULL)
    {
        (void)m_state.fetchAnd(~kJobFenceLockBit, gp::MemoryOrder::Release);
        return JobSyncResult::BusyWaiting;
    }

    m_state.store(0ULL, gp::MemoryOrder::Release);
    return JobSyncResult::Success;
}

JobSyncResult JobFence::wait(const JobWaitMode mode) noexcept
{
    if (isSignaled(gp::MemoryOrder::Acquire))
    {
        return JobSyncResult::Success;
    }

    Fiber* const currentFiber = (mode == JobWaitMode::Auto) ? Fiber::getCurrentFiber() : nullptr;
    const JobFiberParker parker = (mode == JobWaitMode::Auto) ? JobFiberParker::getThreadParker() : JobFiberParker{};
    const bool hasCustomParker = parker.isValid();
    const bool hasReturnFiber = (currentFiber != nullptr) && !currentFiber->isThreadAnchor() &&
                                (currentFiber->getReturnFiber() != nullptr) &&
                                currentFiber->getReturnFiber()->isValid();
    const bool canYieldFiber =
        (currentFiber != nullptr) && !currentFiber->isThreadAnchor() && (hasCustomParker || hasReturnFiber);

    JobWaitNode waitNode{};
    waitNode.next = nullptr;
    waitNode.waitingFiber = canYieldFiber ? currentFiber : nullptr;
    waitNode.wakeCallback = (canYieldFiber && hasCustomParker) ? parker.wakeCallback : nullptr;
    waitNode.wakeUserData = (canYieldFiber && hasCustomParker) ? parker.userData : nullptr;
    waitNode.targetCount = 0u;
    waitNode.state.store(JobWaitNodeState::Waiting, gp::MemoryOrder::Relaxed);

    const gp::UIntPtr prev = detail::acquireFenceLock(m_state);
    if ((prev & kJobFenceSignaledBit) != 0ULL)
    {
        (void)m_state.fetchAnd(~kJobFenceLockBit, gp::MemoryOrder::Release);
        return JobSyncResult::Success;
    }

    JobWaitNode* const oldHead = reinterpret_cast<JobWaitNode*>(prev & kJobFencePointerMask);
    waitNode.next = oldHead;

    const gp::UIntPtr newHeadBits = reinterpret_cast<gp::UIntPtr>(&waitNode);
    GP_ASSERT(
        (newHeadBits & kJobFenceFlagsMask) == 0ULL,
        "gp::thread::JobWaitNode address must be at least 4-byte aligned for JobFence pointer packing."
    );

    // Publish the new head pointer and clear kJobFenceLockBit with Release ordering.
    m_state.store(newHeadBits, gp::MemoryOrder::Release);

    if (canYieldFiber)
    {
        while (!waitNode.isSignaled(gp::MemoryOrder::Acquire))
        {
            JobSyncResult yieldRes = JobSyncResult::Success;
            if (hasCustomParker)
            {
                yieldRes = parker.yieldCallback(*currentFiber, parker.userData);
            }
            else
            {
                const FiberResult switchRes = currentFiber->yieldToReturnFiber();
                yieldRes = (switchRes == FiberResult::Success) ? JobSyncResult::Success : JobSyncResult::YieldFailed;
            }

            if (yieldRes != JobSyncResult::Success) [[unlikely]]
            {
                const gp::UIntPtr lockedBits = detail::acquireFenceLock(m_state);
                JobWaitNode* headNode = reinterpret_cast<JobWaitNode*>(lockedBits & kJobFencePointerMask);
                JobWaitNode* prevNode = nullptr;
                JobWaitNode* curNode = headNode;
                bool removedFromList = false;

                while (curNode != nullptr)
                {
                    if (curNode == &waitNode)
                    {
                        if (prevNode == nullptr)
                        {
                            headNode = curNode->next;
                        }
                        else
                        {
                            prevNode->next = curNode->next;
                        }
                        curNode->next = nullptr;
                        removedFromList = true;
                        break;
                    }
                    prevNode = curNode;
                    curNode = curNode->next;
                }

                const gp::UIntPtr preservedSignaled = (lockedBits & kJobFenceSignaledBit);
                m_state.store(reinterpret_cast<gp::UIntPtr>(headNode) | preservedSignaled, gp::MemoryOrder::Release);

                if (removedFromList)
                {
                    waitNode.state.store(JobWaitNodeState::Signaled, gp::MemoryOrder::Release);
                }
                else
                {
                    while (!waitNode.isSignaled(gp::MemoryOrder::Acquire))
                    {
                        gp::platform::Thread::spinWait();
                    }
                }

                return JobSyncResult::YieldFailed;
            }
        }
    }
    else
    {
        while (!waitNode.isSignaled(gp::MemoryOrder::Acquire))
        {
            gp::platform::Thread::spinWait();
        }
    }

    return JobSyncResult::Success;
}

}   // namespace gp::thread
