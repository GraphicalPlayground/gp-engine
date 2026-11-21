// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/linux/system/LinuxFutex.hpp"
#include "memory/MemoryOrder.hpp"
#include <cerrno>
#include <ctime>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace gp::platform::linux
{

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

    struct timespec relativeTimeout{};
    const struct timespec* timeoutPtr = nullptr;

    if (timeoutNanoseconds != kInfiniteFutexTimeoutNs)
    {
        relativeTimeout.tv_sec = static_cast<time_t>(timeoutNanoseconds / 1'000'000'000ULL);
        relativeTimeout.tv_nsec = static_cast<long>(timeoutNanoseconds % 1'000'000'000ULL);
        timeoutPtr = &relativeTimeout;
    }

    const long rc = ::syscall(
        SYS_futex,
        reinterpret_cast<const gp::UInt32*>(&address),
        FUTEX_WAIT_PRIVATE,
        expectedValue,
        timeoutPtr,
        nullptr,
        0
    );

    if (rc == 0)
    {
        return FutexResult::Success;
    }

    const int err = errno;
    if (err == EAGAIN || err == EWOULDBLOCK)
    {
        return FutexResult::ValueMismatch;
    }
    if (err == ETIMEDOUT)
    {
        return FutexResult::TimedOut;
    }
    if (err == EINTR)
    {
        return FutexResult::Success;
    }

    return FutexResult::WaitFailed;
}

void Futex::wakeOne(const gp::thread::Atomic<gp::UInt32>& address) noexcept
{
    (void)::syscall(
        SYS_futex, reinterpret_cast<const gp::UInt32*>(&address), FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0
    );
}

void Futex::wakeAll(const gp::thread::Atomic<gp::UInt32>& address) noexcept
{
    (void)::syscall(
        SYS_futex, reinterpret_cast<const gp::UInt32*>(&address), FUTEX_WAKE_PRIVATE, 0x7FFFFFFF, nullptr, nullptr, 0
    );
}

}   // namespace gp::platform::linux
