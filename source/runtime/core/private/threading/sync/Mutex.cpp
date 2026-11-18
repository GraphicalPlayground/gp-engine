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

        const gp::UInt32 prev = m_state.fetchOr(kMutexLockedBit | kMutexMayHaveWaitersBit, gp::MemoryOrder::Acquire);
        if ((prev & kMutexLockedBit) == 0u)
        {
            // If we stole the lock while setting kMutexMayHaveWaitersBit and no prior waiters were flagged,
            // clear the redundant waiter bit so unlock() stays on the 1-instruction fast path.
            if ((prev & kMutexMayHaveWaitersBit) == 0u)
            {
                m_state.store(kMutexLockedBit, gp::MemoryOrder::Relaxed);
            }
#if GP_DO_CHECK
            Fiber::adjustActiveNonYieldingLockCount(1);
#endif
            return;
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
