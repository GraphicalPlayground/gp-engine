// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/sync/RecursiveMutex.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Futex.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/fibers/Fiber.hpp"

namespace gp::thread
{

gp::UInt64 RecursiveMutex::currentExecutionOwnerToken() noexcept
{
    const Fiber* const activeFiber = Fiber::getCurrentFiber();
    if (activeFiber != nullptr && !activeFiber->isThreadAnchor())
    {
        return kRecursiveMutexFiberOwnerBit | static_cast<gp::UInt64>(activeFiber->getId());
    }

    const gp::UInt64 osThreadId = gp::platform::Thread::currentThreadId();
    return (osThreadId != 0ULL) ? (osThreadId & ~kRecursiveMutexFiberOwnerBit) : 1ULL;
}

bool RecursiveMutex::isOwnedByCurrentContext() const noexcept
{
    const gp::UInt64 currentOwner = m_ownerToken.load(gp::MemoryOrder::Relaxed);
    if (currentOwner == kInvalidExecutionOwnerToken)
    {
        return false;
    }
    return currentOwner == currentExecutionOwnerToken();
}

void RecursiveMutex::lock() noexcept
{
    const gp::UInt64 callerToken = currentExecutionOwnerToken();
    if (m_ownerToken.load(gp::MemoryOrder::Relaxed) == callerToken)
    {
        GP_ASSERT(m_recursionDepth < 0xFFFFFFFFu, "RecursiveMutex recursion depth overflow.");
        ++m_recursionDepth;
        return;
    }

    gp::UInt32 expected = 0u;
    if (m_state.compareExchangeWeak(expected, kMutexLockedBit, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed))
        [[likely]]
    {
        m_ownerToken.store(callerToken, gp::MemoryOrder::Relaxed);
        m_recursionDepth = 1u;
#if GP_DO_CHECK
        Fiber::adjustActiveNonYieldingLockCount(1);
#endif
        return;
    }

    lockContended(callerToken);
}

bool RecursiveMutex::tryLock() noexcept
{
    const gp::UInt64 callerToken = currentExecutionOwnerToken();
    if (m_ownerToken.load(gp::MemoryOrder::Relaxed) == callerToken)
    {
        if (m_recursionDepth == 0xFFFFFFFFu) [[unlikely]]
        {
            return false;
        }
        ++m_recursionDepth;
        return true;
    }

    const gp::UInt32 current = m_state.load(gp::MemoryOrder::Relaxed);
    if ((current & kMutexLockedBit) != 0u)
    {
        return false;
    }

    if ((m_state.fetchOr(kMutexLockedBit, gp::MemoryOrder::Acquire) & kMutexLockedBit) == 0u)
    {
        m_ownerToken.store(callerToken, gp::MemoryOrder::Relaxed);
        m_recursionDepth = 1u;
#if GP_DO_CHECK
        Fiber::adjustActiveNonYieldingLockCount(1);
#endif
        return true;
    }

    return false;
}

void RecursiveMutex::unlock() noexcept
{
    const gp::UInt64 callerToken = currentExecutionOwnerToken();
    GP_ASSERT(
        m_ownerToken.load(gp::MemoryOrder::Relaxed) == callerToken,
        "RecursiveMutex::unlock() called by an execution context that does not own the lock."
    );
    GP_ASSERT(m_recursionDepth > 0u, "RecursiveMutex::unlock() called with zero recursion depth.");

    if (m_recursionDepth > 1u)
    {
        --m_recursionDepth;
        return;
    }

    m_recursionDepth = 0u;
    m_ownerToken.store(kInvalidExecutionOwnerToken, gp::MemoryOrder::Relaxed);
#if GP_DO_CHECK
    Fiber::adjustActiveNonYieldingLockCount(-1);
#endif

    const gp::UInt32 prev = m_state.exchange(0u, gp::MemoryOrder::Release);
    GP_ASSERT((prev & kMutexLockedBit) != 0u, "RecursiveMutex::unlock() observed unlocked state.");
    if ((prev & kMutexMayHaveWaitersBit) != 0u) [[unlikely]]
    {
        gp::platform::Futex::wakeOne(m_state);
    }
}

GP_COLD void RecursiveMutex::lockContended(const gp::UInt64 callerToken) noexcept
{
    const bool inChildFiber = (callerToken & kRecursiveMutexFiberOwnerBit) != 0ULL;

    for (;;)
    {
        for (gp::UInt32 spin = 0u; spin < kMutexSpinIterationCount; ++spin)
        {
            const gp::UInt32 current = m_state.load(gp::MemoryOrder::Relaxed);
            if ((current & kMutexLockedBit) == 0u)
            {
                if ((m_state.fetchOr(kMutexLockedBit, gp::MemoryOrder::Acquire) & kMutexLockedBit) == 0u)
                {
                    m_ownerToken.store(callerToken, gp::MemoryOrder::Relaxed);
                    m_recursionDepth = 1u;
#if GP_DO_CHECK
                    Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                    return;
                }
            }
            gp::platform::Thread::spinWait();
        }

        if (inChildFiber)
        {
            gp::platform::Thread::yieldExecution();
            continue;
        }

        gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
        for (;;)
        {
            if ((expected & kMutexLockedBit) == 0u)
            {
                if (m_state.compareExchangeWeak(
                        expected, expected | kMutexLockedBit, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
                    ))
                {
                    m_ownerToken.store(callerToken, gp::MemoryOrder::Relaxed);
                    m_recursionDepth = 1u;
#if GP_DO_CHECK
                    Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                    return;
                }
                continue;
            }

            if ((expected & kMutexMayHaveWaitersBit) == 0u)
            {
                if (!m_state.compareExchangeWeak(
                        expected, expected | kMutexMayHaveWaitersBit, gp::MemoryOrder::Relaxed, gp::MemoryOrder::Relaxed
                    ))
                {
                    continue;
                }
            }

            break;
        }

        (void)gp::platform::Futex::waitOnAddress(m_state, kMutexLockedBit | kMutexMayHaveWaitersBit);

        while ((m_state.exchange(kMutexLockedBit | kMutexMayHaveWaitersBit, gp::MemoryOrder::Acquire) &
                kMutexLockedBit) != 0u)
        {
            (void)gp::platform::Futex::waitOnAddress(m_state, kMutexLockedBit | kMutexMayHaveWaitersBit);
        }

        m_ownerToken.store(callerToken, gp::MemoryOrder::Relaxed);
        m_recursionDepth = 1u;
#if GP_DO_CHECK
        Fiber::adjustActiveNonYieldingLockCount(1);
#endif
        return;
    }
}

}   // namespace gp::thread
