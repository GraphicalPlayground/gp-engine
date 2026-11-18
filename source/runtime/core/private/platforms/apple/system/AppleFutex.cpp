// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/apple/system/AppleFutex.hpp"
#include "memory/MemoryOrder.hpp"
#include <cerrno>

extern "C"
{
    int __ulock_wait(gp::UInt32 operation, void* addr, gp::UInt64 value, gp::UInt32 timeoutMicroseconds) noexcept;
    int __ulock_wake(gp::UInt32 operation, void* addr, gp::UInt64 wakeValue) noexcept;
}

namespace gp::platform::apple
{

namespace
{

inline constexpr gp::UInt32 kUlockCompareAndWait = 1u;
inline constexpr gp::UInt32 kUlockWakeAllFlag = 0x00000100u;

}   // namespace

FutexResult Futex::waitOnAddress(
    const gp::thread::Atomic<gp::UInt32>& address, const gp::UInt32 expectedValue, const gp::UInt64 timeoutNanoseconds
) noexcept
{
    if (address.load(gp::MemoryOrder::Acquire) != expectedValue)
    {
        return FutexResult::ValueMismatch;
    }

    if (timeoutNanoseconds == 0ULL)
    {
        return FutexResult::TimedOut;
    }

    gp::UInt32 timeoutMicroseconds = 0u;
    if (timeoutNanoseconds != kInfiniteFutexTimeoutNs)
    {
        const gp::UInt64 roundedUs = (timeoutNanoseconds + 999ULL) / 1'000ULL;
        if (roundedUs == 0ULL)
        {
            timeoutMicroseconds = 1u;
        }
        else if (roundedUs > 0xFFFFFFFFULL)
        {
            timeoutMicroseconds = 0xFFFFFFFFu;
        }
        else
        {
            timeoutMicroseconds = static_cast<gp::UInt32>(roundedUs);
        }
    }

    const int rc = ::__ulock_wait(
        kUlockCompareAndWait,
        const_cast<gp::thread::Atomic<gp::UInt32>*>(&address),
        static_cast<gp::UInt64>(expectedValue),
        timeoutMicroseconds
    );

    if (rc >= 0)
    {
        return FutexResult::Success;
    }

    const int err = (rc < -1) ? -rc : errno;
    if (err == ETIMEDOUT)
    {
        return FutexResult::TimedOut;
    }
    if (err == EFAULT || err == EINTR)
    {
        return FutexResult::Success;
    }

    return (address.load(gp::MemoryOrder::Acquire) != expectedValue) ? FutexResult::ValueMismatch
                                                                     : FutexResult::WaitFailed;
}

void Futex::wakeOne(const gp::thread::Atomic<gp::UInt32>& address) noexcept
{
    (void)::__ulock_wake(kUlockCompareAndWait, const_cast<gp::thread::Atomic<gp::UInt32>*>(&address), 0ULL);
}

void Futex::wakeAll(const gp::thread::Atomic<gp::UInt32>& address) noexcept
{
    (void)::__ulock_wake(
        kUlockCompareAndWait | kUlockWakeAllFlag, const_cast<gp::thread::Atomic<gp::UInt32>*>(&address), 0ULL
    );
}

}   // namespace gp::platform::apple
