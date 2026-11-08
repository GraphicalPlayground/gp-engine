// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/sync/JobCounter.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/fibers/Fiber.hpp"

namespace gp::thread
{

JobCounter::~JobCounter() noexcept
{
    GP_ASSERT(
        getWaiterCount(gp::MemoryOrder::Acquire) == 0u && m_waitListHead == nullptr,
        "gp::thread::JobCounter destroyed while fibers or threads are still parked on its wait list."
    );
}

void JobCounter::acquireWaitListLock() noexcept
{
    for (;;)
    {
        const gp::UInt64 prev = m_state.fetchOr(kJobCounterLockBit, gp::MemoryOrder::Acquire);
        if ((prev & kJobCounterLockBit) == 0ULL) [[likely]]
        {
            return;
        }

        while ((m_state.load(gp::MemoryOrder::Relaxed) & kJobCounterLockBit) != 0ULL)
        {
            gp::platform::Thread::spinWait();
        }
    }
}

void JobCounter::notifyWaiters() noexcept
{
    acquireWaitListLock();

    JobWaitNode* wakeHead = nullptr;
    JobWaitNode* wakeTail = nullptr;
    gp::UInt32 wokenCount = 0u;
    bool anyRemainingHasThreshold = false;
    gp::UInt64 lockedState = m_state.load(gp::MemoryOrder::Acquire);

    for (;;)
    {
        const JobCounterValue currentCount = unpackCount(lockedState);
        JobWaitNode* remainingHead = nullptr;
        JobWaitNode* remainingTail = nullptr;
        anyRemainingHasThreshold = false;

        JobWaitNode* cursor = m_waitListHead;
        while (cursor != nullptr)
        {
            JobWaitNode* const nextNode = cursor->next;
            cursor->next = nullptr;

            if (currentCount <= cursor->targetCount)
            {
                // Reverse LIFO stack insertion order by prepending to wakeHead so waiters are woken in FIFO arrival
                // order.
                if (wakeHead == nullptr)
                {
                    wakeHead = cursor;
                    wakeTail = cursor;
                }
                else
                {
                    cursor->next = wakeHead;
                    wakeHead = cursor;
                }
                ++wokenCount;
            }
            else
            {
                if (cursor->targetCount > 0u)
                {
                    anyRemainingHasThreshold = true;
                }

                if (remainingHead == nullptr)
                {
                    remainingHead = cursor;
                    remainingTail = cursor;
                }
                else
                {
                    remainingTail->next = cursor;
                    remainingTail = cursor;
                }
            }

            cursor = nextNode;
        }

        m_waitListHead = remainingHead;

        // If no waiters remain, or the active job count did not drop further while we scanned the list, exit the loop.
        lockedState = m_state.load(gp::MemoryOrder::Acquire);
        if (m_waitListHead == nullptr || unpackCount(lockedState) == currentCount) [[likely]]
        {
            break;
        }
    }

    (void)wakeTail;

    // Atomically subtract wokenCount from waiterCount (bits [32..61]), clear kJobCounterHasThresholdWaiterBit (bit
    // [62]) if no remaining waiter has targetCount > 0, and clear kJobCounterLockBit (bit [63]) in a single fetchSub
    // without modifying the active job count (bits [0..31]).
    const bool clearThresholdBit =
        ((lockedState & kJobCounterHasThresholdWaiterBit) != 0ULL) && !anyRemainingHasThreshold;
    const gp::UInt64 unlockAndSubDelta = (static_cast<gp::UInt64>(wokenCount) << kJobCounterWaiterShift) |
                                         (clearThresholdBit ? kJobCounterHasThresholdWaiterBit : 0ULL) |
                                         kJobCounterLockBit;
    (void)m_state.fetchSub(unlockAndSubDelta, gp::MemoryOrder::Release);

    // Notify all detached waiters outside the wait-list lock. Copy node fields into registers before storing
    // JobWaitNodeState::Signaled because the waiter's stack frame may unwind immediately upon observing Signaled.
    JobWaitNode* wakeCursor = wakeHead;
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

JobSyncResult JobCounter::add(const JobCounterValue delta, JobCounterValue* const outNewCount) noexcept
{
    if (delta == 0u)
    {
        return JobSyncResult::InvalidArguments;
    }

    gp::UInt64 expected = m_state.load(gp::MemoryOrder::Relaxed);
    for (;;)
    {
        const JobCounterValue currentCount = unpackCount(expected);
        const gp::UInt64 newCount64 = static_cast<gp::UInt64>(currentCount) + static_cast<gp::UInt64>(delta);
        if (newCount64 > static_cast<gp::UInt64>(kMaxJobCounterValue)) [[unlikely]]
        {
            return JobSyncResult::CounterOverflow;
        }

        const gp::UInt64 desired = expected + static_cast<gp::UInt64>(delta);
        if (m_state.compareExchangeWeak(expected, desired, gp::MemoryOrder::AcqRel, gp::MemoryOrder::Relaxed))
        {
            if (outNewCount != nullptr)
            {
                *outNewCount = static_cast<JobCounterValue>(newCount64);
            }
            return JobSyncResult::Success;
        }
    }
}

JobSyncResult JobCounter::decrement(const JobCounterValue delta, JobCounterValue* const outNewCount) noexcept
{
    if (delta == 0u)
    {
        return JobSyncResult::InvalidArguments;
    }

    const gp::UInt64 currentSnapshot = m_state.load(gp::MemoryOrder::Relaxed);
    if (unpackCount(currentSnapshot) < delta) [[unlikely]]
    {
        return JobSyncResult::CounterUnderflow;
    }

    // Single-instruction fast path: subtract delta from bits [0..31]. Because count >= delta, this subtraction never
    // borrows from bits [32..63].
    const gp::UInt64 prev = m_state.fetchSub(static_cast<gp::UInt64>(delta), gp::MemoryOrder::AcqRel);
    const JobCounterValue prevCount = unpackCount(prev);
    if (prevCount < delta) [[unlikely]]
    {
        // Roll back concurrent underflow without leaving borrowed upper bits in m_state.
        (void)m_state.fetchAdd(static_cast<gp::UInt64>(delta), gp::MemoryOrder::Relaxed);
        return JobSyncResult::CounterUnderflow;
    }

    const JobCounterValue newCount = prevCount - delta;
    if (outNewCount != nullptr)
    {
        *outNewCount = newCount;
    }

    // When newCount reaches 0, inspect the wait list if any waiter is parked or another core holds kJobCounterLockBit
    // (prev >> 32 != 0). On intermediate decrements (newCount > 0), only inspect the wait list if at least one waiter
    // has targetCount > 0 (kJobCounterHasThresholdWaiterBit) or another core is concurrently linking a waiter
    // (kJobCounterLockBit).
    const bool shouldNotify = (newCount == 0u)
                                  ? ((prev >> kJobCounterWaiterShift) != 0ULL)
                                  : ((prev & (kJobCounterHasThresholdWaiterBit | kJobCounterLockBit)) != 0ULL);
    if (shouldNotify) [[unlikely]]
    {
        notifyWaiters();
    }

    return JobSyncResult::Success;
}

JobSyncResult JobCounter::reset(const JobCounterValue newCount) noexcept
{
    acquireWaitListLock();

    const gp::UInt64 lockedState = m_state.load(gp::MemoryOrder::Acquire);
    if (unpackWaiterCount(lockedState) != 0u || m_waitListHead != nullptr)
    {
        (void)m_state.fetchAnd(~kJobCounterLockBit, gp::MemoryOrder::Release);
        return JobSyncResult::BusyWaiting;
    }

    m_state.store(static_cast<gp::UInt64>(newCount), gp::MemoryOrder::Release);
    return JobSyncResult::Success;
}

JobSyncResult JobCounter::wait(const JobCounterValue targetCount, const JobWaitMode mode) noexcept
{
    // Lock-free fast path when the counter is already at or below targetCount.
    if (getCount(gp::MemoryOrder::Acquire) <= targetCount)
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
    waitNode.targetCount = targetCount;
    waitNode.state.store(JobWaitNodeState::Waiting, gp::MemoryOrder::Relaxed);

    acquireWaitListLock();

    const gp::UInt64 lockedState = m_state.load(gp::MemoryOrder::Acquire);
    if (unpackCount(lockedState) <= targetCount)
    {
        (void)m_state.fetchAnd(~kJobCounterLockBit, gp::MemoryOrder::Release);
        return JobSyncResult::Success;
    }

    GP_ASSERT(
        unpackWaiterCount(lockedState) < kMaxJobCounterWaiters,
        "gp::thread::JobCounter exceeded maximum 30-bit waiter count."
    );

    waitNode.next = m_waitListHead;
    m_waitListHead = &waitNode;

    // Simultaneously increment waiterCount by 1, set kJobCounterHasThresholdWaiterBit if targetCount > 0 (and not
    // already set), and clear kJobCounterLockBit (bit 63 wraps 1 + 1 -> 0) in a single atomic fetchAdd.
    const bool setThresholdBit = (targetCount > 0u) && ((lockedState & kJobCounterHasThresholdWaiterBit) == 0ULL);
    const gp::UInt64 unlockAndAddDelta =
        kJobCounterAddWaiterAndUnlockDelta | (setThresholdBit ? kJobCounterHasThresholdWaiterBit : 0ULL);
    (void)m_state.fetchAdd(unlockAndAddDelta, gp::MemoryOrder::Release);

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
                // Safely unlink the stack-allocated waitNode before returning so m_waitListHead never retains a
                // dangling stack pointer on yield failure.
                acquireWaitListLock();
                JobWaitNode* prevNode = nullptr;
                JobWaitNode* curNode = m_waitListHead;
                bool removedFromList = false;
                while (curNode != nullptr)
                {
                    if (curNode == &waitNode)
                    {
                        if (prevNode == nullptr)
                        {
                            m_waitListHead = curNode->next;
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

                if (removedFromList)
                {
                    bool anyRemainingHasThreshold = false;
                    for (const JobWaitNode* scan = m_waitListHead; scan != nullptr; scan = scan->next)
                    {
                        if (scan->targetCount > 0u)
                        {
                            anyRemainingHasThreshold = true;
                            break;
                        }
                    }

                    const gp::UInt64 unlinkState = m_state.load(gp::MemoryOrder::Acquire);
                    const bool clearThresholdBit =
                        ((unlinkState & kJobCounterHasThresholdWaiterBit) != 0ULL) && !anyRemainingHasThreshold;
                    const gp::UInt64 unlinkSubDelta = kJobCounterOneWaiter |
                                                      (clearThresholdBit ? kJobCounterHasThresholdWaiterBit : 0ULL) |
                                                      kJobCounterLockBit;
                    (void)m_state.fetchSub(unlinkSubDelta, gp::MemoryOrder::Release);
                    waitNode.state.store(JobWaitNodeState::Signaled, gp::MemoryOrder::Release);
                }
                else
                {
                    (void)m_state.fetchAnd(~kJobCounterLockBit, gp::MemoryOrder::Release);
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
