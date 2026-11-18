// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/sync/SharedMutex.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Futex.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/fibers/Fiber.hpp"

namespace gp::thread
{

GP_COLD void SharedMutex::lockSlow() noexcept
{
    const bool inChildFiber = Fiber::isExecutingInChildFiber();

    for (;;)
    {
        for (gp::UInt32 spin = 0u; spin < kMutexSpinIterationCount; ++spin)
        {
            gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
            if ((expected & (kSharedMutexWriterLockedBit | kSharedMutexReaderCountMask)) == 0u)
            {
                if (m_state.compareExchangeWeak(
                        expected,
                        expected | kSharedMutexWriterLockedBit,
                        gp::MemoryOrder::Acquire,
                        gp::MemoryOrder::Relaxed
                    ))
                {
#if GP_DO_CHECK
                    Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                    return;
                }
            }
            gp::platform::Thread::spinWait();
        }

        gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
        for (;;)
        {
            if ((expected & (kSharedMutexWriterLockedBit | kSharedMutexReaderCountMask)) == 0u)
            {
                if (m_state.compareExchangeWeak(
                        expected,
                        expected | kSharedMutexWriterLockedBit,
                        gp::MemoryOrder::Acquire,
                        gp::MemoryOrder::Relaxed
                    ))
                {
#if GP_DO_CHECK
                    Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                    return;
                }
                continue;
            }

            if ((expected & kSharedMutexWriterWaitingBit) == 0u)
            {
                if (!m_state.compareExchangeWeak(
                        expected,
                        expected | kSharedMutexWriterWaitingBit,
                        gp::MemoryOrder::Relaxed,
                        gp::MemoryOrder::Relaxed
                    ))
                {
                    continue;
                }
                expected |= kSharedMutexWriterWaitingBit;
            }

            break;
        }

        if (inChildFiber)
        {
            gp::platform::Thread::yieldExecution();
        }
        else
        {
            (void)gp::platform::Futex::waitOnAddress(m_state, expected);

            // After waking from futex, preserve kSharedMutexWriterWaitingBit when claiming exclusive ownership so
            // any additional parked writers are woken when we subsequently unlock().
            for (;;)
            {
                gp::UInt32 cur = m_state.load(gp::MemoryOrder::Relaxed);
                if ((cur & (kSharedMutexWriterLockedBit | kSharedMutexReaderCountMask)) == 0u)
                {
                    if (m_state.compareExchangeWeak(
                            cur,
                            cur | kSharedMutexWriterLockedBit | kSharedMutexWriterWaitingBit,
                            gp::MemoryOrder::Acquire,
                            gp::MemoryOrder::Relaxed
                        ))
                    {
#if GP_DO_CHECK
                        Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                        return;
                    }
                    continue;
                }

                if ((cur & kSharedMutexWriterWaitingBit) == 0u)
                {
                    if (!m_state.compareExchangeWeak(
                            cur, cur | kSharedMutexWriterWaitingBit, gp::MemoryOrder::Relaxed, gp::MemoryOrder::Relaxed
                        ))
                    {
                        continue;
                    }
                    cur |= kSharedMutexWriterWaitingBit;
                }

                (void)gp::platform::Futex::waitOnAddress(m_state, cur);
            }
        }
    }
}

GP_COLD void SharedMutex::unlockSlow() noexcept
{
    gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
    for (;;)
    {
        if ((expected & kSharedMutexWriterWaitingBit) != 0u)
        {
            const gp::UInt32 desired = expected & ~kSharedMutexWriterWaitingBit;
            if (m_state.compareExchangeWeak(expected, desired, gp::MemoryOrder::Release, gp::MemoryOrder::Relaxed))
            {
                gp::platform::Futex::wakeOne(m_state);
                return;
            }
            continue;
        }

        if ((expected & kSharedMutexReaderWaitingBit) != 0u)
        {
            const gp::UInt32 desired = expected & ~kSharedMutexReaderWaitingBit;
            if (m_state.compareExchangeWeak(expected, desired, gp::MemoryOrder::Release, gp::MemoryOrder::Relaxed))
            {
                m_readerWakeEpoch.fetchAdd(1u, gp::MemoryOrder::Release);
                gp::platform::Futex::wakeAll(m_readerWakeEpoch);
                return;
            }
            continue;
        }

        return;
    }
}

GP_COLD void SharedMutex::lockSharedSlow() noexcept
{
    const bool inChildFiber = Fiber::isExecutingInChildFiber();

    for (;;)
    {
        for (gp::UInt32 spin = 0u; spin < kMutexSpinIterationCount; ++spin)
        {
            gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
            if ((expected & (kSharedMutexWriterLockedBit | kSharedMutexWriterWaitingBit)) == 0u)
            {
                GP_ASSERT(
                    (expected & kSharedMutexReaderCountMask) < kSharedMutexMaxReaders,
                    "SharedMutex reader count overflow."
                );
                if (m_state.compareExchangeWeak(
                        expected, expected + kSharedMutexOneReader, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
                    ))
                {
#if GP_DO_CHECK
                    Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                    return;
                }
            }
            gp::platform::Thread::spinWait();
        }

        const gp::UInt32 observedEpoch = m_readerWakeEpoch.load(gp::MemoryOrder::Acquire);
        gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);

        for (;;)
        {
            if ((expected & (kSharedMutexWriterLockedBit | kSharedMutexWriterWaitingBit)) == 0u)
            {
                GP_ASSERT(
                    (expected & kSharedMutexReaderCountMask) < kSharedMutexMaxReaders,
                    "SharedMutex reader count overflow."
                );
                if (m_state.compareExchangeWeak(
                        expected, expected + kSharedMutexOneReader, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
                    ))
                {
#if GP_DO_CHECK
                    Fiber::adjustActiveNonYieldingLockCount(1);
#endif
                    return;
                }
                continue;
            }

            if ((expected & kSharedMutexReaderWaitingBit) == 0u)
            {
                if (!m_state.compareExchangeWeak(
                        expected,
                        expected | kSharedMutexReaderWaitingBit,
                        gp::MemoryOrder::Relaxed,
                        gp::MemoryOrder::Relaxed
                    ))
                {
                    continue;
                }
            }

            break;
        }

        if (inChildFiber)
        {
            gp::platform::Thread::yieldExecution();
        }
        else
        {
            (void)gp::platform::Futex::waitOnAddress(m_readerWakeEpoch, observedEpoch);
        }
    }
}

GP_COLD void SharedMutex::unlockSharedSlow() noexcept
{
    gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
    while ((expected & kSharedMutexReaderCountMask) == 0u && (expected & kSharedMutexWriterWaitingBit) != 0u)
    {
        if (m_state.compareExchangeWeak(
                expected, expected & ~kSharedMutexWriterWaitingBit, gp::MemoryOrder::Release, gp::MemoryOrder::Relaxed
            ))
        {
            gp::platform::Futex::wakeOne(m_state);
            return;
        }
    }
}

GP_COLD void SharedMutex::wakeWaitingReadersSlow() noexcept
{
    gp::UInt32 expected = m_state.load(gp::MemoryOrder::Relaxed);
    while ((expected & kSharedMutexReaderWaitingBit) != 0u && (expected & kSharedMutexWriterWaitingBit) == 0u)
    {
        if (m_state.compareExchangeWeak(
                expected, expected & ~kSharedMutexReaderWaitingBit, gp::MemoryOrder::Release, gp::MemoryOrder::Relaxed
            ))
        {
            m_readerWakeEpoch.fetchAdd(1u, gp::MemoryOrder::Release);
            gp::platform::Futex::wakeAll(m_readerWakeEpoch);
            return;
        }
    }
}

}   // namespace gp::thread
