// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/sync/OnceFlag.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Futex.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/fibers/Fiber.hpp"
#include "threading/sync/MutexTypes.hpp"

namespace gp::thread
{

GP_COLD bool OnceFlag::tryBeginExecutionSlow() noexcept
{
    const bool inChildFiber = Fiber::isExecutingInChildFiber();

    for (;;)
    {
        gp::UInt32 current = m_state.load(gp::MemoryOrder::Acquire);
        if (current == static_cast<gp::UInt32>(OnceState::Completed))
        {
            return false;
        }

        if (current == static_cast<gp::UInt32>(OnceState::NotStarted))
        {
            if (m_state.compareExchangeStrong(
                    current,
                    static_cast<gp::UInt32>(OnceState::Running),
                    gp::MemoryOrder::Acquire,
                    gp::MemoryOrder::Acquire
                ))
            {
                return true;
            }
            continue;
        }

        for (gp::UInt32 spin = 0u; spin < kMutexSpinIterationCount; ++spin)
        {
            current = m_state.load(gp::MemoryOrder::Acquire);
            if (current == static_cast<gp::UInt32>(OnceState::Completed))
            {
                return false;
            }
            if (current == static_cast<gp::UInt32>(OnceState::NotStarted))
            {
                break;
            }
            gp::platform::Thread::spinWait();
        }

        if (inChildFiber)
        {
            gp::platform::Thread::yieldExecution();
            continue;
        }

        current = m_state.load(gp::MemoryOrder::Acquire);
        if (current == static_cast<gp::UInt32>(OnceState::Completed))
        {
            return false;
        }
        if (current == static_cast<gp::UInt32>(OnceState::NotStarted))
        {
            continue;
        }

        if (current == static_cast<gp::UInt32>(OnceState::Running))
        {
            if (!m_state.compareExchangeStrong(
                    current,
                    static_cast<gp::UInt32>(OnceState::RunningWithWaiters),
                    gp::MemoryOrder::Relaxed,
                    gp::MemoryOrder::Acquire
                ))
            {
                if (current == static_cast<gp::UInt32>(OnceState::Completed))
                {
                    return false;
                }
                if (current == static_cast<gp::UInt32>(OnceState::NotStarted))
                {
                    continue;
                }
            }
        }

        (void)gp::platform::Futex::waitOnAddress(m_state, static_cast<gp::UInt32>(OnceState::RunningWithWaiters));
    }
}

GP_COLD void OnceFlag::completeExecutionSlow() noexcept
{
    gp::platform::Futex::wakeAll(m_state);
}

}   // namespace gp::thread
