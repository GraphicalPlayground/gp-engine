// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/sync/Mutex.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Futex.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/fibers/Fiber.hpp"

namespace gp::thread
{

GP_COLD void Mutex::lockSlow() noexcept
{
    const bool inChildFiber = Fiber::isExecutingInChildFiber();

    for (;;)
    {
        for (gp::UInt32 spin = 0u; spin < kMutexSpinIterationCount; ++spin)
        {
            const gp::UInt32 current = m_state.load(gp::MemoryOrder::Relaxed);
            if ((current & kMutexLockedBit) == 0u)
            {
                if ((m_state.fetchOr(kMutexLockedBit, gp::MemoryOrder::Acquire) & kMutexLockedBit) == 0u)
                {
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

        // Once woken from Futex::waitOnAddress, re-acquire with kMutexMayHaveWaitersBit preserved so any additional
        // threads parked on m_state are guaranteed to be woken when we unlock().
        while ((m_state.exchange(kMutexLockedBit | kMutexMayHaveWaitersBit, gp::MemoryOrder::Acquire) &
                kMutexLockedBit) != 0u)
        {
            (void)gp::platform::Futex::waitOnAddress(m_state, kMutexLockedBit | kMutexMayHaveWaitersBit);
        }
#if GP_DO_CHECK
        Fiber::adjustActiveNonYieldingLockCount(1);
#endif
        return;
    }
}

GP_COLD void Mutex::unlockSlow() noexcept
{
    gp::platform::Futex::wakeOne(m_state);
}

}   // namespace gp::thread
